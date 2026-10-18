#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

enum class TokenType {
  // Literals
  Number,
  String,
  Identifier,

  // Keywords
  KeywordPrint,
  KeywordLet,
  KeywordIf,
  KeywordThen,
  KeywordElse,
  KeywordGoto,
  KeywordFor,
  KeywordNext,
  KeywordGosub,
  KeywordReturn,
  KeywordEnd,
  KeywordStop,
  KeywordRem,
  KeywordData,
  KeywordRead,
  KeywordRestore,
  KeywordDim,
  KeywordOption,
  KeywordBase,
  KeywordInput,
  KeywordLine,
  KeywordOn,
  KeywordError,
  KeywordResume,
  KeywordCls,
  KeywordLocate,
  KeywordColor,
  KeywordTo,
  KeywordStep,
  KeywordDef,
  KeywordFn,
  KeywordAppend,
  KeywordHash,  // #
  KeywordOpen,
  KeywordClose,
  KeywordOutput,
  KeywordAs,
  KeywordDefInt,
  KeywordDefSng,
  KeywordDefDbl,
  KeywordDefStr,
  KeywordChDir,
  KeywordMkDir,
  KeywordRmDir,
  KeywordKill,
  KeywordName,
  KeywordFiles,
  KeywordShell,
  KeywordKey,
  KeywordTimer,
  KeywordPlay,
  KeywordCom,
  KeywordPen,
  KeywordStrig,
  KeywordOff,
  KeywordWhile,
  KeywordWend,
  KeywordSwap,
  KeywordErase,
  KeywordClear,
  KeywordScreen,
  KeywordWidth,
  KeywordLprint,
  KeywordMid,
  KeywordTron,
  KeywordTroff,
  KeywordCont,
  KeywordMerge,
  KeywordField,
  KeywordGet,
  KeywordPut,
  KeywordLset,
  KeywordRset,
  KeywordPset,
  KeywordPreset,
  KeywordCircle,
  KeywordPaint,
  KeywordDraw,
  KeywordSound,
  KeywordBeep,
  KeywordPalette,
  KeywordWindow,
  KeywordView,
  KeywordRandom,
  KeywordBinary,
  KeywordRandomize,
  KeywordWrite,

  // Punctuation
  Newline,
  Colon,
  Comma,
  Semicolon,
  LParen,
  RParen,

  // Operators
  Plus,
  Minus,
  Star,
  Slash,
  Eq,
  Ne,
  Lt,
  Le,
  Gt,
  Ge,
  And,
  Or,
  Not,
  Xor,
  Eqv,
  Imp,
  Caret,       // ^
  Backslash,   // integer division
  Mod,         // MOD
  Question,    // ? (PRINT shorthand)

  Eof,
  Error,
};

struct Token {
  TokenType type;
  std::string lexeme;
  double number;      // valid when type == Number
  std::int32_t line;  // source line number this token came from
  std::int32_t col;   // column within the source line
};

// Lexes a single physical source line (without its line number) into tokens.
class Lexer {
public:
  explicit Lexer(std::string source, std::int32_t line_number);

  std::vector<Token> scan();

private:
  std::string source_;
  std::int32_t line_number_;
  std::size_t start_ = 0;
  std::size_t current_ = 0;

  char peek();
  char advance();
  bool match(char expected);
  void skip_whitespace();
  Token scan_token();
  Token scan_number();
  Token scan_string();
  Token scan_identifier_or_keyword();
  Token make_token(TokenType type, std::string lexeme);
  Token error_token(const std::string& message);
  bool is_alpha(char c) const;
  bool is_digit(char c) const;
  bool is_at_end() const;
  static TokenType keyword(const std::string& text);
};
