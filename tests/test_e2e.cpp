#include "test_runner.h"
#include "compiler.h"
#include "headless_screen.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"

#include <memory>

static std::unique_ptr<Program> compile_source(const std::string& source,
                                               std::int32_t line_number) {
  Lexer lexer(source, line_number);
  auto tokens = lexer.scan();

  Parser parser(std::move(tokens));
  if (!parser.parse()) return nullptr;

  auto program = std::make_unique<Program>(parser.release_program());

  Compiler compiler(*program);
  if (!compiler.compile()) return nullptr;

  return program;
}

TEST(e2e_print_string_on_headless_screen) {
  auto program = compile_source("10 PRINT \"Hello\"", 10);
  REQUIRE(program != nullptr);

  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  Vm vm(screen);
  REQUIRE(vm.interpret(compiler.code(), compiler.constants()));

  screen.refresh();
  CHECK(screen.last_output() == "Hello\n");
}

TEST(e2e_print_number_on_headless_screen) {
  auto program = compile_source("20 PRINT 42", 20);
  REQUIRE(program != nullptr);

  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  Vm vm(screen);
  REQUIRE(vm.interpret(compiler.code(), compiler.constants()));

  screen.refresh();
  CHECK(screen.last_output() == "42\n");
}
