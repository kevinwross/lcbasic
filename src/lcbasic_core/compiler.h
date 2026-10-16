#pragma once

#include "parser.h"
#include "value.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Simple stack-based bytecode with line-numbered jumps for GW-BASIC control
// flow (GOTO, IF/THEN, GOSUB, FOR/NEXT).
enum class OpCode : std::uint8_t {
  Constant,     // push constant[operand]
  LoadVar,      // push variable[operand]
  StoreVar,     // pop and store to variable[operand]
  Add, Sub, Mul, Div,
  Eq, Ne, Lt, Le, Gt, Ge,   // comparison -> push 1.0 or 0.0
  Print,        // pop and print
  Pop,          // discard top of stack
  Jmp,          // jump to line number stored in operand
  JmpFalse,     // jump to line if top of stack is zero
  Call,         // GOSUB: push return PC, jump to line
  Ret,          // RETURN: pop return PC, jump
  ForInit,      // FOR var=start TO end STEP step (end/step on stack)
  ForNext,      // NEXT var: increment, check limit, jump back or exit
  Halt,         // stop execution
};

struct Instruction {
  OpCode op;
  std::uint32_t operand;  // constant/var index, or target line number
  std::int32_t line;      // source line for error reporting / breakpoints
};

struct LineEntry {
  std::int32_t line_number;
  std::size_t pc_offset;
};

// Fixup for forward line references resolved after compilation.
struct Fixup {
  std::size_t code_index;   // index in code_ to patch
  std::int32_t target_line; // line number to resolve
};

// Compiles a Program into a flat bytecode stream with a line table and
// variable table. Forward line references are resolved via fixups.
class Compiler {
public:
  explicit Compiler(const Program& program);

  bool compile();
  const std::vector<std::string>& errors() const { return errors_; }

  const std::vector<Instruction>& code() const { return code_; }
  const std::vector<Value>& constants() const { return constants_; }
  const std::vector<LineEntry>& line_table() const { return line_table_; }
  const std::unordered_map<std::string, std::uint8_t>& variable_table()
      const {
    return variable_table_;
  }

  // Resolve a line number to a PC offset (binary search on line_table_).
  std::optional<std::size_t> find_line(std::int32_t line) const;

private:
  const Program& program_;
  std::vector<std::string> errors_;

  std::vector<Instruction> code_;
  std::vector<Value> constants_;
  std::vector<LineEntry> line_table_;
  std::vector<Fixup> fixups_;
  std::unordered_map<std::string, std::uint8_t> variable_table_;

  std::size_t next_pc() const { return code_.size(); }

  std::uint8_t add_constant(const Value& v);
  std::uint8_t variable_index(const std::string& name);
  void emit(OpCode op, std::int32_t line, std::uint32_t operand = 0);
  void record_fixup(std::size_t code_index, std::int32_t target_line);
  void resolve_fixups();

  void compile_program();
  void compile_statement(const Statement& stmt);
  void compile_expr(const Expr& expr);
};
