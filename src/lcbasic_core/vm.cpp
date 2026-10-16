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

bool Vm::interpret(const std::vector<Instruction>& code,
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

bool Vm::run() {
  // run() is a no-op until a program is loaded via interpret().
  // Programs are executed by the CLI through interpret() directly.
  return true;
}
