#include "test_runner.h"
#include "compiler.h"
#include "headless_screen.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"

#include <sstream>

#include <memory>

static std::unique_ptr<Program> compile_source(const std::string& source) {
  auto program = std::make_unique<Program>();

  std::istringstream iss(source);
  std::string line;
  while (std::getline(iss, line)) {
    if (line.empty()) continue;

    // Extract line number.
    std::istringstream liss(line);
    std::int32_t line_number = 0;
    liss >> line_number;
    if (liss.fail()) continue;

    std::string rest;
    std::getline(liss, rest);
    std::size_t start = rest.find_first_not_of(" \t");
    if (start != std::string::npos) rest = rest.substr(start);

    // Prepend line number so the parser sees it as the first token.
    std::string numbered = std::to_string(line_number) + " " + rest;
    Lexer lexer(numbered, line_number);
    auto tokens = lexer.scan();

    Parser parser(std::move(tokens));
    if (!parser.parse()) return nullptr;

    auto parsed = parser.release_program();
    for (auto& s : parsed.lines) {
      program->lines.push_back(std::move(s));
    }
  }
  return program;
}

TEST(e2e_print_string_on_headless_screen) {
  auto program = compile_source("10 PRINT \"Hello\"");
  REQUIRE(program != nullptr);

  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  Vm vm(screen);
  REQUIRE(vm.run(compiler.code(), compiler.constants(), compiler.line_table(),
                 compiler.variable_table()));

  screen.refresh();
  CHECK(screen.last_output() == "Hello\n");
}

TEST(e2e_print_number_on_headless_screen) {
  auto program = compile_source("20 PRINT 42");
  REQUIRE(program != nullptr);

  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  Vm vm(screen);
  REQUIRE(vm.run(compiler.code(), compiler.constants(), compiler.line_table(),
                 compiler.variable_table()));

  screen.refresh();
  CHECK(screen.last_output() == "42\n");
}

TEST(e2e_variables_and_arithmetic) {
  auto program = compile_source("10 X = 3\n20 Y = 4\n30 PRINT X + Y");
  REQUIRE(program != nullptr);

  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  Vm vm(screen);
  REQUIRE(vm.run(compiler.code(), compiler.constants(), compiler.line_table(),
                 compiler.variable_table()));

  screen.refresh();
  CHECK(screen.last_output() == "7\n");
}

TEST(e2e_goto) {
  auto program = compile_source(
      "10 PRINT \"A\"\n20 GOTO 40\n30 PRINT \"B\"\n40 PRINT \"C\"");
  REQUIRE(program != nullptr);

  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  Vm vm(screen);
  REQUIRE(vm.run(compiler.code(), compiler.constants(), compiler.line_table(),
                 compiler.variable_table()));

  screen.refresh();
  CHECK(screen.last_output() == "A\nC\n");
}

TEST(e2e_if_then) {
  auto program = compile_source(
      "10 X = 5\n20 IF X = 5 THEN 40\n30 PRINT \"NO\"\n40 PRINT \"YES\"");
  REQUIRE(program != nullptr);

  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  Vm vm(screen);
  REQUIRE(vm.run(compiler.code(), compiler.constants(), compiler.line_table(),
                 compiler.variable_table()));

  screen.refresh();
  CHECK(screen.last_output() == "YES\n");
}

TEST(e2e_for_loop) {
  auto program = compile_source(
      "10 FOR I = 1 TO 3\n20 PRINT I\n30 NEXT I");
  REQUIRE(program != nullptr);

  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  Vm vm(screen);
  REQUIRE(vm.run(compiler.code(), compiler.constants(), compiler.line_table(),
                 compiler.variable_table()));

  screen.refresh();
  CHECK(screen.last_output() == "1\n2\n3\n");
}

TEST(e2e_gosub_return) {
  // Subroutines go after END in GW-BASIC.
  auto program = compile_source(
      "10 PRINT \"A\"\n20 GOSUB 60\n30 PRINT \"B\"\n40 END\n60 PRINT \"C\"\n70 RETURN");
  REQUIRE(program != nullptr);

  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  Vm vm(screen);
  REQUIRE(vm.run(compiler.code(), compiler.constants(), compiler.line_table(),
                 compiler.variable_table()));

  screen.refresh();
  CHECK(screen.last_output() == "A\nC\nB\n");
}
