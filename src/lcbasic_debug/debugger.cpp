#include "debugger.h"

#include "lexer.h"
#include "parser.h"

#include <algorithm>
#include <sstream>

namespace debugger {

namespace {

const char* opcode_name(OpCode op) {
  switch (op) {
    case OpCode::Constant: return "CONST";
    case OpCode::LoadVar: return "LOAD";
    case OpCode::StoreVar: return "STORE";
    case OpCode::Add: return "ADD";
    case OpCode::Sub: return "SUB";
    case OpCode::Mul: return "MUL";
    case OpCode::Div: return "DIV";
    case OpCode::Eq: return "EQ";
    case OpCode::Ne: return "NE";
    case OpCode::Lt: return "LT";
    case OpCode::Le: return "LE";
    case OpCode::Gt: return "GT";
    case OpCode::Ge: return "GE";
    case OpCode::Print: return "PRINT";
    case OpCode::Pop: return "POP";
    case OpCode::Jmp: return "JMP";
    case OpCode::JmpFalse: return "JMPF";
    case OpCode::Call: return "CALL";
    case OpCode::Ret: return "RET";
    case OpCode::ForInit: return "FOR";
    case OpCode::ForNext: return "NEXT";
    case OpCode::Dim: return "DIM";
    case OpCode::ArrayLoad: return "ALOAD";
    case OpCode::ArrayStore: return "ASTORE";
    case OpCode::ReadVar: return "READ";
    case OpCode::ReadArray: return "AREAD";
    case OpCode::Restore: return "RESTORE";
    case OpCode::SetOptionBase: return "OPTION";
    case OpCode::NextZone: return "ZONE";
    case OpCode::DefType: return "DEFTYPE";
    case OpCode::TimerFunc: return "TIMER";
    case OpCode::ChDir: return "CHDIR";
    case OpCode::MkDir: return "MKDIR";
    case OpCode::RmDir: return "RMDIR";
    case OpCode::KillFile: return "KILL";
    case OpCode::NameFile: return "NAME";
    case OpCode::Files: return "FILES";
    case OpCode::Shell: return "SHELL";
    case OpCode::OnEvent: return "ONEVENT";
    case OpCode::EventControl: return "EVCTL";
    case OpCode::Input: return "INPUT";
    case OpCode::Cls: return "CLS";
    case OpCode::Locate: return "LOCATE";
    case OpCode::Color: return "COLOR";
    case OpCode::Erase: return "ERASE";
    case OpCode::Clear: return "CLEAR";
    case OpCode::RaiseError: return "ERROR";
    case OpCode::MidAssign: return "MIDASSIGN";
    case OpCode::CallBuiltin: return "BUILTIN";
    case OpCode::Open: return "OPEN";
    case OpCode::Close: return "CLOSE";
    case OpCode::PrintFile: return "PRINT#";
    case OpCode::InputFile: return "INPUT#";
    case OpCode::Eof: return "EOF";
    case OpCode::OnError: return "ONERR";
    case OpCode::Resume: return "RESUME";
    case OpCode::Halt: return "HALT";
  }
  return "???";
}

const char* var_type_name(VarType t) {
  switch (t) {
    case VarType::VT_INT16: return "int16";
    case VarType::VT_INT32: return "int32";
    case VarType::VT_SGL: return "single";
    case VarType::VT_DBL: return "double";
    case VarType::VT_STR: return "string";
  }
  return "?";
}

// Format a value for debugger display: integers without a trailing ".000000",
// strings quoted.
std::string format_value(const Value& v) {
  if (v.is_string()) return "\"" + v.as_string() + "\"";
  std::ostringstream oss;
  oss << v.as_double();
  return oss.str();
}

// Evaluate an expression AST against the VM's variable state.
struct EvalContext {
  const Vm* vm;
  bool ok = true;
  std::string error;
};

Value eval_expr(EvalContext& ctx, const Expr& expr);

Value eval_binary(EvalContext& ctx, const BinaryExpr& b) {
  Value l = eval_expr(ctx, *b.left);
  Value r = eval_expr(ctx, *b.right);
  double a = l.as_double();
  double c = r.as_double();
  switch (b.op) {
    case TokenType::Plus:
      if (l.is_string() || r.is_string()) {
        return Value(l.as_string() + r.as_string());
      }
      return Value(a + c);
    case TokenType::Minus: return Value(a - c);
    case TokenType::Star: return Value(a * c);
    case TokenType::Slash:
      if (c == 0.0) {
        ctx.ok = false;
        ctx.error = "division by zero";
        return Value{};
      }
      return Value(a / c);
    case TokenType::Eq: return Value(a == c ? 1.0 : 0.0);
    case TokenType::Ne: return Value(a != c ? 1.0 : 0.0);
    case TokenType::Lt: return Value(a < c ? 1.0 : 0.0);
    case TokenType::Le: return Value(a <= c ? 1.0 : 0.0);
    case TokenType::Gt: return Value(a > c ? 1.0 : 0.0);
    case TokenType::Ge: return Value(a >= c ? 1.0 : 0.0);
    default:
      ctx.ok = false;
      ctx.error = "unsupported operator";
      return Value{};
  }
}

Value eval_expr(EvalContext& ctx, const Expr& expr) {
  return std::visit(
      [&](auto&& arg) -> Value {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, NumberExpr>) {
          return Value(arg.value);
        } else if constexpr (std::is_same_v<T, StringExpr>) {
          return Value(arg.value);
        } else if constexpr (std::is_same_v<T, VariableExpr>) {
          auto v = ctx.vm->get_variable(arg.name);
          if (!v) {
            // Uninitialised variables read as zero (GW-BASIC behavior).
            return Value(0.0);
          }
          return *v;
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
          return eval_binary(ctx, arg);
        } else if constexpr (std::is_same_v<T, ArrayRefExpr>) {
          ctx.ok = false;
          ctx.error = "array references not supported in evaluation";
          return Value{};
        } else if constexpr (std::is_same_v<T, FnCallExpr>) {
          ctx.ok = false;
          ctx.error = "FN functions not supported in evaluation";
          return Value{};
        } else if constexpr (std::is_same_v<T, BuiltinExpr>) {
          ctx.ok = false;
          ctx.error = "builtin not supported in evaluation";
          return Value{};
        } else if constexpr (std::is_same_v<T, FunctionExpr>) {
          ctx.ok = false;
          ctx.error = "functions not supported in evaluation";
          return Value{};
        }
      },
      expr.data);
}

}  // namespace

Debugger::Debugger(Vm& vm) : vm_(vm) {}

// ----- Breakpoints -----

void Debugger::set_breakpoint(std::int32_t line) { breakpoints_.insert(line); }

void Debugger::clear_breakpoint(std::int32_t line) {
  breakpoints_.erase(line);
}

void Debugger::clear_all_breakpoints() { breakpoints_.clear(); }

bool Debugger::has_breakpoint(std::int32_t line) const {
  return breakpoints_.count(line) > 0;
}

// ----- Execution control -----

Debugger::StopReason Debugger::resume(Mode m) {
  mode_ = m;
  first_ = true;
  // If resuming from a stop, suppress a breakpoint on the line we stopped at
  // until execution moves to a different line. On the initial run there is no
  // such line, so breakpoints apply immediately.
  left_resume_line_ = !paused_;
  step_depth_ = vm_.call_stack().size();
  step_line_ = vm_.current_line();

  ExecStatus st = vm_.run_with_hook(this);

  paused_ = (st == ExecStatus::Paused);

  switch (st) {
    case ExecStatus::Paused:
      return reason_;
    case ExecStatus::Halted:
      reason_ = StopReason::Halted;
      return reason_;
    case ExecStatus::Error:
      reason_ = StopReason::Error;
      return reason_;
    case ExecStatus::Running:
      break;
  }
  return reason_;
}

Debugger::StopReason Debugger::run() { return continue_(); }

Debugger::StopReason Debugger::step_into() { return resume(Mode::StepInto); }

Debugger::StopReason Debugger::step_over() { return resume(Mode::StepOver); }

Debugger::StopReason Debugger::step_out() { return resume(Mode::StepOut); }

Debugger::StopReason Debugger::continue_() { return resume(Mode::Continue); }

// ----- DbgHook -----

bool Debugger::on_instruction(std::size_t /*pc*/, std::int32_t line,
                              std::size_t call_depth) {
  // Always execute the instruction we were paused on when resuming.
  if (first_) {
    first_ = false;
    return false;
  }

  switch (mode_) {
    case Mode::StepInto:
      reason_ = StopReason::Step;
      return true;

    case Mode::StepOver:
      // Pause when we return to the same depth at a new line, or when we
      // return from a GOSUB (shallower depth).
      if (call_depth < step_depth_) {
        reason_ = StopReason::Step;
        return true;
      }
      if (call_depth == step_depth_ && line != step_line_) {
        reason_ = StopReason::Step;
        return true;
      }
      return false;

    case Mode::StepOut:
      if (call_depth < step_depth_) {
        reason_ = StopReason::Step;
        return true;
      }
      return false;

    case Mode::Continue:
      // Don't re-trigger a breakpoint on the line we just resumed from
      // until execution has moved to a different line.
      if (line != step_line_) left_resume_line_ = true;
      if (!left_resume_line_) return false;
      if (breakpoints_.count(line) > 0) {
        reason_ = StopReason::Breakpoint;
        return true;
      }
      return false;
  }
  return false;
}

// ----- State inspection -----

std::vector<VariableView> Debugger::variables() const {
  std::vector<VariableView> out;
  const auto& names = vm_.variable_names();
  for (std::size_t i = 0; i < names.size(); i++) {
    auto opt = vm_.get_variable(names[i]);
    Value val = opt ? *opt : Value{};
    VariableView v;
    v.name = names[i];
    v.type = var_type_name(val.is_string() ? VarType::VT_STR : VarType::VT_DBL);
    v.value = format_value(val);
    out.push_back(std::move(v));
  }
  return out;
}

std::vector<StackFrame> Debugger::stack_frames() const {
  std::vector<StackFrame> frames;
  const auto& calls = vm_.call_stack();

  // The main frame: the current line.
  StackFrame main_frame;
  main_frame.name = "main";
  main_frame.line = vm_.current_line();
  main_frame.depth = 0;
  frames.push_back(main_frame);

  // GOSUB frames: each call_stack entry is a return PC.
  const auto& code = vm_.code();
  for (std::size_t i = 0; i < calls.size(); i++) {
    StackFrame f;
    std::size_t ret_pc = calls[i];
    std::int32_t ret_line = 0;
    if (ret_pc < code.size()) ret_line = code[ret_pc].line;
    f.name = "GOSUB -> line " + std::to_string(ret_line);
    f.line = ret_line;
    f.depth = i + 1;
    frames.push_back(f);
  }

  // FOR frames (for information; they're not call frames but useful).
  auto for_frames = vm_.for_frames();
  for (auto& ff : for_frames) {
    StackFrame f;
    const auto& names = vm_.variable_names();
    std::string var = (ff.var_index < names.size()) ? names[ff.var_index] : "?";
    f.name = "FOR " + var;
    f.line = 0;
    f.depth = 0;
    frames.push_back(f);
  }

  return frames;
}

// ----- Expression evaluation -----

Debugger::EvalResult Debugger::evaluate(const std::string& expression) const {
  EvalResult result;

  Lexer lexer(expression, 0);
  auto tokens = lexer.scan();

  Parser parser(std::move(tokens));
  // Wrap in a fake line so the parser accepts a bare expression.
  // Actually, we need to parse an expression. Let's use a trick: parse
  // "0 X = <expr>" isn't right either. Instead, let's make the parser
  // parse a bare expression by using parse via a synthetic PRINT statement.
  std::string wrapped = "0 PRINT " + expression;
  Lexer wlexer(wrapped, 0);
  auto wtokens = wlexer.scan();
  Parser wparser(std::move(wtokens));
  if (!wparser.parse()) {
    result.ok = false;
    result.error = "parse error";
    return result;
  }
  Program prog = wparser.release_program();
  if (prog.lines.empty()) {
    result.ok = false;
    result.error = "empty expression";
    return result;
  }
  const Statement& stmt = prog.lines[0].second;
  auto* print = std::get_if<PrintStmt>(&stmt.data);
  if (!print || print->args.empty()) {
    result.ok = false;
    result.error = "not an expression";
    return result;
  }

  EvalContext ctx;
  ctx.vm = &vm_;
  Value v = eval_expr(ctx, *print->args[0]);
  if (!ctx.ok) {
    result.ok = false;
    result.error = ctx.error;
    return result;
  }

  result.ok = true;
  if (v.is_string()) {
    result.value = "\"" + v.as_string() + "\"";
  } else {
    std::ostringstream oss;
    oss << v.as_double();
    result.value = oss.str();
  }
  return result;
}

// ----- Disassembly -----

std::string Debugger::disassemble() const {
  std::ostringstream oss;
  const auto& code = vm_.code();
  for (std::size_t i = 0; i < code.size(); i++) {
    const auto& inst = code[i];
    oss << (i == vm_.current_pc() ? "-> " : "   ");
    oss << i << ": " << opcode_name(inst.op) << " " << inst.operand
        << "  (line " << inst.line << ")\n";
  }
  return oss.str();
}

// ----- Snapshots -----

void Debugger::save_snapshot() { snapshots_.push_back(vm_.snapshot()); }

bool Debugger::restore_snapshot() {
  if (snapshots_.empty()) return false;
  vm_.restore(snapshots_.back());
  return true;
}

}  // namespace debugger
