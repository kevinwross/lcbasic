#pragma once

#include "parser.h"
#include "value.h"

#include <cstdint>
#include <string>
#include <vector>

// Simple stack-based bytecode. Each statement compiles to a linear run of
// instructions terminated by OP_PRINT / OP_HALT.
enum class OpCode : std::uint8_t {
  Constant,   // push constant[uint8 index]
  Add,        // pop b, pop a, push a + b
  Sub,
  Mul,
  Div,
  Print,      // pop and print to screen
  Pop,        // discard top of stack
  Halt,       // stop execution
};

struct Instruction {
  OpCode op;
  std::uint8_t operand;  // constant index for OP_CONSTANT, else unused
  std::int32_t line;     // source line for error reporting / breakpoints
};

// Compiles a Program into a flat bytecode stream with a parallel line table
// mapping each instruction index to a source line.
class Compiler {
public:
  explicit Compiler(const Program& program);

  bool compile();
  const std::vector<std::string>& errors() const { return errors_; }

  // Flat execution stream: instructions concatenated across lines, with a
  // parallel line table mapping each instruction index to a source line.
  const std::vector<Instruction>& code() const { return code_; }
  const std::vector<Value>& constants() const { return constants_; }

private:
  const Program& program_;
  std::vector<std::string> errors_;

  std::vector<Instruction> code_;
  std::vector<Value> constants_;

  std::uint8_t add_constant(const Value& value);

  void compile_statement(const Statement& stmt);
  void compile_expr(const Expr& expr);
  void emit(OpCode op, std::int32_t line, std::uint8_t operand = 0);
};
