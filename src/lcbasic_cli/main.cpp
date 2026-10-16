#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include "host.h"
#include "ansi_screen.h"
#include "headless_screen.h"
#include "basic_format.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ---------- Program store ----------

struct ProgramStore {
  // line number -> raw source text
  std::vector<std::pair<std::int32_t, std::string>> lines;

  // List a range of lines (or all if from_line/to_line are -1).
  void list(std::int32_t from_line = -1, std::int32_t to_line = -1) const {
    for (const auto& [ln, src] : lines) {
      if (from_line >= 0 && ln < from_line) continue;
      if (to_line >= 0 && ln > to_line) continue;
      std::cout << ln << " " << src << "\n";
    }
  }

  void clear() { lines.clear(); }

  bool empty() const { return lines.empty(); }
};

// ---------- Helpers ----------

static std::vector<std::pair<std::int32_t, std::string>> load_bas_file(
    const std::string& path) {
  auto br = fstok::load_bas_file(path);
  if (br.program.empty()) {
    std::cerr << "lcbasic: cannot open '" << path << "'\n";
    std::exit(1);
  }
  std::vector<std::pair<std::int32_t, std::string>> lines(br.program.begin(),
                                                          br.program.end());
  std::sort(lines.begin(), lines.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  return lines;
}

// Execute a single direct-mode statement (no line number).
static bool run_direct_statement(const std::string& source, Screen& screen,
                                 Host& host);

// Compile a Program and run it on the given screen.
static bool run_program(const Program& program, Screen& screen, Host& host) {
  Compiler compiler(program);
  if (!compiler.compile()) {
    for (const auto& err : compiler.errors()) {
      std::cerr << "lcbasic: " << err << "\n";
    }
    return false;
  }

  Vm vm(screen, host);
  if (!vm.run(compiler.code(), compiler.constants(), compiler.line_table(),
              compiler.variable_table(), compiler.variable_names(),
              compiler.data_values())) {
    for (const auto& err : vm.errors()) {
      std::cerr << "lcbasic: " << err << "\n";
    }
    return false;
  }
  return true;
}

// ---------- REPL ----------

static void repl(Screen& screen, Host& host) {
  std::cout << "lcbasic direct mode. Type statements and press RETURN.\n";
  std::cout << "Type 'bye' to exit.\n\n";

  ProgramStore store;

  while (true) {
    std::cout << "> ";
    std::flush(std::cout);
    std::string input;
    if (!std::getline(std::cin, input)) break;

    // Trim trailing \r
    if (!input.empty() && input.back() == '\r') input.pop_back();

    // Trim leading whitespace
    std::size_t start = input.find_first_not_of(" \t");
    if (start == std::string::npos) continue;
    input = input.substr(start);

    if (input == "bye" || input == "exit") break;

    // Check for a leading line number.
    std::istringstream iss(input);
    std::int32_t line_number = 0;
    iss >> line_number;

    std::string rest;
    std::getline(iss, rest);
    std::size_t rstart = rest.find_first_not_of(" \t");
    if (rstart != std::string::npos) rest = rest.substr(rstart);

    bool had_line_number = (iss.fail() == false) && !rest.empty() ||
                           (input[0] >= '0' && input[0] <= '9');

    if (had_line_number && !rest.empty()) {
      // Numbered line: store it.
      // Replace or add.
      bool found = false;
      for (auto& [ln, src] : store.lines) {
        if (ln == line_number) {
          src = rest;
          found = true;
          break;
        }
      }
      if (!found) {
        store.lines.emplace_back(line_number, rest);
        // Keep sorted by line number.
        std::sort(store.lines.begin(), store.lines.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
      }
    } else if (input == "LIST" || input == "list") {
      store.list();
    } else if (input.rfind("LIST ", 0) == 0 || input.rfind("list ", 0) == 0) {
      std::string args = input.substr(5);
      std::istringstream aiss(args);
      std::int32_t from = -1, to = -1;
      aiss >> from;
      if (aiss.peek() == '-') {
        aiss.ignore();
        aiss >> to;
      }
      store.list(from, to);
    } else if (input == "NEW" || input == "new") {
      store.clear();
    } else if (input == "RUN" || input == "run") {
      // Build program from store.
      Program program;
      for (auto& [ln, src] : store.lines) {
        // Prepend line number so the parser sees it as the first token.
        std::string numbered = std::to_string(ln) + " " + src;
        Lexer lexer(numbered, ln);
        auto tokens = lexer.scan();
        Parser parser(std::move(tokens));
        if (!parser.parse()) {
          for (const auto& err : parser.errors()) {
            std::cerr << "lcbasic: " << err << "\n";
          }
          goto run_done;
        }
        auto parsed = parser.release_program();
        for (auto& s : parsed.lines) {
          program.lines.push_back(std::move(s));
        }
      }
      run_program(program, screen, host);
    run_done:;
    } else if (rest.empty() && had_line_number) {
      // Just a line number with no statement: delete that line.
      store.lines.erase(
          std::remove_if(store.lines.begin(), store.lines.end(),
                         [&](const auto& p) { return p.first == line_number; }),
          store.lines.end());
    } else if (input.rfind("SAVE ", 0) == 0 || input.rfind("save ", 0) == 0) {
      // SAVE "file" [,B]  — ,B writes tokenized binary format
      std::string args = input.substr(5);
      // Skip leading whitespace.
      std::size_t astart = args.find_first_not_of(" \t");
      if (astart == std::string::npos) {
        std::cerr << "lcbasic: SAVE requires a filename\n";
      } else {
        args = args.substr(astart);
        std::string token;
        bool binary = false;

        if (args.front() == '"') {
          // Quoted filename: find closing quote.
          std::size_t endq = args.find('"', 1);
          if (endq != std::string::npos) {
            token = args.substr(1, endq - 1);
            std::string after = args.substr(endq + 1);
            // Skip whitespace, check for ,B
            std::size_t p = after.find_first_not_of(" \t");
            if (p != std::string::npos && after[p] == ',') {
              std::size_t q = after.find_first_not_of(" \t", p + 1);
              if (q != std::string::npos &&
                  (after[q] == 'B' || after[q] == 'b')) {
                binary = true;
              }
            }
          } else {
            token = args.substr(1);
          }
        } else {
          // Unquoted: read to comma or end.
          std::size_t comma = args.find(',');
          if (comma != std::string::npos) {
            token = args.substr(0, comma);
            std::string after = args.substr(comma + 1);
            std::size_t p = after.find_first_not_of(" \t");
            if (p != std::string::npos &&
                (after[p] == 'B' || after[p] == 'b')) {
              binary = true;
            }
          } else {
            token = args;
          }
        }

        fstok::BasicProgram prog;
        for (auto& [ln, src] : store.lines) {
          prog[ln] = src;
        }
        fstok::save_bas_file(token, prog, binary, false);
        std::cout << "Saved '" << token << "' ("
                  << (binary ? "binary" : "text") << ")\n";
      }
    } else if (input.rfind("LOAD ", 0) == 0 || input.rfind("load ", 0) == 0) {
      // LOAD "file" — auto-detect text vs binary
      std::string args = input.substr(5);
      std::size_t astart = args.find_first_not_of(" \t");
      if (astart == std::string::npos) {
        std::cerr << "lcbasic: LOAD requires a filename\n";
      } else {
        args = args.substr(astart);
        std::string token;

        if (args.front() == '"') {
          std::size_t endq = args.find('"', 1);
          if (endq != std::string::npos) {
            token = args.substr(1, endq - 1);
          } else {
            token = args.substr(1);
          }
        } else {
          token = args;
        }

        auto br = fstok::load_bas_file(token);
        if (br.program.empty()) {
          std::cerr << "lcbasic: could not load '" << token << "'\n";
        } else {
          store.clear();
          for (auto& [ln, src] : br.program) {
            store.lines.emplace_back(ln, src);
          }
          std::sort(store.lines.begin(), store.lines.end(),
                    [](const auto& a, const auto& b) { return a.first < b.first; });
          std::cout << "Loaded '" << token << "' ("
                    << (br.is_binary ? "binary" : "text") << ")\n";
        }
      }
    } else {
      // Direct-mode statement (no line number): execute immediately.
      if (!run_direct_statement(input, screen, host)) {
        // Error already printed.
      }
    }
  }
}

// Execute a single direct-mode statement (no line number).
static bool run_direct_statement(const std::string& source, Screen& screen,
                                 Host& host) {
  Lexer lexer(source, 0);
  auto tokens = lexer.scan();

  // Direct mode: wrap the statement with a dummy line number 0.
  std::string numbered = "0 " + source;
  Lexer lexer2(numbered, 0);
  auto tokens2 = lexer2.scan();

  Parser parser2(std::move(tokens2));
  if (!parser2.parse()) {
    for (const auto& err : parser2.errors()) {
      std::cerr << "lcbasic: " << err << "\n";
    }
    return false;
  }

  Program program = parser2.release_program();
  return run_program(program, screen, host);
}

// ---------- main ----------

int main(int argc, char** argv) {
  if (argc < 2) {
    // No argument: start direct-mode REPL with headless screen.
    RealHost host;
    HeadlessScreen screen(25, 80);
    repl(screen, host);
    return 0;
  }

  std::string arg = argv[1];

  if (arg == "--help" || arg == "-h") {
    std::cout << "Usage: lcbasic [program.bas]\n";
    std::cout << "  Run a GW-BASIC program from a .bas file.\n";
    std::cout << "  Without arguments, starts the direct-mode REPL.\n";
    return 0;
  }

  auto file_lines = load_bas_file(arg);

  Program program;
  for (auto& [ln, src] : file_lines) {
    // Prepend line number so the parser sees it as the first token.
    std::string numbered = std::to_string(ln) + " " + src;
    Lexer lexer(numbered, ln);
    auto tokens = lexer.scan();
    Parser parser(std::move(tokens));
    if (!parser.parse()) {
      for (const auto& err : parser.errors()) {
        std::cerr << "lcbasic: " << err << "\n";
      }
      return 1;
    }
    auto parsed = parser.release_program();
    for (auto& s : parsed.lines) {
      program.lines.push_back(std::move(s));
    }
  }

  RealHost host;
  AnsiScreen screen;
  if (!run_program(program, screen, host)) {
    screen.refresh();
    return 1;
  }

  screen.refresh();
  return 0;
}
