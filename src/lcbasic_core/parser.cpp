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
      while (!is_at_end() && !check(TokenType::Newline)) advance();
      if (check(TokenType::Newline)) advance();
    }
  }
  return errors_.empty();
}

Program Parser::release_program() { return std::move(program_); }

bool Parser::parse_statement() {
  if (!check(TokenType::Number)) {
    error_at_current("expected line number");
    return false;
  }
  Token line_tok = advance();
  std::int32_t line_number = static_cast<std::int32_t>(line_tok.number);

  for (;;) {
    Statement stmt;
    stmt.line_number = line_number;

    if (check(TokenType::KeywordPrint)) {
      // Look ahead: PRINT #filenum is file output, otherwise regular PRINT.
      std::size_t print_save = current_;
      advance(); // consume PRINT
      bool is_print_file = check(TokenType::KeywordHash);
      current_ = print_save; // put PRINT back
      if (is_print_file) {
        match(TokenType::KeywordPrint);
        consume(TokenType::KeywordHash, "expected '#' after PRINT");
        PrintFileStmt pfile;
        consume(TokenType::Number, "expected file number after '#'");
        pfile.filenum = static_cast<std::int32_t>(tokens_[current_ - 1].number);
        if (match(TokenType::Comma)) {
        }
        do {
          auto expr = parse_expression();
          if (!expr) return false;
          pfile.args.push_back(std::move(expr));
        } while (match(TokenType::Comma) || match(TokenType::Semicolon));
        pfile.trailing_separator =
            (current_ > 0 &&
             (tokens_[current_ - 1].type == TokenType::Comma ||
              tokens_[current_ - 1].type == TokenType::Semicolon));
        stmt.data = std::move(pfile);
      } else {
        match(TokenType::KeywordPrint);
        PrintStmt print;
        if (!check(TokenType::Newline) && !check(TokenType::Colon) &&
            !is_at_end()) {
          do {
            auto expr = parse_expression();
            if (!expr) return false;
            print.args.push_back(std::move(expr));
          } while (match(TokenType::Comma) || match(TokenType::Semicolon));
          print.trailing_separator =
              (current_ > 0 &&
               (tokens_[current_ - 1].type == TokenType::Comma ||
                tokens_[current_ - 1].type == TokenType::Semicolon));
        }
        stmt.data = std::move(print);
      }
    } else if (match(TokenType::KeywordLet)) {
      consume(TokenType::Identifier, "expected variable name after LET");
      LetStmt let;
      let.var = tokens_[current_ - 1].lexeme;
      consume(TokenType::Eq, "expected '=' after variable name");
      let.value = parse_expression();
      if (!let.value) return false;
      stmt.data = std::move(let);
    } else if (check(TokenType::Identifier)) {
      Token var_tok = peek();
      std::size_t save = current_;
      advance();
      if (check(TokenType::Eq)) {
        current_ = save;
        consume(TokenType::Identifier, "expected variable name");
        consume(TokenType::Eq, "expected '=' after variable name");
        auto value = parse_expression();
        if (!value) return false;
        LetStmt let;
        let.var = var_tok.lexeme;
        let.value = std::move(value);
        stmt.data = std::move(let);
      } else if (check(TokenType::LParen)) {
        // Array assignment: arr(i) = expr
        current_ = save;
        consume(TokenType::Identifier, "expected array name");
        LetStmt let;
        let.var = tokens_[current_ - 1].lexeme;
        consume(TokenType::LParen, "expected '(' after array name");
        do {
          auto idx = parse_expression();
          if (!idx) return false;
          let.indices.push_back(std::move(idx));
        } while (match(TokenType::Comma));
        consume(TokenType::RParen, "expected ')' after array indices");
        consume(TokenType::Eq, "expected '=' after array reference");
        let.value = parse_expression();
        if (!let.value) return false;
        stmt.data = std::move(let);
      } else {
        current_ = save;
        error_at_current("unknown statement");
        return false;
      }
    } else if (match(TokenType::KeywordGoto)) {
      consume(TokenType::Number, "expected line number after GOTO");
      GotoStmt g;
      g.target_line = static_cast<std::int32_t>(tokens_[current_ - 1].number);
      stmt.data = std::move(g);
    } else if (match(TokenType::KeywordIf)) {
      auto cond = parse_expression();
      if (!cond) return false;
      consume(TokenType::KeywordThen, "expected THEN after IF condition");
      IfStmt if_stmt;
      if_stmt.condition = std::move(cond);
      if (check(TokenType::Number)) {
        Token then_line_tok = advance();
        if_stmt.then_line = static_cast<std::int32_t>(then_line_tok.number);
        if (match(TokenType::KeywordElse)) {
          consume(TokenType::Number, "expected line number after ELSE");
          if_stmt.else_line =
              static_cast<std::int32_t>(tokens_[current_ - 1].number);
        }
      } else {
        while (!is_at_end() && !check(TokenType::Newline) &&
               !check(TokenType::Colon) && !check(TokenType::KeywordElse)) {
          auto inner = parse_statement_no_line();
          if (!inner) break;
          if_stmt.then_stmts.push_back(std::move(inner));
        }
        if (match(TokenType::KeywordElse)) {
          while (!is_at_end() && !check(TokenType::Newline) &&
                 !check(TokenType::Colon)) {
            auto inner = parse_statement_no_line();
            if (!inner) break;
            if_stmt.else_stmts.push_back(std::move(inner));
          }
        }
      }
      stmt.data = std::move(if_stmt);
    } else if (match(TokenType::KeywordFor)) {
      consume(TokenType::Identifier, "expected variable name after FOR");
      ForStmt for_stmt;
      for_stmt.var = tokens_[current_ - 1].lexeme;
      consume(TokenType::Eq, "expected '=' after FOR variable");
      for_stmt.start = parse_expression();
      if (!for_stmt.start) return false;
      consume(TokenType::KeywordTo, "expected TO after FOR start");
      for_stmt.end = parse_expression();
      if (!for_stmt.end) return false;
      if (match(TokenType::KeywordStep)) {
        for_stmt.step = parse_expression();
        if (!for_stmt.step) return false;
      } else {
        for_stmt.step = std::make_unique<Expr>();
        for_stmt.step->data = NumberExpr{1.0};
      }
      stmt.data = std::move(for_stmt);
    } else if (match(TokenType::KeywordNext)) {
      consume(TokenType::Identifier, "expected variable name after NEXT");
      NextStmt next_stmt;
      next_stmt.var = tokens_[current_ - 1].lexeme;
      stmt.data = std::move(next_stmt);
    } else if (match(TokenType::KeywordGosub)) {
      consume(TokenType::Number, "expected line number after GOSUB");
      GosubStmt g;
      g.target_line = static_cast<std::int32_t>(tokens_[current_ - 1].number);
      stmt.data = std::move(g);
    } else if (match(TokenType::KeywordReturn)) {
      stmt.data = ReturnStmt{};
    } else if (match(TokenType::KeywordEnd)) {
      stmt.data = EndStmt{};
    } else if (match(TokenType::KeywordStop)) {
      stmt.data = StopStmt{};
    } else if (match(TokenType::KeywordRem)) {
      while (!is_at_end() && !check(TokenType::Newline) &&
             !check(TokenType::Colon)) {
        advance();
      }
      continue;
    } else if (match(TokenType::KeywordDim)) {
      DimStmt dim;
      consume(TokenType::Identifier, "expected array name after DIM");
      dim.name = tokens_[current_ - 1].lexeme;
      // Check for type suffix
      dim.type = deduce_var_type(dim.name);
      consume(TokenType::LParen, "expected '(' after array name");
      do {
        if (!check(TokenType::Number)) {
          error_at_current("expected numeric constant for array bound");
          return false;
        }
        Token ub_tok = advance();
        dim.upper_bounds.push_back(static_cast<std::int32_t>(ub_tok.number));
      } while (match(TokenType::Comma));
      consume(TokenType::RParen, "expected ')' after dimensions");
      stmt.data = std::move(dim);
    } else if (match(TokenType::KeywordOption)) {
      consume(TokenType::KeywordBase, "expected BASE after OPTION");
      consume(TokenType::Number, "expected number after OPTION BASE");
      OptionBaseStmt ob;
      ob.base = static_cast<std::int32_t>(tokens_[current_ - 1].number);
      option_base_ = ob.base;
      stmt.data = std::move(ob);
    } else if (match(TokenType::KeywordData)) {
      DataStmt data;
      do {
        if (check(TokenType::String)) {
          Token t = advance();
          data.values.push_back(Value(t.lexeme));
        } else if (check(TokenType::Number)) {
          Token t = advance();
          data.values.push_back(Value(t.number));
        } else {
          error_at_current("expected DATA value");
          return false;
        }
      } while (match(TokenType::Comma));
      stmt.data = std::move(data);
    } else if (match(TokenType::KeywordRead)) {
      ReadStmt read;
      do {
        ReadTarget target;
        if (check(TokenType::Identifier)) {
          Token name_tok = advance();
          target.name = name_tok.lexeme;
          if (check(TokenType::LParen)) {
            // Array element: arr(i, j)
            target.is_array = true;
            advance(); // consume '('
            do {
              auto idx = parse_expression();
              if (!idx) return false;
              target.indices.push_back(std::move(idx));
            } while (match(TokenType::Comma));
            consume(TokenType::RParen, "expected ')' after array indices");
          }
        } else {
          error_at_current("expected variable name after READ");
          return false;
        }
        read.targets.push_back(std::move(target));
      } while (match(TokenType::Comma));
      stmt.data = std::move(read);
    } else if (match(TokenType::KeywordRestore)) {
      stmt.data = RestoreStmt{};
    } else if (match(TokenType::KeywordDef)) {
      consume(TokenType::KeywordFn, "expected FN after DEF");
      DefFnStmt def;
      consume(TokenType::Identifier, "expected function name after DEF FN");
      def.name = tokens_[current_ - 1].lexeme;
      consume(TokenType::LParen, "expected '(' after function name");
      consume(TokenType::Identifier, "expected parameter name");
      def.param = tokens_[current_ - 1].lexeme;
      consume(TokenType::RParen, "expected ')' after parameter name");
      consume(TokenType::Eq, "expected '=' after DEF FN parameter");
      def.body = parse_expression();
      if (!def.body) return false;
      stmt.data = std::move(def);
    } else if (match(TokenType::KeywordOpen)) {
      OpenStmt open;
      open.filename = parse_expression();
      if (!open.filename) return false;
      consume(TokenType::KeywordFor, "expected FOR after OPEN filename");
      if (match(TokenType::KeywordOutput)) open.mode = "O";
      else if (match(TokenType::KeywordInput)) open.mode = "I";
      else if (match(TokenType::KeywordAppend)) open.mode = "A";
      else error_at_current("expected INPUT/OUTPUT/APPEND after FOR");
      consume(TokenType::KeywordAs, "expected AS after mode");
      consume(TokenType::KeywordHash, "expected '#' after AS");
      consume(TokenType::Number, "expected file number after '#'");
      open.filenum = static_cast<std::int32_t>(tokens_[current_ - 1].number);
      stmt.data = std::move(open);
    } else if (match(TokenType::KeywordClose)) {
      CloseStmt close;
      consume(TokenType::KeywordHash, "expected '#' after CLOSE");
      consume(TokenType::Number, "expected file number after '#'");
      close.filenum = static_cast<std::int32_t>(tokens_[current_ - 1].number);
      stmt.data = std::move(close);
    } else if (match(TokenType::KeywordPrint)) {
      consume(TokenType::KeywordHash, "expected '#' after PRINT");
      PrintFileStmt pfile;
      consume(TokenType::Number, "expected file number after '#'");
      pfile.filenum = static_cast<std::int32_t>(tokens_[current_ - 1].number);
      if (match(TokenType::Comma)) {
        // optional comma after filenum
      }
      do {
        auto expr = parse_expression();
        if (!expr) return false;
        pfile.args.push_back(std::move(expr));
      } while (match(TokenType::Comma) || match(TokenType::Semicolon));
      pfile.trailing_separator =
          (current_ > 0 &&
           (tokens_[current_ - 1].type == TokenType::Comma ||
            tokens_[current_ - 1].type == TokenType::Semicolon));
      stmt.data = std::move(pfile);
    } else if (match(TokenType::KeywordInput)) {
      consume(TokenType::KeywordHash, "expected '#' after INPUT");
      InputFileStmt ifile;
      consume(TokenType::Number, "expected file number after '#'");
      ifile.filenum = static_cast<std::int32_t>(tokens_[current_ - 1].number);
      if (match(TokenType::Comma)) {
        // optional comma
      }
      do {
        if (!check(TokenType::Identifier)) {
          error_at_current("expected variable name after INPUT #");
          return false;
        }
        Token var_tok = advance();
        ifile.vars.push_back(var_tok.lexeme);
      } while (match(TokenType::Comma));
      stmt.data = std::move(ifile);
    } else if (match(TokenType::KeywordOn)) {
      consume(TokenType::KeywordError, "expected ERROR after ON");
      consume(TokenType::KeywordGoto, "expected GOTO after ON ERROR");
      OnErrorStmt oerr;
      consume(TokenType::Number, "expected line number after ON ERROR GOTO");
      oerr.target_line = static_cast<std::int32_t>(tokens_[current_ - 1].number);
      stmt.data = std::move(oerr);
    } else if (match(TokenType::KeywordResume)) {
      ResumeStmt res;
      if (match(TokenType::KeywordNext)) {
        res.next = true;
      } else if (check(TokenType::Number)) {
        Token t = advance();
        res.target_line = static_cast<std::int32_t>(t.number);
      }
      stmt.data = std::move(res);
    } else {
      error_at_current("unknown statement");
      return false;
    }

    program_.lines.emplace_back(line_number, std::move(stmt));

    if (match(TokenType::Colon)) continue;
    break;
  }

  match(TokenType::Newline);
  return true;
}

std::unique_ptr<Statement> Parser::parse_statement_no_line() {
  auto stmt = std::make_unique<Statement>();
  stmt->line_number = -1;

  if (check(TokenType::KeywordPrint)) {
    // Look ahead: PRINT #filenum is file output.
    std::size_t print_save = current_;
    advance(); // PRINT
    bool is_print_file = check(TokenType::KeywordHash);
    current_ = print_save;
    if (is_print_file) {
      match(TokenType::KeywordPrint);
      consume(TokenType::KeywordHash, "expected '#' after PRINT");
      PrintFileStmt pfile;
      consume(TokenType::Number, "expected file number after '#'");
      pfile.filenum = static_cast<std::int32_t>(tokens_[current_ - 1].number);
      if (match(TokenType::Comma)) {
      }
      while (!is_at_end() && !check(TokenType::Newline) &&
             !check(TokenType::Colon) && !check(TokenType::KeywordElse)) {
        auto expr = parse_expression();
        if (!expr) return nullptr;
        pfile.args.push_back(std::move(expr));
        if (!match(TokenType::Comma) && !match(TokenType::Semicolon)) break;
      }
      stmt->data = std::move(pfile);
    } else {
      match(TokenType::KeywordPrint);
      PrintStmt print;
      while (!is_at_end() && !check(TokenType::Newline) &&
             !check(TokenType::Colon) && !check(TokenType::KeywordElse)) {
        auto expr = parse_expression();
        if (!expr) return nullptr;
        print.args.push_back(std::move(expr));
        if (!match(TokenType::Comma) && !match(TokenType::Semicolon)) break;
      }
      stmt->data = std::move(print);
    }
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
  } else if (check(TokenType::Identifier)) {
    Token var_tok = peek();
    std::size_t save = current_;
    advance();
    if (check(TokenType::Eq)) {
      current_ = save;
      consume(TokenType::Identifier, "expected variable name");
      consume(TokenType::Eq, "expected '=' after variable name");
      LetStmt let;
      let.var = var_tok.lexeme;
      let.value = parse_expression();
      stmt->data = std::move(let);
    } else {
      current_ = save;
      error_at_current("expected statement");
      return nullptr;
    }
  } else if (match(TokenType::KeywordEnd) || match(TokenType::KeywordStop)) {
    // Reuse the already-consumed token
    if (tokens_[current_ - 1].type == TokenType::KeywordEnd)
      stmt->data = EndStmt{};
    else
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
    // Check if followed by '(' -> array reference or function call
    if (check(TokenType::LParen)) {
      // Look ahead: is this a DIM'd array? For now, treat as array ref.
      std::vector<std::unique_ptr<Expr>> indices;
      advance(); // consume '('
      if (!check(TokenType::RParen)) {
        do {
          auto idx = parse_expression();
          if (!idx) return nullptr;
          indices.push_back(std::move(idx));
        } while (match(TokenType::Comma));
      }
      consume(TokenType::RParen, "expected ')' after indices");
      auto e = std::make_unique<Expr>();
      e->data = ArrayRefExpr{std::move(t.lexeme), std::move(indices)};
      return e;
    }
    auto e = std::make_unique<Expr>();
    e->data = VariableExpr{std::move(t.lexeme)};
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
