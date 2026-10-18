#include "test_runner.h"
#include "e2e_util.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {
class ScriptedHost : public RealHost {
public:
  explicit ScriptedHost(std::vector<std::string> lines)
    : lines_(std::move(lines)) {}
  std::string input_line() override {
    if (idx_ < lines_.size()) return lines_[idx_++];
    return "";
  }
private:
  std::vector<std::string> lines_;
  std::size_t idx_ = 0;
};
}  // namespace

// ---------- Case-insensitivity ----------

TEST(ext_lowercase_keywords) {
  auto r = e2e::run("10 print \"hi\"\n20 end");
  REQUIRE(r.ran);
  CHECK(r.output == "hi\n");
}

TEST(ext_mixed_case_keywords) {
  auto r = e2e::run("10 PrInT \"hi\"\n20 EnD");
  REQUIRE(r.ran);
  CHECK(r.output == "hi\n");
}

TEST(ext_case_insensitive_variables) {
  auto r = e2e::run("10 A = 1\n20 a = 2\n30 PRINT A\n40 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 2 \n");
}

// ---------- Comments / shorthand ----------

TEST(ext_apostrophe_comment) {
  auto r = e2e::run("10 ' a comment\n20 PRINT \"OK\"\n30 END");
  REQUIRE(r.ran);
  CHECK(r.output == "OK\n");
}

TEST(ext_rem_consumes_colon) {
  auto r = e2e::run("10 REM foo: PRINT \"X\"\n20 PRINT \"Y\"\n30 END");
  REQUIRE(r.ran);
  CHECK(r.output == "Y\n");
}

TEST(ext_question_print) {
  auto r = e2e::run("10 ? \"HI\"\n20 END");
  REQUIRE(r.ran);
  CHECK(r.output == "HI\n");
}

// ---------- Arithmetic operators ----------

TEST(ext_exponent) {
  auto r = e2e::run(
      "10 PRINT 2 ^ 3\n20 PRINT 2 ^ 3 ^ 2\n30 PRINT -2 ^ 2\n40 END");
  REQUIRE(r.ran);
  // 2^3=8, (2^3)^2=64 (left-assoc), -(2^2)=-4
  CHECK(r.output == " 8 \n 64 \n-4 \n");
}

TEST(ext_integer_division_and_mod) {
  auto r = e2e::run(
      "10 PRINT 7 \\ 2\n20 PRINT 7 MOD 2\n30 PRINT 10 \\ 3\n"
      "40 PRINT 10 MOD 3\n50 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 3 \n 1 \n 3 \n 1 \n");
}

TEST(ext_arithmetic_precedence) {
  // ^ > unary - > * / > \ > MOD > + -
  auto r = e2e::run("10 PRINT 2 + 3 * 4 ^ 2\n20 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 50 \n");
}

TEST(ext_integer_division_by_zero) {
  auto r = e2e::run("10 PRINT 7 \\ 0\n20 END");
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 11") != std::string::npos);
}

// ---------- DIM / DATA ----------

TEST(ext_dim_expression_bound) {
  auto r = e2e::run(
      "10 N = 3\n20 DIM A(N)\n30 A(1) = 7\n40 A(3) = 9\n"
      "50 PRINT A(1); A(3)\n60 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 7  9 \n");
}

TEST(ext_dim_redefinition_errors) {
  auto r = e2e::run("10 DIM A(3)\n20 DIM A(4)\n30 END");
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 10") != std::string::npos);
}

TEST(ext_dim_multiple_arrays) {
  // A single DIM statement declaring many arrays (including multi-dimensional
  // ones), as used by real GW-BASIC programs.
  auto r = e2e::run(
      "330 DIM G(8,8),C(9,2),K(3,3),N(3),Z(8,8),D(8),EORD(8)\n"
      "340 G(1,1) = 5\n350 C(9,2) = 7\n360 N(3) = 9\n"
      "370 PRINT G(1,1); C(9,2); N(3)\n380 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 5  7  9 \n");
}

TEST(ext_data_unquoted_strings) {
  auto r = e2e::run(
      "10 DATA HELLO,WORLD\n20 READ A$,B$\n30 PRINT A$;B$\n40 END");
  REQUIRE(r.ran);
  CHECK(r.output == "HELLOWORLD\n");
}

TEST(ext_data_negative_number) {
  auto r = e2e::run("10 DATA -5,10\n20 READ A,B\n30 PRINT A; B\n40 END");
  REQUIRE(r.ran);
  CHECK(r.output == "-5  10 \n");
}

TEST(ext_restore_line) {
  auto r = e2e::run(
      "10 DATA 1\n20 DATA 2\n30 RESTORE 20\n40 READ X\n50 PRINT X\n60 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 2 \n");
}

// ---------- SWAP ----------

TEST(ext_swap_strings) {
  auto r = e2e::run(
      "10 A$=\"X\"\n20 B$=\"Y\"\n30 SWAP A$,B$\n40 PRINT A$;B$\n50 END");
  REQUIRE(r.ran);
  CHECK(r.output == "YX\n");
}

TEST(ext_swap_array_elements) {
  auto r = e2e::run(
      "10 DIM A(3)\n20 A(1)=1\n30 A(2)=2\n40 SWAP A(1),A(2)\n"
      "50 PRINT A(1);A(2)\n60 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 2  1 \n");
}

// ---------- RANDOMIZE ----------

TEST(ext_randomize) {
  auto r = e2e::run("10 RANDOMIZE 1\n20 PRINT \"OK\"\n30 END");
  REQUIRE(r.ran);
  CHECK(r.output == "OK\n");
}

// ---------- INPUT prompt comma ----------

TEST(ext_input_prompt_comma) {
  ScriptedHost host({"42"});
  auto r = e2e::run_with_host(
      "10 INPUT \"Enter\", X\n20 PRINT X\n30 END", host);
  REQUIRE(r.ran);
  CHECK(r.output == "Enter?  42 \n");
}

// ---------- File I/O ----------

TEST(ext_print_file_separators) {
  std::string path = "/tmp/lcbasic_pf_sep_test.txt";
  std::remove(path.c_str());
  auto r = e2e::run(
      "10 OPEN \"" + path + "\" FOR OUTPUT AS #1\n"
      "20 PRINT #1, \"A\", \"B\"\n30 PRINT #1, \"C\"; \"D\"\n"
      "40 CLOSE #1\n50 END");
  REQUIRE(r.ran);
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  std::remove(path.c_str());
  CHECK(ss.str() == "A B\nCD\n");
}

TEST(ext_input_file_multiple) {
  std::string path = "/tmp/lcbasic_if_multi_test.txt";
  {
    std::ofstream f(path);
    f << "10,20\n30,40\n";
  }
  auto r = e2e::run(
      "10 OPEN \"" + path + "\" FOR INPUT AS #1\n"
      "20 INPUT #1, A, B\n30 PRINT A; B\n40 CLOSE #1\n50 END");
  std::remove(path.c_str());
  REQUIRE(r.ran);
  CHECK(r.output == " 10  20 \n");
}

TEST(ext_line_input_file) {
  std::string path = "/tmp/lcbasic_lif_test.txt";
  {
    std::ofstream f(path);
    f << "hello, world\n";
  }
  auto r = e2e::run(
      "10 OPEN \"" + path + "\" FOR INPUT AS #1\n"
      "20 LINE INPUT #1, A$\n30 PRINT \"[\" + A$ + \"]\"\n"
      "40 CLOSE #1\n50 END");
  std::remove(path.c_str());
  REQUIRE(r.ran);
  CHECK(r.output == "[hello, world]\n");
}

TEST(ext_write_file) {
  std::string path = "/tmp/lcbasic_write_test.txt";
  std::remove(path.c_str());
  auto r = e2e::run(
      "10 OPEN \"" + path + "\" FOR OUTPUT AS #1\n"
      "20 WRITE #1, \"A\", 42\n30 CLOSE #1\n40 END");
  REQUIRE(r.ran);
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  std::remove(path.c_str());
  CHECK(ss.str().find("\"A\"") != std::string::npos);
  CHECK(ss.str().find("42") != std::string::npos);
  CHECK(ss.str().back() == '\n');
}

// ---------- STOP / CONT ----------

TEST(ext_stop_cont) {
  auto program = e2e::parse_source(
      "10 PRINT \"A\"\n20 STOP\n30 PRINT \"B\"\n40 END");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());

  HeadlessScreen screen(25, 80);
  RealHost host;
  Vm vm(screen, host);
  vm.load(compiler.code(), compiler.constants(), compiler.line_table(),
          compiler.variable_table(), compiler.variable_names(),
          compiler.data_values(), compiler.fn_descs());

  CHECK_FALSE(vm.run());   // stopped
  CHECK(vm.stopped());
  screen.refresh();
  CHECK(screen.last_output() == "A\nBreak in 20\n");

  CHECK(vm.run());         // continue to completion
  CHECK_FALSE(vm.stopped());
  screen.refresh();
  CHECK(screen.last_output() == "A\nBreak in 20\nB\n");
}

// ---------- DEF FN still works after the recursion refactor ----------

TEST(ext_def_fn_called_twice) {
  auto r = e2e::run(
      "10 DEF FN F(X) = X + X\n20 PRINT FN F(3); FN F(4)\n30 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 6  8 \n");
}

// ---------- INKEY$ / DATE$ / TIME$ / INPUT$ ----------

namespace {
class KeyScreen : public HeadlessScreen {
public:
  explicit KeyScreen(int key) : key_(key) {}
  std::optional<int> get_key(bool /*blocking*/) override {
    if (key_ < 0) return std::nullopt;
    int k = key_;
    key_ = -1;
    return k;
  }
private:
  int key_;
};
}  // namespace

TEST(ext_inkey) {
  auto program = e2e::parse_source("10 A$ = INKEY$\n20 PRINT A$\n30 END");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());
  KeyScreen screen('A');
  RealHost host;
  Vm vm(screen, host);
  vm.load(compiler.code(), compiler.constants(), compiler.line_table(),
          compiler.variable_table(), compiler.variable_names(),
          compiler.data_values(), compiler.fn_descs());
  REQUIRE(vm.run());
  screen.refresh();
  CHECK(screen.last_output() == "A\n");
}

TEST(ext_date_time) {
  auto r = e2e::run("10 PRINT LEN(DATE$)\n20 PRINT LEN(TIME$)\n30 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 10 \n 8 \n");
}

TEST(ext_input_dollar_file) {
  std::string path = "/tmp/lcbasic_inputdollar_test.txt";
  {
    std::ofstream f(path);
    f << "HELLO";
  }
  auto r = e2e::run(
      "10 OPEN \"" + path + "\" FOR INPUT AS #1\n"
      "20 A$ = INPUT$(3, #1)\n30 PRINT A$\n40 CLOSE #1\n50 END");
  std::remove(path.c_str());
  REQUIRE(r.ran);
  CHECK(r.output == "HEL\n");
}

TEST(ext_key_event) {
  auto program = e2e::parse_source(
      "10 ON KEY(1) GOSUB 100\n"
      "20 KEY(1) ON\n"
      "30 FOR I = 1 TO 3\n40 NEXT I\n"
      "50 KEY(1) OFF\n60 PRINT \"DONE\"\n70 END\n"
      "100 PRINT \"KEY\"\n110 RETURN");
  REQUIRE(program != nullptr);
  Compiler compiler(*program);
  REQUIRE(compiler.compile());
  KeyScreen screen('A');
  RealHost host;
  Vm vm(screen, host);
  vm.load(compiler.code(), compiler.constants(), compiler.line_table(),
          compiler.variable_table(), compiler.variable_names(),
          compiler.data_values(), compiler.fn_descs());
  REQUIRE(vm.run());
  screen.refresh();
  std::string out = screen.last_output();
  CHECK(out.find("KEY") != std::string::npos);
  CHECK(out.find("DONE") != std::string::npos);
}

// ---------- Compatibility constructs exercised by startrek.bas ----------

TEST(ext_more_than_256_variables) {
  // Variable indices used to be uint8_t, so the 257th variable collided with
  // an earlier one (and a string could land on a numeric slot).
  std::string src;
  for (int i = 0; i < 300; ++i) {
    src += std::to_string(10 * (i + 1)) + " A" + std::to_string(i) + " = " +
           std::to_string(i) + "\n";
  }
  src += "10000 B$ = \"OK\"\n";
  src += "10010 PRINT B$\n";
  src += "10020 PRINT A0; A299\n";
  src += "10030 END\n";
  auto r = e2e::run(src);
  REQUIRE(r.ran);
  CHECK(r.output == "OK\n 0  299 \n");
}

TEST(ext_nested_if) {
  auto r = e2e::run(
      "10 A = 1 : B = 2\n"
      "20 IF A = 1 THEN IF B = 2 THEN 50\n"
      "30 PRINT \"NO\"\n40 END\n"
      "50 PRINT \"YES\"\n60 END");
  REQUIRE(r.ran);
  CHECK(r.output == "YES\n");
}

TEST(ext_let_array_assignment) {
  auto r = e2e::run(
      "10 DIM C(2,2)\n20 LET C(1,2) = 42\n30 PRINT C(1,2)\n40 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 42 \n");
}

TEST(ext_leading_dot_number) {
  auto r = e2e::run(
      "10 IF .98 > .9 THEN PRINT \"YES\"\n20 PRINT .5\n30 END");
  REQUIRE(r.ran);
  CHECK(r.output == "YES\n .5 \n");
}

TEST(ext_input_prompt_ansi) {
  ScriptedHost host({"hello"});
  auto r = e2e::run_with_host(
      "10 INPUT PROMPT \"CMD\" : A$\n20 PRINT A$\n30 END", host);
  REQUIRE(r.ran);
  CHECK(r.output == "CMDhello\n");
}

TEST(ext_goto_rem_line) {
  auto r = e2e::run(
      "10 GOTO 30\n20 PRINT \"NO\"\n30 REM JUMP HERE\n"
      "40 PRINT \"OK\"\n50 END");
  REQUIRE(r.ran);
  CHECK(r.output == "OK\n");
}

TEST(ext_then_array_assignment) {
  auto r = e2e::run(
      "10 DIM D(3)\n20 D(1) = -5\n30 IF D(1) < 0 THEN D(1) = 0\n"
      "40 PRINT D(1)\n50 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 0 \n");
}

TEST(ext_then_on_goto) {
  auto r = e2e::run(
      "10 Z = 2\n20 IF Z > 0 THEN ON Z GOTO 40, 60\n"
      "30 PRINT \"NO\"\n40 PRINT \"A\"\n50 END\n60 PRINT \"B\"\n70 END");
  REQUIRE(r.ran);
  CHECK(r.output == "B\n");
}

TEST(ext_then_return) {
  auto r = e2e::run(
      "10 GOSUB 100\n20 PRINT \"DONE\"\n30 END\n"
      "100 IF 1 = 1 THEN RETURN\n110 PRINT \"NO\"\n120 RETURN");
  REQUIRE(r.ran);
  CHECK(r.output == "DONE\n");
}

TEST(ext_if_then_colon_is_conditional) {
  // GW-BASIC: everything after THEN (including colons) is conditional.
  auto r = e2e::run(
      "10 K$ = \"2\"\n"
      "20 IF K$ = \"1\" THEN GOSUB 100: GOTO 500\n"
      "30 PRINT \"FELL THROUGH\"\n40 END\n"
      "100 PRINT \"SUB\"\n110 RETURN\n"
      "500 PRINT \"JUMPED\"\n510 END");
  REQUIRE(r.ran);
  CHECK(r.output == "FELL THROUGH\n");
}

TEST(ext_if_then_gosub_runs) {
  auto r = e2e::run(
      "10 K$ = \"1\"\n"
      "20 IF K$ = \"1\" THEN GOSUB 100: GOTO 500\n"
      "30 PRINT \"FELL\"\n40 END\n"
      "100 PRINT \"SUB\"\n110 RETURN\n"
      "500 PRINT \"JUMPED\"\n510 END");
  REQUIRE(r.ran);
  CHECK(r.output == "SUB\nJUMPED\n");
}

TEST(ext_print_using_numeric) {
  auto r = e2e::run(
      "10 PRINT USING \"###\"; 5\n"
      "20 PRINT USING \"###\"; 42\n"
      "30 END");
  REQUIRE(r.ran);
  CHECK(r.output == "  5\n 42\n");
}

TEST(ext_print_using_string) {
  auto r = e2e::run(
      "10 PRINT USING \"\\      \\\"; \"AB\"\n"
      "20 PRINT USING \"\\      \\\"; \"ABCDEFGH\"\n"
      "30 END");
  REQUIRE(r.ran);
  CHECK(r.output == "AB      \nABCDEFGH\n");
}

TEST(ext_eof_builtin) {
  std::string path = "/tmp/lcbasic_eof_test.txt";
  {
    std::ofstream f(path);
    f << "one\ntwo\n";
  }
  auto r = e2e::run(
      "10 OPEN \"" + path + "\" FOR INPUT AS #1\n"
      "20 IF EOF(1) THEN 60\n"
      "30 LINE INPUT #1, L$\n"
      "40 PRINT L$\n"
      "50 GOTO 20\n"
      "60 CLOSE #1\n"
      "70 END");
  std::remove(path.c_str());
  REQUIRE(r.ran);
  CHECK(r.output == "one\ntwo\n");
}

TEST(ext_dos_path_separators) {
  std::string dir = "/tmp/lcbasic_path_test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  {
    std::ofstream f(dir + "/data.dat");
    f << "hello\n";
  }
  auto r = e2e::run(
      "10 OPEN \"/tmp/lcbasic_path_test\\data.dat\" FOR INPUT AS #1\n"
      "20 LINE INPUT #1, A$\n30 PRINT A$\n40 CLOSE #1\n50 END");
  std::filesystem::remove_all(dir);
  REQUIRE(r.ran);
  CHECK(r.output == "hello\n");
}
