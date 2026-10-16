#pragma once

#include "compiler.h"
#include "screen.h"
#include "value.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

// Stack-based bytecode VM. Output goes through a Screen so the same program
// runs on ncurses, headless, or (later) GPU backends unchanged.
class Vm {
public:
  explicit Vm(Screen& screen);

  // Run the compiled bytecode. Returns true on clean HALT, false on error.
  bool run();

  const std::vector<std::string>& errors() const { return errors_; }

  // Execute compiled bytecode directly (used by the CLI).
  bool interpret(const std::vector<Instruction>& code,
                 const std::vector<Value>& constants);

private:
  Screen& screen_;
  std::vector<Value> stack_;
  std::vector<std::string> errors_;

  std::size_t ip_ = 0;  // instruction pointer

  Value pop();
  void push(Value v);
};
