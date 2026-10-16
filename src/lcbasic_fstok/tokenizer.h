#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// GW-BASIC-compatible token byte mapping. These are the high-bit-set token
// values used in tokenized .BAS files for keywords and operators.
namespace fstok {

// Token bytes for keywords (GW-BASIC-compatible subset).
enum Token : std::uint8_t {
  TOK_END       = 0x80,
  TOK_FOR       = 0x81,
  TOK_NEXT      = 0x82,
  TOK_DATA      = 0x83,
  TOK_INPUTHASH = 0x84,   // INPUT#
  TOK_INPUT     = 0x85,
  TOK_DIM       = 0x86,
  TOK_READ      = 0x87,
  TOK_LET       = 0x88,
  TOK_GOTO      = 0x89,
  TOK_RUN       = 0x8A,
  TOK_IF        = 0x8B,
  TOK_RESTORE   = 0x8C,
  TOK_GOSUB     = 0x8D,
  TOK_RETURN    = 0x8E,
  TOK_REM       = 0x8F,
  TOK_STOP      = 0x90,
  TOK_PRINTHASH = 0x91,   // PRINT#
  TOK_PRINT     = 0x92,
  TOK_CLEAR     = 0x93,
  TOK_LIST      = 0x94,
  TOK_NEW       = 0x95,
  TOK_TAB       = 0x96,
  TOK_TO        = 0x97,
  TOK_FN        = 0x98,
  TOK_SPC       = 0x99,
  TOK_THEN      = 0x9A,
  TOK_NOT       = 0x9B,
  TOK_OR        = 0x9C,
  TOK_AND       = 0x9D,
  TOK_XOR       = 0x9E,
  TOK_EQV       = 0x9F,
  TOK_QUESTION  = 0xA0,   // ? (PRINT shorthand)
  TOK_MOD       = 0xA1,

  // Numeric type markers (prefix bytes in numeric constants)
  NUM_INT16     = 0x02,
  NUM_SINGLE    = 0x04,
  NUM_DOUBLE    = 0x08,
};

// Returns the token byte for a keyword, or 0 if not a keyword.
std::uint8_t keyword_token(const std::string& word);

// Returns the keyword string for a token byte, or empty if not a keyword.
std::string token_keyword(std::uint8_t tok);

// Tokenize a single text line into raw bytes (no line number / offset header).
// Returns the tokenized byte stream for the statement portion of a line.
std::vector<std::uint8_t> tokenize_line(const std::string& text);

// Detokenize a byte stream back into text. Stops at 0x00 line terminator.
std::string detokenize_line(const std::vector<std::uint8_t>& bytes,
                            std::size_t& pos);

}  // namespace fstok
