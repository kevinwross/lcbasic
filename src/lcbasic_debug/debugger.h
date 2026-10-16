#pragma once

#include "vm.h"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace debugger {

// A stack frame in the debugger view.
struct StackFrame {
  std::string name;      // e.g. "main", "GOSUB 100"
  std::int32_t line = 0; // current line in this frame
  std::size_t depth = 0;
};

// A variable view for the debugger.
struct VariableView {
  std::string name;
  std::string value;
  std::string type;  // "int16", "int32", "single", "double", "string"
};

// In-process debugger driving a Vm. Implements the VM's DbgHook so it can
// pause execution at breakpoints and step boundaries.
class Debugger : public DbgHook {
public:
  // Reasons execution stopped.
  enum class StopReason {
    NotStarted,
    Step,       // completed a step
    Breakpoint, // hit a breakpoint
    Halted,     // program finished
    Error,      // runtime error
  };

  explicit Debugger(Vm& vm);

  // ----- Breakpoints -----
  void set_breakpoint(std::int32_t line);
  void clear_breakpoint(std::int32_t line);
  void clear_all_breakpoints();
  bool has_breakpoint(std::int32_t line) const;
  const std::set<std::int32_t>& breakpoints() const { return breakpoints_; }

  // ----- Execution control -----
  // Start or resume execution according to the current mode.
  StopReason run();
  StopReason step_into();
  StopReason step_over();
  StopReason step_out();
  StopReason continue_();

  // ----- State inspection -----
  StopReason stop_reason() const { return reason_; }
  std::int32_t current_line() const { return vm_.current_line(); }
  std::size_t current_pc() const { return vm_.current_pc(); }
  bool halted() const { return vm_.is_halted(); }

  std::vector<VariableView> variables() const;
  std::vector<StackFrame> stack_frames() const;
  const Vm& vm() const { return vm_; }
  Vm& vm() { return vm_; }

  // ----- Expression evaluation -----
  // Evaluate a BASIC expression using the current variable state.
  // Returns the value as a string, or an error message on failure.
  struct EvalResult {
    bool ok = false;
    std::string value;
    std::string error;
  };
  EvalResult evaluate(const std::string& expression) const;

  // ----- Disassembly -----
  std::string disassemble() const;

  // ----- Snapshots -----
  void save_snapshot();
  bool restore_snapshot();

  // DbgHook implementation.
  bool on_instruction(std::size_t pc, std::int32_t line,
                      std::size_t call_depth) override;

private:
  enum class Mode { Continue, StepInto, StepOver, StepOut };

  Vm& vm_;
  std::set<std::int32_t> breakpoints_;
  Mode mode_ = Mode::Continue;
  StopReason reason_ = StopReason::NotStarted;

  bool first_ = true;             // skip the first hook call after resuming
  bool paused_ = false;           // true if we're resuming from a stop
  bool left_resume_line_ = false; // for Continue: left the resumed-from line
  std::size_t step_depth_ = 0;    // call depth when the step began
  std::int32_t step_line_ = 0;    // line when the step began

  std::vector<Vm::Snapshot> snapshots_;

  StopReason resume(Mode m);
};

}  // namespace debugger
