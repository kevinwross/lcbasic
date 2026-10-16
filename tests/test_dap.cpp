#include "test_runner.h"
#include "dap_server.h"
#include "json.h"

#include <fstream>
#include <sstream>
#include <string>

// ---------- JSON tests ----------

TEST(json_parse_number) {
  dap::Json j;
  REQUIRE(dap::Json::parse("42", j));
  CHECK(j.is_number());
  CHECK(j.as_int() == 42);
}

TEST(json_parse_string) {
  dap::Json j;
  REQUIRE(dap::Json::parse("\"hello\\nworld\"", j));
  CHECK(j.is_string());
  CHECK(j.as_string() == "hello\nworld");
}

TEST(json_parse_object) {
  dap::Json j;
  REQUIRE(dap::Json::parse("{\"seq\":1,\"command\":\"initialize\"}", j));
  CHECK(j.is_object());
  CHECK(j.get_int("seq") == 1);
  CHECK(j.get_string("command") == "initialize");
}

TEST(json_parse_array) {
  dap::Json j;
  REQUIRE(dap::Json::parse("[1, 2, 3]", j));
  CHECK(j.is_array());
  CHECK(j.as_array().size() == 3);
  CHECK(j.as_array()[0].as_int() == 1);
}

TEST(json_dump_roundtrip) {
  dap::Json obj;
  obj["name"] = "test";
  obj["value"] = 42;
  obj["nested"] = dap::Json::Object{{"a", dap::Json(1)}};
  std::string text = obj.dump();

  dap::Json parsed;
  REQUIRE(dap::Json::parse(text, parsed));
  CHECK(parsed.get_string("name") == "test");
  CHECK(parsed.get_int("value") == 42);
  CHECK(parsed["nested"].get_int("a") == 1);
}

// ---------- DAP protocol tests ----------

static void write_program(const std::string& path) {
  std::ofstream f(path);
  f << "10 X = 1\n";
  f << "20 Y = 2\n";
  f << "30 PRINT X + Y\n";
  f << "40 END\n";
}

static dap::Json make_request(int seq, const std::string& command) {
  dap::Json req;
  req["seq"] = seq;
  req["type"] = "request";
  req["command"] = command;
  return req;
}

TEST(dap_initialize) {
  std::istringstream in("");
  std::ostringstream out;
  dap::DapServer server(in, out);
  auto msgs = server.process_message(make_request(1, "initialize"));
  REQUIRE(!msgs.empty());
  CHECK(msgs[0].get_string("type") == "response");
  CHECK(msgs[0].get_bool("success"));
  CHECK(msgs[0].get_string("command") == "initialize");
}

TEST(dap_launch_and_breakpoint_flow) {
  write_program("/tmp/dap_test.bas");

  std::istringstream in("");
  std::ostringstream out;
  dap::DapServer server(in, out);

  server.process_message(make_request(1, "initialize"));

  dap::Json launch = make_request(2, "launch");
  launch["arguments"] = dap::Json::Object{{"program", dap::Json("/tmp/dap_test.bas")}};
  auto launch_msgs = server.process_message(launch);
  REQUIRE(!launch_msgs.empty());
  CHECK(launch_msgs[0].get_bool("success"));

  // Set a breakpoint on line 30.
  dap::Json bp = make_request(3, "setBreakpoints");
  dap::Json bp_args;
  bp_args["source"] = dap::Json::Object{{"path", dap::Json("/tmp/dap_test.bas")}};
  bp_args["breakpoints"] = dap::Json::Array{
      dap::Json::Object{{"line", dap::Json(30)}}};
  bp["arguments"] = bp_args;
  auto bp_msgs = server.process_message(bp);
  REQUIRE(!bp_msgs.empty());
  CHECK(bp_msgs[0].get_bool("success"));

  // configurationDone should start execution and hit the breakpoint.
  auto cfg_msgs = server.process_message(make_request(4, "configurationDone"));
  // Should contain a stopped event.
  bool stopped = false;
  for (auto& m : cfg_msgs) {
    if (m.get_string("type") == "event" &&
        m.get_string("event") == "stopped") {
      stopped = true;
      CHECK(m["body"].get_string("reason") == "breakpoint");
    }
  }
  CHECK(stopped);

  // stackTrace should return the current frame.
  auto st_msgs = server.process_message(make_request(5, "stackTrace"));
  REQUIRE(!st_msgs.empty());
  CHECK(st_msgs[0].get_bool("success"));
  CHECK(st_msgs[0]["body"]["stackFrames"].as_array().size() >= 1);

  // variables should include X and Y.
  auto var_msgs = server.process_message(make_request(6, "variables"));
  REQUIRE(!var_msgs.empty());
  CHECK(var_msgs[0].get_bool("success"));

  // evaluate X + Y should be 3.
  dap::Json eval = make_request(7, "evaluate");
  eval["arguments"] = dap::Json::Object{{"expression", dap::Json("X + Y")}};
  auto eval_msgs = server.process_message(eval);
  REQUIRE(!eval_msgs.empty());
  CHECK(eval_msgs[0].get_bool("success"));
  CHECK(eval_msgs[0]["body"].get_string("result") == "3");

  std::remove("/tmp/dap_test.bas");
}

TEST(dap_continue_to_termination) {
  write_program("/tmp/dap_test2.bas");

  std::istringstream in("");
  std::ostringstream out;
  dap::DapServer server(in, out);

  server.process_message(make_request(1, "initialize"));

  dap::Json launch = make_request(2, "launch");
  launch["arguments"] = dap::Json::Object{{"program", dap::Json("/tmp/dap_test2.bas")}};
  server.process_message(launch);

  dap::Json bp = make_request(3, "setBreakpoints");
  dap::Json bp_args;
  bp_args["breakpoints"] = dap::Json::Array{
      dap::Json::Object{{"line", dap::Json(20)}}};
  bp["arguments"] = bp_args;
  server.process_message(bp);

  server.process_message(make_request(4, "configurationDone"));

  // Continue should run to completion and emit terminated.
  auto cont_msgs = server.process_message(make_request(5, "continue"));
  bool terminated = false;
  for (auto& m : cont_msgs) {
    if (m.get_string("type") == "event" &&
        m.get_string("event") == "terminated") {
      terminated = true;
    }
  }
  CHECK(terminated);

  std::remove("/tmp/dap_test2.bas");
}

TEST(dap_framing_roundtrip) {
  // Simulate a real DAP stream with Content-Length framing.
  std::string body =
      "{\"seq\":1,\"type\":\"request\",\"command\":\"initialize\"}";
  std::string framed =
      "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;

  std::istringstream in(framed);
  std::ostringstream out;
  dap::DapServer server(in, out);
  server.run();

  // The output should be a framed response.
  std::string result = out.str();
  CHECK(result.find("Content-Length:") == 0);
  CHECK(result.find("\"command\":\"initialize\"") != std::string::npos);
  CHECK(result.find("\"success\":true") != std::string::npos);
}

TEST(dap_step_over_advances_line) {
  write_program("/tmp/dap_test3.bas");

  std::istringstream in("");
  std::ostringstream out;
  dap::DapServer server(in, out);

  server.process_message(make_request(1, "initialize"));
  dap::Json launch = make_request(2, "launch");
  launch["arguments"] =
      dap::Json::Object{{"program", dap::Json("/tmp/dap_test3.bas")}};
  server.process_message(launch);

  dap::Json bp = make_request(3, "setBreakpoints");
  dap::Json bp_args;
  bp_args["breakpoints"] =
      dap::Json::Array{dap::Json::Object{{"line", dap::Json(10)}}};
  bp["arguments"] = bp_args;
  server.process_message(bp);

  server.process_message(make_request(4, "configurationDone"));

  // We're stopped at line 10; get the line from stack trace.
  auto st1 = server.process_message(make_request(5, "stackTrace"));
  int line1 = st1[0]["body"]["stackFrames"].as_array()[0].get_int("line");
  CHECK(line1 == 10);

  // Step over should advance to line 20.
  server.process_message(make_request(6, "next"));
  auto st2 = server.process_message(make_request(7, "stackTrace"));
  int line2 = st2[0]["body"]["stackFrames"].as_array()[0].get_int("line");
  CHECK(line2 == 20);

  std::remove("/tmp/dap_test3.bas");
}
