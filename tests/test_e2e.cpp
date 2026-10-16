#include "test_runner.h"
#include "compiler.h"
#include "headless_screen.h"
#include "host.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"

#include <sstream>
#include <cstdio>

static std::unique_ptr<Program> compile_source(const std::string& source) {
  auto program = std::make_unique<Program>();

  std::istringstream iss(source);
  std::string line;
  while (std::getline(iss, line)) {
    if (line.empty()) continue;

    std::istringstream liss(line);
    std::int32_t line_number = 0;
    liss >> line_number;
    if (liss.fail()) continue;

    std::string rest;
    std::getline(liss, rest);
    std::size_t start = rest.find_first_not_of(" \t");
    if (start != std::string::npos) rest = rest.substr(start);

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

static bool run_compiled(Compiler& compiler, HeadlessScreen& screen) {
  RealHost host;
  Vm vm(screen, host);
  vm.load(compiler.code(), compiler.constants(), compiler.line_table(),
          compiler.variable_table(), compiler.variable_names(),
          compiler.data_values());
  return vm.run();
}

TEST(e2e_print_string_on_headless_screen) {
  auto program = compile_source("10 PRINT \"Hello\"");
  REQUIRE(program != nullptr);

  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  REQUIRE(run_compiled(compiler, screen));

  screen.refresh();
  CHECK(screen.last_output() == "Hello\n");
}

TEST(e2e_print_number_on_headless_screen) {
  auto program = compile_source("20 PRINT 42");
  REQUIRE(program != nullptr);

  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  REQUIRE(run_compiled(compiler, screen));

  screen.refresh();
  CHECK(screen.last_output() == "42\n");
}

TEST(e2e_variables_and_arithmetic) {
  auto program = compile_source("10 X = 3\n20 Y = 4\n30 PRINT X + Y");
  REQUIRE(program != nullptr);

  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  REQUIRE(run_compiled(compiler, screen));

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
  REQUIRE(run_compiled(compiler, screen));

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
  REQUIRE(run_compiled(compiler, screen));

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
  REQUIRE(run_compiled(compiler, screen));

  screen.refresh();
  CHECK(screen.last_output() == "1\n2\n3\n");
}

TEST(e2e_gosub_return) {
  auto program = compile_source(
      "10 PRINT \"A\"\n20 GOSUB 60\n30 PRINT \"B\"\n40 END\n60 PRINT \"C\"\n70 RETURN");
  REQUIRE(program != nullptr);

  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  REQUIRE(run_compiled(compiler, screen));

  screen.refresh();
  CHECK(screen.last_output() == "A\nC\nB\n");
}

TEST(e2e_type_suffix_integer) {
  auto program = compile_source("10 X% = 7\n20 PRINT X%");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());
  HeadlessScreen screen(25, 80);
  REQUIRE(run_compiled(compiler, screen));
  screen.refresh();
  CHECK(screen.last_output() == "7\n");
}

TEST(e2e_dim_array) {
  auto program = compile_source(
      "10 DIM A(3)\n20 A(1) = 10\n30 A(2) = 20\n40 A(3) = 30\n"
      "50 PRINT A(1)\n60 PRINT A(2)\n70 PRINT A(3)");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());
  HeadlessScreen screen(25, 80);
  REQUIRE(run_compiled(compiler, screen));
  screen.refresh();
  CHECK(screen.last_output() == "10\n20\n30\n");
}

TEST(e2e_data_read) {
  auto program = compile_source(
      "10 DATA 10, 20, 30\n20 READ X\n30 READ Y\n40 READ Z\n"
      "50 PRINT X\n60 PRINT Y\n70 PRINT Z");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());
  HeadlessScreen screen(25, 80);
  REQUIRE(run_compiled(compiler, screen));
  screen.refresh();
  CHECK(screen.last_output() == "10\n20\n30\n");
}

TEST(e2e_data_read_string) {
  auto program = compile_source(
      "10 DATA \"hello\"\n20 READ X$\n30 PRINT X$");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());
  HeadlessScreen screen(25, 80);
  REQUIRE(run_compiled(compiler, screen));
  screen.refresh();
  CHECK(screen.last_output() == "hello\n");
}

TEST(e2e_restore) {
  auto program = compile_source(
      "10 DATA 1, 2\n20 READ X\n30 READ Y\n40 RESTORE\n50 READ Z\n"
      "60 PRINT X\n70 PRINT Y\n80 PRINT Z");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());
  HeadlessScreen screen(25, 80);
  REQUIRE(run_compiled(compiler, screen));
  screen.refresh();
  CHECK(screen.last_output() == "1\n2\n1\n");
}

TEST(e2e_option_base) {
  auto program = compile_source(
      "10 OPTION BASE 1\n20 DIM A(3)\n30 A(1) = 100\n40 PRINT A(1)");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());
  HeadlessScreen screen(25, 80);
  REQUIRE(run_compiled(compiler, screen));
  screen.refresh();
  CHECK(screen.last_output() == "100\n");
}

TEST(e2e_file_write_read) {
  // Write to a temp file, then read it back.
  auto program = compile_source(
      "10 OPEN \"T\" FOR OUTPUT AS #1\n20 PRINT #1, \"hello file\"\n30 CLOSE #1\n"
      "40 OPEN \"T\" FOR INPUT AS #1\n50 INPUT #1, X$\n60 CLOSE #1\n70 PRINT X$");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());
  HeadlessScreen screen(25, 80);
  REQUIRE(run_compiled(compiler, screen));
  screen.refresh();
  CHECK(screen.last_output() == "hello file\n");
  // Clean up
  std::remove("T");
}

TEST(e2e_error_handling) {
  // Trigger a runtime error by opening a nonexistent file.
  auto program = compile_source(
      "10 ON ERROR GOTO 100\n20 OPEN \"nonexistent.txt\" FOR INPUT AS #1\n"
      "30 PRINT \"after open\"\n40 GOTO 200\n"
      "100 PRINT \"error: \"; ERR\n110 PRINT \"line: \"; ERL\n"
      "120 RESUME NEXT\n200 END");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());
  HeadlessScreen screen(25, 80);
  REQUIRE(run_compiled(compiler, screen));
  screen.refresh();
  std::string out = screen.last_output();
  CHECK(out.find("error:") != std::string::npos);
  CHECK(out.find("line:") != std::string::npos);
}
