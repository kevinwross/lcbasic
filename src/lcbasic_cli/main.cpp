#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include "ansi_screen.h"
#include "headless_screen.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Reads a .bas file, splitting into (line_number, source) pairs.
static std::vector<std::pair<std::int32_t, std::string>> load_bas(
    const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "lcbasic: cannot open '" << path << "'\n";
    std::exit(1);
  }

  std::vector<std::pair<std::int32_t, std::string>> lines;
  std::string raw;
  while (std::getline(file, raw)) {
    // Strip trailing \r
    if (!raw.empty() && raw.back() == '\r') raw.pop_back();
    if (raw.empty()) continue;

    std::istringstream iss(raw);
    std::int32_t line_number = 0;
    iss >> line_number;
    if (iss.fail()) {
      // Direct-mode statement (no line number) — not supported in M0.
      std::cerr << "lcbasic: expected line number at start of line: " << raw
                << "\n";
      std::exit(1);
    }
    std::string rest;
    std::getline(iss, rest);
    // Trim leading space
    std::size_t start = rest.find_first_not_of(" \t");
    if (start != std::string::npos) rest = rest.substr(start);
    lines.emplace_back(line_number, rest);
  }
  return lines;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: lcbasic <program.bas>\n";
    return 1;
  }

  auto lines = load_bas(argv[1]);

  // Build the program AST.
  Program program;
  for (auto& [line_number, source] : lines) {
    // Prepend the line number so the parser sees it as the first token.
    std::string full_source = std::to_string(line_number) + " " + source;
    Lexer lexer(full_source, line_number);
    auto tokens = lexer.scan();

    Parser parser(std::move(tokens));
    if (!parser.parse()) {
      for (const auto& err : parser.errors()) {
        std::cerr << "lcbasic: " << err << "\n";
      }
      return 1;
    }
    auto parsed = parser.release_program();
    for (auto& stmt : parsed.statements) {
      program.statements.push_back(std::move(stmt));
    }
  }

  // Compile to bytecode.
  Compiler compiler(program);
  if (!compiler.compile()) {
    for (const auto& err : compiler.errors()) {
      std::cerr << "lcbasic: " << err << "\n";
    }
    return 1;
  }

  // Run on the ANSI screen.
  AnsiScreen screen;
  Vm vm(screen);
  if (!vm.interpret(compiler.code(), compiler.constants())) {
    for (const auto& err : vm.errors()) {
      std::cerr << "lcbasic: " << err << "\n";
    }
    screen.refresh();
    return 1;
  }

  screen.refresh();
  return 0;
}
