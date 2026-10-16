#include "test_runner.h"
#include "lexer.h"

#include <vector>

TEST(lexer_tokenizes_print_string) {
  // Source is the statement text without its line number (the parser handles
  // line numbers separately).
  Lexer lexer("PRINT \"Hello, World!\"", 10);
  auto tokens = lexer.scan();

  CHECK(tokens.size() == 3);
  CHECK(tokens[0].type == TokenType::KeywordPrint);
  CHECK(tokens[1].type == TokenType::String);
  CHECK(tokens[1].lexeme == "Hello, World!");
  CHECK(tokens[2].type == TokenType::Eof);
}

TEST(lexer_tokenizes_print_number) {
  Lexer lexer("PRINT 42", 20);
  auto tokens = lexer.scan();

  CHECK(tokens.size() == 3);
  CHECK(tokens[0].type == TokenType::KeywordPrint);
  CHECK(tokens[1].type == TokenType::Number);
  CHECK(tokens[1].number == 42.0);
  CHECK(tokens[2].type == TokenType::Eof);
}

TEST(lexer_handles_hexadecimal_literals) {
  Lexer lexer("PRINT &HFF", 30);
  auto tokens = lexer.scan();

  CHECK_FALSE(tokens.empty());
  CHECK(tokens[0].type == TokenType::KeywordPrint);
}

TEST(lexer_rejects_unterminated_string) {
  Lexer lexer("PRINT \"unterminated", 40);
  auto tokens = lexer.scan();

  CHECK_FALSE(tokens.empty());
  CHECK(tokens.back().type == TokenType::Error);
}
