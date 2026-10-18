#include "program_loader.h"

#include "lexer.h"

#include <fstream>
#include <sstream>

bool parse_program_text(const std::string& text, Program& out,
                        std::string& error) {
  std::istringstream iss(text);
  std::string line;
  while (std::getline(iss, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) continue;

    std::istringstream liss(line);
    std::int32_t ln = 0;
    liss >> ln;
    if (liss.fail()) {
      error = "expected line number: " + line;
      return false;
    }
    std::string rest;
    std::getline(liss, rest);
    std::size_t s = rest.find_first_not_of(" \t");
    if (s != std::string::npos) rest = rest.substr(s);

    // Prepend the line number so the parser sees it as the first token.
    std::string numbered = std::to_string(ln) + " " + rest;
    Lexer lexer(numbered, ln);
    auto tokens = lexer.scan();
    Parser parser(std::move(tokens));
    if (!parser.parse()) {
      for (const auto& e : parser.errors()) error += e + "\n";
      return false;
    }
    auto parsed = parser.release_program();
    for (auto& st : parsed.lines) {
      out.lines.push_back(std::move(st));
    }
  }
  return true;
}

bool load_program_file(const std::string& path, Program& out,
                       std::string& error) {
  std::ifstream file(path);
  if (!file.is_open()) {
    error = "cannot open '" + path + "'";
    return false;
  }
  std::ostringstream oss;
  oss << file.rdbuf();
  return parse_program_text(oss.str(), out, error);
}
