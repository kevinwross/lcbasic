#include "vm.h"

#include <sstream>

Vm::Vm(Screen& screen) : screen_(screen) {}

Value Vm::pop() {
  if (stack_.empty()) {
    errors_.push_back("stack underflow");
    return Value{};
  }
  Value v = stack_.back();
  stack_.pop_back();
  return v;
}

void Vm::push(Value v) { stack_.push_back(std::move(v)); }

bool Vm::run(const std::vector<Instruction>& code,
             const std::vector<Value>& constants,
             const std::vector<LineEntry>& /*line_table*/,
             const std::unordered_map<std::string, std::uint8_t>& variable_table) {
  // Size the variable value vector to hold all known variables.
  std::uint8_t max_idx = 0;
  for (const auto& [name, idx] : variable_table) {
    if (idx > max_idx) max_idx = idx;
  }
  var_values_.assign(max_idx + 1, Value{}); // all zero-initialised

  ip_ = 0;
  return execute(code, constants);
}

bool Vm::execute(const std::vector<Instruction>& code,
                 const std::vector<Value>& constants) {
  while (ip_ < code.size()) {
    const Instruction& inst = code[ip_++];
    switch (inst.op) {
      case OpCode::Constant: {
        if (inst.operand >= constants.size()) {
          errors_.push_back("bad constant index");
          return false;
        }
        push(constants[inst.operand]);
        break;
      }
      case OpCode::LoadVar: {
        if (inst.operand >= var_values_.size()) {
          errors_.push_back("bad variable index");
          return false;
        }
        push(var_values_[inst.operand]);
        break;
      }
      case OpCode::StoreVar: {
        if (inst.operand >= var_values_.size()) {
          errors_.push_back("bad variable index");
          return false;
        }
        var_values_[inst.operand] = pop();
        break;
      }
      case OpCode::Add: {
        Value b = pop();
        Value a = pop();
        push(Value(a.as_double() + b.as_double()));
        break;
      }
      case OpCode::Sub: {
        Value b = pop();
        Value a = pop();
        push(Value(a.as_double() - b.as_double()));
        break;
      }
      case OpCode::Mul: {
        Value b = pop();
        Value a = pop();
        push(Value(a.as_double() * b.as_double()));
        break;
      }
      case OpCode::Div: {
        Value b = pop();
        Value a = pop();
        push(Value(a.as_double() / b.as_double()));
        break;
      }
      case OpCode::Eq: {
        Value b = pop();
        Value a = pop();
        push(Value(a.as_double() == b.as_double() ? 1.0 : 0.0));
        break;
      }
      case OpCode::Ne: {
        Value b = pop();
        Value a = pop();
        push(Value(a.as_double() != b.as_double() ? 1.0 : 0.0));
        break;
      }
      case OpCode::Lt: {
        Value b = pop();
        Value a = pop();
        push(Value(a.as_double() < b.as_double() ? 1.0 : 0.0));
        break;
      }
      case OpCode::Le: {
        Value b = pop();
        Value a = pop();
        push(Value(a.as_double() <= b.as_double() ? 1.0 : 0.0));
        break;
      }
      case OpCode::Gt: {
        Value b = pop();
        Value a = pop();
        push(Value(a.as_double() > b.as_double() ? 1.0 : 0.0));
        break;
      }
      case OpCode::Ge: {
        Value b = pop();
        Value a = pop();
        push(Value(a.as_double() >= b.as_double() ? 1.0 : 0.0));
        break;
      }
      case OpCode::Print: {
        Value v = pop();
        if (v.is_string()) {
          screen_.print(v.as_string());
        } else {
          std::ostringstream oss;
          oss << v.as_double();
          screen_.print(oss.str());
        }
        break;
      }
      case OpCode::Pop: {
        pop();
        break;
      }
      case OpCode::Jmp: {
        ip_ = inst.operand;
        break;
      }
      case OpCode::JmpFalse: {
        Value v = pop();
        if (v.as_double() == 0.0) {
          ip_ = inst.operand;
        }
        break;
      }
      case OpCode::Call: {
        call_stack_.push_back(ip_); // return address
        ip_ = inst.operand;
        break;
      }
      case OpCode::Ret: {
        if (call_stack_.empty()) {
          errors_.push_back("RETURN without GOSUB");
          return false;
        }
        ip_ = call_stack_.back();
        call_stack_.pop_back();
        break;
      }
      case OpCode::ForInit: {
        // Stack has: [start, end, step] where step is on top.
        // Variable already holds start (stored by StoreVar before ForInit).
        double step = pop().as_double();
        double limit = pop().as_double();
        ForFrame frame;
        frame.var_index = static_cast<std::uint8_t>(inst.operand);
        frame.limit = limit;
        frame.step = step;
        frame.body_start_pc = ip_; // next instruction is loop body start
        for_stack_.push_back(std::move(frame));
        break;
      }
      case OpCode::ForNext: {
        if (for_stack_.empty()) {
          errors_.push_back("NEXT without FOR");
          return false;
        }
        ForFrame& frame = for_stack_.back();

        // var += step
        double var_val = var_values_[frame.var_index].as_double();
        var_val += frame.step;
        var_values_[frame.var_index] = Value(var_val);

        // Check condition
        bool cont;
        if (frame.step > 0.0) {
          cont = var_val <= frame.limit;
        } else {
          cont = var_val >= frame.limit;
        }

        if (cont) {
          ip_ = frame.body_start_pc; // jump back to loop body
        } else {
          for_stack_.pop_back(); // exit loop
        }
        break;
      }
      case OpCode::Halt:
        return true;
      default: {
        std::ostringstream oss;
        oss << "unknown opcode at ip " << (ip_ - 1);
        errors_.push_back(oss.str());
        return false;
      }
    }
  }
  errors_.push_back("ran off end of code without HALT");
  return false;
}
