#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

enum class TokenType {
  Number,
  String,
  Identifier,
  KeywordPrint,
  KeywordLet,
  Let,  // alias for keyword LET
  Newline,
  Colon,
  Comma,
  Semicolon,
  LParen,
  RParen,
  Plus,
  Minus,
  Star,
  Slash,
  Eq,
  Ne,
  Lt,
  Le,
  Gt,
  Ge,
  Eof,
  Error,
};

struct Token {
  TokenType type;
  std::string lexeme;
  double number;      // valid when type == Number
  std::int32_t line;  // source line number this token came from
  std::int32_t col;   // column within the source line
};

// Lexes a single physical source line (without its line number) into tokens.
class Lexer {
public:
  explicit Lexer(std::string source, std::int32_t line_number);

  std::vector<Token> scan();

private:
  std::string source_;
  std::int32_t line_number_;
  std::size_t start_ = 0;
  std::size_t current_ = 0;

  char peek();
  char advance();
  bool match(char expected);
  void skip_whitespace();
  Token scan_token();
  Token scan_number();
  Token scan_string();
  Token scan_identifier_or_keyword();
  Token make_token(TokenType type, std::string lexeme);
  Token error_token(const std::string& message);
  bool is_alpha(char c) const;
  bool is_digit(char c) const;
  bool is_at_end() const;
};
