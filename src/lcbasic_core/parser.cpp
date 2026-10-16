#include "parser.h"

#include <sstream>

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

bool Parser::is_at_end() const {
  return current_ >= tokens_.size() || tokens_[current_].type == TokenType::Eof;
}

Token Parser::peek() const { return tokens_[current_]; }

Token Parser::advance() {
  if (!is_at_end()) current_++;
  return tokens_[current_ - 1];
}

bool Parser::check(TokenType type) const {
  if (is_at_end()) return false;
  return tokens_[current_].type == type;
}

bool Parser::match(TokenType type) {
  if (!check(type)) return false;
  advance();
  return true;
}

Token Parser::consume(TokenType type, const std::string& message) {
  if (check(type)) return advance();
  error_at_current(message);
  return Token{TokenType::Error, message, 0.0, 0, 0};
}

void Parser::error_at_current(const std::string& message) {
  Token t = peek();
  std::ostringstream oss;
  oss << "line " << t.line << " col " << t.col << ": " << message;
  errors_.push_back(oss.str());
}

bool Parser::parse() {
  while (!is_at_end()) {
    if (!parse_statement()) {
      // Skip to next line to resynchronise.
      while (!is_at_end() && !check(TokenType::Newline)) advance();
      if (check(TokenType::Newline)) advance();
    }
  }
  return errors_.empty();
}

Program Parser::release_program() { return std::move(program_); }

bool Parser::parse_statement() {
  // Expect a line number.
  if (!check(TokenType::Number)) {
    error_at_current("expected line number");
    return false;
  }
  Token line_tok = advance();
  std::int32_t line_number = static_cast<std::int32_t>(line_tok.number);

  // Parse statements on this line (separated by ':').
  for (;;) {
    Statement stmt;
    stmt.line_number = line_number;

    if (match(TokenType::KeywordPrint)) {
      PrintStmt print;
      // Parse print argument list
      do {
        auto expr = parse_expression();
        if (!expr) return false;
        print.args.push_back(std::move(expr));
      } while (match(TokenType::Comma) || match(TokenType::Semicolon));
      stmt.data = std::move(print);
    } else {
      error_at_current("unknown statement");
      return false;
    }

    program_.statements.push_back(std::move(stmt));

    if (match(TokenType::Colon)) continue;
    break;
  }

  // Consume trailing newline.
  match(TokenType::Newline);
  return true;
}

std::unique_ptr<Expr> Parser::parse_expression() {
  return parse_additive();
}

std::unique_ptr<Expr> Parser::parse_additive() {
  auto left = parse_multiplicative();
  if (!left) return nullptr;

  while (match(TokenType::Plus) || match(TokenType::Minus)) {
    TokenType op = tokens_[current_ - 1].type;
    auto right = parse_multiplicative();
    if (!right) return nullptr;
    auto e = std::make_unique<Expr>();
    e->data = BinaryExpr{std::move(left), op, std::move(right)};
    left = std::move(e);
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_multiplicative() {
  auto left = parse_primary();
  if (!left) return nullptr;

  while (match(TokenType::Star) || match(TokenType::Slash)) {
    TokenType op = tokens_[current_ - 1].type;
    auto right = parse_primary();
    if (!right) return nullptr;
    auto e = std::make_unique<Expr>();
    e->data = BinaryExpr{std::move(left), op, std::move(right)};
    left = std::move(e);
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_primary() {
  if (check(TokenType::Number)) {
    Token t = advance();
    auto e = std::make_unique<Expr>();
    e->data = NumberExpr{t.number};
    return e;
  }
  if (check(TokenType::String)) {
    Token t = advance();
    auto e = std::make_unique<Expr>();
    e->data = StringExpr{t.lexeme};
    return e;
  }
  if (match(TokenType::LParen)) {
    auto e = parse_expression();
    if (!e) return nullptr;
    consume(TokenType::RParen, "expected ')'");
    return e;
  }
  error_at_current("expected expression");
  return nullptr;
}
