#include "parser.h"
#include "builtins.h"

#include <cctype>
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

    if (check(TokenType::KeywordPrint) || check(TokenType::Question)) {
      // Look ahead: PRINT #filenum is file output, otherwise regular PRINT.
      bool is_print_file = false;
      if (check(TokenType::KeywordPrint)) {
        std::size_t print_save = current_;
        advance(); // consume PRINT
        is_print_file = check(TokenType::KeywordHash);
        current_ = print_save; // put PRINT back
      }
      if (is_print_file) {
        match(TokenType::KeywordPrint);
        consume(TokenType::KeywordHash, "expected '#' after PRINT");
        PrintFileStmt pfile;
        consume(TokenType::Number, "expected file number after '#'");
        pfile.filenum = static_cast<std::int32_t>(tokens_[current_ - 1].number);
        if (match(TokenType::Comma)) {
        }
        while (!is_at_end() && !check(TokenType::Newline) &&
               !check(TokenType::Colon)) {
          auto expr = parse_expression();
          if (!expr) return false;
          pfile.args.push_back(std::move(expr));
          if (check(TokenType::Comma) || check(TokenType::Semicolon)) {
            pfile.separators.push_back(peek().type);
            advance();
            if (is_at_end() || check(TokenType::Newline) ||
                check(TokenType::Colon)) {
              break;
            }
          } else {
            pfile.separators.push_back(TokenType::Eof);
            break;
          }
        }
        pfile.trailing_separator =
            (!pfile.separators.empty() &&
             pfile.separators.back() != TokenType::Eof);
        stmt.data = std::move(pfile);
      } else {
        advance();  // consume PRINT or '?'
        PrintStmt print;
        while (!is_at_end() && !check(TokenType::Newline) &&
               !check(TokenType::Colon)) {
          auto expr = parse_expression();
          if (!expr) return false;
          print.args.push_back(std::move(expr));
          if (check(TokenType::Comma) || check(TokenType::Semicolon)) {
            print.separators.push_back(peek().type);
            advance();
            if (is_at_end() || check(TokenType::Newline) ||
                check(TokenType::Colon)) {
              break;  // trailing separator
            }
          } else {
            print.separators.push_back(TokenType::Eof);
            break;
          }
        }
        print.trailing_separator =
            (!print.separators.empty() &&
             print.separators.back() != TokenType::Eof);
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
      // REM consumes the rest of the line (including any colons).
      while (!is_at_end() && !check(TokenType::Newline)) {
        advance();
      }
      break;
    } else if (match(TokenType::KeywordDim)) {
      // DIM supports a comma-separated list of arrays:
      //   DIM G(8,8), C(9,2), N(3)
      DimStmt dim;
      do {
        DimEntry entry;
        consume(TokenType::Identifier, "expected array name after DIM");
        entry.name = tokens_[current_ - 1].lexeme;
        entry.type = deduce_var_type(entry.name);
        consume(TokenType::LParen, "expected '(' after array name");
        do {
          auto ub = parse_expression();
          if (!ub) return false;
          entry.upper_bounds.push_back(std::move(ub));
        } while (match(TokenType::Comma));
        consume(TokenType::RParen, "expected ')' after dimensions");
        dim.arrays.push_back(std::move(entry));
      } while (match(TokenType::Comma));
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
        } else if (check(TokenType::Minus)) {
          advance();
          consume(TokenType::Number, "expected number after '-' in DATA");
          data.values.push_back(Value(-tokens_[current_ - 1].number));
        } else if (check(TokenType::Identifier)) {
          // Unquoted string literal.
          Token t = advance();
          data.values.push_back(Value(t.lexeme));
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
      RestoreStmt r;
      if (check(TokenType::Number)) {
        Token t = advance();
        r.target_line = static_cast<std::int32_t>(t.number);
      }
      stmt.data = std::move(r);
    } else if (match(TokenType::KeywordDef)) {
      consume(TokenType::KeywordFn, "expected FN after DEF");
      DefFnStmt def;
      consume(TokenType::Identifier, "expected function name after DEF FN");
      def.name = tokens_[current_ - 1].lexeme;
      consume(TokenType::LParen, "expected '(' after function name");
      do {
        consume(TokenType::Identifier, "expected parameter name");
        def.params.push_back(tokens_[current_ - 1].lexeme);
      } while (match(TokenType::Comma));
      consume(TokenType::RParen, "expected ')' after parameter list");
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
      else if (match(TokenType::KeywordRandom)) open.mode = "R";
      else if (match(TokenType::KeywordBinary)) open.mode = "B";
      else error_at_current("expected INPUT/OUTPUT/APPEND/RANDOM after FOR");
      consume(TokenType::KeywordAs, "expected AS after mode");
      consume(TokenType::KeywordHash, "expected '#' after AS");
      consume(TokenType::Number, "expected file number after '#'");
      open.filenum = static_cast<std::int32_t>(tokens_[current_ - 1].number);
      // Optional LEN= for RANDOM files (LEN is an identifier, not a keyword).
      if (check(TokenType::Identifier) &&
          tokens_[current_].lexeme.size() == 3 &&
          (tokens_[current_].lexeme == "LEN" ||
           tokens_[current_].lexeme == "len")) {
        advance();  // LEN
        consume(TokenType::Eq, "expected '=' after LEN");
        open.record_len = parse_expression();
        if (!open.record_len) return false;
      }
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
    } else if (check(TokenType::KeywordInput)) {
      advance();  // consume INPUT
      if (check(TokenType::KeywordHash)) {
        // INPUT #filenum, var[, var...]
        advance();  // consume '#'
        InputFileStmt ifile;
        consume(TokenType::Number, "expected file number after '#'");
        ifile.filenum = static_cast<std::int32_t>(tokens_[current_ - 1].number);
        if (match(TokenType::Comma)) {
          // optional comma
        }
        do {
          ReadTarget target;
          consume(TokenType::Identifier,
                  "expected variable name after INPUT #");
          target.name = tokens_[current_ - 1].lexeme;
          if (check(TokenType::LParen)) {
            target.is_array = true;
            advance();
            do {
              auto idx = parse_expression();
              if (!idx) return false;
              target.indices.push_back(std::move(idx));
            } while (match(TokenType::Comma));
            consume(TokenType::RParen, "expected ')' after indices");
          }
          ifile.targets.push_back(std::move(target));
        } while (match(TokenType::Comma));
        stmt.data = std::move(ifile);
      } else {
        // INPUT [prompt;] var[, var...]
        InputStmt in;
        if (check(TokenType::String)) {
          in.prompt = parse_expression();
          if (!in.prompt) return false;
          if (!match(TokenType::Semicolon) && !match(TokenType::Comma)) {
            error_at_current("expected ';' or ',' after INPUT prompt");
            return false;
          }
          in.prompt_comma =
              (tokens_[current_ - 1].type == TokenType::Comma);
        }
        do {
          ReadTarget target;
          consume(TokenType::Identifier,
                  "expected variable name after INPUT");
          target.name = tokens_[current_ - 1].lexeme;
          if (check(TokenType::LParen)) {
            target.is_array = true;
            advance();
            do {
              auto idx = parse_expression();
              if (!idx) return false;
              target.indices.push_back(std::move(idx));
            } while (match(TokenType::Comma));
            consume(TokenType::RParen, "expected ')' after indices");
          }
          in.targets.push_back(std::move(target));
        } while (match(TokenType::Comma));
        stmt.data = std::move(in);
      }
    } else if (match(TokenType::KeywordOn)) {
      if (match(TokenType::KeywordError)) {
        consume(TokenType::KeywordGoto, "expected GOTO after ON ERROR");
        OnErrorStmt oerr;
        consume(TokenType::Number, "expected line number after ON ERROR GOTO");
        oerr.target_line =
            static_cast<std::int32_t>(tokens_[current_ - 1].number);
        stmt.data = std::move(oerr);
      } else if (check(TokenType::KeywordKey) ||
                 check(TokenType::KeywordTimer) ||
                 check(TokenType::KeywordPlay) ||
                 check(TokenType::KeywordCom) ||
                 check(TokenType::KeywordPen) ||
                 check(TokenType::KeywordStrig)) {
        // ON KEY(n) GOSUB line / ON TIMER(n) GOSUB line / ...
        OnEventStmt ev;
        if (match(TokenType::KeywordKey)) {
          ev.kind = EventKind::Key;
          if (match(TokenType::LParen)) {
            consume(TokenType::Number, "expected key number");
            ev.source = static_cast<int>(tokens_[current_ - 1].number);
            consume(TokenType::RParen, "expected ')' after key number");
          }
        } else if (match(TokenType::KeywordTimer)) {
          ev.kind = EventKind::Timer;
          if (match(TokenType::LParen)) {
            consume(TokenType::Number, "expected timer interval");
            ev.source = static_cast<int>(tokens_[current_ - 1].number);
            consume(TokenType::RParen, "expected ')' after timer interval");
          }
        } else if (match(TokenType::KeywordPlay)) {
          ev.kind = EventKind::Play;
        } else if (match(TokenType::KeywordCom)) {
          ev.kind = EventKind::Com;
        } else if (match(TokenType::KeywordPen)) {
          ev.kind = EventKind::Pen;
        } else {
          ev.kind = EventKind::Strig;
        }
        consume(TokenType::KeywordGosub, "expected GOSUB after ON event");
        consume(TokenType::Number, "expected line number after GOSUB");
        ev.target_line =
            static_cast<std::int32_t>(tokens_[current_ - 1].number);
        stmt.data = std::move(ev);
      } else {
        // ON expr GOTO/GOSUB line[, line...]
        OnGotoStmt og;
        og.selector = parse_expression();
        if (!og.selector) return false;
        if (match(TokenType::KeywordGoto)) {
          og.gosub = false;
        } else if (match(TokenType::KeywordGosub)) {
          og.gosub = true;
        } else {
          error_at_current("expected GOTO or GOSUB after ON expression");
          return false;
        }
        do {
          consume(TokenType::Number, "expected line number");
          og.targets.push_back(
              static_cast<std::int32_t>(tokens_[current_ - 1].number));
        } while (match(TokenType::Comma));
        stmt.data = std::move(og);
      }
    } else if (match(TokenType::KeywordResume)) {
      ResumeStmt res;
      if (match(TokenType::KeywordNext)) {
        res.next = true;
      } else if (check(TokenType::Number)) {
        Token t = advance();
        res.target_line = static_cast<std::int32_t>(t.number);
      }
      stmt.data = std::move(res);
    } else if (check(TokenType::KeywordDefInt) ||
               check(TokenType::KeywordDefSng) ||
               check(TokenType::KeywordDefDbl) ||
               check(TokenType::KeywordDefStr)) {
      DefTypeStmt dt;
      TokenType kw = peek().type;
      if (kw == TokenType::KeywordDefInt) dt.type = VarType::VT_INT16;
      else if (kw == TokenType::KeywordDefSng) dt.type = VarType::VT_SGL;
      else if (kw == TokenType::KeywordDefDbl) dt.type = VarType::VT_DBL;
      else dt.type = VarType::VT_STR;
      advance();  // consume DEF* keyword
      do {
        consume(TokenType::Identifier, "expected letter after DEF");
        char lo = static_cast<char>(
            std::toupper(static_cast<unsigned char>(
                tokens_[current_ - 1].lexeme[0])));
        char hi = lo;
        if (match(TokenType::Minus)) {
          consume(TokenType::Identifier, "expected letter after '-'");
          hi = static_cast<char>(
              std::toupper(static_cast<unsigned char>(
                  tokens_[current_ - 1].lexeme[0])));
        }
        dt.ranges.push_back({lo, hi});
      } while (match(TokenType::Comma));
      stmt.data = std::move(dt);
    } else if (match(TokenType::KeywordChDir)) {
      ChDirStmt c;
      c.path = parse_expression();
      if (!c.path) return false;
      stmt.data = std::move(c);
    } else if (match(TokenType::KeywordMkDir)) {
      MkDirStmt c;
      c.path = parse_expression();
      if (!c.path) return false;
      stmt.data = std::move(c);
    } else if (match(TokenType::KeywordRmDir)) {
      RmDirStmt c;
      c.path = parse_expression();
      if (!c.path) return false;
      stmt.data = std::move(c);
    } else if (match(TokenType::KeywordKill)) {
      KillStmt c;
      c.path = parse_expression();
      if (!c.path) return false;
      stmt.data = std::move(c);
    } else if (match(TokenType::KeywordName)) {
      NameStmt n;
      n.old_name = parse_expression();
      if (!n.old_name) return false;
      consume(TokenType::KeywordAs, "expected AS in NAME");
      n.new_name = parse_expression();
      if (!n.new_name) return false;
      stmt.data = std::move(n);
    } else if (match(TokenType::KeywordFiles)) {
      FilesStmt f;
      if (!check(TokenType::Newline) && !check(TokenType::Colon) &&
          !is_at_end()) {
        f.pattern = parse_expression();
        if (!f.pattern) return false;
      }
      stmt.data = std::move(f);
    } else if (match(TokenType::KeywordShell)) {
      ShellStmt sh;
      if (!check(TokenType::Newline) && !check(TokenType::Colon) &&
          !is_at_end()) {
        sh.command = parse_expression();
        if (!sh.command) return false;
      }
      stmt.data = std::move(sh);
    } else if (check(TokenType::KeywordKey) ||
               check(TokenType::KeywordTimer) ||
               check(TokenType::KeywordPlay) ||
               check(TokenType::KeywordCom) ||
               check(TokenType::KeywordPen) ||
               check(TokenType::KeywordStrig)) {
      // KEY(n) ON/OFF/STOP, TIMER ON/OFF/STOP, etc.
      EventControlStmt ec;
      TokenType kw = peek().type;
      advance();
      if (kw == TokenType::KeywordKey) {
        ec.kind = EventKind::Key;
        if (match(TokenType::LParen)) {
          consume(TokenType::Number, "expected key number");
          ec.source = static_cast<int>(tokens_[current_ - 1].number);
          consume(TokenType::RParen, "expected ')' after key number");
        }
      } else if (kw == TokenType::KeywordTimer) {
        ec.kind = EventKind::Timer;
        if (match(TokenType::LParen)) {
          consume(TokenType::Number, "expected timer interval");
          ec.source = static_cast<int>(tokens_[current_ - 1].number);
          consume(TokenType::RParen, "expected ')' after timer interval");
        }
      } else if (kw == TokenType::KeywordPlay) {
        ec.kind = EventKind::Play;
      } else if (kw == TokenType::KeywordCom) {
        ec.kind = EventKind::Com;
      } else if (kw == TokenType::KeywordPen) {
        ec.kind = EventKind::Pen;
      } else {
        ec.kind = EventKind::Strig;
      }
      if (match(TokenType::KeywordOn)) {
        ec.action = 0;
      } else if (match(TokenType::KeywordOff)) {
        ec.action = 1;
      } else if (match(TokenType::KeywordStop)) {
        ec.action = 2;
      } else {
        error_at_current("expected ON, OFF or STOP");
        return false;
      }
      stmt.data = std::move(ec);
    } else if (check(TokenType::KeywordLine)) {
      std::size_t save = current_;
      advance();  // LINE
      if (match(TokenType::KeywordInput)) {
        if (check(TokenType::KeywordHash)) {
          // LINE INPUT #n, var$
          advance();  // '#'
          InputFileStmt ifile;
          ifile.line_input = true;
          consume(TokenType::Number, "expected file number after '#'");
          ifile.filenum =
              static_cast<std::int32_t>(tokens_[current_ - 1].number);
          if (match(TokenType::Comma)) {
          }
          do {
            ReadTarget target;
            consume(TokenType::Identifier,
                    "expected variable after LINE INPUT #");
            target.name = tokens_[current_ - 1].lexeme;
            if (check(TokenType::LParen)) {
              target.is_array = true;
              advance();
              do {
                auto idx = parse_expression();
                if (!idx) return false;
                target.indices.push_back(std::move(idx));
              } while (match(TokenType::Comma));
              consume(TokenType::RParen, "expected ')' after indices");
            }
            ifile.targets.push_back(std::move(target));
          } while (match(TokenType::Comma));
          stmt.data = std::move(ifile);
        } else {
          // LINE INPUT [prompt;] var$
          InputStmt in;
          in.line_input = true;
          if (check(TokenType::String)) {
            in.prompt = parse_expression();
            if (!in.prompt) return false;
            if (!match(TokenType::Semicolon) && !match(TokenType::Comma)) {
              error_at_current(
                  "expected ';' or ',' after LINE INPUT prompt");
              return false;
            }
            in.prompt_comma =
                (tokens_[current_ - 1].type == TokenType::Comma);
          }
          do {
            ReadTarget target;
            consume(TokenType::Identifier,
                    "expected variable after LINE INPUT");
            target.name = tokens_[current_ - 1].lexeme;
            if (check(TokenType::LParen)) {
              target.is_array = true;
              advance();
              do {
                auto idx = parse_expression();
                if (!idx) return false;
                target.indices.push_back(std::move(idx));
              } while (match(TokenType::Comma));
              consume(TokenType::RParen, "expected ')' after indices");
            }
            in.targets.push_back(std::move(target));
          } while (match(TokenType::Comma));
          stmt.data = std::move(in);
        }
      } else {
        // Graphics LINE — parsed but not implemented.
        current_ = save;
        advance();
        while (!is_at_end() && !check(TokenType::Newline) &&
               !check(TokenType::Colon)) {
          advance();
        }
        stmt.data = StubStmt{"LINE"};
      }
    } else if (match(TokenType::KeywordWhile)) {
      WhileStmt w;
      w.condition = parse_expression();
      if (!w.condition) return false;
      stmt.data = std::move(w);
    } else if (match(TokenType::KeywordWend)) {
      stmt.data = WendStmt{};
    } else if (match(TokenType::KeywordSwap)) {
      auto parse_lvalue = [&](LValue& lv) -> bool {
        consume(TokenType::Identifier, "expected variable after SWAP");
        lv.name = tokens_[current_ - 1].lexeme;
        if (match(TokenType::LParen)) {
          do {
            auto idx = parse_expression();
            if (!idx) return false;
            lv.indices.push_back(std::move(idx));
          } while (match(TokenType::Comma));
          consume(TokenType::RParen, "expected ')' after SWAP index");
        }
        return true;
      };
      SwapStmt sw;
      if (!parse_lvalue(sw.a)) return false;
      consume(TokenType::Comma, "expected ',' in SWAP");
      if (!parse_lvalue(sw.b)) return false;
      stmt.data = std::move(sw);
    } else if (match(TokenType::KeywordErase)) {
      EraseStmt e;
      do {
        consume(TokenType::Identifier, "expected array name after ERASE");
        e.names.push_back(tokens_[current_ - 1].lexeme);
      } while (match(TokenType::Comma));
      stmt.data = std::move(e);
    } else if (match(TokenType::KeywordClear)) {
      stmt.data = ClearStmt{};
    } else if (match(TokenType::KeywordRandomize)) {
      RandomizeStmt r;
      if (!check(TokenType::Newline) && !check(TokenType::Colon) &&
          !is_at_end()) {
        r.seed = parse_expression();
        if (!r.seed) return false;
      }
      stmt.data = std::move(r);
    } else if (match(TokenType::KeywordWrite)) {
      WriteFileStmt w;
      consume(TokenType::KeywordHash, "expected '#' after WRITE");
      consume(TokenType::Number, "expected file number after '#'");
      w.filenum = static_cast<std::int32_t>(tokens_[current_ - 1].number);
      if (match(TokenType::Comma)) {
        // optional comma
      }
      do {
        auto expr = parse_expression();
        if (!expr) return false;
        w.args.push_back(std::move(expr));
      } while (match(TokenType::Comma));
      stmt.data = std::move(w);
    } else if (match(TokenType::KeywordCls)) {
      stmt.data = ClsStmt{};
    } else if (match(TokenType::KeywordLocate)) {
      LocateStmt l;
      l.row = parse_expression();
      if (!l.row) return false;
      consume(TokenType::Comma, "expected ',' in LOCATE");
      l.col = parse_expression();
      if (!l.col) return false;
      stmt.data = std::move(l);
    } else if (match(TokenType::KeywordColor)) {
      ColorStmt c;
      c.fg = parse_expression();
      if (!c.fg) return false;
      if (match(TokenType::Comma)) {
        c.bg = parse_expression();
        if (!c.bg) return false;
      }
      stmt.data = std::move(c);
    } else if (match(TokenType::KeywordScreen)) {
      ScreenStmt s;
      s.mode = parse_expression();
      if (!s.mode) return false;
      while (match(TokenType::Comma)) {
        if (!parse_expression()) return false;
      }
      stmt.data = std::move(s);
    } else if (match(TokenType::KeywordWidth)) {
      WidthStmt w;
      w.cols = parse_expression();
      if (!w.cols) return false;
      stmt.data = std::move(w);
    } else if (match(TokenType::KeywordError)) {
      ErrorStmt e;
      e.code = parse_expression();
      if (!e.code) return false;
      stmt.data = std::move(e);
    } else if (match(TokenType::KeywordMid)) {
      consume(TokenType::LParen, "expected '(' after MID$");
      consume(TokenType::Identifier, "expected string variable");
      MidAssignStmt m;
      m.target = tokens_[current_ - 1].lexeme;
      consume(TokenType::Comma, "expected ',' after MID$ target");
      m.start = parse_expression();
      if (!m.start) return false;
      if (match(TokenType::Comma)) {
        m.length = parse_expression();
        if (!m.length) return false;
      }
      consume(TokenType::RParen, "expected ')' after MID$ arguments");
      consume(TokenType::Eq, "expected '=' in MID$ assignment");
      m.value = parse_expression();
      if (!m.value) return false;
      stmt.data = std::move(m);
    } else if (match(TokenType::KeywordLprint)) {
      // LPRINT behaves like PRINT (mapped to the screen for now).
      PrintStmt print;
      while (!is_at_end() && !check(TokenType::Newline) &&
             !check(TokenType::Colon)) {
        auto expr = parse_expression();
        if (!expr) return false;
        print.args.push_back(std::move(expr));
        if (check(TokenType::Comma) || check(TokenType::Semicolon)) {
          print.separators.push_back(peek().type);
          advance();
          if (is_at_end() || check(TokenType::Newline) ||
              check(TokenType::Colon)) {
            break;
          }
        } else {
          print.separators.push_back(TokenType::Eof);
          break;
        }
      }
      print.trailing_separator =
          (!print.separators.empty() &&
           print.separators.back() != TokenType::Eof);
      stmt.data = std::move(print);
    } else if (match(TokenType::KeywordField)) {
      FieldStmt f;
      consume(TokenType::KeywordHash, "expected '#' after FIELD");
      consume(TokenType::Number, "expected file number after '#'");
      f.filenum = static_cast<std::int32_t>(tokens_[current_ - 1].number);
      consume(TokenType::Comma, "expected ',' after FIELD file number");
      do {
        auto len = parse_expression();
        if (!len) return false;
        consume(TokenType::KeywordAs, "expected AS in FIELD");
        consume(TokenType::Identifier, "expected string variable in FIELD");
        f.fields.emplace_back(std::move(len), tokens_[current_ - 1].lexeme);
      } while (match(TokenType::Comma));
      stmt.data = std::move(f);
    } else if (match(TokenType::KeywordLset) ||
               match(TokenType::KeywordRset)) {
      LSetStmt l;
      l.right = (tokens_[current_ - 1].type == TokenType::KeywordRset);
      consume(TokenType::Identifier, "expected variable in LSET/RSET");
      l.var = tokens_[current_ - 1].lexeme;
      consume(TokenType::Eq, "expected '=' in LSET/RSET");
      l.value = parse_expression();
      if (!l.value) return false;
      stmt.data = std::move(l);
    } else if (match(TokenType::KeywordGet) ||
               match(TokenType::KeywordPut)) {
      GetPutStmt g;
      g.put = (tokens_[current_ - 1].type == TokenType::KeywordPut);
      consume(TokenType::KeywordHash, "expected '#' after GET/PUT");
      consume(TokenType::Number, "expected file number after '#'");
      g.filenum = static_cast<std::int32_t>(tokens_[current_ - 1].number);
      if (match(TokenType::Comma)) {
        g.record = parse_expression();
        if (!g.record) return false;
      }
      stmt.data = std::move(g);
    } else if (check(TokenType::KeywordTron) ||
               check(TokenType::KeywordTroff) ||
               check(TokenType::KeywordCont) ||
               check(TokenType::KeywordMerge) ||
               check(TokenType::KeywordPset) ||
               check(TokenType::KeywordPreset) ||
               check(TokenType::KeywordCircle) ||
               check(TokenType::KeywordPaint) ||
               check(TokenType::KeywordDraw) ||
               check(TokenType::KeywordSound) ||
               check(TokenType::KeywordBeep) ||
               check(TokenType::KeywordPalette) ||
               check(TokenType::KeywordWindow) ||
               check(TokenType::KeywordView)) {
      // Parsed but not implemented (graphics / sound / tracing). Consume the
      // rest of the statement and emit nothing.
      std::string name = peek().lexeme;
      while (!is_at_end() && !check(TokenType::Newline) &&
             !check(TokenType::Colon)) {
        advance();
      }
      stmt.data = StubStmt{name};
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
        if (check(TokenType::Comma) || check(TokenType::Semicolon)) {
          print.separators.push_back(peek().type);
          advance();
          if (is_at_end() || check(TokenType::Newline) ||
              check(TokenType::Colon) || check(TokenType::KeywordElse)) {
            break;
          }
        } else {
          print.separators.push_back(TokenType::Eof);
          break;
        }
      }
      print.trailing_separator =
          (!print.separators.empty() &&
           print.separators.back() != TokenType::Eof);
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
  return parse_imp();
}

std::unique_ptr<Expr> Parser::parse_imp() {
  auto left = parse_eqv();
  if (!left) return nullptr;
  while (match(TokenType::Imp)) {
    auto right = parse_eqv();
    if (!right) return nullptr;
    auto e = std::make_unique<Expr>();
    e->data = BinaryExpr{std::move(left), TokenType::Imp, std::move(right)};
    left = std::move(e);
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_eqv() {
  auto left = parse_xor();
  if (!left) return nullptr;
  while (match(TokenType::Eqv)) {
    auto right = parse_xor();
    if (!right) return nullptr;
    auto e = std::make_unique<Expr>();
    e->data = BinaryExpr{std::move(left), TokenType::Eqv, std::move(right)};
    left = std::move(e);
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_xor() {
  auto left = parse_or();
  if (!left) return nullptr;
  while (match(TokenType::Xor)) {
    auto right = parse_or();
    if (!right) return nullptr;
    auto e = std::make_unique<Expr>();
    e->data = BinaryExpr{std::move(left), TokenType::Xor, std::move(right)};
    left = std::move(e);
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_or() {
  auto left = parse_and();
  if (!left) return nullptr;
  while (match(TokenType::Or)) {
    auto right = parse_and();
    if (!right) return nullptr;
    auto e = std::make_unique<Expr>();
    e->data = BinaryExpr{std::move(left), TokenType::Or, std::move(right)};
    left = std::move(e);
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_and() {
  auto left = parse_not();
  if (!left) return nullptr;
  while (match(TokenType::And)) {
    auto right = parse_not();
    if (!right) return nullptr;
    auto e = std::make_unique<Expr>();
    e->data = BinaryExpr{std::move(left), TokenType::And, std::move(right)};
    left = std::move(e);
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_not() {
  if (match(TokenType::Not)) {
    auto operand = parse_not();
    if (!operand) return nullptr;
    auto e = std::make_unique<Expr>();
    e->data = UnaryExpr{TokenType::Not, std::move(operand)};
    return e;
  }
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
  auto left = parse_mod();
  if (!left) return nullptr;
  while (match(TokenType::Plus) || match(TokenType::Minus)) {
    TokenType op = tokens_[current_ - 1].type;
    auto right = parse_mod();
    if (!right) return nullptr;
    auto e = std::make_unique<Expr>();
    e->data = BinaryExpr{std::move(left), op, std::move(right)};
    left = std::move(e);
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_mod() {
  auto left = parse_intdiv();
  if (!left) return nullptr;
  while (match(TokenType::Mod)) {
    auto right = parse_intdiv();
    if (!right) return nullptr;
    auto e = std::make_unique<Expr>();
    e->data = BinaryExpr{std::move(left), TokenType::Mod, std::move(right)};
    left = std::move(e);
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_intdiv() {
  auto left = parse_multiplicative();
  if (!left) return nullptr;
  while (match(TokenType::Backslash)) {
    auto right = parse_multiplicative();
    if (!right) return nullptr;
    auto e = std::make_unique<Expr>();
    e->data =
        BinaryExpr{std::move(left), TokenType::Backslash, std::move(right)};
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
    auto operand = parse_power();
    if (!operand) return nullptr;
    auto e = std::make_unique<Expr>();
    e->data = UnaryExpr{TokenType::Minus, std::move(operand)};
    return e;
  }
  if (match(TokenType::Plus)) {
    return parse_power();
  }
  return parse_power();
}

std::unique_ptr<Expr> Parser::parse_power() {
  auto left = parse_primary();
  if (!left) return nullptr;
  // GW-BASIC exponentiation is left-associative: 2^3^2 = (2^3)^2 = 64.
  while (match(TokenType::Caret)) {
    auto right = parse_power_operand();
    if (!right) return nullptr;
    auto e = std::make_unique<Expr>();
    e->data = BinaryExpr{std::move(left), TokenType::Caret, std::move(right)};
    left = std::move(e);
  }
  return left;
}

// Operand after '^': allows a leading sign but does not consume further '^'
// (so the exponentiation loop stays left-associative).
std::unique_ptr<Expr> Parser::parse_power_operand() {
  if (match(TokenType::Minus)) {
    auto operand = parse_power_operand();
    if (!operand) return nullptr;
    auto e = std::make_unique<Expr>();
    e->data = UnaryExpr{TokenType::Minus, std::move(operand)};
    return e;
  }
  if (match(TokenType::Plus)) {
    return parse_power_operand();
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
  if (match(TokenType::KeywordFn)) {
    // FN name(arg, arg, ...)
    consume(TokenType::Identifier, "expected function name after FN");
    FnCallExpr call;
    call.name = tokens_[current_ - 1].lexeme;
    consume(TokenType::LParen, "expected '(' after function name");
    if (!check(TokenType::RParen)) {
      do {
        auto a = parse_expression();
        if (!a) return nullptr;
        call.args.push_back(std::move(a));
      } while (match(TokenType::Comma));
    }
    consume(TokenType::RParen, "expected ')' after function arguments");
    auto e = std::make_unique<Expr>();
    e->data = std::move(call);
    return e;
  }
  if (match(TokenType::KeywordTimer)) {
    auto e = std::make_unique<Expr>();
    e->data = BuiltinExpr{"TIMER"};
    return e;
  }
  if (match(TokenType::KeywordHash)) {
    // '#' marks a file number inside INPUT$(n, #f).
    return parse_primary();
  }
  if (match(TokenType::KeywordMid)) {
    // MID$(str, start[, length]) used as a function.
    consume(TokenType::LParen, "expected '(' after MID$");
    FunctionExpr fn;
    fn.name = "MID$";
    do {
      auto a = parse_expression();
      if (!a) return nullptr;
      fn.args.push_back(std::move(a));
    } while (match(TokenType::Comma));
    consume(TokenType::RParen, "expected ')' after MID$ arguments");
    auto e = std::make_unique<Expr>();
    e->data = std::move(fn);
    return e;
  }
  if (check(TokenType::Identifier)) {
    Token t = advance();
    std::string name = t.lexeme;
    if (check(TokenType::LParen)) {
      std::vector<std::unique_ptr<Expr>> args;
      advance();  // consume '('
      if (!check(TokenType::RParen)) {
        do {
          auto a = parse_expression();
          if (!a) return nullptr;
          args.push_back(std::move(a));
        } while (match(TokenType::Comma));
      }
      consume(TokenType::RParen, "expected ')' after arguments");

      if (find_builtin(name)) {
        auto e = std::make_unique<Expr>();
        e->data = FunctionExpr{name, std::move(args)};
        return e;
      }
      // Otherwise it is an array reference.
      auto e = std::make_unique<Expr>();
      e->data = ArrayRefExpr{std::move(name), std::move(args)};
      return e;
    }
    // No parentheses: zero-argument built-ins.
    if (auto b = find_builtin(name)) {
      if (*b == Builtin::Csrlin || *b == Builtin::Rnd ||
          *b == Builtin::Inkey || *b == Builtin::Date ||
          *b == Builtin::Time) {
        auto e = std::make_unique<Expr>();
        e->data = FunctionExpr{std::move(name), {}};
        return e;
      }
    }
    auto e = std::make_unique<Expr>();
    e->data = VariableExpr{std::move(name)};
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
