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
      // Resynchronise to next line.
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

    if (check(TokenType::KeywordPrint)) {
      advance(); // PRINT
      PrintStmt print;
      if (!check(TokenType::Newline) && !check(TokenType::Colon) &&
          !is_at_end()) {
        do {
          auto expr = parse_expression();
          if (!expr) return false;
          print.args.push_back(std::move(expr));
        } while (match(TokenType::Comma) || match(TokenType::Semicolon));
        print.trailing_separator =
            (tokens_.size() > current_ &&
             (tokens_[current_ - 1].type == TokenType::Comma ||
              tokens_[current_ - 1].type == TokenType::Semicolon));
      }
      stmt.data = std::move(print);
    } else if (match(TokenType::KeywordLet)) {
      // LET var = expr
      if (!check(TokenType::Identifier)) {
        error_at_current("expected variable name after LET");
        return false;
      }
      Token var_tok = advance();
      consume(TokenType::Eq, "expected '=' after variable name");
      auto value = parse_expression();
      if (!value) return false;
      LetStmt let;
      let.var = var_tok.lexeme;
      let.value = std::move(value);
      stmt.data = std::move(let);
    } else if (check(TokenType::Identifier)) {
      // Implicit LET: var = expr
      Token var_tok = peek();
      // Check that next token is '='
      std::size_t save = current_;
      advance();
      if (check(TokenType::Eq)) {
        current_ = save; // rewind, parse as LetStmt
        consume(TokenType::Identifier, "expected variable name");
        consume(TokenType::Eq, "expected '=' after variable name");
        auto value = parse_relational();
        if (!value) return false;
        LetStmt let;
        let.var = var_tok.lexeme;
        let.value = std::move(value);
        stmt.data = std::move(let);
      } else {
        current_ = save;
        error_at_current("unknown statement");
        return false;
      }
    } else if (match(TokenType::KeywordGoto)) {
      consume(TokenType::Number, "expected line number after GOTO");
      GotoStmt goto_stmt;
      goto_stmt.target_line = static_cast<std::int32_t>(tokens_[current_ - 1].number);
      stmt.data = std::move(goto_stmt);
    } else if (match(TokenType::KeywordIf)) {
      auto cond = parse_expression();
      if (!cond) return false;
      consume(TokenType::KeywordThen, "expected THEN after IF condition");

      IfStmt if_stmt;
      if_stmt.condition = std::move(cond);

      if (check(TokenType::Number)) {
        // THEN line_number
        Token then_line_tok = advance();
        if_stmt.then_line = static_cast<std::int32_t>(then_line_tok.number);
        if (match(TokenType::KeywordElse)) {
          consume(TokenType::Number, "expected line number after ELSE");
          if_stmt.else_line =
              static_cast<std::int32_t>(tokens_[current_ - 1].number);
        }
      } else {
        // THEN statement(s)
        std::vector<std::unique_ptr<Statement>> then_stmts;
        // Parse statements until ELSE, colon, or end of line
        while (!is_at_end() && !check(TokenType::Newline) &&
               !check(TokenType::Colon) && !check(TokenType::KeywordElse)) {
          // Parse a single statement (without line number)
          auto inner = parse_statement_no_line();
          if (!inner) break;
          then_stmts.push_back(std::move(inner));
        }
        if_stmt.then_stmts = std::move(then_stmts);

        if (match(TokenType::KeywordElse)) {
          std::vector<std::unique_ptr<Statement>> else_stmts;
          while (!is_at_end() && !check(TokenType::Newline) &&
                 !check(TokenType::Colon)) {
            auto inner = parse_statement_no_line();
            if (!inner) break;
            else_stmts.push_back(std::move(inner));
          }
          if_stmt.else_stmts = std::move(else_stmts);
        }
      }
      stmt.data = std::move(if_stmt);
    } else if (match(TokenType::KeywordFor)) {
      if (!check(TokenType::Identifier)) {
        error_at_current("expected variable name after FOR");
        return false;
      }
      Token var_tok = advance();
      consume(TokenType::Eq, "expected '=' after FOR variable");
      auto start = parse_expression();
      if (!start) return false;
      consume(TokenType::KeywordTo, "expected TO after FOR start");
      auto end = parse_expression();
      if (!end) return false;
      std::unique_ptr<Expr> step;
      if (match(TokenType::KeywordStep)) {
        step = parse_expression();
        if (!step) return false;
      } else {
        step = std::make_unique<Expr>();
        step->data = NumberExpr{1.0};
      }
      ForStmt for_stmt;
      for_stmt.var = var_tok.lexeme;
      for_stmt.start = std::move(start);
      for_stmt.end = std::move(end);
      for_stmt.step = std::move(step);
      stmt.data = std::move(for_stmt);
    } else if (match(TokenType::KeywordNext)) {
      if (!check(TokenType::Identifier)) {
        error_at_current("expected variable name after NEXT");
        return false;
      }
      Token var_tok = advance();
      NextStmt next_stmt;
      next_stmt.var = var_tok.lexeme;
      stmt.data = std::move(next_stmt);
    } else if (match(TokenType::KeywordGosub)) {
      consume(TokenType::Number, "expected line number after GOSUB");
      GosubStmt gosub_stmt;
      gosub_stmt.target_line =
          static_cast<std::int32_t>(tokens_[current_ - 1].number);
      stmt.data = std::move(gosub_stmt);
    } else if (match(TokenType::KeywordReturn)) {
      stmt.data = ReturnStmt{};
    } else if (match(TokenType::KeywordEnd)) {
      stmt.data = EndStmt{};
    } else if (match(TokenType::KeywordStop)) {
      stmt.data = StopStmt{};
    } else if (match(TokenType::KeywordRem)) {
      // REM: skip rest of line
      while (!is_at_end() && !check(TokenType::Newline) &&
             !check(TokenType::Colon)) {
        advance();
      }
      continue; // don't add REM as a statement
    } else {
      error_at_current("unknown statement");
      return false;
    }

    program_.lines.emplace_back(line_number, std::move(stmt));

    if (match(TokenType::Colon)) continue;
    break;
  }

  // Consume trailing newline.
  match(TokenType::Newline);
  return true;
}

// Parse a statement without a leading line number (for IF/THEN blocks).
std::unique_ptr<Statement> Parser::parse_statement_no_line() {
  auto stmt = std::make_unique<Statement>();
  stmt->line_number = -1;

  if (check(TokenType::KeywordPrint)) {
    advance();
    PrintStmt print;
    while (!is_at_end() && !check(TokenType::Newline) &&
           !check(TokenType::Colon) && !check(TokenType::KeywordElse)) {
      auto expr = parse_expression();
      if (!expr) return nullptr;
      print.args.push_back(std::move(expr));
      if (!match(TokenType::Comma) && !match(TokenType::Semicolon)) break;
    }
    stmt->data = std::move(print);
  } else if (match(TokenType::KeywordGoto)) {
    consume(TokenType::Number, "expected line number after GOTO");
    GotoStmt g;
    g.target_line = static_cast<std::int32_t>(tokens_[current_ - 1].number);
    stmt->data = std::move(g);
  } else if (match(TokenType::KeywordLet)) {
    consume(TokenType::Identifier, "expected variable name after LET");
    LetStmt let;
    let.var = tokens_[current_ - 1].lexeme;
    consume(TokenType::Eq, "expected '=' after variable name");
    let.value = parse_expression();
    stmt->data = std::move(let);
  } else if (match(TokenType::KeywordEnd)) {
    stmt->data = EndStmt{};
  } else if (match(TokenType::KeywordStop)) {
    stmt->data = StopStmt{};
  } else {
    error_at_current("expected statement");
    return nullptr;
  }

  return stmt;
}

std::unique_ptr<Expr> Parser::parse_expression() {
  return parse_relational();
}

std::unique_ptr<Expr> Parser::parse_relational() {
  auto left = parse_additive();
  if (!left) return nullptr;

  while (match(TokenType::Eq) || match(TokenType::Ne) ||
         match(TokenType::Lt) || match(TokenType::Le) ||
         match(TokenType::Gt) || match(TokenType::Ge)) {
    TokenType op = tokens_[current_ - 1].type;
    auto right = parse_additive();
    if (!right) return nullptr;
    auto e = std::make_unique<Expr>();
    e->data = BinaryExpr{std::move(left), op, std::move(right)};
    left = std::move(e);
  }
  return left;
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
  auto left = parse_unary();
  if (!left) return nullptr;

  while (match(TokenType::Star) || match(TokenType::Slash)) {
    TokenType op = tokens_[current_ - 1].type;
    auto right = parse_unary();
    if (!right) return nullptr;
    auto e = std::make_unique<Expr>();
    e->data = BinaryExpr{std::move(left), op, std::move(right)};
    left = std::move(e);
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_unary() {
  if (match(TokenType::Minus)) {
    auto operand = parse_unary();
    if (!operand) return nullptr;
    auto e = std::make_unique<Expr>();
    // Unary minus: 0 - operand
    e->data = BinaryExpr{std::make_unique<Expr>(), TokenType::Minus,
                         std::move(operand)};
    std::get<BinaryExpr>(e->data).left->data = NumberExpr{0.0};
    return e;
  }
  if (match(TokenType::Plus)) {
    return parse_unary();
  }
  return parse_primary();
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
  if (check(TokenType::Identifier)) {
    Token t = advance();
    auto e = std::make_unique<Expr>();
    e->data = VariableExpr{t.lexeme};
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
