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
    // Patch the operand (target line -> PC offset).
    code_[fixup.code_index].operand = static_cast<std::uint32_t>(*pc);
  }
}

void Compiler::compile_program() {
  // Assign PC offsets to each line (line table).
  for (const auto& [line_number, stmt] : program_.lines) {
    line_table_.push_back(LineEntry{line_number, next_pc()});
    compile_statement(stmt);
  }
  // Each line implicitly falls through to the next; after the last line,
  // emit Halt.
  emit(OpCode::Halt, program_.lines.empty() ? 0
                                            : program_.lines.back().first);

  // Resolve forward line references (GOTO, IF/THEN line, GOSUB, FOR/NEXT).
  resolve_fixups();
}

void Compiler::compile_statement(const Statement& stmt) {
  std::int32_t line = stmt.line_number;

  std::visit(
      [&](auto&& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, PrintStmt>) {
          for (std::size_t i = 0; i < s.args.size(); ++i) {
            compile_expr(*s.args[i]);
            if (i > 0) {
              // Print a separator: comma = tab zone, semicolon = nothing.
              // For M0/M1, just print a space between items.
              auto sp_idx = add_constant(Value(std::string(" ")));
              emit(OpCode::Constant, line, sp_idx);
              emit(OpCode::Print, line);
            }
            emit(OpCode::Print, line);
          }
          if (!s.trailing_separator) {
            // GW-BASIC PRINT defaults to a trailing newline.
            auto nl_idx = add_constant(Value(std::string("\n")));
            emit(OpCode::Constant, line, nl_idx);
            emit(OpCode::Print, line);
          }
        } else if constexpr (std::is_same_v<T, LetStmt>) {
          compile_expr(*s.value);
          std::uint8_t vidx = variable_index(s.var);
          emit(OpCode::StoreVar, line, vidx);
        } else if constexpr (std::is_same_v<T, GotoStmt>) {
          emit(OpCode::Jmp, line, 0); // placeholder, fixed up later
          record_fixup(code_.size() - 1, s.target_line);
        } else if constexpr (std::is_same_v<T, IfStmt>) {
          compile_expr(*s.condition);
          // If condition is false, skip the THEN branch.
          emit(OpCode::JmpFalse, line, 0); // placeholder
          std::size_t jmp_false_idx = code_.size() - 1;

          if (s.then_line >= 0) {
            // THEN line_number: jump to that line.
            emit(OpCode::Jmp, line, 0); // placeholder
            record_fixup(code_.size() - 1, s.then_line);
          } else {
            // THEN statement(s): compile inline.
            for (auto& inner : s.then_stmts) {
              // inner->line_number is -1; use the enclosing line.
              Statement inner_stmt = std::move(*inner);
              inner_stmt.line_number = line;
              compile_statement(inner_stmt);
            }
          }

          if (s.else_line >= 0 || !s.else_stmts.empty()) {
            // Skip the ELSE branch after THEN.
            emit(OpCode::Jmp, line, 0); // placeholder
            std::size_t skip_else_idx = code_.size() - 1;

            // Patch the JmpFalse target to here (start of ELSE).
            auto here_pc = next_pc();
            code_[jmp_false_idx].operand =
                static_cast<std::uint32_t>(here_pc);

            if (s.else_line >= 0) {
              emit(OpCode::Jmp, line, 0); // placeholder
              record_fixup(code_.size() - 1, s.else_line);
            } else {
              for (auto& inner : s.else_stmts) {
                Statement inner_stmt = std::move(*inner);
                inner_stmt.line_number = line;
                compile_statement(inner_stmt);
              }
            }

            // Patch the skip-ELSE jump to after the ELSE block.
            auto after_else_pc = next_pc();
            code_[skip_else_idx].operand =
                static_cast<std::uint32_t>(after_else_pc);
          } else {
            // No ELSE: patch JmpFalse to continue after THEN.
            auto after_then_pc = next_pc();
            code_[jmp_false_idx].operand =
                static_cast<std::uint32_t>(after_then_pc);
          }
        } else if constexpr (std::is_same_v<T, ForStmt>) {
          // FOR var = start TO end [STEP step]
          compile_expr(*s.start);
          std::uint8_t vidx = variable_index(s.var);
          emit(OpCode::StoreVar, line, vidx); // var = start

          compile_expr(*s.end);   // push end
          compile_expr(*s.step);  // push step

          emit(OpCode::ForInit, line, vidx); // create ForFrame
          std::size_t for_init_idx = code_.size() - 1;
          std::size_t body_start_pc = next_pc();

          // Note: the FOR body is the statements following this FOR until
          // the matching NEXT. Since our program is a flat list, the body
          // statements are compiled as subsequent lines/statements. The
          // ForNext instruction will jump back to body_start_pc.
          // We record body_start_pc in the ForInit operand? No - ForInit
          // operand is the variable index. The VM's ForFrame stores the
          // body_start_pc (set to PC after ForInit).

          (void)for_init_idx;
          (void)body_start_pc;
        } else if constexpr (std::is_same_v<T, NextStmt>) {
          // NEXT var: handled at runtime via ForNext instruction.
          std::uint8_t vidx = variable_index(s.var);
          emit(OpCode::ForNext, line, vidx);
        } else if constexpr (std::is_same_v<T, GosubStmt>) {
          emit(OpCode::Call, line, 0); // placeholder
          record_fixup(code_.size() - 1, s.target_line);
        } else if constexpr (std::is_same_v<T, ReturnStmt>) {
          emit(OpCode::Ret, line);
        } else if constexpr (std::is_same_v<T, EndStmt>) {
          emit(OpCode::Halt, line);
        } else if constexpr (std::is_same_v<T, StopStmt>) {
          emit(OpCode::Halt, line);
        }
      },
      stmt.data);
}

void Compiler::compile_expr(const Expr& expr) {
  std::visit(
      [&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, NumberExpr>) {
          auto idx = add_constant(Value(arg.value));
          emit(OpCode::Constant, 0, idx);
        } else if constexpr (std::is_same_v<T, StringExpr>) {
          auto idx = add_constant(Value(arg.value));
          emit(OpCode::Constant, 0, idx);
        } else if constexpr (std::is_same_v<T, VariableExpr>) {
          std::uint8_t vidx = variable_index(arg.name);
          emit(OpCode::LoadVar, 0, vidx);
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
          compile_expr(*arg.left);
          compile_expr(*arg.right);
          switch (arg.op) {
            case TokenType::Plus: emit(OpCode::Add, 0); break;
            case TokenType::Minus: emit(OpCode::Sub, 0); break;
            case TokenType::Star: emit(OpCode::Mul, 0); break;
            case TokenType::Slash: emit(OpCode::Div, 0); break;
            case TokenType::Eq: emit(OpCode::Eq, 0); break;
            case TokenType::Ne: emit(OpCode::Ne, 0); break;
            case TokenType::Lt: emit(OpCode::Lt, 0); break;
            case TokenType::Le: emit(OpCode::Le, 0); break;
            case TokenType::Gt: emit(OpCode::Gt, 0); break;
            case TokenType::Ge: emit(OpCode::Ge, 0); break;
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
