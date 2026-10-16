#pragma once

#include "compiler.h"
#include "screen.h"
#include "value.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Stack-based bytecode VM. Output goes through a Screen so the same program
// runs on ncurses, headless, or (later) GPU backends unchanged.
class Vm {
public:
  explicit Vm(Screen& screen);

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

private:
  Screen& screen_;
  std::vector<Value> var_values_;          // indexed by variable index
  ArrayStore arrays_;                      // DIM'd arrays
  DataPool data_pool_;                     // DATA pool
  std::int32_t option_base_ = 0;           // OPTION BASE setting
  std::vector<std::string> variable_names_; // index -> name
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

  std::size_t ip_ = 0;

  Value pop();
  void push(Value v);
  bool execute(const std::vector<Instruction>& code,
               const std::vector<Value>& constants);
  std::string var_name(std::uint8_t idx) const;
  VarType var_type(std::uint8_t idx) const;
};
