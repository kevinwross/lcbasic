#pragma once

#include "compiler.h"
#include "host.h"
#include "screen.h"
#include "value.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Execution status returned by stepping / running.
enum class ExecStatus {
  Running,  // still executing
  Paused,   // paused at a safe point (debugger)
  Halted,   // clean HALT
  Error,    // runtime error
};

// Debug hook: the VM calls on_instruction() before executing each
// instruction. Returning true requests a pause (the VM stops before
// executing that instruction).
class DbgHook {
public:
  virtual ~DbgHook() = default;
  virtual bool on_instruction(std::size_t pc, std::int32_t line,
                              std::size_t call_depth) = 0;
};

// Stack-based bytecode VM with file I/O, GW-BASIC error handling, and
// single-step execution support for the debugger.
class Vm {
public:
  explicit Vm(Screen& screen, Host& host);

  // Load a compiled program and reset execution state.
  void load(const std::vector<Instruction>& code,
            const std::vector<Value>& constants,
            const std::vector<LineEntry>& line_table,
            const std::unordered_map<std::string, std::uint8_t>& variable_table,
            const std::vector<std::string>& variable_names,
            const std::vector<Value>& data_values);

  // Run to completion (no debug hook). Returns true on clean HALT.
  bool run();

  // Run with the current debug hook attached. Returns the execution status.
  ExecStatus run_with_hook(DbgHook* hook);

  // Execute a single instruction. Returns the status after one step.
  ExecStatus step_one();

  // Attach/detach a debug hook.
  void set_hook(DbgHook* hook) { hook_ = hook; }

  const std::vector<std::string>& errors() const { return errors_; }

  // ----- State accessors (for the debugger) -----
  std::size_t current_pc() const { return ip_; }
  std::int32_t current_line() const;
  bool is_halted() const { return halted_; }
  bool has_error() const { return !errors_.empty(); }

  const std::vector<Value>& var_values() const { return var_values_; }
  const std::vector<std::string>& variable_names() const {
    return variable_names_;
  }
  const std::vector<Value>& stack() const { return stack_; }
  const std::vector<std::size_t>& call_stack() const { return call_stack_; }
  const std::vector<Instruction>& code() const { return code_; }
  const std::vector<Value>& constants() const { return constants_; }
  const std::vector<LineEntry>& line_table() const { return line_table_; }
  const ArrayStore& arrays() const { return arrays_; }
  const DataPool& data_pool() const { return data_pool_; }

  struct ForFrameView {
    std::uint8_t var_index;
    double limit;
    double step;
    std::size_t body_start_pc;
  };
  std::vector<ForFrameView> for_frames() const;

  int err_code() const { return err_code_; }
  int erl_line() const { return erl_line_; }

  // Set a variable's value by name (debugger write support).
  bool set_variable(const std::string& name, const Value& v);

  // ----- Snapshot / restore -----
  struct Snapshot {
    std::size_t ip;
    std::vector<Value> var_values;
    std::vector<Value> stack;
    std::vector<std::size_t> call_stack;
    std::vector<Value> data_values_seen;
    std::size_t data_ptr;
  };
  Snapshot snapshot() const;
  void restore(const Snapshot& s);

  // Look up a variable value by name (debugger read support).
  std::optional<Value> get_variable(const std::string& name) const;

private:
  Screen& screen_;
  Host& host_;

  std::vector<Instruction> code_;
  std::vector<Value> constants_;
  std::vector<LineEntry> line_table_;

  std::vector<Value> var_values_;          // indexed by variable index
  ArrayStore arrays_;                      // DIM'd arrays
  DataPool data_pool_;                     // DATA pool
  std::int32_t option_base_ = 0;           // OPTION BASE setting
  std::vector<std::string> variable_names_; // index -> name
  std::vector<int> file_handles_;          // filenum -> host file handle (-1 = closed)
  std::vector<Value> stack_;
  std::vector<std::size_t> call_stack_;    // return addresses for GOSUB
  struct ForFrame {
    std::uint8_t var_index;
    double limit;
    double step;
    std::size_t body_start_pc;
  };
  std::vector<ForFrame> for_stack_;
  std::vector<std::string> errors_;

  // Error handling
  std::int32_t error_handler_line_ = -1;
  std::size_t handler_pc_ = 0;
  int err_code_ = 0;
  int erl_line_ = 0;
  bool in_error_handler_ = false;

  std::size_t ip_ = 0;
  bool halted_ = false;
  DbgHook* hook_ = nullptr;

  Value pop();
  void push(Value v);
  std::string var_name(std::uint8_t idx) const;
  VarType var_type(std::uint8_t idx) const;
  int file_handle_for(std::uint8_t filenum_idx);
  std::optional<std::size_t> find_pc_for_line(std::int32_t line) const;
  [[noreturn]] void trap_error(int error_code, std::int32_t line);
  ExecStatus execute(bool single_step);
};
