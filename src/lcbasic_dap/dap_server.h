#pragma once

#include "compiler.h"
#include "debugger.h"
#include "headless_screen.h"
#include "host.h"
#include "json.h"
#include "parser.h"
#include "vm.h"

#include <istream>
#include <memory>
#include <ostream>
#include <string>

namespace dap {

// A minimal Debug Adapter Protocol server driving the lcbasic debugger.
// Speaks the Content-Length framed JSON protocol over a pair of streams
// (stdin/stdout for the real binary, stringstreams for tests).
class DapServer {
public:
  DapServer(std::istream& in, std::ostream& out);
  ~DapServer();

  // Process messages until EOF or a disconnect request.
  void run();

  // Process a single message (for tests). Returns the messages produced.
  std::vector<Json> process_message(const Json& request);

  const std::vector<Json>& sent() const { return sent_; }
  void clear_sent() { sent_.clear(); }

private:
  std::istream& in_;
  std::ostream& out_;
  int seq_ = 1;
  std::vector<Json> sent_;

  // Program + execution state
  std::unique_ptr<Program> program_;
  std::unique_ptr<HeadlessScreen> screen_;
  std::unique_ptr<RealHost> host_;
  std::unique_ptr<Vm> vm_;
  std::unique_ptr<debugger::Debugger> dbg_;
  bool started_ = false;
  bool terminated_ = false;
  std::size_t output_sent_ = 0;

  // ----- Protocol I/O -----
  bool read_message(Json& out);
  void write_message(const Json& msg);
  void enqueue(Json msg);

  void send_response(const Json& request, const Json& body);
  void send_error(const Json& request, const std::string& message);
  void send_event(const std::string& event, const Json& body);

  // ----- Command dispatch -----
  void dispatch(const Json& request);

  void handle_initialize(const Json& request);
  void handle_launch(const Json& request);
  void handle_attach(const Json& request);
  void handle_set_breakpoints(const Json& request);
  void handle_configuration_done(const Json& request);
  void handle_set_exception_breakpoints(const Json& request);
  void handle_threads(const Json& request);
  void handle_stack_trace(const Json& request);
  void handle_scopes(const Json& request);
  void handle_variables(const Json& request);
  void handle_evaluate(const Json& request);
  void handle_continue(const Json& request);
  void handle_next(const Json& request);
  void handle_step_in(const Json& request);
  void handle_step_out(const Json& request);
  void handle_pause(const Json& request);
  void handle_disconnect(const Json& request);

  // ----- Helpers -----
  bool load_program(const std::string& path);
  void flush_output();
  void on_stopped(debugger::Debugger::StopReason reason);
  std::string stop_reason_str(debugger::Debugger::StopReason reason);
};

}  // namespace dap
