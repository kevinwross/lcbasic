#include "lexer.h"

#include <cctype>
#include <string_view>
#include <unordered_map>

namespace {

const std::unordered_map<std::string_view, TokenType> kKeywords = {
    {"PRINT", TokenType::KeywordPrint},
    {"LET", TokenType::KeywordLet},
    {"IF", TokenType::KeywordIf},
    {"THEN", TokenType::KeywordThen},
    {"ELSE", TokenType::KeywordElse},
    {"GOTO", TokenType::KeywordGoto},
    {"FOR", TokenType::KeywordFor},
    {"NEXT", TokenType::KeywordNext},
    {"GOSUB", TokenType::KeywordGosub},
    {"RETURN", TokenType::KeywordReturn},
    {"END", TokenType::KeywordEnd},
    {"STOP", TokenType::KeywordStop},
    {"REM", TokenType::KeywordRem},
    {"DATA", TokenType::KeywordData},
    {"READ", TokenType::KeywordRead},
    {"RESTORE", TokenType::KeywordRestore},
    {"DIM", TokenType::KeywordDim},
    {"OPTION", TokenType::KeywordOption},
    {"BASE", TokenType::KeywordBase},
    {"INPUT", TokenType::KeywordInput},
    {"LINE", TokenType::KeywordLine},
    {"ON", TokenType::KeywordOn},
    {"ERROR", TokenType::KeywordError},
    {"RESUME", TokenType::KeywordResume},
    {"CLS", TokenType::KeywordCls},
    {"LOCATE", TokenType::KeywordLocate},
    {"COLOR", TokenType::KeywordColor},
    {"TO", TokenType::KeywordTo},
    {"STEP", TokenType::KeywordStep},
    {"DEF", TokenType::KeywordDef},
    {"FN", TokenType::KeywordFn},
};

}  // namespace

Lexer::Lexer(std::string source, std::int32_t line_number)
  : source_(std::move(source)), line_number_(line_number) {}

char Lexer::peek() {
  if (is_at_end()) return '\0';
  return source_[current_];
}

char Lexer::advance() {
  return source_[current_++];
}

bool Lexer::match(char expected) {
  if (is_at_end()) return false;
  if (source_[current_] != expected) return false;
  current_++;
  return true;
}

void Lexer::skip_whitespace() {
  for (;;) {
    char c = peek();
    switch (c) {
      case ' ':
      case '\r':
      case '\t':
        advance();
        break;
      default:
        return;
    }
  }
}

bool Lexer::is_at_end() const { return current_ >= source_.size(); }

bool Lexer::is_alpha(char c) const {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool Lexer::is_digit(char c) const {
  return std::isdigit(static_cast<unsigned char>(c));
}

Token Lexer::make_token(TokenType type, std::string lexeme) {
  return Token{type, std::move(lexeme), 0.0, line_number_,
               static_cast<std::int32_t>(start_)};
}

Token Lexer::error_token(const std::string& message) {
  return Token{TokenType::Error, message, 0.0, line_number_,
               static_cast<std::int32_t>(start_)};
}

TokenType Lexer::keyword(const std::string& text) {
  auto it = kKeywords.find(text);
  if (it != kKeywords.end()) return it->second;
  return TokenType::Identifier;
}

Token Lexer::scan_number() {
  while (is_digit(peek())) advance();

  // Fractional part
  if (peek() == '.' && is_digit(source_[current_ + 1])) {
    advance(); // consume '.'
    while (is_digit(peek())) advance();
  }

  // Exponent (E or D, GW-BASIC supports both)
  if (peek() == 'E' || peek() == 'D' || peek() == 'e' || peek() == 'd') {
    advance();
    if (peek() == '+' || peek() == '-') advance();
    while (is_digit(peek())) advance();
  }

  std::string lexeme = source_.substr(start_, current_ - start_);
  double value = std::stod(lexeme);
  return Token{TokenType::Number, lexeme, value, line_number_,
               static_cast<std::int32_t>(start_)};
}

Token Lexer::scan_string() {
  // Opening quote already consumed by caller.
  while (peek() != '"' && !is_at_end()) {
    if (peek() == '\n') break;
    advance();
  }

  if (is_at_end()) {
    return error_token("unterminated string");
  }

  // Closing quote
  advance();

  std::string lexeme = source_.substr(start_, current_ - start_);
  // Strip the surrounding quotes for the stored lexeme value.
  std::string value = lexeme.substr(1, lexeme.size() - 2);
  return Token{TokenType::String, value, 0.0, line_number_,
               static_cast<std::int32_t>(start_)};
}

Token Lexer::scan_identifier_or_keyword() {
  while (is_alpha(peek()) || is_digit(peek()) || peek() == '%' ||
         peek() == '!' || peek() == '#' || peek() == '$' || peek() == '&') {
    advance();
  }

  std::string lexeme = source_.substr(start_, current_ - start_);
  TokenType type = keyword(lexeme);

  return Token{type, lexeme, 0.0, line_number_, static_cast<std::int32_t>(start_)};
}

Token Lexer::scan_token() {
  skip_whitespace();
  start_ = current_;

  if (is_at_end()) {
    return Token{TokenType::Eof, "", 0.0, line_number_,
                 static_cast<std::int32_t>(current_)};
  }

  char c = advance();

  if (is_digit(c)) {
    current_ = start_;
    return scan_number();
  }

  if (c == '"') {
    return scan_string();
  }

  if (is_alpha(c)) {
    current_ = start_;
    return scan_identifier_or_keyword();
  }

  switch (c) {
    case ':': return make_token(TokenType::Colon, ":");
    case ',': return make_token(TokenType::Comma, ",");
    case ';': return make_token(TokenType::Semicolon, ";");
    case '(': return make_token(TokenType::LParen, "(");
    case ')': return make_token(TokenType::RParen, ")");
    case '+': return make_token(TokenType::Plus, "+");
    case '-': return make_token(TokenType::Minus, "-");
    case '*': return make_token(TokenType::Star, "*");
    case '/': return make_token(TokenType::Slash, "/");
    case '=': return make_token(TokenType::Eq, "=");
    case '<':
      if (match('>')) return make_token(TokenType::Ne, "<>");
      if (match('=')) return make_token(TokenType::Le, "<=");
      return make_token(TokenType::Lt, "<");
    case '>':
      if (match('=')) return make_token(TokenType::Ge, ">=");
      return make_token(TokenType::Gt, ">");
    default:
      return error_token("unexpected character");
  }
}

std::vector<Token> Lexer::scan() {
  std::vector<Token> tokens;
  for (;;) {
    Token t = scan_token();
    tokens.push_back(t);
    if (t.type == TokenType::Eof || t.type == TokenType::Error) break;
  }
  return tokens;
}
