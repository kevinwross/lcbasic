#pragma once

#include "parser.h"
#include "value.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Stack-based bytecode with line-numbered jumps for GW-BASIC control flow.
enum class OpCode : std::uint8_t {
  Constant,     // push constant[operand]
  LoadVar,      // push variable[operand]
  StoreVar,     // pop and store to variable[operand]
  Add, Sub, Mul, Div,
  Eq, Ne, Lt, Le, Gt, Ge,
  Print,        // pop and print
  Pop,          // discard top of stack
  Jmp,          // jump to PC offset stored in operand
  JmpFalse,     // jump to PC if top of stack is zero
  Call,         // GOSUB: push return PC, jump to PC offset
  Ret,          // RETURN: pop return PC, jump
  ForInit,      // FOR var=start TO end STEP step
  ForNext,      // NEXT var
  Dim,          // DIM array: pop upper bounds, dimension array[operand]
  ArrayLoad,    // push array[operand][indices on stack]
  ArrayStore,   // pop value, store to array[operand][indices on stack]
  ReadVar,      // read next DATA value into variable[operand]
  ReadArray,    // read next DATA value into array[operand][indices]
  Restore,      // reset DATA pointer
  SetOptionBase,// set OPTION BASE
  NextZone,     // PRINT comma: advance to the next 14-column print zone
  Open,         // OPEN filenum AS filename (mode)
  Close,        // CLOSE filenum
  PrintFile,    // PRINT #filenum, values
  InputFile,    // INPUT #filenum, variable
  Eof,          // push EOF(filenum)
  OnError,      // ON ERROR GOTO line
  Resume,       // RESUME [NEXT | line]
  DefType,      // DEFINT/DEFSNG/DEFDBL/DEFSTR for a letter range
  TimerFunc,    // push the host timer value
  ChDir,        // CHDIR path
  MkDir,        // MKDIR path
  RmDir,        // RMDIR path
  KillFile,     // KILL path
  NameFile,     // NAME old AS new
  Files,        // FILES [pattern]
  Shell,        // SHELL [command]
  OnEvent,      // ON <event> GOSUB line
  EventControl, // <event> ON/OFF/STOP
  Input,        // INPUT vars (operand = count; stack: prompt, var indices)
  Cls,          // clear screen
  Locate,       // stack: row, col
  Color,        // stack: fg, bg
  Erase,        // operand = array variable index
  Clear,        // clear variables/arrays/data/stacks
  RaiseError,   // stack: error code
  MidAssign,    // operand = target var index; stack: value, start, length
  Halt,         // stop execution
};

struct Instruction {
  OpCode op;
  std::uint32_t operand;
  std::int32_t line;
};

struct LineEntry {
  std::int32_t line_number;
  std::size_t pc_offset;
};

struct Fixup {
  std::size_t code_index;
  std::int32_t target_line;
};

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
  const std::vector<std::string>& variable_names() const {
    return variable_names_;
  }

  const std::vector<Value>& data_values() const { return data_values_; }

  std::optional<std::size_t> find_line(std::int32_t line) const;

private:
  const Program& program_;
  std::vector<std::string> errors_;

  std::vector<Instruction> code_;
  std::vector<Value> constants_;
  std::vector<LineEntry> line_table_;
  std::vector<Fixup> fixups_;
  std::unordered_map<std::string, std::uint8_t> variable_table_;
  std::vector<std::string> variable_names_;
  std::vector<Value> data_values_;
  std::int32_t option_base_ = 0;

  // DEF FN definitions: name -> compiled subroutine + parameter slots.
  struct FnDef {
    std::vector<std::uint8_t> param_vars;
    std::uint8_t return_var = 0;
    std::size_t body_pc = 0;
  };
  std::unordered_map<std::string, FnDef> functions_;
  // Active parameter renames while compiling a DEF FN body.
  std::unordered_map<std::string, std::string> rename_;

  // WHILE/WEND nesting.
  struct WhileFrame {
    std::size_t cond_pc;
    std::size_t jmpfalse_idx;
  };
  std::vector<WhileFrame> while_stack_;

  std::size_t next_pc() const { return code_.size(); }

  std::uint8_t add_constant(const Value& v);
  std::uint8_t variable_index(const std::string& name);
  void emit(OpCode op, std::int32_t line, std::uint32_t operand = 0);
  void record_fixup(std::size_t code_index, std::int32_t target_line);
  void resolve_fixups();

  void compile_program();
  void compile_statement(const Statement& stmt);
  void compile_expr(const Expr& expr, std::int32_t line);
};
