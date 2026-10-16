#pragma once

#include "lexer.h"
#include "value.h"

#include <memory>
#include <string>
#include <variant>
#include <vector>

// ---------- AST ----------

struct Expr;  // forward

struct NumberExpr { double value; };
struct StringExpr { std::string value; };
struct BinaryExpr {
  std::unique_ptr<Expr> left;
  TokenType op;
  std::unique_ptr<Expr> right;
};

struct Expr {
  std::variant<NumberExpr, StringExpr, BinaryExpr> data;
};

struct PrintStmt { std::vector<std::unique_ptr<Expr>> args; };

struct Statement {
  std::int32_t line_number;
  std::variant<PrintStmt> data;
};

struct Program { std::vector<Statement> statements; };

// ---------- Parser ----------

class Parser {
public:
  explicit Parser(std::vector<Token> tokens);

  // Returns true if parsing succeeded; errors are collected in errors().
  bool parse();
  Program release_program();
  const std::vector<std::string>& errors() const { return errors_; }

private:
  std::vector<Token> tokens_;
  std::size_t current_ = 0;
  Program program_;
  std::vector<std::string> errors_;

  bool is_at_end() const;
  Token peek() const;
  Token advance();
  bool check(TokenType type) const;
  bool match(TokenType type);
  Token consume(TokenType type, const std::string& message);
  void error_at_current(const std::string& message);

  // Grammar
  bool parse_statement();
  std::unique_ptr<Expr> parse_expression();
  std::unique_ptr<Expr> parse_additive();
  std::unique_ptr<Expr> parse_multiplicative();
  std::unique_ptr<Expr> parse_primary();
};
