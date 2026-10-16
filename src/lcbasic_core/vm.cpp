#include "vm.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

Vm::Vm(Screen& screen, Host& host) : screen_(screen), host_(host) {}

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
  return deduce_var_type(var_name(idx));
}

std::int32_t Vm::current_line() const {
  if (ip_ < code_.size()) return code_[ip_].line;
  if (!code_.empty()) return code_.back().line;
  return 0;
}

std::vector<Vm::ForFrameView> Vm::for_frames() const {
  std::vector<ForFrameView> out;
  for (const auto& f : for_stack_) {
    out.push_back(ForFrameView{f.var_index, f.limit, f.step, f.body_start_pc});
  }
  return out;
}

std::optional<std::size_t> Vm::find_pc_for_line(std::int32_t line) const {
  auto it = std::lower_bound(
      line_table_.begin(), line_table_.end(), line,
      [](const LineEntry& e, std::int32_t v) { return e.line_number < v; });
  if (it != line_table_.end() && it->line_number == line) return it->pc_offset;
  return std::nullopt;
}

[[noreturn]] void Vm::trap_error(int error_code, std::int32_t line) {
  err_code_ = error_code;
  erl_line_ = line;
  if (error_handler_line_ >= 0 && !in_error_handler_) {
    in_error_handler_ = true;
    throw std::runtime_error("__error_trap__");
  }
  errors_.push_back("error " + std::to_string(error_code) + " at line " +
                    std::to_string(line));
  throw std::runtime_error("__fatal__");
}

void Vm::load(const std::vector<Instruction>& code,
              const std::vector<Value>& constants,
              const std::vector<LineEntry>& line_table,
              const std::unordered_map<std::string, std::uint8_t>& variable_table,
              const std::vector<std::string>& variable_names,
              const std::vector<Value>& data_values) {
  code_ = code;
  constants_ = constants;
  line_table_ = line_table;

  std::uint8_t max_idx = 0;
  for (const auto& [name, idx] : variable_table) {
    if (idx > max_idx) max_idx = idx;
  }
  var_values_.assign(max_idx + 1, Value{});
  variable_names_ = variable_names;

  file_handles_.assign(256, -1);
  data_pool_ = DataPool{};
  for (const auto& v : data_values) {
    data_pool_.add(v);
  }

  ip_ = 0;
  halted_ = false;
  error_handler_line_ = -1;
  handler_pc_ = 0;
  err_code_ = 0;
  erl_line_ = 0;
  in_error_handler_ = false;
  errors_.clear();
  stack_.clear();
  call_stack_.clear();
  for_stack_.clear();
  arrays_ = ArrayStore{};
}

bool Vm::run() {
  ExecStatus st = execute(false);
  return st == ExecStatus::Halted;
}

ExecStatus Vm::run_with_hook(DbgHook* hook) {
  hook_ = hook;
  ExecStatus st = execute(false);
  hook_ = nullptr;
  return st;
}

ExecStatus Vm::step_one() {
  return execute(true);
}

// Execute instructions. If single_step is true, only one instruction runs.
// The hook (if set) is consulted before each instruction.
ExecStatus Vm::execute(bool single_step) {
  while (ip_ < code_.size()) {
    if (hook_) {
      if (hook_->on_instruction(ip_, code_[ip_].line, call_stack_.size())) {
        return ExecStatus::Paused;
      }
    }

    const Instruction inst = code_[ip_];
    ip_++;
    std::int32_t current_line = inst.line;

    try {
      switch (inst.op) {
        case OpCode::Constant: {
          if (inst.operand >= constants_.size()) {
            trap_error(5, current_line);
          }
          push(constants_[inst.operand]);
          break;
        }
        case OpCode::LoadVar: {
          if (inst.operand >= var_values_.size()) {
            trap_error(5, current_line);
            break;
          }
          std::string name = var_name(inst.operand);
          if (name == "ERR") {
            push(Value(static_cast<double>(err_code_)));
          } else if (name == "ERL") {
            push(Value(static_cast<double>(erl_line_)));
          } else {
            push(var_values_[inst.operand]);
          }
          break;
        }
        case OpCode::StoreVar: {
          if (inst.operand >= var_values_.size()) {
            trap_error(5, current_line);
            break;
          }
          std::string name = var_name(inst.operand);
          Value v = pop();
          if (name == "ERR") {
            err_code_ = static_cast<int>(v.as_double());
          } else if (name == "ERL") {
            erl_line_ = static_cast<std::int32_t>(v.as_double());
          } else {
            var_values_[inst.operand] = v.coerce(var_type(inst.operand));
          }
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
            trap_error(3, current_line);
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
            trap_error(3, current_line);
            break;
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
          } catch (const std::exception&) {
            trap_error(5, current_line);
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
          std::reverse(indices.begin(), indices.end());
          try {
            arrays_.at(var_name(vidx), indices) = val;
          } catch (const std::exception&) {
            trap_error(5, current_line);
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
          trap_error(5, current_line);
          break;
        }
        case OpCode::Restore: {
          data_pool_.restore();
          break;
        }
        case OpCode::SetOptionBase: {
          option_base_ = static_cast<std::int32_t>(inst.operand);
          break;
        }
        case OpCode::Open: {
          double fn_d = pop().as_double();
          std::string mode = pop().as_string();
          std::string filename = pop().as_string();
          int filenum = static_cast<int>(fn_d);
          if (filenum < 1 || filenum > 255) {
            trap_error(5, current_line);
            break;
          }
          if (file_handles_[filenum] >= 0) {
            host_.file_close(file_handles_[filenum]);
          }
          int handle = host_.file_open(filename, mode);
          if (handle < 0) {
            trap_error(52, current_line);
            break;
          }
          file_handles_[filenum] = handle;
          break;
        }
        case OpCode::Close: {
          double fn_d = pop().as_double();
          int filenum = static_cast<int>(fn_d);
          if (filenum < 1 || filenum > 255 || file_handles_[filenum] < 0) {
            trap_error(52, current_line);
            break;
          }
          host_.file_close(file_handles_[filenum]);
          file_handles_[filenum] = -1;
          break;
        }
        case OpCode::PrintFile: {
          Value val = pop();
          double fn_d = pop().as_double();
          int filenum = static_cast<int>(fn_d);
          if (filenum < 1 || filenum > 255 || file_handles_[filenum] < 0) {
            trap_error(52, current_line);
            break;
          }
          int handle = file_handles_[filenum];
          host_.file_write(handle, val.as_string());
          break;
        }
        case OpCode::InputFile: {
          double fn_d = pop().as_double();
          int filenum = static_cast<int>(fn_d);
          if (filenum < 1 || filenum > 255 || file_handles_[filenum] < 0) {
            trap_error(52, current_line);
            break;
          }
          int handle = file_handles_[filenum];
          std::string line_text = host_.file_read_line(handle);
          std::uint8_t vidx = static_cast<std::uint8_t>(inst.operand);
          if (vidx >= var_values_.size()) {
            trap_error(5, current_line);
            break;
          }
          var_values_[vidx] = Value(line_text).coerce(var_type(vidx));
          break;
        }
        case OpCode::Eof: {
          double fn_d = pop().as_double();
          int filenum = static_cast<int>(fn_d);
          if (filenum < 1 || filenum > 255 || file_handles_[filenum] < 0) {
            push(Value(-1.0));
          } else {
            push(Value(host_.file_eof(file_handles_[filenum]) ? -1.0 : 0.0));
          }
          break;
        }
        case OpCode::OnError: {
          handler_pc_ = inst.operand;
          error_handler_line_ = current_line;
          break;
        }
        case OpCode::Resume: {
          if (!in_error_handler_) {
            trap_error(3, current_line);
            break;
          }
          in_error_handler_ = false;
          if ((inst.operand & 1) == 1) {
            break;
          } else {
            auto pc = find_pc_for_line(erl_line_);
            if (pc) ip_ = *pc;
          }
          break;
        }
        case OpCode::Halt: {
          halted_ = true;
          return ExecStatus::Halted;
        }
        default: {
          trap_error(5, current_line);
          break;
        }
      }
    } catch (const std::runtime_error& e) {
      if (e.what() == std::string("__error_trap__")) {
        if (handler_pc_ > 0) {
          ip_ = handler_pc_;
          continue;
        }
      }
      return ExecStatus::Error;
    }

    if (single_step) {
      if (ip_ < code_.size()) return ExecStatus::Paused;
      return ExecStatus::Halted;
    }
  }

  halted_ = true;
  return ExecStatus::Halted;
}

// ----- Debugger state access -----

std::optional<Value> Vm::get_variable(const std::string& name) const {
  for (std::size_t i = 0; i < variable_names_.size(); i++) {
    if (variable_names_[i] == name) {
      if (i < var_values_.size()) return var_values_[i];
      return Value{};
    }
  }
  return std::nullopt;
}

bool Vm::set_variable(const std::string& name, const Value& v) {
  for (std::size_t i = 0; i < variable_names_.size(); i++) {
    if (variable_names_[i] == name) {
      if (i < var_values_.size()) {
        var_values_[i] = v.coerce(var_type(static_cast<std::uint8_t>(i)));
        return true;
      }
    }
  }
  return false;
}

Vm::Snapshot Vm::snapshot() const {
  Snapshot s;
  s.ip = ip_;
  s.var_values = var_values_;
  s.stack = stack_;
  s.call_stack = call_stack_;
  s.data_ptr = 0;  // DataPool pointer is internal; snapshot captures values.
  return s;
}

void Vm::restore(const Snapshot& s) {
  ip_ = s.ip;
  var_values_ = s.var_values;
  stack_ = s.stack;
  call_stack_ = s.call_stack;
}
