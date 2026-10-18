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
struct ArrayRefExpr {
  std::string name;
  std::vector<std::unique_ptr<Expr>> indices;
};
struct BinaryExpr {
  std::unique_ptr<Expr> left;
  TokenType op;
  std::unique_ptr<Expr> right;
};
struct UnaryExpr {
  TokenType op;
  std::unique_ptr<Expr> operand;
};
struct FnCallExpr {
  std::string name;
  std::vector<std::unique_ptr<Expr>> args;
};
struct BuiltinExpr { std::string name; };
struct FunctionExpr {
  std::string name;
  std::vector<std::unique_ptr<Expr>> args;
};

struct Expr {
  std::variant<NumberExpr, StringExpr, VariableExpr, ArrayRefExpr,
               BinaryExpr, UnaryExpr, FnCallExpr, BuiltinExpr, FunctionExpr>
      data;
};

// ---------- Statements ----------

struct PrintStmt {
  std::vector<std::unique_ptr<Expr>> args;
  // Separator that follows each argument: Comma, Semicolon, or Eof (none).
  std::vector<TokenType> separators;
  bool trailing_separator = false;
};

struct LetStmt {
  std::string var;
  std::unique_ptr<Expr> value;
  std::vector<std::unique_ptr<Expr>> indices; // non-empty = array assignment
};

struct GotoStmt { std::int32_t target_line; };

struct IfStmt {
  std::unique_ptr<Expr> condition;
  std::int32_t then_line = -1;
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

struct DimStmt {
  std::string name;
  VarType type = VarType::VT_DBL;
  std::vector<std::unique_ptr<Expr>> upper_bounds;  // expressions
};

struct OptionBaseStmt { std::int32_t base; };

struct DataStmt { std::vector<Value> values; };

struct ReadTarget {
  bool is_array = false;
  std::string name;
  std::vector<std::unique_ptr<Expr>> indices; // for array refs
};
struct ReadStmt { std::vector<ReadTarget> targets; };

struct RestoreStmt {
  std::int32_t target_line = -1;  // RESTORE line; -1 = start
};

struct DefFnStmt {
  std::string name;
  std::vector<std::string> params;
  std::unique_ptr<Expr> body;
};

struct OpenStmt {
  std::int32_t filenum;
  std::string mode;   // "I", "O", "A", "R", "B"
  std::unique_ptr<Expr> filename;
  std::unique_ptr<Expr> record_len;  // optional LEN= for RANDOM
};

struct CloseStmt { std::int32_t filenum; };

struct PrintFileStmt {
  std::int32_t filenum;
  std::vector<std::unique_ptr<Expr>> args;
  std::vector<TokenType> separators;
  bool trailing_separator = false;
};

struct InputFileStmt {
  std::int32_t filenum;
  std::vector<ReadTarget> targets;
  bool line_input = false;
};

struct OnErrorStmt { std::int32_t target_line; };

struct ResumeStmt {
  bool next = false;   // RESUME NEXT
  std::int32_t target_line = -1; // RESUME line
};

// DEFINT/DEFSNG/DEFDBL/DEFSTR: default type for variables starting with
// the given letter ranges (e.g. DEFINT A-Z).
struct DefTypeStmt {
  VarType type;
  std::vector<std::pair<char, char>> ranges;  // inclusive letter ranges
};

// DOS / filesystem commands.
struct ChDirStmt { std::unique_ptr<Expr> path; };
struct MkDirStmt { std::unique_ptr<Expr> path; };
struct RmDirStmt { std::unique_ptr<Expr> path; };
struct KillStmt { std::unique_ptr<Expr> path; };
struct NameStmt {
  std::unique_ptr<Expr> old_name;
  std::unique_ptr<Expr> new_name;
};
struct FilesStmt { std::unique_ptr<Expr> pattern; };  // may be null
struct ShellStmt { std::unique_ptr<Expr> command; };  // may be null

// Event trapping.
enum class EventKind { Key, Timer, Play, Com, Pen, Strig };

struct OnEventStmt {
  EventKind kind = EventKind::Timer;
  int source = 0;              // key number / timer interval / etc.
  std::int32_t target_line = -1;
};

struct EventControlStmt {
  EventKind kind = EventKind::Timer;
  int source = 0;
  int action = 0;  // 0 = ON, 1 = OFF, 2 = STOP
};

// Keyboard input: INPUT [prompt;] var[(indices)][, ...] / LINE INPUT ...
struct InputStmt {
  std::unique_ptr<Expr> prompt;   // may be null
  bool prompt_comma = false;      // prompt followed by ',' (adds "? ")
  std::vector<ReadTarget> targets;
  bool line_input = false;
};

// WHILE expr ... WEND
struct WhileStmt { std::unique_ptr<Expr> condition; };
struct WendStmt {};

// ON expr GOTO line[, line...] / ON expr GOSUB line[, line...]
struct OnGotoStmt {
  std::unique_ptr<Expr> selector;
  std::vector<std::int32_t> targets;
  bool gosub = false;
};

// Screen operations.
struct ClsStmt {};
struct LocateStmt {
  std::unique_ptr<Expr> row;
  std::unique_ptr<Expr> col;
};
struct ColorStmt {
  std::unique_ptr<Expr> fg;
  std::unique_ptr<Expr> bg;  // may be null
};
struct ScreenStmt { std::unique_ptr<Expr> mode; };   // stubbed
struct WidthStmt { std::unique_ptr<Expr> cols; };    // stubbed

// SWAP a, b  (variables or array elements)
struct LValue {
  std::string name;
  std::vector<std::unique_ptr<Expr>> indices;
  bool is_array() const { return !indices.empty(); }
};
struct SwapStmt {
  LValue a;
  LValue b;
};

struct RandomizeStmt { std::unique_ptr<Expr> seed; };

struct WriteFileStmt {
  std::int32_t filenum;
  std::vector<std::unique_ptr<Expr>> args;
};

struct EraseStmt { std::vector<std::string> names; };
struct ClearStmt {};
struct ErrorStmt { std::unique_ptr<Expr> code; };

// MID$(target$, start[, length]) = value$
struct MidAssignStmt {
  std::string target;
  std::unique_ptr<Expr> start;
  std::unique_ptr<Expr> length;  // may be null
  std::unique_ptr<Expr> value;
};

// A parsed-but-unimplemented statement (graphics/sound/etc.).
struct StubStmt { std::string name; };

// Random-access files.
struct FieldStmt {
  std::int32_t filenum;
  std::vector<std::pair<std::unique_ptr<Expr>, std::string>> fields;
};
struct LSetStmt {
  std::string var;
  bool right = false;   // false = LSET, true = RSET
  std::unique_ptr<Expr> value;
};
struct GetPutStmt {
  std::int32_t filenum;
  std::unique_ptr<Expr> record;  // may be null
  bool put = false;
};

struct Statement {
  std::int32_t line_number;
  std::variant<PrintStmt, LetStmt, GotoStmt, IfStmt, ForStmt, NextStmt,
               GosubStmt, ReturnStmt, EndStmt, StopStmt, DimStmt,
               OptionBaseStmt, DataStmt, ReadStmt, RestoreStmt, DefFnStmt,
               OpenStmt, CloseStmt, PrintFileStmt, InputFileStmt,
               OnErrorStmt, ResumeStmt, DefTypeStmt, ChDirStmt, MkDirStmt,
               RmDirStmt, KillStmt, NameStmt, FilesStmt, ShellStmt,
               OnEventStmt, EventControlStmt, InputStmt, WhileStmt,
               WendStmt, OnGotoStmt, ClsStmt, LocateStmt, ColorStmt,
               ScreenStmt, WidthStmt, SwapStmt, EraseStmt, ClearStmt,
               ErrorStmt, MidAssignStmt, StubStmt, FieldStmt, LSetStmt,
               GetPutStmt, RandomizeStmt, WriteFileStmt>
      data;
};

struct Program {
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
  std::int32_t option_base_ = 0; // current OPTION BASE setting

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
  std::unique_ptr<Expr> parse_expression();   // precedence entry point
  std::unique_ptr<Expr> parse_imp();          // IMP (lowest)
  std::unique_ptr<Expr> parse_eqv();          // EQV
  std::unique_ptr<Expr> parse_xor();          // XOR
  std::unique_ptr<Expr> parse_or();           // OR
  std::unique_ptr<Expr> parse_and();          // AND
  std::unique_ptr<Expr> parse_not();          // NOT
  std::unique_ptr<Expr> parse_relational();   // =, <>, <, <=, >, >=
  std::unique_ptr<Expr> parse_additive();     // + -
  std::unique_ptr<Expr> parse_mod();          // MOD
  std::unique_ptr<Expr> parse_intdiv();       // integer division
  std::unique_ptr<Expr> parse_multiplicative(); // * /
  std::unique_ptr<Expr> parse_unary();        // unary -
  std::unique_ptr<Expr> parse_power();        // ^ (left-associative)
  std::unique_ptr<Expr> parse_power_operand();// operand after ^ (allows sign)
  std::unique_ptr<Expr> parse_primary();      // number, string, var, array, fn
};
