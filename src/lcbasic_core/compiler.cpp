#include "compiler.h"
#include "builtins.h"

#include <algorithm>

Compiler::Compiler(const Program& program) : program_(program) {}

std::uint8_t Compiler::add_constant(const Value& v) {
  constants_.push_back(v);
  return static_cast<std::uint8_t>(constants_.size() - 1);
}

std::uint8_t Compiler::variable_index(const std::string& name) {
  auto it = variable_table_.find(name);
  if (it != variable_table_.end()) return it->second;
  std::uint8_t idx = static_cast<std::uint8_t>(variable_table_.size());
  variable_table_.emplace(name, idx);
  variable_names_.push_back(name);
  return idx;
}

void Compiler::emit(OpCode op, std::int32_t line, std::uint32_t operand) {
  code_.push_back(Instruction{op, operand, line});
}

void Compiler::record_fixup(std::size_t code_index,
                            std::int32_t target_line) {
  fixups_.push_back(Fixup{code_index, target_line});
}

std::optional<std::size_t> Compiler::find_line(std::int32_t line) const {
  auto it = std::lower_bound(
      line_table_.begin(), line_table_.end(), line,
      [](const LineEntry& e, std::int32_t v) { return e.line_number < v; });
  if (it != line_table_.end() && it->line_number == line) return it->pc_offset;
  return std::nullopt;
}

void Compiler::resolve_fixups() {
  for (const auto& fixup : fixups_) {
    auto pc = find_line(fixup.target_line);
    if (!pc) {
      errors_.push_back("undefined line number " +
                        std::to_string(fixup.target_line));
      continue;
    }
    code_[fixup.code_index].operand = static_cast<std::uint32_t>(*pc);
  }
}

void Compiler::compile_program() {
  // Pre-pass: record the DATA pool index at the start of each line so that
  // RESTORE <line> can be resolved at compile time.
  std::size_t data_count = 0;
  for (const auto& [line_number, stmt] : program_.lines) {
    data_line_index_.push_back({line_number, data_count});
    if (auto* d = std::get_if<DataStmt>(&stmt.data)) {
      data_count += d->values.size();
    }
  }

  for (const auto& [line_number, stmt] : program_.lines) {
    line_table_.push_back(LineEntry{line_number, next_pc()});
    compile_statement(stmt);
  }
  emit(OpCode::Halt, program_.lines.empty() ? 0
                                            : program_.lines.back().first);
  resolve_fixups();
}

std::size_t Compiler::data_index_for_line(std::int32_t line) const {
  // The first line >= the target determines the DATA position.
  for (const auto& [ln, idx] : data_line_index_) {
    if (ln >= line) return idx;
  }
  return data_values_.size();
}

void Compiler::compile_statement(const Statement& stmt) {
  std::int32_t line = stmt.line_number;

  std::visit(
      [&](auto&& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, PrintStmt>) {
          for (std::size_t i = 0; i < s.args.size(); ++i) {
            compile_expr(*s.args[i], line);
            emit(OpCode::Print, line);
            TokenType sep =
                (i < s.separators.size()) ? s.separators[i] : TokenType::Eof;
            if (sep == TokenType::Comma) {
              // Comma advances to the next 14-column print zone.
              emit(OpCode::NextZone, line);
            }
            // Semicolon or none: no separator.
          }
          if (!s.trailing_separator) {
            auto nl_idx = add_constant(Value(std::string("\n")));
            emit(OpCode::Constant, line, nl_idx);
            emit(OpCode::Print, line);
          }
        } else if constexpr (std::is_same_v<T, LetStmt>) {
          if (!s.indices.empty()) {
            // Array assignment: arr(i, j) = value
            // Push indices first (reverse order for stack: last index
            // pushed last, so ArrayStore pops them in reverse).
            // We push them in order; ArrayStore will pop them.
            for (auto& idx : s.indices) {
              compile_expr(*idx, line);
            }
            compile_expr(*s.value, line);
            std::uint8_t vidx = variable_index(s.var);
            std::uint8_t num_indices =
                static_cast<std::uint8_t>(s.indices.size());
            std::uint32_t operand =
                (static_cast<std::uint32_t>(num_indices) << 8) | vidx;
            emit(OpCode::ArrayStore, line, operand);
          } else {
            compile_expr(*s.value, line);
            std::uint8_t vidx = variable_index(s.var);
            emit(OpCode::StoreVar, line, vidx);
          }
        } else if constexpr (std::is_same_v<T, GotoStmt>) {
          emit(OpCode::Jmp, line, 0);
          record_fixup(code_.size() - 1, s.target_line);
        } else if constexpr (std::is_same_v<T, IfStmt>) {
          compile_expr(*s.condition, line);
          emit(OpCode::JmpFalse, line, 0);
          std::size_t jmp_false_idx = code_.size() - 1;

          if (s.then_line >= 0) {
            emit(OpCode::Jmp, line, 0);
            record_fixup(code_.size() - 1, s.then_line);
          } else {
            for (auto& inner : s.then_stmts) {
              Statement inner_stmt = std::move(*inner);
              inner_stmt.line_number = line;
              compile_statement(inner_stmt);
            }
          }

          if (s.else_line >= 0 || !s.else_stmts.empty()) {
            emit(OpCode::Jmp, line, 0);
            std::size_t skip_else_idx = code_.size() - 1;
            auto here_pc = next_pc();
            code_[jmp_false_idx].operand =
                static_cast<std::uint32_t>(here_pc);

            if (s.else_line >= 0) {
              emit(OpCode::Jmp, line, 0);
              record_fixup(code_.size() - 1, s.else_line);
            } else {
              for (auto& inner : s.else_stmts) {
                Statement inner_stmt = std::move(*inner);
                inner_stmt.line_number = line;
                compile_statement(inner_stmt);
              }
            }
            auto after_else_pc = next_pc();
            code_[skip_else_idx].operand =
                static_cast<std::uint32_t>(after_else_pc);
          } else {
            auto after_then_pc = next_pc();
            code_[jmp_false_idx].operand =
                static_cast<std::uint32_t>(after_then_pc);
          }
        } else if constexpr (std::is_same_v<T, ForStmt>) {
          compile_expr(*s.start, line);
          std::uint8_t vidx = variable_index(s.var);
          emit(OpCode::StoreVar, line, vidx);
          compile_expr(*s.end, line);
          compile_expr(*s.step, line);
          emit(OpCode::ForInit, line, vidx);
        } else if constexpr (std::is_same_v<T, NextStmt>) {
          std::uint8_t vidx = variable_index(s.var);
          emit(OpCode::ForNext, line, vidx);
        } else if constexpr (std::is_same_v<T, GosubStmt>) {
          emit(OpCode::Call, line, 0);
          record_fixup(code_.size() - 1, s.target_line);
        } else if constexpr (std::is_same_v<T, ReturnStmt>) {
          emit(OpCode::Ret, line);
        } else if constexpr (std::is_same_v<T, EndStmt>) {
          emit(OpCode::Halt, line);
        } else if constexpr (std::is_same_v<T, StopStmt>) {
          emit(OpCode::Stop, line);
        } else if constexpr (std::is_same_v<T, DimStmt>) {
          // DIM may declare several arrays in one statement.
          for (const auto& entry : s.arrays) {
            for (const auto& ub : entry.upper_bounds) {
              compile_expr(*ub, line);
            }
            std::uint8_t vidx = variable_index(entry.name);
            std::uint8_t num_dims =
                static_cast<std::uint8_t>(entry.upper_bounds.size());
            std::uint32_t operand =
                (static_cast<std::uint32_t>(num_dims) << 8) | vidx;
            emit(OpCode::Dim, line, operand);
          }
        } else if constexpr (std::is_same_v<T, OptionBaseStmt>) {
          emit(OpCode::SetOptionBase, line,
               static_cast<std::uint32_t>(s.base));
        } else if constexpr (std::is_same_v<T, DataStmt>) {
          // Collect DATA values for the VM's DataPool (runtime init).
          for (const auto& v : s.values) {
            data_values_.push_back(v);
          }
          // DATA is a non-executing statement; no code emitted.
        } else if constexpr (std::is_same_v<T, ReadStmt>) {
          for (const auto& target : s.targets) {
            if (target.is_array) {
              // Compile index expressions onto the stack.
              for (auto& idx : target.indices) {
                compile_expr(*idx, line);
              }
              std::uint8_t vidx = variable_index(target.name);
              std::uint8_t num_indices =
                  static_cast<std::uint8_t>(target.indices.size());
              std::uint32_t operand =
                  (static_cast<std::uint32_t>(num_indices) << 8) | vidx;
              emit(OpCode::ReadArray, line, operand);
            } else {
              std::uint8_t vidx = variable_index(target.name);
              emit(OpCode::ReadVar, line, vidx);
            }
          }
        } else if constexpr (std::is_same_v<T, RestoreStmt>) {
          if (s.target_line >= 0) {
            emit(OpCode::RestoreIndex, line,
                 static_cast<std::uint32_t>(data_index_for_line(s.target_line)));
          } else {
            emit(OpCode::Restore, line);
          }
        } else if constexpr (std::is_same_v<T, DefFnStmt>) {
          // DEF FN name(p1, p2, ...) = expr
          // Compile the body as a subroutine that leaves its result on the
          // value stack. FnBind/FnRet save and restore parameters so that
          // recursion works.
          emit(OpCode::Jmp, line, 0);  // skip body during normal execution
          std::size_t skip_idx = code_.size() - 1;

          FnDesc desc;
          desc.body_pc = next_pc();

          for (const auto& p : s.params) {
            std::string mangled =
                std::string("\x01FN_") + s.name + "_" + p;
            rename_[p] = mangled;
            desc.param_vars.push_back(variable_index(mangled));
          }

          compile_expr(*s.body, line);

          for (const auto& p : s.params) rename_.erase(p);

          emit(OpCode::FnRet, line);

          // Patch the skip jump to after the body.
          code_[skip_idx].operand = static_cast<std::uint32_t>(next_pc());

          int fn_id = static_cast<int>(fn_descs_.size());
          fn_descs_.push_back(std::move(desc));
          functions_[s.name] = fn_id;
        } else if constexpr (std::is_same_v<T, DefTypeStmt>) {
          // Emit one DefType opcode per letter range. Operand encoding:
          // bits 0-7 = lo letter, bits 8-15 = hi letter, bits 16-23 = type.
          for (auto& [lo, hi] : s.ranges) {
            std::uint32_t operand =
                (static_cast<std::uint32_t>(lo) & 0xFF) |
                ((static_cast<std::uint32_t>(hi) & 0xFF) << 8) |
                ((static_cast<std::uint32_t>(s.type) & 0xFF) << 16);
            emit(OpCode::DefType, line, operand);
          }
        } else if constexpr (std::is_same_v<T, ChDirStmt>) {
          compile_expr(*s.path, line);
          emit(OpCode::ChDir, line, 0);
        } else if constexpr (std::is_same_v<T, MkDirStmt>) {
          compile_expr(*s.path, line);
          emit(OpCode::MkDir, line, 0);
        } else if constexpr (std::is_same_v<T, RmDirStmt>) {
          compile_expr(*s.path, line);
          emit(OpCode::RmDir, line, 0);
        } else if constexpr (std::is_same_v<T, KillStmt>) {
          compile_expr(*s.path, line);
          emit(OpCode::KillFile, line, 0);
        } else if constexpr (std::is_same_v<T, NameStmt>) {
          compile_expr(*s.new_name, line);
          compile_expr(*s.old_name, line);
          emit(OpCode::NameFile, line, 0);
        } else if constexpr (std::is_same_v<T, FilesStmt>) {
          if (s.pattern) {
            compile_expr(*s.pattern, line);
          } else {
            auto idx = add_constant(Value(std::string("*.*")));
            emit(OpCode::Constant, line, idx);
          }
          emit(OpCode::Files, line, 0);
        } else if constexpr (std::is_same_v<T, ShellStmt>) {
          if (s.command) {
            compile_expr(*s.command, line);
          } else {
            auto idx = add_constant(Value(std::string("")));
            emit(OpCode::Constant, line, idx);
          }
          emit(OpCode::Shell, line, 0);
        } else if constexpr (std::is_same_v<T, OnEventStmt>) {
          std::uint32_t packed =
              (static_cast<std::uint32_t>(s.source) & 0xFFFF) |
              ((static_cast<std::uint32_t>(s.kind) & 0xFF) << 16);
          auto idx = add_constant(Value(static_cast<double>(packed)));
          emit(OpCode::Constant, line, idx);
          emit(OpCode::OnEvent, line, 0);
          record_fixup(code_.size() - 1, s.target_line);
        } else if constexpr (std::is_same_v<T, EventControlStmt>) {
          std::uint32_t operand =
              (static_cast<std::uint32_t>(s.source) & 0xFFFF) |
              ((static_cast<std::uint32_t>(s.kind) & 0xFF) << 16) |
              ((static_cast<std::uint32_t>(s.action) & 0xFF) << 24);
          emit(OpCode::EventControl, line, operand);
        } else if constexpr (std::is_same_v<T, OpenStmt>) {
          if (s.mode == "R" || s.mode == "B") {
            // RANDOM/BINARY file: stack is [filename, record_len].
            compile_expr(*s.filename, line);
            if (s.record_len) {
              compile_expr(*s.record_len, line);
            } else {
              auto idx = add_constant(Value(128.0));
              emit(OpCode::Constant, line, idx);
            }
            emit(OpCode::RandomOpen, line,
                 static_cast<std::uint32_t>(s.filenum));
          } else {
            // Sequential file: stack is [filename, mode, filenum].
            compile_expr(*s.filename, line);
            auto mode_idx = add_constant(Value(s.mode));
            emit(OpCode::Constant, line, mode_idx);
            auto fn_idx = add_constant(Value(static_cast<double>(s.filenum)));
            emit(OpCode::Constant, line, fn_idx);
            emit(OpCode::Open, line, 0);
          }
        } else if constexpr (std::is_same_v<T, FieldStmt>) {
          for (const auto& [len_expr, var] : s.fields) {
            compile_expr(*len_expr, line);
            auto idx = add_constant(
                Value(static_cast<double>(variable_index(var))));
            emit(OpCode::Constant, line, idx);
            emit(OpCode::Field, line,
                 static_cast<std::uint32_t>(s.filenum));
          }
        } else if constexpr (std::is_same_v<T, LSetStmt>) {
          compile_expr(*s.value, line);
          std::uint32_t operand =
              (static_cast<std::uint32_t>(variable_index(s.var)) << 1) |
              (s.right ? 1u : 0u);
          emit(OpCode::LSet, line, operand);
        } else if constexpr (std::is_same_v<T, GetPutStmt>) {
          if (s.record) {
            compile_expr(*s.record, line);
          } else {
            auto idx = add_constant(Value(-1.0));
            emit(OpCode::Constant, line, idx);
          }
          emit(s.put ? OpCode::PutRecord : OpCode::GetRecord, line,
               static_cast<std::uint32_t>(s.filenum));
        } else if constexpr (std::is_same_v<T, CloseStmt>) {
          auto fn_idx = add_constant(Value(static_cast<double>(s.filenum)));
          emit(OpCode::Constant, line, fn_idx);
          emit(OpCode::Close, line, 0);
        } else if constexpr (std::is_same_v<T, PrintFileStmt>) {
          // PRINT #filenum, values (separators written to the file too)
          for (std::size_t i = 0; i < s.args.size(); ++i) {
            auto fn_idx = add_constant(Value(static_cast<double>(s.filenum)));
            emit(OpCode::Constant, line, fn_idx);
            compile_expr(*s.args[i], line);
            emit(OpCode::PrintFile, line, 0);
            TokenType sep =
                (i < s.separators.size()) ? s.separators[i] : TokenType::Eof;
            if (sep == TokenType::Comma) {
              auto fn2 =
                  add_constant(Value(static_cast<double>(s.filenum)));
              emit(OpCode::Constant, line, fn2);
              auto sp_idx = add_constant(Value(std::string(" ")));
              emit(OpCode::Constant, line, sp_idx);
              emit(OpCode::PrintFile, line, 0);
            }
          }
          if (!s.trailing_separator) {
            auto fn_idx = add_constant(Value(static_cast<double>(s.filenum)));
            emit(OpCode::Constant, line, fn_idx);
            auto nl_idx = add_constant(Value(std::string("\n")));
            emit(OpCode::Constant, line, nl_idx);
            emit(OpCode::PrintFile, line, 0);
          }
        } else if constexpr (std::is_same_v<T, InputFileStmt>) {
          auto fn_idx = add_constant(Value(static_cast<double>(s.filenum)));
          emit(OpCode::Constant, line, fn_idx);
          for (const auto& target : s.targets) {
            for (auto& idx : target.indices) {
              compile_expr(*idx, line);
            }
            auto n_idx = add_constant(
                Value(static_cast<double>(target.indices.size())));
            emit(OpCode::Constant, line, n_idx);
            auto v_idx = add_constant(
                Value(static_cast<double>(variable_index(target.name))));
            emit(OpCode::Constant, line, v_idx);
          }
          emit(OpCode::InputFile,
               line,
               static_cast<std::uint32_t>(s.targets.size()) |
                   (s.line_input ? 0x100u : 0u));
        } else if constexpr (std::is_same_v<T, OnErrorStmt>) {
          if (s.target_line == 0) {
            // ON ERROR GOTO 0 disables the handler.
            emit(OpCode::OnError, line, 0);
          } else {
            emit(OpCode::OnError, line, 0);
            record_fixup(code_.size() - 1, s.target_line);
          }
        } else if constexpr (std::is_same_v<T, ResumeStmt>) {
          if (s.next) {
            emit(OpCode::Resume, line, 1); // operand=1 means NEXT
          } else if (s.target_line >= 0) {
            emit(OpCode::Resume, line, 0);
            record_fixup(code_.size() - 1, s.target_line);
          } else {
            emit(OpCode::Resume, line, 0); // operand=0 means same line
            record_fixup(code_.size() - 1, line);
          }
        } else if constexpr (std::is_same_v<T, InputStmt>) {
          // Prompt (or empty string), then for each target: indices, count,
          // variable index. Then Input with the target count.
          if (s.prompt) {
            compile_expr(*s.prompt, line);
          } else {
            auto idx = add_constant(Value(std::string("")));
            emit(OpCode::Constant, line, idx);
          }
          for (const auto& target : s.targets) {
            for (auto& idx : target.indices) {
              compile_expr(*idx, line);
            }
            auto n_idx = add_constant(
                Value(static_cast<double>(target.indices.size())));
            emit(OpCode::Constant, line, n_idx);
            auto v_idx = add_constant(
                Value(static_cast<double>(variable_index(target.name))));
            emit(OpCode::Constant, line, v_idx);
          }
          emit(OpCode::Input,
               line,
               static_cast<std::uint32_t>(s.targets.size()) |
                   (s.line_input ? 0x100u : 0u) |
                   (s.prompt_comma ? 0x200u : 0u));
        } else if constexpr (std::is_same_v<T, WhileStmt>) {
          std::size_t cond_pc = next_pc();
          compile_expr(*s.condition, line);
          emit(OpCode::JmpFalse, line, 0);
          while_stack_.push_back(WhileFrame{cond_pc, code_.size() - 1});
        } else if constexpr (std::is_same_v<T, WendStmt>) {
          if (while_stack_.empty()) {
            errors_.push_back("WEND without WHILE");
          } else {
            WhileFrame frame = while_stack_.back();
            while_stack_.pop_back();
            emit(OpCode::Jmp, line,
                 static_cast<std::uint32_t>(frame.cond_pc));
            code_[frame.jmpfalse_idx].operand =
                static_cast<std::uint32_t>(next_pc());
          }
        } else if constexpr (std::is_same_v<T, OnGotoStmt>) {
          std::string sel_var = std::string("\x01ON_SEL");
          compile_expr(*s.selector, line);
          std::uint8_t sel_idx = variable_index(sel_var);
          emit(OpCode::StoreVar, line, sel_idx);
          for (std::size_t i = 0; i < s.targets.size(); i++) {
            emit(OpCode::LoadVar, line, sel_idx);
            auto c = add_constant(Value(static_cast<double>(i + 1)));
            emit(OpCode::Constant, line, c);
            emit(OpCode::Eq, line);
            emit(OpCode::JmpFalse, line, 0);
            std::size_t skip_idx = code_.size() - 1;
            emit(s.gosub ? OpCode::Call : OpCode::Jmp, line, 0);
            record_fixup(code_.size() - 1, s.targets[i]);
            code_[skip_idx].operand = static_cast<std::uint32_t>(next_pc());
          }
        } else if constexpr (std::is_same_v<T, ClsStmt>) {
          emit(OpCode::Cls, line);
        } else if constexpr (std::is_same_v<T, LocateStmt>) {
          compile_expr(*s.col, line);
          compile_expr(*s.row, line);
          emit(OpCode::Locate, line);
        } else if constexpr (std::is_same_v<T, ColorStmt>) {
          if (s.bg) {
            compile_expr(*s.bg, line);
          } else {
            auto c = add_constant(Value(0.0));
            emit(OpCode::Constant, line, c);
          }
          compile_expr(*s.fg, line);
          emit(OpCode::Color, line);
        } else if constexpr (std::is_same_v<T, ScreenStmt>) {
          // SCREEN is accepted but is a no-op in text mode.
          compile_expr(*s.mode, line);
          emit(OpCode::Pop, line);
        } else if constexpr (std::is_same_v<T, WidthStmt>) {
          compile_expr(*s.cols, line);
          emit(OpCode::Pop, line);
        } else if constexpr (std::is_same_v<T, SwapStmt>) {
          // SWAP a, b using untyped temp slots (so strings work too).
          auto load_lvalue = [&](const LValue& lv) {
            if (lv.is_array()) {
              for (auto& idx : lv.indices) compile_expr(*idx, line);
              std::uint8_t vidx = variable_index(lv.name);
              std::uint8_t n = static_cast<std::uint8_t>(lv.indices.size());
              emit(OpCode::ArrayLoad, line,
                   (static_cast<std::uint32_t>(n) << 8) | vidx);
            } else {
              emit(OpCode::LoadVar, line, variable_index(lv.name));
            }
          };
          auto store_lvalue = [&](const LValue& lv, std::uint32_t slot) {
            if (lv.is_array()) {
              for (auto& idx : lv.indices) compile_expr(*idx, line);
              emit(OpCode::LoadTemp, line, slot);
              std::uint8_t vidx = variable_index(lv.name);
              std::uint8_t n = static_cast<std::uint8_t>(lv.indices.size());
              emit(OpCode::ArrayStore, line,
                   (static_cast<std::uint32_t>(n) << 8) | vidx);
            } else {
              emit(OpCode::LoadTemp, line, slot);
              emit(OpCode::StoreVar, line, variable_index(lv.name));
            }
          };
          load_lvalue(s.a);
          emit(OpCode::StoreTemp, line, 0);
          load_lvalue(s.b);
          emit(OpCode::StoreTemp, line, 1);
          store_lvalue(s.a, 1);
          store_lvalue(s.b, 0);
        } else if constexpr (std::is_same_v<T, EraseStmt>) {
          for (const auto& name : s.names) {
            emit(OpCode::Erase, line, variable_index(name));
          }
        } else if constexpr (std::is_same_v<T, ClearStmt>) {
          emit(OpCode::Clear, line);
        } else if constexpr (std::is_same_v<T, RandomizeStmt>) {
          if (s.seed) {
            compile_expr(*s.seed, line);
          } else {
            auto idx = add_constant(Value(0.0));
            emit(OpCode::Constant, line, idx);
          }
          emit(OpCode::Randomize, line);
        } else if constexpr (std::is_same_v<T, WriteFileStmt>) {
          auto fn_idx = add_constant(Value(static_cast<double>(s.filenum)));
          emit(OpCode::Constant, line, fn_idx);
          for (auto& a : s.args) compile_expr(*a, line);
          emit(OpCode::WriteFile, line,
               static_cast<std::uint32_t>(s.args.size()));
        } else if constexpr (std::is_same_v<T, ErrorStmt>) {
          compile_expr(*s.code, line);
          emit(OpCode::RaiseError, line);
        } else if constexpr (std::is_same_v<T, MidAssignStmt>) {
          compile_expr(*s.value, line);
          compile_expr(*s.start, line);
          if (s.length) {
            compile_expr(*s.length, line);
          } else {
            auto c = add_constant(Value(-1.0));
            emit(OpCode::Constant, line, c);
          }
          emit(OpCode::MidAssign, line, variable_index(s.target));
        } else if constexpr (std::is_same_v<T, StubStmt>) {
          // Parsed but unimplemented statement: no code emitted.
        }
      },
      stmt.data);
}

void Compiler::compile_expr(const Expr& expr, std::int32_t line) {
  std::visit(
      [&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, NumberExpr>) {
          auto idx = add_constant(Value(arg.value));
          emit(OpCode::Constant, line, idx);
        } else if constexpr (std::is_same_v<T, StringExpr>) {
          auto idx = add_constant(Value(arg.value));
          emit(OpCode::Constant, line, idx);
        } else if constexpr (std::is_same_v<T, VariableExpr>) {
          std::string name = arg.name;
          auto it = rename_.find(name);
          if (it != rename_.end()) name = it->second;
          std::uint8_t vidx = variable_index(name);
          emit(OpCode::LoadVar, line, vidx);
        } else if constexpr (std::is_same_v<T, ArrayRefExpr>) {
          for (auto& idx : arg.indices) {
            compile_expr(*idx, line);
          }
          std::uint8_t vidx = variable_index(arg.name);
          std::uint8_t num_indices =
              static_cast<std::uint8_t>(arg.indices.size());
          std::uint32_t operand =
              (static_cast<std::uint32_t>(num_indices) << 8) | vidx;
          emit(OpCode::ArrayLoad, line, operand);
        } else if constexpr (std::is_same_v<T, FnCallExpr>) {
          auto it = functions_.find(arg.name);
          if (it == functions_.end()) {
            errors_.push_back("undefined function FN " + arg.name);
            emit(OpCode::Constant, line, add_constant(Value(0.0)));
            return;
          }
          int fn_id = it->second;
          const FnDesc& desc = fn_descs_[static_cast<std::size_t>(fn_id)];
          if (desc.param_vars.size() != arg.args.size()) {
            errors_.push_back("wrong number of arguments to FN " + arg.name);
          }
          // Push the argument values, then bind and call.
          for (auto& a : arg.args) compile_expr(*a, line);
          emit(OpCode::FnBind, line, static_cast<std::uint32_t>(fn_id));
        } else if constexpr (std::is_same_v<T, BuiltinExpr>) {
          if (arg.name == "TIMER") {
            emit(OpCode::TimerFunc, line);
          } else {
            errors_.push_back("unknown builtin: " + arg.name);
            emit(OpCode::Constant, line, add_constant(Value(0.0)));
          }
        } else if constexpr (std::is_same_v<T, FunctionExpr>) {
          auto b = find_builtin(arg.name);
          if (!b) {
            errors_.push_back("unknown function: " + arg.name);
            emit(OpCode::Constant, line, add_constant(Value(0.0)));
            return;
          }
          for (auto& a : arg.args) compile_expr(*a, line);
          std::uint32_t operand =
              (static_cast<std::uint32_t>(*b) << 8) |
              (static_cast<std::uint32_t>(arg.args.size()) & 0xFF);
          emit(OpCode::CallBuiltin, line, operand);
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
          if (arg.op == TokenType::Not) {
            compile_expr(*arg.operand, line);
            emit(OpCode::Not, line);
          } else if (arg.op == TokenType::Minus) {
            auto z = add_constant(Value(0.0));
            emit(OpCode::Constant, line, z);
            compile_expr(*arg.operand, line);
            emit(OpCode::Sub, line);
          } else {
            errors_.push_back("unsupported unary operator");
          }
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
          compile_expr(*arg.left, line);
          compile_expr(*arg.right, line);
          switch (arg.op) {
            case TokenType::Plus: emit(OpCode::Add, line); break;
            case TokenType::Minus: emit(OpCode::Sub, line); break;
            case TokenType::Star: emit(OpCode::Mul, line); break;
            case TokenType::Slash: emit(OpCode::Div, line); break;
            case TokenType::Eq: emit(OpCode::Eq, line); break;
            case TokenType::Ne: emit(OpCode::Ne, line); break;
            case TokenType::Lt: emit(OpCode::Lt, line); break;
            case TokenType::Le: emit(OpCode::Le, line); break;
            case TokenType::Gt: emit(OpCode::Gt, line); break;
            case TokenType::Ge: emit(OpCode::Ge, line); break;
            case TokenType::And: emit(OpCode::And, line); break;
            case TokenType::Or: emit(OpCode::Or, line); break;
            case TokenType::Xor: emit(OpCode::Xor, line); break;
            case TokenType::Eqv: emit(OpCode::Eqv, line); break;
            case TokenType::Imp: emit(OpCode::Imp, line); break;
            case TokenType::Caret: emit(OpCode::Pow, line); break;
            case TokenType::Backslash: emit(OpCode::IntDiv, line); break;
            case TokenType::Mod: emit(OpCode::Mod, line); break;
            default: errors_.push_back("unsupported binary operator"); break;
          }
        }
      },
      expr.data);
}

bool Compiler::compile() {
  compile_program();
  return errors_.empty();
}
