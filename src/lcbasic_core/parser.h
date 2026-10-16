#pragma once

#include "lexer.h"
#include "value.h"

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

// ---------- Forward declarations ----------

struct Expr;
struct Statement;

// ---------- Expressions ----------

struct NumberExpr { double value; };
struct StringExpr { std::string value; };
struct VariableExpr { std::string name; };
struct BinaryExpr {
  std::unique_ptr<Expr> left;
  TokenType op;
  std::unique_ptr<Expr> right;
};

struct Expr {
  std::variant<NumberExpr, StringExpr, VariableExpr, BinaryExpr> data;
};

// ---------- Statements ----------

struct PrintStmt {
  std::vector<std::unique_ptr<Expr>> args;
  bool trailing_separator = false;  // comma/semicolon suppresses newline
};

struct LetStmt {
  std::string var;
  std::unique_ptr<Expr> value;
};

struct GotoStmt { std::int32_t target_line; };

struct IfStmt {
  std::unique_ptr<Expr> condition;
  std::int32_t then_line = -1;  // -1 means THEN is followed by statement(s)
  std::vector<std::unique_ptr<Statement>> then_stmts;
  std::int32_t else_line = -1;
  std::vector<std::unique_ptr<Statement>> else_stmts;
};

struct ForStmt {
  std::string var;
  std::unique_ptr<Expr> start, end, step;
};

struct NextStmt { std::string var; };

struct GosubStmt { std::int32_t target_line; };
struct ReturnStmt {};
struct EndStmt {};
struct StopStmt {};

struct Statement {
  std::int32_t line_number;
  std::variant<PrintStmt, LetStmt, GotoStmt, IfStmt, ForStmt, NextStmt,
               GosubStmt, ReturnStmt, EndStmt, StopStmt>
      data;
};

struct Program {
  // line number -> statements on that line
  std::vector<std::pair<std::int32_t, Statement>> lines;
};

// ---------- Parser ----------

class Parser {
public:
  explicit Parser(std::vector<Token> tokens);

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
  std::unique_ptr<Statement> parse_statement_no_line();
  std::unique_ptr<Expr> parse_expression();   // entry point -> relational
  std::unique_ptr<Expr> parse_relational();   // =, <>, <, <=, >, >=
  std::unique_ptr<Expr> parse_additive();     // + -
  std::unique_ptr<Expr> parse_multiplicative(); // * /
  std::unique_ptr<Expr> parse_unary();        // NOT, -
  std::unique_ptr<Expr> parse_primary();      // number, string, var, (expr)
};
