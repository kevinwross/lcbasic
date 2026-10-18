#include "vm.h"
#include "number_format.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
  std::string name = var_name(idx);
  if (has_type_suffix(name)) return deduce_var_type(name);
  if (!name.empty()) {
    char c = static_cast<char>(
        std::toupper(static_cast<unsigned char>(name[0])));
    if (c >= 'A' && c <= 'Z') return letter_types_[c - 'A'];
  }
  return VarType::VT_DBL;
}

int Vm::compare_values(const Value& a, const Value& b, std::int32_t line) {
  if (a.is_string() != b.is_string()) trap_error(13, line);  // Type mismatch
  if (a.is_string()) {
    int c = a.as_string().compare(b.as_string());
    return (c < 0) ? -1 : (c > 0 ? 1 : 0);
  }
  double x = a.as_double(), y = b.as_double();
  return (x < y) ? -1 : (x > y ? 1 : 0);
}

Value Vm::coerce_value(const Value& v, VarType t, std::int32_t line) {
  if (t == VarType::VT_STR) {
    if (!v.is_string()) trap_error(13, line);  // Type mismatch
    return v;
  }
  if (v.is_string()) trap_error(13, line);     // Type mismatch
  double d = v.as_double();
  switch (t) {
    case VarType::VT_INT16: {
      double r = std::round(d);
      if (r < -32768.0 || r > 32767.0) trap_error(6, line);  // Overflow
      return Value(static_cast<Int16>(r));
    }
    case VarType::VT_INT32: {
      double r = std::round(d);
      if (r < -2147483648.0 || r > 2147483647.0) trap_error(6, line);
      return Value(static_cast<Int32>(r));
    }
    case VarType::VT_SGL: {
      float f = static_cast<float>(d);
      if (std::isinf(f)) trap_error(6, line);  // Overflow
      return Value(f);
    }
    case VarType::VT_DBL:
      return Value(d);
    default:
      return v;
  }
}

Value Vm::call_builtin(Builtin b, const std::vector<Value>& args,
                       std::int32_t line) {
  auto num = [&](std::size_t i) -> double {
    return (i < args.size()) ? args[i].as_double() : 0.0;
  };
  auto str = [&](std::size_t i) -> std::string {
    return (i < args.size()) ? args[i].as_string() : std::string();
  };

  switch (b) {
    case Builtin::Abs: return Value(std::fabs(num(0)));
    case Builtin::Sgn: {
      double x = num(0);
      return Value(x < 0.0 ? -1.0 : (x > 0.0 ? 1.0 : 0.0));
    }
    case Builtin::Int: return Value(std::floor(num(0)));
    case Builtin::Fix: return Value(std::trunc(num(0)));
    case Builtin::Sqr: {
      double x = num(0);
      if (x < 0.0) trap_error(5, line);
      return Value(std::sqrt(x));
    }
    case Builtin::Sin: return Value(std::sin(num(0)));
    case Builtin::Cos: return Value(std::cos(num(0)));
    case Builtin::Tan: return Value(std::tan(num(0)));
    case Builtin::Atn: return Value(std::atan(num(0)));
    case Builtin::Log: {
      double x = num(0);
      if (x <= 0.0) trap_error(5, line);
      return Value(std::log(x));
    }
    case Builtin::Exp: return Value(std::exp(num(0)));
    case Builtin::Rnd: {
      double s = args.empty() ? 1.0 : num(0);
      return Value(host_.rnd(s));
    }
    case Builtin::CInt: {
      double x = std::round(num(0));
      if (x < -32768.0 || x > 32767.0) trap_error(6, line);
      return Value(static_cast<Int16>(x));
    }
    case Builtin::CSng: {
      float f = static_cast<float>(num(0));
      if (std::isinf(f)) trap_error(6, line);
      return Value(f);
    }
    case Builtin::CDbl: return Value(num(0));
    case Builtin::Len:
      return Value(static_cast<double>(str(0).size()));
    case Builtin::Left: {
      std::string s = str(0);
      int n = static_cast<int>(num(1));
      if (n < 0) trap_error(5, line);
      if (n > static_cast<int>(s.size())) n = static_cast<int>(s.size());
      return Value(s.substr(0, static_cast<std::size_t>(n)));
    }
    case Builtin::Right: {
      std::string s = str(0);
      int n = static_cast<int>(num(1));
      if (n < 0) trap_error(5, line);
      if (n > static_cast<int>(s.size())) n = static_cast<int>(s.size());
      return Value(s.substr(s.size() - static_cast<std::size_t>(n)));
    }
    case Builtin::Mid: {
      std::string s = str(0);
      int start = static_cast<int>(num(1));
      int len = (args.size() >= 3) ? static_cast<int>(num(2))
                                   : static_cast<int>(s.size());
      if (start < 1) start = 1;
      if (len < 0) trap_error(5, line);
      if (start > static_cast<int>(s.size())) return Value(std::string());
      int pos = start - 1;
      int n = std::min(len, static_cast<int>(s.size()) - pos);
      return Value(s.substr(static_cast<std::size_t>(pos),
                            static_cast<std::size_t>(n)));
    }
    case Builtin::Instr: {
      int start = 1;
      std::string s1, s2;
      if (args.size() >= 3) {
        start = static_cast<int>(num(0));
        s1 = str(1);
        s2 = str(2);
      } else {
        s1 = str(0);
        s2 = str(1);
      }
      if (start < 1) start = 1;
      if (start > static_cast<int>(s1.size()) + 1) return Value(0.0);
      std::size_t pos = s1.find(s2, static_cast<std::size_t>(start - 1));
      return Value(pos == std::string::npos
                       ? 0.0
                       : static_cast<double>(pos + 1));
    }
    case Builtin::Val: {
      std::string s = str(0);
      const char* c = s.c_str();
      char* end = nullptr;
      double v = std::strtod(c, &end);
      if (end == c) return Value(0.0);
      return Value(v);
    }
    case Builtin::Str: {
      std::string s = format_basic_number(num(0));
      if (!s.empty() && s.back() == ' ') s.pop_back();
      return Value(s);
    }
    case Builtin::Chr: {
      int c = static_cast<int>(num(0));
      return Value(std::string(1, static_cast<char>(c & 0xFF)));
    }
    case Builtin::Asc: {
      std::string s = str(0);
      return Value(s.empty()
                       ? 0.0
                       : static_cast<double>(
                             static_cast<unsigned char>(s[0])));
    }
    case Builtin::Space: {
      int n = static_cast<int>(num(0));
      if (n < 0) n = 0;
      return Value(std::string(static_cast<std::size_t>(n), ' '));
    }
    case Builtin::StringFn: {
      int n = static_cast<int>(num(0));
      if (n < 0) n = 0;
      char c = ' ';
      if (args.size() >= 2 && args[1].is_string()) {
        std::string s = args[1].as_string();
        c = s.empty() ? ' ' : s[0];
      } else {
        c = static_cast<char>(static_cast<int>(num(1)) & 0xFF);
      }
      return Value(std::string(static_cast<std::size_t>(n), c));
    }
    case Builtin::UCase: {
      std::string s = str(0);
      for (char& c : s) {
        c = static_cast<char>(
            std::toupper(static_cast<unsigned char>(c)));
      }
      return Value(s);
    }
    case Builtin::LCase: {
      std::string s = str(0);
      for (char& c : s) {
        c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));
      }
      return Value(s);
    }
    case Builtin::Hex: {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%X",
                    static_cast<unsigned>(static_cast<long>(num(0))));
      return Value(std::string(buf));
    }
    case Builtin::Oct: {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%o",
                    static_cast<unsigned>(static_cast<long>(num(0))));
      return Value(std::string(buf));
    }
    case Builtin::Tab: {
      int target = static_cast<int>(num(0));
      int col = screen_.column();  // 1-based
      int spaces = target - col;
      if (spaces < 0) spaces = 0;
      return Value(std::string(static_cast<std::size_t>(spaces), ' '));
    }
    case Builtin::Spc: {
      int n = static_cast<int>(num(0));
      if (n < 0) n = 0;
      return Value(std::string(static_cast<std::size_t>(n), ' '));
    }
    case Builtin::Fre:
      return Value(65536.0);
    case Builtin::Pos:
      return Value(static_cast<double>(screen_.column() - 1));
    case Builtin::Csrlin:
      return Value(static_cast<double>(screen_.row() - 1));
    case Builtin::Inkey: {
      auto k = screen_.get_key(false);
      if (!k) return Value(std::string(""));
      return Value(std::string(1, static_cast<char>(*k)));
    }
    case Builtin::Date:
      return Value(host_.date_string());
    case Builtin::Time:
      return Value(host_.time_string());
    case Builtin::InputDollar: {
      int n = static_cast<int>(num(0));
      if (n < 0) n = 0;
      std::string out;
      if (args.size() >= 2) {
        int filenum = static_cast<int>(num(1));
        if (filenum < 1 || filenum > 255 || file_handles_[filenum] < 0) {
          trap_error(52, line);
          return Value(std::string(""));
        }
        out = host_.file_read_chars(file_handles_[filenum], n);
      } else {
        for (int i = 0; i < n; i++) {
          auto k = screen_.get_key(true);
          if (!k) break;
          out += static_cast<char>(*k);
        }
      }
      return Value(out);
    }
  }
  return Value{};
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
  if (handler_pc_ > 0 && !in_error_handler_) {
    in_error_handler_ = true;
    throw std::runtime_error("__error_trap__");
  }
  errors_.push_back("error " + std::to_string(error_code) + " at line " +
                    std::to_string(line));
  throw std::runtime_error("__fatal__");
}

bool Vm::check_events() {
  if (event_active_) return false;

  // KEY: fire the enabled KEY handler if a key is waiting.
  constexpr int kKey = static_cast<int>(EventKind::Key);
  EventSlot& key = events_[kKey];
  if (key.enabled && !key.stopped && key.handler_pc != 0) {
    auto k = screen_.get_key(false);
    if (k) {
      key.in_handler = true;
      event_active_ = true;
      event_depth_ = call_stack_.size();
      call_stack_.push_back(ip_);
      ip_ = key.handler_pc;
      return true;
    }
  }

  // TIMER: fire when the interval has elapsed.
  constexpr int kTimer = static_cast<int>(EventKind::Timer);
  EventSlot& timer = events_[kTimer];
  if (timer.enabled && !timer.stopped && timer.handler_pc != 0) {
    double now = host_.timer();
    if (now - timer.last_fire >= timer.interval) {
      timer.last_fire = now;
      timer.in_handler = true;
      event_active_ = true;
      event_depth_ = call_stack_.size();
      call_stack_.push_back(ip_);
      ip_ = timer.handler_pc;
      return true;
    }
  }
  return false;
}

void Vm::load(const std::vector<Instruction>& code,
              const std::vector<Value>& constants,
              const std::vector<LineEntry>& line_table,
              const std::unordered_map<std::string, std::uint8_t>& variable_table,
              const std::vector<std::string>& variable_names,
              const std::vector<Value>& data_values,
              const std::vector<FnDesc>& fn_descs) {
  code_ = code;
  constants_ = constants;
  line_table_ = line_table;
  fn_descs_ = fn_descs;
  temp_values_.assign(8, Value{});
  fn_frames_.clear();
  fn_return_.clear();

  std::uint8_t max_idx = 0;
  for (const auto& [name, idx] : variable_table) {
    if (idx > max_idx) max_idx = idx;
  }
  var_values_.assign(max_idx + 1, Value{});
  var_init_.assign(max_idx + 1, false);
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

  letter_types_.fill(VarType::VT_DBL);
  for (auto& ev : events_) ev = EventSlot{};
  event_depth_ = 0;
  event_active_ = false;
  last_line_ = -1;
  stopped_ = false;
}

bool Vm::run() {
  stopped_ = false;
  ExecStatus st = execute(false);
  return st == ExecStatus::Halted;
}

ExecStatus Vm::run_with_hook(DbgHook* hook) {
  hook_ = hook;
  stopped_ = false;
  ExecStatus st = execute(false);
  hook_ = nullptr;
  return st;
}

ExecStatus Vm::step_one() {
  stopped_ = false;
  return execute(true);
}

// Execute instructions. If single_step is true, only one instruction runs.
// The hook (if set) is consulted before each instruction.
ExecStatus Vm::execute(bool single_step) {
  while (ip_ < code_.size()) {
    // Fire due timer events at line boundaries.
    if (code_[ip_].line != last_line_) {
      last_line_ = code_[ip_].line;
      if (check_events()) {
        continue;  // ip_ moved to the handler
      }
    }

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
          } else if (!var_init_[inst.operand]) {
            // Uninitialised: empty string for string vars, zero otherwise.
            push(default_value(var_type(inst.operand)));
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
            var_values_[inst.operand] =
                coerce_value(v, var_type(inst.operand), current_line);
            var_init_[inst.operand] = true;
          }
          break;
        }
        case OpCode::Add: {
          Value b = pop();
          Value a = pop();
          if (a.is_string() && b.is_string()) {
            push(Value(a.as_string() + b.as_string()));
          } else if (!a.is_string() && !b.is_string()) {
            push(Value(a.as_double() + b.as_double()));
          } else {
            trap_error(13, current_line);  // Type mismatch
          }
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
          if (b.as_double() == 0.0) {
            trap_error(11, current_line);  // Division by zero
            break;
          }
          push(Value(a.as_double() / b.as_double()));
          break;
        }
        case OpCode::Eq: {
          Value b = pop();
          Value a = pop();
          push(Value(compare_values(a, b, current_line) == 0 ? -1.0 : 0.0));
          break;
        }
        case OpCode::Ne: {
          Value b = pop();
          Value a = pop();
          push(Value(compare_values(a, b, current_line) != 0 ? -1.0 : 0.0));
          break;
        }
        case OpCode::Lt: {
          Value b = pop();
          Value a = pop();
          push(Value(compare_values(a, b, current_line) < 0 ? -1.0 : 0.0));
          break;
        }
        case OpCode::Le: {
          Value b = pop();
          Value a = pop();
          push(Value(compare_values(a, b, current_line) <= 0 ? -1.0 : 0.0));
          break;
        }
        case OpCode::Gt: {
          Value b = pop();
          Value a = pop();
          push(Value(compare_values(a, b, current_line) > 0 ? -1.0 : 0.0));
          break;
        }
        case OpCode::Ge: {
          Value b = pop();
          Value a = pop();
          push(Value(compare_values(a, b, current_line) >= 0 ? -1.0 : 0.0));
          break;
        }
        case OpCode::And: {
          Value b = pop();
          Value a = pop();
          Int16 x = static_cast<Int16>(std::round(a.as_double()));
          Int16 y = static_cast<Int16>(std::round(b.as_double()));
          push(Value(static_cast<double>(static_cast<Int16>(x & y))));
          break;
        }
        case OpCode::Or: {
          Value b = pop();
          Value a = pop();
          Int16 x = static_cast<Int16>(std::round(a.as_double()));
          Int16 y = static_cast<Int16>(std::round(b.as_double()));
          push(Value(static_cast<double>(static_cast<Int16>(x | y))));
          break;
        }
        case OpCode::Xor: {
          Value b = pop();
          Value a = pop();
          Int16 x = static_cast<Int16>(std::round(a.as_double()));
          Int16 y = static_cast<Int16>(std::round(b.as_double()));
          push(Value(static_cast<double>(static_cast<Int16>(x ^ y))));
          break;
        }
        case OpCode::Eqv: {
          Value b = pop();
          Value a = pop();
          Int16 x = static_cast<Int16>(std::round(a.as_double()));
          Int16 y = static_cast<Int16>(std::round(b.as_double()));
          push(Value(static_cast<double>(
              static_cast<Int16>(~(x ^ y)))));
          break;
        }
        case OpCode::Imp: {
          Value b = pop();
          Value a = pop();
          Int16 x = static_cast<Int16>(std::round(a.as_double()));
          Int16 y = static_cast<Int16>(std::round(b.as_double()));
          push(Value(static_cast<double>(
              static_cast<Int16>((~x) | y))));
          break;
        }
        case OpCode::Not: {
          Value a = pop();
          Int16 x = static_cast<Int16>(std::round(a.as_double()));
          push(Value(static_cast<double>(static_cast<Int16>(~x))));
          break;
        }
        case OpCode::Print: {
          Value v = pop();
          if (v.is_string()) {
            screen_.print(v.as_string());
          } else {
            screen_.print(format_basic_number(v.as_double()));
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
          // If we just returned from an event handler, clear the flags.
          if (event_active_ && call_stack_.size() <= event_depth_) {
            event_active_ = false;
            for (auto& ev : events_) ev.in_handler = false;
          }
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
          var_values_[frame.var_index] =
              coerce_value(Value(var_val), var_type(frame.var_index),
                           current_line);
          var_init_[frame.var_index] = true;
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
          std::string dim_name = var_name(vidx);
          if (arrays_.contains(dim_name)) {
            trap_error(10, current_line);  // Duplicate definition
            break;
          }
          arrays_.dim(dim_name, option_base_, upper_bounds, var_type(vidx));
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
          } catch (const std::out_of_range&) {
            trap_error(9, current_line);  // Subscript out of range
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
            std::string name = var_name(vidx);
            // Coerce to the array's element type (Type mismatch / Overflow).
            val = coerce_value(val, arrays_.type_of(name), current_line);
            arrays_.at(name, indices) = val;
          } catch (const std::out_of_range&) {
            trap_error(9, current_line);  // Subscript out of range
          }
          break;
        }
        case OpCode::ReadVar: {
          std::uint8_t vidx = static_cast<std::uint8_t>(inst.operand);
          Value v = data_pool_.read();
          var_values_[vidx] = coerce_value(v, var_type(vidx), current_line);
          var_init_[vidx] = true;
          break;
        }
        case OpCode::ReadArray: {
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
            std::string name = var_name(vidx);
            Value v = data_pool_.read();
            v = coerce_value(v, arrays_.type_of(name), current_line);
            arrays_.at(name, indices) = v;
          } catch (const std::out_of_range&) {
            trap_error(9, current_line);
          }
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
        case OpCode::NextZone: {
          // Advance to the next 14-column print zone.
          int col = screen_.column();  // 1-based
          int next_zone = ((col - 1) / 14 + 1) * 14 + 1;
          int spaces = next_zone - col;
          if (spaces < 0) spaces = 0;
          for (int i = 0; i < spaces; i++) screen_.putc(' ');
          break;
        }
        case OpCode::DefType: {
          char lo = static_cast<char>(inst.operand & 0xFF);
          char hi = static_cast<char>((inst.operand >> 8) & 0xFF);
          VarType t = static_cast<VarType>((inst.operand >> 16) & 0xFF);
          if (lo < 'A') lo = 'A';
          if (hi > 'Z') hi = 'Z';
          for (char c = lo; c <= hi; c++) {
            letter_types_[c - 'A'] = t;
          }
          break;
        }
        case OpCode::TimerFunc: {
          push(Value(host_.timer()));
          break;
        }
        case OpCode::ChDir: {
          std::string path = pop().as_string();
          if (!host_.dos_chdir(path)) trap_error(76, current_line);
          break;
        }
        case OpCode::MkDir: {
          std::string path = pop().as_string();
          if (!host_.dos_mkdir(path)) trap_error(76, current_line);
          break;
        }
        case OpCode::RmDir: {
          std::string path = pop().as_string();
          if (!host_.dos_rmdir(path)) trap_error(76, current_line);
          break;
        }
        case OpCode::KillFile: {
          std::string path = pop().as_string();
          if (!host_.dos_kill(path)) trap_error(53, current_line);
          break;
        }
        case OpCode::NameFile: {
          std::string from = pop().as_string();
          std::string to = pop().as_string();
          if (!host_.dos_rename(from, to)) trap_error(53, current_line);
          break;
        }
        case OpCode::Files: {
          std::string pattern = pop().as_string();
          auto files = host_.dos_files(pattern);
          for (const auto& f : files) {
            screen_.print(f);
            screen_.print("\n");
          }
          break;
        }
        case OpCode::Shell: {
          std::string cmd = pop().as_string();
          host_.dos_shell(cmd);
          break;
        }
        case OpCode::OnEvent: {
          double packed = pop().as_double();
          int source = static_cast<int>(packed) & 0xFFFF;
          int kind = (static_cast<int>(packed) >> 16) & 0xFF;
          if (kind < 0 || kind >= 6) {
            trap_error(5, current_line);
            break;
          }
          EventSlot& ev = events_[kind];
          ev.source = source;
          ev.handler_pc = inst.operand;  // patched to target PC by fixup
          ev.handler_line = current_line;
          if (kind == static_cast<int>(EventKind::Timer)) {
            ev.interval = source > 0 ? static_cast<double>(source) : 1.0;
          }
          break;
        }
        case OpCode::EventControl: {
          int source = inst.operand & 0xFFFF;
          int kind = (inst.operand >> 16) & 0xFF;
          int action = (inst.operand >> 24) & 0xFF;
          if (kind < 0 || kind >= 6) {
            trap_error(5, current_line);
            break;
          }
          EventSlot& ev = events_[kind];
          if (action == 0) {        // ON
            ev.enabled = true;
            ev.stopped = false;
            ev.last_fire = host_.timer();
          } else if (action == 1) { // OFF
            ev.enabled = false;
          } else {                  // STOP
            ev.stopped = true;
          }
          (void)source;
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
          // Close a random-access file if that's what this number refers to.
          auto rit = random_files_.find(filenum);
          if (rit != random_files_.end()) {
            if (rit->second.handle >= 0) {
              host_.random_close(rit->second.handle);
            }
            random_files_.erase(rit);
            break;
          }
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
          std::string out_text =
              val.is_string() ? val.as_string()
                              : format_basic_number(val.as_double());
          host_.file_write(handle, out_text);
          break;
        }
        case OpCode::InputFile: {
          int count = static_cast<int>(inst.operand & 0xFF);
          bool line_input = (inst.operand & 0x100) != 0;

          struct Target {
            std::uint8_t vidx;
            bool is_array;
            std::vector<std::int32_t> indices;
          };
          std::vector<Target> targets(static_cast<std::size_t>(count));
          for (int i = count - 1; i >= 0; --i) {
            std::uint8_t vidx = static_cast<std::uint8_t>(pop().as_double());
            int n = static_cast<int>(std::round(pop().as_double()));
            std::vector<std::int32_t> indices;
            for (int k = 0; k < n; k++) {
              indices.push_back(
                  static_cast<std::int32_t>(std::round(pop().as_double())));
            }
            std::reverse(indices.begin(), indices.end());
            targets[static_cast<std::size_t>(i)] =
                Target{vidx, n > 0, std::move(indices)};
          }

          double fn_d = pop().as_double();
          int filenum = static_cast<int>(fn_d);
          if (filenum < 1 || filenum > 255 || file_handles_[filenum] < 0) {
            trap_error(52, current_line);
            break;
          }
          int handle = file_handles_[filenum];

          std::vector<std::string> items;
          if (line_input) {
            items.push_back(host_.file_read_line(handle));
          } else {
            while (static_cast<int>(items.size()) < count) {
              std::string line_text = host_.file_read_line(handle);
              std::string cur;
              for (char c : line_text) {
                if (c == ',') {
                  items.push_back(cur);
                  cur.clear();
                } else {
                  cur += c;
                }
              }
              items.push_back(cur);
              if (line_text.empty()) break;  // EOF guard
            }
          }

          auto trim = [](std::string s) {
            std::size_t a = s.find_first_not_of(" \t");
            if (a == std::string::npos) return std::string();
            std::size_t b = s.find_last_not_of(" \t");
            return s.substr(a, b - a + 1);
          };

          for (int i = 0; i < count; i++) {
            std::string item =
                (i < static_cast<int>(items.size())) ? trim(items[i]) : "";
            const Target& t = targets[static_cast<std::size_t>(i)];
            VarType vt = t.is_array ? arrays_.type_of(var_name(t.vidx))
                                    : var_type(t.vidx);
            Value v;
            if (vt == VarType::VT_STR) {
              if (item.size() >= 2 && item.front() == '"' &&
                  item.back() == '"') {
                item = item.substr(1, item.size() - 2);
              }
              v = Value(item);
            } else {
              try {
                v = Value(std::stod(item));
              } catch (...) {
                trap_error(13, current_line);  // Type mismatch
                break;
              }
            }
            if (t.is_array) {
              try {
                v = coerce_value(v, vt, current_line);
                arrays_.at(var_name(t.vidx), t.indices) = v;
              } catch (const std::out_of_range&) {
                trap_error(9, current_line);
              }
            } else {
              var_values_[t.vidx] = coerce_value(v, vt, current_line);
              var_init_[t.vidx] = true;
            }
          }
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
          if (inst.operand == 0) {
            // ON ERROR GOTO 0: disable the handler.
            handler_pc_ = 0;
            error_handler_line_ = -1;
            in_error_handler_ = false;
          } else {
            handler_pc_ = inst.operand;
            error_handler_line_ = current_line;
          }
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
        case OpCode::Input: {
          int count = static_cast<int>(inst.operand & 0xFF);
          bool line_input = (inst.operand & 0x100) != 0;
          bool prompt_comma = (inst.operand & 0x200) != 0;

          struct Target {
            std::uint8_t vidx;
            bool is_array;
            std::vector<std::int32_t> indices;
          };
          std::vector<Target> targets(static_cast<std::size_t>(count));
          for (int i = count - 1; i >= 0; --i) {
            std::uint8_t vidx = static_cast<std::uint8_t>(pop().as_double());
            int n = static_cast<int>(std::round(pop().as_double()));
            std::vector<std::int32_t> indices;
            for (int k = 0; k < n; k++) {
              indices.push_back(
                  static_cast<std::int32_t>(std::round(pop().as_double())));
            }
            std::reverse(indices.begin(), indices.end());
            targets[static_cast<std::size_t>(i)] =
                Target{vidx, n > 0, std::move(indices)};
          }

          std::string prompt = pop().as_string();
          if (!prompt.empty()) {
            screen_.print(prompt);
            if (prompt_comma) screen_.print("? ");
          } else if (!line_input) {
            screen_.print("? ");
          }
          screen_.refresh();

          auto trim = [](std::string s) {
            std::size_t a = s.find_first_not_of(" \t");
            if (a == std::string::npos) return std::string();
            std::size_t b = s.find_last_not_of(" \t");
            return s.substr(a, b - a + 1);
          };

          int attempts = 0;
          for (;;) {
            std::string line = host_.input_line();
            std::vector<std::string> items;
            if (line_input) {
              items.push_back(line);
            } else {
              std::string cur;
              for (char c : line) {
                if (c == ',') {
                  items.push_back(cur);
                  cur.clear();
                } else {
                  cur += c;
                }
              }
              items.push_back(cur);
            }

            std::vector<Value> parsed(static_cast<std::size_t>(count));
            bool ok = true;
            for (int i = 0; i < count; i++) {
              std::string item =
                  (i < static_cast<int>(items.size())) ? trim(items[i]) : "";
              const Target& t = targets[static_cast<std::size_t>(i)];
              VarType vt = t.is_array ? arrays_.type_of(var_name(t.vidx))
                                      : var_type(t.vidx);
              if (vt == VarType::VT_STR) {
                if (item.size() >= 2 && item.front() == '"' &&
                    item.back() == '"') {
                  item = item.substr(1, item.size() - 2);
                }
                parsed[static_cast<std::size_t>(i)] = Value(item);
              } else {
                try {
                  parsed[static_cast<std::size_t>(i)] =
                      Value(std::stod(item));
                } catch (...) {
                  ok = false;
                  break;
                }
              }
            }

            if (!ok) {
              if (++attempts >= 5) {
                trap_error(13, current_line);  // Type mismatch
                break;
              }
              screen_.print("?Redo from start\n");
              screen_.refresh();
              continue;
            }

            for (int i = 0; i < count; i++) {
              const Target& t = targets[static_cast<std::size_t>(i)];
              if (t.is_array) {
                try {
                  VarType vt = arrays_.type_of(var_name(t.vidx));
                  Value v = coerce_value(parsed[static_cast<std::size_t>(i)],
                                         vt, current_line);
                  arrays_.at(var_name(t.vidx), t.indices) = v;
                } catch (const std::out_of_range&) {
                  trap_error(9, current_line);
                }
              } else {
                var_values_[t.vidx] =
                    coerce_value(parsed[static_cast<std::size_t>(i)],
                                 var_type(t.vidx), current_line);
                var_init_[t.vidx] = true;
              }
            }
            break;
          }
          break;
        }
        case OpCode::Cls: {
          screen_.clear();
          break;
        }
        case OpCode::Locate: {
          int row = static_cast<int>(std::round(pop().as_double()));
          int col = static_cast<int>(std::round(pop().as_double()));
          screen_.locate(row, col);
          break;
        }
        case OpCode::Color: {
          int fg = static_cast<int>(std::round(pop().as_double()));
          int bg = static_cast<int>(std::round(pop().as_double()));
          screen_.color(fg, bg);
          break;
        }
        case OpCode::Erase: {
          arrays_.erase(var_name(static_cast<std::uint8_t>(inst.operand)));
          break;
        }
        case OpCode::Clear: {
          for (std::size_t i = 0; i < var_values_.size(); i++) {
            var_values_[i] = default_value(var_type(
                static_cast<std::uint8_t>(i)));
            var_init_[i] = false;
          }
          arrays_.clear();
          data_pool_ = DataPool{};
          for_stack_.clear();
          break;
        }
        case OpCode::RaiseError: {
          int code = static_cast<int>(std::round(pop().as_double()));
          trap_error(code, current_line);
          break;
        }
        case OpCode::MidAssign: {
          std::uint8_t vidx = static_cast<std::uint8_t>(inst.operand);
          double len_d = pop().as_double();
          double start_d = pop().as_double();
          Value val = pop();
          if (var_type(vidx) != VarType::VT_STR) {
            trap_error(13, current_line);  // Type mismatch
            break;
          }
          std::string src = val.as_string();
          std::string target =
              (var_init_[vidx]) ? var_values_[vidx].as_string() : std::string();
          int start = static_cast<int>(start_d);
          int len = (len_d < 0.0) ? static_cast<int>(src.size())
                                  : static_cast<int>(len_d);
          if (start < 1) start = 1;
          for (int i = 0; i < len && i < static_cast<int>(src.size()); i++) {
            int pos = start - 1 + i;
            if (pos >= 0 && pos < static_cast<int>(target.size())) {
              target[pos] = src[i];
            }
          }
          var_values_[vidx] = Value(target);
          var_init_[vidx] = true;
          break;
        }
        case OpCode::CallBuiltin: {
          Builtin b = static_cast<Builtin>((inst.operand >> 8) & 0xFF);
          int count = static_cast<int>(inst.operand & 0xFF);
          std::vector<Value> args(static_cast<std::size_t>(count));
          for (int i = count - 1; i >= 0; --i) {
            args[static_cast<std::size_t>(i)] = pop();
          }
          push(call_builtin(b, args, current_line));
          break;
        }
        case OpCode::RandomOpen: {
          int record_len = static_cast<int>(std::round(pop().as_double()));
          std::string filename = pop().as_string();
          int filenum = static_cast<int>(inst.operand);
          if (record_len <= 0) record_len = 128;
          auto it = random_files_.find(filenum);
          if (it != random_files_.end()) {
            if (it->second.handle >= 0) host_.random_close(it->second.handle);
            random_files_.erase(it);
          }
          int handle = host_.random_open(filename);
          if (handle < 0) {
            trap_error(52, current_line);
            break;
          }
          RandomFile rf;
          rf.handle = handle;
          rf.record_len = record_len;
          rf.buffer.assign(static_cast<std::size_t>(record_len), ' ');
          random_files_[filenum] = std::move(rf);
          break;
        }
        case OpCode::Field: {
          std::uint8_t vidx = static_cast<std::uint8_t>(pop().as_double());
          int len = static_cast<int>(std::round(pop().as_double()));
          int filenum = static_cast<int>(inst.operand);
          auto it = random_files_.find(filenum);
          if (it == random_files_.end()) {
            trap_error(52, current_line);
            break;
          }
          if (len < 0) len = 0;
          RandomField f{vidx, it->second.next_offset, len};
          it->second.fields.push_back(f);
          it->second.next_offset += len;
          if (static_cast<int>(it->second.buffer.size()) <
              it->second.next_offset) {
            it->second.buffer.resize(
                static_cast<std::size_t>(it->second.next_offset), ' ');
          }
          break;
        }
        case OpCode::LSet: {
          Value val = pop();
          std::uint8_t vidx = static_cast<std::uint8_t>(inst.operand >> 1);
          bool right = (inst.operand & 1u) != 0;
          std::string s = val.as_string();
          RandomFile* rf = nullptr;
          RandomField* field = nullptr;
          for (auto& [fn, file] : random_files_) {
            for (auto& f : file.fields) {
              if (f.var_index == vidx) {
                rf = &file;
                field = &f;
                break;
              }
            }
            if (field) break;
          }
          if (field) {
            int len = field->length;
            if (static_cast<int>(s.size()) > len) {
              s = s.substr(0, static_cast<std::size_t>(len));
            }
            std::string padded(static_cast<std::size_t>(len), ' ');
            std::size_t start =
                right ? static_cast<std::size_t>(len - s.size()) : 0;
            padded.replace(start, s.size(), s);
            rf->buffer.replace(static_cast<std::size_t>(field->offset),
                               static_cast<std::size_t>(len), padded);
            var_values_[vidx] = Value(padded);
          } else {
            std::string cur =
                (var_init_[vidx]) ? var_values_[vidx].as_string() : s;
            int len = static_cast<int>(cur.size());
            if (static_cast<int>(s.size()) > len) {
              s = s.substr(0, static_cast<std::size_t>(len));
            }
            std::string padded(static_cast<std::size_t>(len), ' ');
            std::size_t start =
                right ? static_cast<std::size_t>(len - s.size()) : 0;
            padded.replace(start, s.size(), s);
            var_values_[vidx] = Value(padded);
          }
          var_init_[vidx] = true;
          break;
        }
        case OpCode::GetRecord: {
          int rec = static_cast<int>(std::round(pop().as_double()));
          int filenum = static_cast<int>(inst.operand);
          auto it = random_files_.find(filenum);
          if (it == random_files_.end()) {
            trap_error(52, current_line);
            break;
          }
          if (rec < 1) rec = 1;
          it->second.buffer = host_.random_read(
              it->second.handle, rec, it->second.record_len);
          for (auto& f : it->second.fields) {
            std::string s = it->second.buffer.substr(
                static_cast<std::size_t>(f.offset),
                static_cast<std::size_t>(f.length));
            var_values_[f.var_index] = Value(s);
            var_init_[f.var_index] = true;
          }
          break;
        }
        case OpCode::PutRecord: {
          int rec = static_cast<int>(std::round(pop().as_double()));
          int filenum = static_cast<int>(inst.operand);
          auto it = random_files_.find(filenum);
          if (it == random_files_.end()) {
            trap_error(52, current_line);
            break;
          }
          if (rec < 1) rec = 1;
          for (auto& f : it->second.fields) {
            std::string s =
                (var_init_[f.var_index])
                    ? var_values_[f.var_index].as_string()
                    : std::string();
            if (static_cast<int>(s.size()) > f.length) {
              s = s.substr(0, static_cast<std::size_t>(f.length));
            }
            std::string padded(static_cast<std::size_t>(f.length), ' ');
            padded.replace(0, s.size(), s);
            it->second.buffer.replace(static_cast<std::size_t>(f.offset),
                                      static_cast<std::size_t>(f.length),
                                      padded);
          }
          host_.random_write(it->second.handle, rec, it->second.record_len,
                             it->second.buffer);
          break;
        }
        case OpCode::Pow: {
          Value b = pop();
          Value a = pop();
          push(Value(std::pow(a.as_double(), b.as_double())));
          break;
        }
        case OpCode::IntDiv: {
          Value b = pop();
          Value a = pop();
          long y = static_cast<long>(std::round(b.as_double()));
          if (y == 0) {
            trap_error(11, current_line);  // Division by zero
            break;
          }
          long x = static_cast<long>(std::round(a.as_double()));
          push(Value(static_cast<double>(x / y)));
          break;
        }
        case OpCode::Mod: {
          Value b = pop();
          Value a = pop();
          long y = static_cast<long>(std::round(b.as_double()));
          if (y == 0) {
            trap_error(11, current_line);
            break;
          }
          long x = static_cast<long>(std::round(a.as_double()));
          push(Value(static_cast<double>(x % y)));
          break;
        }
        case OpCode::RestoreIndex: {
          data_pool_.restore_to(inst.operand);
          break;
        }
        case OpCode::Randomize: {
          double seed = pop().as_double();
          host_.randomize(seed);
          break;
        }
        case OpCode::WriteFile: {
          int count = static_cast<int>(inst.operand & 0xFF);
          std::vector<Value> vals(static_cast<std::size_t>(count));
          for (int i = count - 1; i >= 0; --i) {
            vals[static_cast<std::size_t>(i)] = pop();
          }
          double fn_d = pop().as_double();
          int filenum = static_cast<int>(fn_d);
          if (filenum < 1 || filenum > 255 || file_handles_[filenum] < 0) {
            trap_error(52, current_line);
            break;
          }
          std::string out;
          for (int i = 0; i < count; i++) {
            if (i > 0) out += ",";
            const Value& v = vals[static_cast<std::size_t>(i)];
            if (v.is_string()) {
              out += "\"" + v.as_string() + "\"";
            } else {
              out += format_basic_number(v.as_double());
            }
          }
          out += "\n";
          host_.file_write(file_handles_[filenum], out);
          break;
        }
        case OpCode::FnBind: {
          std::size_t fn_id = inst.operand;
          if (fn_id >= fn_descs_.size()) {
            trap_error(5, current_line);
            break;
          }
          const FnDesc& desc = fn_descs_[fn_id];
          std::size_t n = desc.param_vars.size();
          std::vector<Value> args(n);
          for (std::size_t i = n; i-- > 0;) {
            args[i] = pop();
          }
          std::vector<Value> saved(n);
          for (std::size_t i = 0; i < n; i++) {
            saved[i] = var_values_[desc.param_vars[i]];
          }
          fn_frames_.push_back({fn_id, std::move(saved)});
          fn_return_.push_back(ip_);
          for (std::size_t i = 0; i < n; i++) {
            var_values_[desc.param_vars[i]] = args[i];
            var_init_[desc.param_vars[i]] = true;
          }
          ip_ = desc.body_pc;
          break;
        }
        case OpCode::FnRet: {
          if (fn_frames_.empty() || fn_return_.empty()) {
            trap_error(5, current_line);
            break;
          }
          auto frame = std::move(fn_frames_.back());
          fn_frames_.pop_back();
          const FnDesc& desc = fn_descs_[frame.first];
          for (std::size_t i = 0; i < desc.param_vars.size() &&
                                  i < frame.second.size();
               i++) {
            var_values_[desc.param_vars[i]] = frame.second[i];
          }
          ip_ = fn_return_.back();
          fn_return_.pop_back();
          break;
        }
        case OpCode::StoreTemp: {
          if (inst.operand >= temp_values_.size()) {
            trap_error(5, current_line);
            break;
          }
          temp_values_[inst.operand] = pop();
          break;
        }
        case OpCode::LoadTemp: {
          if (inst.operand >= temp_values_.size()) {
            trap_error(5, current_line);
            break;
          }
          push(temp_values_[inst.operand]);
          break;
        }
        case OpCode::Stop: {
          std::ostringstream oss;
          oss << "Break in " << current_line << "\n";
          screen_.print(oss.str());
          screen_.refresh();
          stopped_ = true;
          return ExecStatus::Paused;
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
      if (i < var_values_.size()) {
        if (i >= var_init_.size() || !var_init_[i]) {
          return default_value(var_type(static_cast<std::uint8_t>(i)));
        }
        return var_values_[i];
      }
      return Value{};
    }
  }
  return std::nullopt;
}

Value Vm::eval_builtin(Builtin b, const std::vector<Value>& args,
                       std::int32_t line) {
  return call_builtin(b, args, line);
}

std::optional<Value> Vm::get_array_element(
    const std::string& name,
    const std::vector<std::int32_t>& indices) const {
  if (!arrays_.contains(name)) return std::nullopt;
  try {
    return arrays_.at(name, indices);
  } catch (const std::out_of_range&) {
    return std::nullopt;
  }
}

bool Vm::set_variable(const std::string& name, const Value& v) {
  for (std::size_t i = 0; i < variable_names_.size(); i++) {
    if (variable_names_[i] == name) {
      if (i < var_values_.size()) {
        var_values_[i] = coerce_value(
            v, var_type(static_cast<std::uint8_t>(i)), current_line());
        var_init_[i] = true;
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
  s.var_init = var_init_;
  s.stack = stack_;
  s.call_stack = call_stack_;
  s.data_ptr = 0;  // DataPool pointer is internal; snapshot captures values.
  return s;
}

void Vm::restore(const Snapshot& s) {
  ip_ = s.ip;
  var_values_ = s.var_values;
  var_init_ = s.var_init;
  stack_ = s.stack;
  call_stack_ = s.call_stack;
}
