#include "compiler.h"

std::uint8_t Compiler::add_constant(const Value& value) {
  constants_.push_back(value);
  return static_cast<std::uint8_t>(constants_.size() - 1);
}

Compiler::Compiler(const Program& program) : program_(program) {}

void Compiler::emit(OpCode op, std::int32_t line, std::uint8_t operand) {
  code_.push_back(Instruction{op, operand, line});
}

void Compiler::compile_expr(const Expr& expr) {
  std::visit(
      [&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, NumberExpr>) {
          auto idx = add_constant(Value(arg.value));
          emit(OpCode::Constant, 0, idx);  // line filled in by caller context
        } else if constexpr (std::is_same_v<T, StringExpr>) {
          auto idx = add_constant(Value(arg.value));
          emit(OpCode::Constant, 0, idx);
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
          compile_expr(*arg.left);
          compile_expr(*arg.right);
          switch (arg.op) {
            case TokenType::Plus: emit(OpCode::Add, 0); break;
            case TokenType::Minus: emit(OpCode::Sub, 0); break;
            case TokenType::Star: emit(OpCode::Mul, 0); break;
            case TokenType::Slash: emit(OpCode::Div, 0); break;
            default: errors_.push_back("unsupported binary operator"); break;
          }
        }
      },
      expr.data);
}

void Compiler::compile_statement(const Statement& stmt) {
  std::int32_t line = stmt.line_number;
  std::visit(
      [&](auto&& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, PrintStmt>) {
          for (std::size_t i = 0; i < s.args.size(); ++i) {
            if (i > 0) {
              // Semicolon/comma separator: print a space / tab between items.
              // GW-BASIC: comma = tab to next zone, semicolon = no space.
              // For M0, just print the items with no separator.
            }
            compile_expr(*s.args[i]);
            // Patch the line number on the CONSTANT we just emitted.
            if (!code_.empty()) code_.back().line = line;
            emit(OpCode::Print, line);
          }
          // GW-BASIC PRINT defaults to a trailing newline.
          auto nl_idx = add_constant(Value(std::string("\n")));
          emit(OpCode::Constant, line, nl_idx);
          emit(OpCode::Print, line);
        }
      },
      stmt.data);
}

bool Compiler::compile() {
  for (const auto& stmt : program_.statements) {
    compile_statement(stmt);
  }
  emit(OpCode::Halt, program_.statements.empty()
                         ? 0
                         : program_.statements.back().line_number);
  return errors_.empty();
}
