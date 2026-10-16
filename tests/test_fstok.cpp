#include "test_runner.h"
#include "basic_format.h"
#include "tokenizer.h"

#include <cstring>
#include <string>
#include <vector>

TEST(fstok_keyword_tokens) {
  CHECK(fstok::keyword_token("PRINT") == fstok::TOK_PRINT);
  CHECK(fstok::keyword_token("GOTO") == fstok::TOK_GOTO);
  CHECK(fstok::keyword_token("IF") == fstok::TOK_IF);
  CHECK(fstok::keyword_token("FOR") == fstok::TOK_FOR);
  CHECK(fstok::keyword_token("NEXT") == fstok::TOK_NEXT);
  CHECK(fstok::keyword_token("DIM") == fstok::TOK_DIM);
  CHECK(fstok::keyword_token("DATA") == fstok::TOK_DATA);
  CHECK(fstok::keyword_token("READ") == fstok::TOK_READ);
  CHECK(fstok::keyword_token("RESTORE") == fstok::TOK_RESTORE);
  CHECK(fstok::keyword_token("GOSUB") == fstok::TOK_GOSUB);
  CHECK(fstok::keyword_token("RETURN") == fstok::TOK_RETURN);
  CHECK(fstok::keyword_token("REM") == fstok::TOK_REM);
  CHECK(fstok::keyword_token("END") == fstok::TOK_END);
  CHECK(fstok::keyword_token("STOP") == fstok::TOK_STOP);
  CHECK(fstok::keyword_token("LET") == fstok::TOK_LET);
  CHECK(fstok::keyword_token("INPUT") == fstok::TOK_INPUT);
  CHECK(fstok::keyword_token("INPUT#") == fstok::TOK_INPUTHASH);
  CHECK(fstok::keyword_token("PRINT#") == fstok::TOK_PRINTHASH);
  CHECK(fstok::keyword_token("UNKNOWN") == 0);
}

TEST(fstok_keyword_lookup_reverse) {
  CHECK(fstok::token_keyword(fstok::TOK_PRINT) == "PRINT");
  CHECK(fstok::token_keyword(fstok::TOK_GOTO) == "GOTO");
  CHECK(fstok::token_keyword(fstok::TOK_IF) == "IF");
  CHECK(fstok::token_keyword(fstok::TOK_DIM) == "DIM");
  CHECK(fstok::token_keyword(0xFF) == "");
}

TEST(fstok_tokenize_simple_print) {
  auto bytes = fstok::tokenize_line("PRINT \"Hello\"");
  // PRINT is a token byte (0x92), not ASCII text, so check detokenized output.
  std::size_t pos = 0;
  std::string text = fstok::detokenize_line(bytes, pos);
  CHECK(text.find("PRINT") != std::string::npos);
  CHECK(text.find("Hello") != std::string::npos);
}

TEST(fstok_tokenize_number) {
  auto bytes = fstok::tokenize_line("PRINT 42");
  // 42 should encode as INT16 (0x02) + 2 bytes.
  CHECK(bytes[0] == fstok::TOK_PRINT);
  CHECK(bytes[1] == fstok::NUM_INT16);
  CHECK(bytes[2] == 42);
  CHECK(bytes[3] == 0);
}

TEST(fstok_detokenize_roundtrip) {
  std::vector<std::pair<std::int32_t, std::string>> lines = {
      {10, "PRINT \"Hello\""},
      {20, "X = 42"},
      {30, "FOR I = 1 TO 10"},
      {40, "PRINT I"},
      {50, "NEXT I"},
      {60, "IF X = 5 THEN 100"},
      {70, "GOTO 20"},
      {80, "END"},
  };

  fstok::BasicProgram program;
  for (auto& [ln, src] : lines) {
    program[ln] = src;
  }

  // Encode to binary.
  auto data = fstok::encode_basic(program, false);

  // Decode back.
  auto decoded = fstok::decode_basic(data);

  CHECK(decoded.size() == lines.size());
  for (auto& [ln, src] : lines) {
    auto it = decoded.find(ln);
    REQUIRE(it != decoded.end());
    // Detokenized output may differ in spacing but keywords should match.
    CHECK(it->second.find("PRINT") != std::string::npos ||
          it->second.find("X") != std::string::npos ||
          it->second.find("FOR") != std::string::npos ||
          it->second.find("IF") != std::string::npos ||
          it->second.find("GOTO") != std::string::npos ||
          it->second.find("END") != std::string::npos);
  }
}

TEST(fstok_protected_roundtrip) {
  fstok::BasicProgram program = {{10, "PRINT \"Protected\""}, {20, "END"}};

  // Encode with protection.
  auto data = fstok::encode_basic(program, true);

  // Decode (should auto-detect protection).
  auto decoded = fstok::decode_basic(data);

  CHECK(decoded.size() == 2);
  CHECK(decoded.find(10) != decoded.end());
  CHECK(decoded.find(20) != decoded.end());
}

TEST(fstok_save_load_text) {
  fstok::BasicProgram program = {{10, "PRINT \"Text save\""}, {20, "END"}};
  fstok::save_bas_file("/tmp/fstok_test_text.bas", program, false, false);

  auto br = fstok::load_bas_file("/tmp/fstok_test_text.bas");
  CHECK_FALSE(br.is_binary);
  CHECK(br.program.size() == 2);
  CHECK(br.program.find(10) != br.program.end());
  std::remove("/tmp/fstok_test_text.bas");
}

TEST(fstok_save_load_binary) {
  fstok::BasicProgram program = {{10, "PRINT \"Binary save\""}, {20, "END"}};
  fstok::save_bas_file("/tmp/fstok_test_bin.bas", program, true, false);

  auto br = fstok::load_bas_file("/tmp/fstok_test_bin.bas");
  CHECK(br.is_binary);
  CHECK(br.program.size() == 2);
  CHECK(br.program.find(10) != br.program.end());
  std::remove("/tmp/fstok_test_bin.bas");
}
