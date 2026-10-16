#include "test_runner.h"
#include "compiler.h"
#include "debugger.h"
#include "headless_screen.h"
#include "host.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"

#include <sstream>

namespace {

std::unique_ptr<Program> dbg_compile(const std::string& source) {
  auto program = std::make_unique<Program>();
  std::istringstream iss(source);
  std::string line;
  while (std::getline(iss, line)) {
    if (line.empty()) continue;
    std::istringstream liss(line);
    std::int32_t ln = 0;
    liss >> ln;
    if (liss.fail()) continue;
    std::string rest;
    std::getline(liss, rest);
    std::size_t s = rest.find_first_not_of(" \t");
    if (s != std::string::npos) rest = rest.substr(s);
    std::string numbered = std::to_string(ln) + " " + rest;
    Lexer lexer(numbered, ln);
    auto tokens = lexer.scan();
    Parser parser(std::move(tokens));
    if (!parser.parse()) return nullptr;
    auto parsed = parser.release_program();
    for (auto& st : parsed.lines) {
      program->lines.push_back(std::move(st));
    }
  }
  return program;
}

}  // namespace

TEST(dbg_step_into_advances) {
  auto program = dbg_compile(
      "10 X = 1\n20 Y = 2\n30 Z = 3\n40 END");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  RealHost host;
  Vm vm(screen, host);
  vm.load(compiler.code(), compiler.constants(), compiler.line_table(),
          compiler.variable_table(), compiler.variable_names(),
          compiler.data_values());

  debugger::Debugger dbg(vm);
  // Step through a few instructions.
  dbg.step_into();
  dbg.step_into();
  dbg.step_into();
  CHECK(dbg.current_line() >= 10);
}

TEST(dbg_breakpoint) {
  auto program = dbg_compile(
      "10 PRINT \"A\"\n20 PRINT \"B\"\n30 PRINT \"C\"\n40 END");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  RealHost host;
  Vm vm(screen, host);
  vm.load(compiler.code(), compiler.constants(), compiler.line_table(),
          compiler.variable_table(), compiler.variable_names(),
          compiler.data_values());

  debugger::Debugger dbg(vm);
  dbg.set_breakpoint(30);
  auto reason = dbg.continue_();
  CHECK(reason == debugger::Debugger::StopReason::Breakpoint);
  CHECK(dbg.current_line() == 30);
}

TEST(dbg_step_over) {
  auto program = dbg_compile(
      "10 X = 1\n20 Y = 2\n30 Z = 3\n40 END");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  RealHost host;
  Vm vm(screen, host);
  vm.load(compiler.code(), compiler.constants(), compiler.line_table(),
          compiler.variable_table(), compiler.variable_names(),
          compiler.data_values());

  debugger::Debugger dbg(vm);
  dbg.set_breakpoint(10);
  dbg.continue_();
  CHECK(dbg.current_line() == 10);
  dbg.step_over();
  CHECK(dbg.current_line() == 20);
}

TEST(dbg_variables_inspection) {
  auto program = dbg_compile(
      "10 X = 42\n20 Y$ = \"hello\"\n30 PRINT X\n40 END");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  RealHost host;
  Vm vm(screen, host);
  vm.load(compiler.code(), compiler.constants(), compiler.line_table(),
          compiler.variable_table(), compiler.variable_names(),
          compiler.data_values());

  debugger::Debugger dbg(vm);
  dbg.set_breakpoint(30);
  dbg.continue_();

  auto vars = dbg.variables();
  CHECK(!vars.empty());
  // X should be 42.
  bool found_x = false;
  for (auto& v : vars) {
    if (v.name == "X") {
      CHECK(v.value == "42");
      found_x = true;
    }
  }
  CHECK(found_x);
}

TEST(dbg_evaluate) {
  auto program = dbg_compile(
      "10 X = 10\n20 Y = 20\n30 PRINT X + Y\n40 END");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  RealHost host;
  Vm vm(screen, host);
  vm.load(compiler.code(), compiler.constants(), compiler.line_table(),
          compiler.variable_table(), compiler.variable_names(),
          compiler.data_values());

  debugger::Debugger dbg(vm);
  dbg.set_breakpoint(30);
  dbg.continue_();

  auto r = dbg.evaluate("X + Y");
  CHECK(r.ok);
  CHECK(r.value == "30");
}

TEST(dbg_disassemble) {
  auto program = dbg_compile("10 PRINT \"Hi\"\n20 END");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  RealHost host;
  Vm vm(screen, host);
  vm.load(compiler.code(), compiler.constants(), compiler.line_table(),
          compiler.variable_table(), compiler.variable_names(),
          compiler.data_values());

  debugger::Debugger dbg(vm);
  std::string dis = dbg.disassemble();
  CHECK(dis.find("PRINT") != std::string::npos);
  CHECK(dis.find("HALT") != std::string::npos);
}

TEST(dbg_snapshot_restore) {
  auto program = dbg_compile("10 X = 5\n20 Y = 10\n30 PRINT X\n40 END");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  RealHost host;
  Vm vm(screen, host);
  vm.load(compiler.code(), compiler.constants(), compiler.line_table(),
          compiler.variable_table(), compiler.variable_names(),
          compiler.data_values());

  debugger::Debugger dbg(vm);
  dbg.set_breakpoint(20);
  dbg.continue_();
  dbg.save_snapshot();
  auto x_val = vm.get_variable("X");
  REQUIRE(x_val.has_value());
  CHECK(x_val->as_double() == 5.0);
  CHECK(dbg.restore_snapshot());
}
