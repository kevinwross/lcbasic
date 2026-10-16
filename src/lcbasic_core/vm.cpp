#include "vm.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

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

std::string Vm::var_name(std::uint8_t idx) const {
  if (idx < variable_names_.size()) return variable_names_[idx];
  return "?";
}

VarType Vm::var_type(std::uint8_t idx) const {
  // Deduce from name suffix.
  return deduce_var_type(var_name(idx));
}

bool Vm::run(const std::vector<Instruction>& code,
             const std::vector<Value>& constants,
             const std::vector<LineEntry>& /*line_table*/,
             const std::unordered_map<std::string, std::uint8_t>& variable_table,
             const std::vector<std::string>& variable_names,
             const std::vector<Value>& data_values) {
  std::uint8_t max_idx = 0;
  for (const auto& [name, idx] : variable_table) {
    if (idx > max_idx) max_idx = idx;
  }
  var_values_.assign(max_idx + 1, Value{});
  variable_names_ = variable_names;
  option_base_ = 0; // Will be set by OPTION BASE statement at runtime

  for (const auto& v : data_values) {
    data_pool_.add(v);
  }

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
        var_values_[inst.operand] = pop().coerce(var_type(inst.operand));
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
        call_stack_.push_back(ip_);
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
        double step = pop().as_double();
        double limit = pop().as_double();
        ForFrame frame;
        frame.var_index = static_cast<std::uint8_t>(inst.operand);
        frame.limit = limit;
        frame.step = step;
        frame.body_start_pc = ip_;
        for_stack_.push_back(std::move(frame));
        break;
      }
      case OpCode::ForNext: {
        if (for_stack_.empty()) {
          errors_.push_back("NEXT without FOR");
          return false;
        }
        ForFrame& frame = for_stack_.back();
        double var_val = var_values_[frame.var_index].as_double();
        var_val += frame.step;
        var_values_[frame.var_index] = Value(var_val);
        bool cont;
        if (frame.step > 0.0) {
          cont = var_val <= frame.limit;
        } else {
          cont = var_val >= frame.limit;
        }
        if (cont) {
          ip_ = frame.body_start_pc;
        } else {
          for_stack_.pop_back();
        }
        break;
      }
      case OpCode::Dim: {
        std::uint8_t vidx = static_cast<std::uint8_t>(inst.operand & 0xFF);
        std::uint8_t num_dims =
            static_cast<std::uint8_t>((inst.operand >> 8) & 0xFF);
        std::vector<double> bounds_d;
        bounds_d.reserve(num_dims);
        for (std::uint8_t i = 0; i < num_dims; ++i) {
          bounds_d.push_back(pop().as_double());
        }
        std::reverse(bounds_d.begin(), bounds_d.end());
        std::vector<std::int32_t> upper_bounds;
        for (double b : bounds_d) {
          upper_bounds.push_back(static_cast<std::int32_t>(b));
        }
        arrays_.dim(var_name(vidx), option_base_, upper_bounds,
                    var_type(vidx));
        break;
      }
      case OpCode::ArrayLoad: {
        std::uint8_t vidx = static_cast<std::uint8_t>(inst.operand & 0xFF);
        std::uint8_t num_indices =
            static_cast<std::uint8_t>((inst.operand >> 8) & 0xFF);
        std::vector<std::int32_t> indices;
        indices.reserve(num_indices);
        for (std::uint8_t i = 0; i < num_indices; ++i) {
          indices.push_back(static_cast<std::int32_t>(pop().as_double()));
        }
        std::reverse(indices.begin(), indices.end());
        try {
          push(arrays_.at(var_name(vidx), indices));
        } catch (const std::exception& e) {
          errors_.push_back(e.what());
          return false;
        }
        break;
      }
      case OpCode::ArrayStore: {
        std::uint8_t vidx = static_cast<std::uint8_t>(inst.operand & 0xFF);
        std::uint8_t num_indices =
            static_cast<std::uint8_t>((inst.operand >> 8) & 0xFF);
        Value val = pop();
        std::vector<std::int32_t> indices;
        indices.reserve(num_indices);
        for (std::uint8_t i = 0; i < num_indices; ++i) {
          indices.push_back(static_cast<std::int32_t>(pop().as_double()));
        }
        // Indices were pushed in order dim1, dim2, ... so dimN is on top.
        // Pop gives dimN first. Reverse to get dim1 first for row-major.
        std::reverse(indices.begin(), indices.end());
        try {
          arrays_.at(var_name(vidx), indices) = val;
        } catch (const std::exception& e) {
          errors_.push_back(e.what());
          return false;
        }
        break;
      }
      case OpCode::ReadVar: {
        std::uint8_t vidx = static_cast<std::uint8_t>(inst.operand);
        Value v = data_pool_.read();
        var_values_[vidx] = v.coerce(var_type(vidx));
        break;
      }
      case OpCode::ReadArray: {
        errors_.push_back("ReadArray not implemented");
        return false;
      }
      case OpCode::SetOptionBase: {
        option_base_ = static_cast<std::int32_t>(inst.operand);
        break;
      }
      case OpCode::Restore: {
        data_pool_.restore();
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
