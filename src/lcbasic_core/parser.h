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
struct FnCallExpr {
  std::string name;
  std::vector<std::unique_ptr<Expr>> args;
};
struct BuiltinExpr { std::string name; };

struct Expr {
  std::variant<NumberExpr, StringExpr, VariableExpr, ArrayRefExpr,
               BinaryExpr, FnCallExpr, BuiltinExpr>
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
  std::vector<std::int32_t> upper_bounds; // inclusive per dimension
};

struct OptionBaseStmt { std::int32_t base; };

struct DataStmt { std::vector<Value> values; };

struct ReadTarget {
  bool is_array = false;
  std::string name;
  std::vector<std::unique_ptr<Expr>> indices; // for array refs
};
struct ReadStmt { std::vector<ReadTarget> targets; };

struct RestoreStmt {};

struct DefFnStmt {
  std::string name;
  std::vector<std::string> params;
  std::unique_ptr<Expr> body;
};

struct OpenStmt {
  std::int32_t filenum;
  std::string mode;   // "I", "O", "A"
  std::unique_ptr<Expr> filename;
};

struct CloseStmt { std::int32_t filenum; };

struct PrintFileStmt {
  std::int32_t filenum;
  std::vector<std::unique_ptr<Expr>> args;
  bool trailing_separator = false;
};

struct InputFileStmt {
  std::int32_t filenum;
  std::vector<std::string> vars;
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

// Keyboard input: INPUT [prompt;] var[, var...] / LINE INPUT [prompt;] var$
struct InputStmt {
  std::unique_ptr<Expr> prompt;   // may be null
  std::vector<std::string> vars;  // scalar variables
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

// SWAP a, b  (simple variables)
struct SwapStmt {
  std::string a;
  std::string b;
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
               ErrorStmt, MidAssignStmt, StubStmt>
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
  std::unique_ptr<Expr> parse_expression();   // relational (lowest)
  std::unique_ptr<Expr> parse_relational();   // =, <>, <, <=, >, >=
  std::unique_ptr<Expr> parse_additive();     // + -
  std::unique_ptr<Expr> parse_multiplicative(); // * /
  std::unique_ptr<Expr> parse_unary();        // NOT, -
  std::unique_ptr<Expr> parse_primary();      // number, string, var, array, (expr)
};
