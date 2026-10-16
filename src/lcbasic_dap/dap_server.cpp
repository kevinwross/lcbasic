#include "dap_server.h"

#include "basic_format.h"
#include "lexer.h"

#include <algorithm>
#include <sstream>

namespace dap {

DapServer::DapServer(std::istream& in, std::ostream& out)
  : in_(in), out_(out) {}

DapServer::~DapServer() = default;

// ----- Protocol framing -----

bool DapServer::read_message(Json& out) {
  std::string line;
  std::size_t content_length = 0;
  bool have_length = false;

  // Read headers until an empty line.
  while (std::getline(in_, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) break;
    const std::string prefix = "Content-Length:";
    if (line.rfind(prefix, 0) == 0) {
      std::string num = line.substr(prefix.size());
      try {
        content_length = static_cast<std::size_t>(std::stoul(num));
        have_length = true;
      } catch (...) {
        return false;
      }
    }
  }

  if (!have_length) return false;

  std::string body(content_length, '\0');
  in_.read(&body[0], static_cast<std::streamsize>(content_length));
  if (static_cast<std::size_t>(in_.gcount()) != content_length) return false;

  return Json::parse(body, out);
}

void DapServer::write_message(const Json& msg) {
  Json m = msg;
  std::string body = m.dump();
  out_ << "Content-Length: " << body.size() << "\r\n\r\n" << body;
  out_.flush();
}

void DapServer::enqueue(Json msg) {
  if (out_.good()) {
    write_message(msg);
  }
  sent_.push_back(std::move(msg));
}

void DapServer::send_response(const Json& request, const Json& body) {
  Json resp;
  resp["seq"] = seq_++;
  resp["type"] = "response";
  resp["request_seq"] = request.contains("seq") ? request["seq"] : Json(0);
  resp["success"] = true;
  resp["command"] = request.get_string("command");
  if (!body.is_null()) resp["body"] = body;
  enqueue(std::move(resp));
}

void DapServer::send_error(const Json& request, const std::string& message) {
  Json resp;
  resp["seq"] = seq_++;
  resp["type"] = "response";
  resp["request_seq"] = request.contains("seq") ? request["seq"] : Json(0);
  resp["success"] = false;
  resp["command"] = request.get_string("command");
  resp["message"] = message;
  enqueue(std::move(resp));
}

void DapServer::send_event(const std::string& event, const Json& body) {
  Json ev;
  ev["seq"] = seq_++;
  ev["type"] = "event";
  ev["event"] = event;
  if (!body.is_null()) ev["body"] = body;
  enqueue(std::move(ev));
}

// ----- Run loop -----

void DapServer::run() {
  Json request;
  while (read_message(request)) {
    dispatch(request);
    if (terminated_) break;
  }
}

// ----- Dispatch -----

void DapServer::dispatch(const Json& request) {
  std::string command = request.get_string("command");
  if (command == "initialize") handle_initialize(request);
  else if (command == "launch") handle_launch(request);
  else if (command == "attach") handle_attach(request);
  else if (command == "setBreakpoints") handle_set_breakpoints(request);
  else if (command == "setExceptionBreakpoints")
    handle_set_exception_breakpoints(request);
  else if (command == "configurationDone") handle_configuration_done(request);
  else if (command == "threads") handle_threads(request);
  else if (command == "stackTrace") handle_stack_trace(request);
  else if (command == "scopes") handle_scopes(request);
  else if (command == "variables") handle_variables(request);
  else if (command == "evaluate") handle_evaluate(request);
  else if (command == "continue") handle_continue(request);
  else if (command == "next") handle_next(request);
  else if (command == "stepIn") handle_step_in(request);
  else if (command == "stepOut") handle_step_out(request);
  else if (command == "pause") handle_pause(request);
  else if (command == "disconnect") handle_disconnect(request);
  else send_error(request, "unsupported command: " + command);
}

std::vector<Json> DapServer::process_message(const Json& request) {
  clear_sent();
  dispatch(request);
  return sent_;
}

// ----- Handlers -----

void DapServer::handle_initialize(const Json& request) {
  Json caps;
  caps["supportsConfigurationDoneRequest"] = true;
  caps["supportsEvaluateForHovers"] = true;
  caps["supportsSetVariable"] = false;
  caps["supportsStepBack"] = false;
  caps["supportsConditionalBreakpoints"] = false;
  send_response(request, caps);
  send_event("initialized", Json());
}

void DapServer::handle_attach(const Json& request) {
  send_response(request, Json());
}

void DapServer::handle_launch(const Json& request) {
  std::string path;
  if (request.contains("arguments")) {
    const Json& args = request["arguments"];
    path = args.get_string("program");
    if (path.empty()) path = args.get_string("programPath");
  }

  if (path.empty()) {
    send_error(request, "no program specified");
    return;
  }

  if (!load_program(path)) {
    send_error(request, "could not load program: " + path);
    return;
  }
  send_response(request, Json());
}

bool DapServer::load_program(const std::string& path) {
  auto br = fstok::load_bas_file(path);
  if (br.program.empty()) return false;

  auto program = std::make_unique<Program>();
  std::vector<std::pair<std::int32_t, std::string>> lines(br.program.begin(),
                                                          br.program.end());
  std::sort(lines.begin(), lines.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  for (auto& [ln, src] : lines) {
    std::string numbered = std::to_string(ln) + " " + src;
    Lexer lexer(numbered, ln);
    auto tokens = lexer.scan();
    Parser parser(std::move(tokens));
    if (!parser.parse()) return false;
    auto parsed = parser.release_program();
    for (auto& st : parsed.lines) {
      program->lines.push_back(std::move(st));
    }
  }

  Compiler compiler(*program);
  if (!compiler.compile()) return false;

  program_ = std::move(program);

  screen_ = std::make_unique<HeadlessScreen>(25, 80);
  host_ = std::make_unique<RealHost>();
  vm_ = std::make_unique<Vm>(*screen_, *host_);
  // Vm::load copies its arguments, so the compiler's data can go out of scope.
  vm_->load(compiler.code(), compiler.constants(), compiler.line_table(),
            compiler.variable_table(), compiler.variable_names(),
            compiler.data_values());
  dbg_ = std::make_unique<debugger::Debugger>(*vm_);

  return true;
}

void DapServer::handle_set_breakpoints(const Json& request) {
  if (!dbg_) {
    send_error(request, "program not loaded");
    return;
  }
  dbg_->clear_all_breakpoints();

  Json breakpoints = Json::Array{};
  if (request.contains("arguments")) {
    const Json& args = request["arguments"];
    if (args.contains("breakpoints")) {
      for (const auto& bp : args["breakpoints"].as_array()) {
        int line = bp.get_int("line");
        dbg_->set_breakpoint(line);
        Json b;
        b["verified"] = true;
        b["line"] = line;
        breakpoints.as_array().push_back(b);
      }
    }
  }
  Json body;
  body["breakpoints"] = breakpoints;
  send_response(request, body);
}

void DapServer::handle_set_exception_breakpoints(const Json& request) {
  send_response(request, Json());
}

void DapServer::handle_configuration_done(const Json& request) {
  send_response(request, Json());
  if (!dbg_) return;

  started_ = true;
  // Begin execution.
  auto reason = dbg_->continue_();
  on_stopped(reason);
}

void DapServer::on_stopped(debugger::Debugger::StopReason reason) {
  flush_output();
  if (reason == debugger::Debugger::StopReason::Halted ||
      reason == debugger::Debugger::StopReason::Error) {
    send_event("terminated", Json());
    terminated_ = true;
    return;
  }
  Json body;
  body["reason"] = stop_reason_str(reason);
  body["threadId"] = 1;
  body["allThreadsStopped"] = true;
  send_event("stopped", body);
}

std::string DapServer::stop_reason_str(
    debugger::Debugger::StopReason reason) {
  switch (reason) {
    case debugger::Debugger::StopReason::Step: return "step";
    case debugger::Debugger::StopReason::Breakpoint: return "breakpoint";
    case debugger::Debugger::StopReason::Halted: return "exited";
    case debugger::Debugger::StopReason::Error: return "exception";
    case debugger::Debugger::StopReason::NotStarted: return "entry";
  }
  return "unknown";
}

void DapServer::flush_output() {
  if (!screen_) return;
  std::string out = screen_->last_output();
  if (out.size() > output_sent_) {
    std::string delta = out.substr(output_sent_);
    output_sent_ = out.size();
    Json body;
    body["category"] = "stdout";
    body["output"] = delta;
    send_event("output", body);
  }
}

void DapServer::handle_threads(const Json& request) {
  Json thread;
  thread["id"] = 1;
  thread["name"] = "main";
  Json threads = Json::Array{};
  threads.as_array().push_back(thread);
  Json body;
  body["threads"] = threads;
  send_response(request, body);
}

void DapServer::handle_stack_trace(const Json& request) {
  Json frames = Json::Array{};
  if (dbg_) {
    auto stack = dbg_->stack_frames();
    int id = 1;
    for (auto& f : stack) {
      Json frame;
      frame["id"] = id++;
      frame["name"] = f.name;
      frame["line"] = f.line;
      frame["column"] = 1;
      frames.as_array().push_back(frame);
    }
  }
  Json body;
  body["stackFrames"] = frames;
  body["totalFrames"] = static_cast<int>(frames.as_array().size());
  send_response(request, body);
}

void DapServer::handle_scopes(const Json& request) {
  Json scope;
  scope["name"] = "Locals";
  scope["variablesReference"] = 1;
  scope["expensive"] = false;
  Json scopes = Json::Array{};
  scopes.as_array().push_back(scope);
  Json body;
  body["scopes"] = scopes;
  send_response(request, body);
}

void DapServer::handle_variables(const Json& request) {
  Json vars = Json::Array{};
  if (dbg_) {
    auto variables = dbg_->variables();
    for (auto& v : variables) {
      Json var;
      var["name"] = v.name;
      var["value"] = v.value;
      var["type"] = v.type;
      var["variablesReference"] = 0;
      vars.as_array().push_back(var);
    }
  }
  Json body;
  body["variables"] = vars;
  send_response(request, body);
}

void DapServer::handle_evaluate(const Json& request) {
  std::string expr;
  if (request.contains("arguments")) {
    expr = request["arguments"].get_string("expression");
  }
  Json body;
  if (dbg_) {
    auto r = dbg_->evaluate(expr);
    if (r.ok) {
      body["result"] = r.value;
      body["variablesReference"] = 0;
      send_response(request, body);
      return;
    }
    send_error(request, r.error);
    return;
  }
  send_error(request, "no program loaded");
}

void DapServer::handle_continue(const Json& request) {
  send_response(request, Json());
  if (dbg_) on_stopped(dbg_->continue_());
}

void DapServer::handle_next(const Json& request) {
  send_response(request, Json());
  if (dbg_) on_stopped(dbg_->step_over());
}

void DapServer::handle_step_in(const Json& request) {
  send_response(request, Json());
  if (dbg_) on_stopped(dbg_->step_into());
}

void DapServer::handle_step_out(const Json& request) {
  send_response(request, Json());
  if (dbg_) on_stopped(dbg_->step_out());
}

void DapServer::handle_pause(const Json& request) {
  send_response(request, Json());
}

void DapServer::handle_disconnect(const Json& request) {
  send_response(request, Json());
  terminated_ = true;
}

}  // namespace dap
