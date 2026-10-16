#pragma once

#include "compiler.h"
#include "host.h"
#include "screen.h"
#include "value.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Stack-based bytecode VM with file I/O and GW-BASIC error handling.
class Vm {
public:
  explicit Vm(Screen& screen, Host& host);

  // Execute compiled bytecode. Returns true on clean HALT, false on error.
  bool run(const std::vector<Instruction>& code,
           const std::vector<Value>& constants,
           const std::vector<LineEntry>& line_table,
           const std::unordered_map<std::string, std::uint8_t>& variable_table,
           const std::vector<std::string>& variable_names,
           const std::vector<Value>& data_values);

  const std::vector<std::string>& errors() const { return errors_; }

  const std::vector<Value>& var_values() const { return var_values_; }
  const ArrayStore& arrays() const { return arrays_; }
  const DataPool& data_pool() const { return data_pool_; }

  int err_code() const { return err_code_; }
  int erl_line() const { return erl_line_; }

private:
  Screen& screen_;
  Host& host_;
  std::vector<Value> var_values_;          // indexed by variable index
  ArrayStore arrays_;                      // DIM'd arrays
  DataPool data_pool_;                     // DATA pool
  std::int32_t option_base_ = 0;           // OPTION BASE setting
  std::vector<std::string> variable_names_; // index -> name
  std::vector<int> file_handles_;          // filenum -> host file handle (-1 = closed)
  const std::vector<LineEntry>* line_table_ = nullptr; // for line->PC lookup
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
  std::int32_t error_handler_line_ = -1; // line of ON ERROR statement
  std::size_t handler_pc_ = 0;           // PC to jump to on error
  int err_code_ = 0;                     // ERR value
  int erl_line_ = 0;                     // ERL value
  bool in_error_handler_ = false;        // prevent recursive errors

  std::size_t ip_ = 0;

  Value pop();
  void push(Value v);
  std::string var_name(std::uint8_t idx) const;
  VarType var_type(std::uint8_t idx) const;
  int file_handle_for(std::uint8_t filenum_idx);
  std::optional<std::size_t> find_pc_for_line(std::int32_t line) const;
  [[noreturn]] void trap_error(int error_code, std::int32_t line);
  bool execute(const std::vector<Instruction>& code,
               const std::vector<Value>& constants);
};
