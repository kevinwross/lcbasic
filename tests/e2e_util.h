#pragma once

#include "compiler.h"
#include "headless_screen.h"
#include "host.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"

#include <memory>
#include <sstream>
#include <string>

// Shared helpers for end-to-end tests: compile a multi-line BASIC source
// string, run it, and capture output / errors.
namespace e2e {

struct RunResult {
  bool parsed = false;
  bool compiled = false;
  bool ran = false;       // true on clean HALT
  bool trapped = false;   // true if a runtime error was reported
  std::string output;
  std::string error;
};

inline std::unique_ptr<Program> parse_source(const std::string& source) {
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

// Run with a caller-supplied host (for injected input / fake clock).
inline RunResult run_with_host(const std::string& source, Host& host) {
  RunResult res;
  auto program = parse_source(source);
  if (!program) return res;
  res.parsed = true;

  Compiler compiler(*program);
  if (!compiler.compile()) {
    for (auto& e : compiler.errors()) res.error += e + "\n";
    return res;
  }
  res.compiled = true;

  HeadlessScreen screen(25, 80);
  Vm vm(screen, host);
  vm.load(compiler.code(), compiler.constants(), compiler.line_table(),
          compiler.variable_table(), compiler.variable_names(),
          compiler.data_values(), compiler.fn_descs());
  res.ran = vm.run();
  for (auto& e : vm.errors()) res.error += e + "\n";
  res.trapped = !res.ran && !res.error.empty();
  screen.refresh();
  res.output = screen.last_output();
  return res;
}

inline RunResult run(const std::string& source) {
  RealHost host;
  return run_with_host(source, host);
}

}  // namespace e2e
