#include "compiler.h"

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
  for (const auto& [line_number, stmt] : program_.lines) {
    line_table_.push_back(LineEntry{line_number, next_pc()});
    compile_statement(stmt);
  }
  emit(OpCode::Halt, program_.lines.empty() ? 0
                                            : program_.lines.back().first);
  resolve_fixups();
}

void Compiler::compile_statement(const Statement& stmt) {
  std::int32_t line = stmt.line_number;

  std::visit(
      [&](auto&& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, PrintStmt>) {
          for (std::size_t i = 0; i < s.args.size(); ++i) {
            compile_expr(*s.args[i], line);
            if (i > 0) {
              auto sp_idx = add_constant(Value(std::string(" ")));
              emit(OpCode::Constant, line, sp_idx);
              emit(OpCode::Print, line);
            }
            emit(OpCode::Print, line);
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
          emit(OpCode::Halt, line);
        } else if constexpr (std::is_same_v<T, DimStmt>) {
          // Evaluate each upper bound expression, push onto stack.
          for (std::int32_t ub : s.upper_bounds) {
            auto idx = add_constant(Value(static_cast<double>(ub)));
            emit(OpCode::Constant, line, idx);
          }
          std::uint8_t vidx = variable_index(s.name);
          std::uint8_t num_dims =
              static_cast<std::uint8_t>(s.upper_bounds.size());
          // Encode var index in low byte, num_dims in high byte.
          std::uint32_t operand =
              (static_cast<std::uint32_t>(num_dims) << 8) | vidx;
          emit(OpCode::Dim, line, operand);
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
              // Compile index expressions
              for (auto& idx : target.indices) {
                compile_expr(*idx, line);
              }
              std::uint8_t vidx = variable_index(target.name);
              emit(OpCode::ReadArray, line, vidx);
            } else {
              std::uint8_t vidx = variable_index(target.name);
              emit(OpCode::ReadVar, line, vidx);
            }
          }
        } else if constexpr (std::is_same_v<T, RestoreStmt>) {
          emit(OpCode::Restore, line);
        } else if constexpr (std::is_same_v<T, DefFnStmt>) {
          // DEF FN is a compile-time definition; no code emitted.
          (void)s;
        } else if constexpr (std::is_same_v<T, OpenStmt>) {
          // OPEN filenum AS filename (mode)
          // Stack order at runtime: [filename, mode, filenum] (filenum on top)
          compile_expr(*s.filename, line);
          auto mode_idx = add_constant(Value(s.mode));
          emit(OpCode::Constant, line, mode_idx);
          auto fn_idx = add_constant(Value(static_cast<double>(s.filenum)));
          emit(OpCode::Constant, line, fn_idx);
          emit(OpCode::Open, line, 0);
        } else if constexpr (std::is_same_v<T, CloseStmt>) {
          auto fn_idx = add_constant(Value(static_cast<double>(s.filenum)));
          emit(OpCode::Constant, line, fn_idx);
          emit(OpCode::Close, line, 0);
        } else if constexpr (std::is_same_v<T, PrintFileStmt>) {
          // PRINT #filenum, values
          for (std::size_t i = 0; i < s.args.size(); ++i) {
            auto fn_idx = add_constant(Value(static_cast<double>(s.filenum)));
            emit(OpCode::Constant, line, fn_idx);
            compile_expr(*s.args[i], line);
            if (i > 0) {
              auto sp_idx = add_constant(Value(std::string(" ")));
              emit(OpCode::Constant, line, sp_idx);
              emit(OpCode::Print, line);
            }
            emit(OpCode::PrintFile, line, 0);
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
          for (const auto& var : s.vars) {
            std::uint8_t vidx = variable_index(var);
            emit(OpCode::InputFile, line, vidx);
          }
        } else if constexpr (std::is_same_v<T, OnErrorStmt>) {
          emit(OpCode::OnError, line, 0);
          record_fixup(code_.size() - 1, s.target_line);
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
          std::uint8_t vidx = variable_index(arg.name);
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
