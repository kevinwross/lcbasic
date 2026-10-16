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
            compile_expr(*s.args[i]);
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
              compile_expr(*idx);
            }
            compile_expr(*s.value);
            std::uint8_t vidx = variable_index(s.var);
            std::uint8_t num_indices =
                static_cast<std::uint8_t>(s.indices.size());
            std::uint32_t operand =
                (static_cast<std::uint32_t>(num_indices) << 8) | vidx;
            emit(OpCode::ArrayStore, line, operand);
          } else {
            compile_expr(*s.value);
            std::uint8_t vidx = variable_index(s.var);
            emit(OpCode::StoreVar, line, vidx);
          }
        } else if constexpr (std::is_same_v<T, GotoStmt>) {
          emit(OpCode::Jmp, line, 0);
          record_fixup(code_.size() - 1, s.target_line);
        } else if constexpr (std::is_same_v<T, IfStmt>) {
          compile_expr(*s.condition);
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
          compile_expr(*s.start);
          std::uint8_t vidx = variable_index(s.var);
          emit(OpCode::StoreVar, line, vidx);
          compile_expr(*s.end);
          compile_expr(*s.step);
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
                compile_expr(*idx);
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
          // The VM stores function definitions separately.
          // For M2, we just record it (no-op at compile time).
          (void)s;
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
        } else if constexpr (std::is_same_v<T, ArrayRefExpr>) {
          for (auto& idx : arg.indices) {
            compile_expr(*idx);
          }
          std::uint8_t vidx = variable_index(arg.name);
          std::uint8_t num_indices =
              static_cast<std::uint8_t>(arg.indices.size());
          std::uint32_t operand =
              (static_cast<std::uint32_t>(num_indices) << 8) | vidx;
          emit(OpCode::ArrayLoad, 0, operand);
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
