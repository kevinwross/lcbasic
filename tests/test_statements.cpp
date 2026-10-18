#include "test_runner.h"
#include "e2e_util.h"

#include <string>
#include <vector>

namespace {
// A host that returns queued lines for INPUT statements.
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

// ---------- Missing statements ----------

TEST(stmt_while_wend) {
  auto r = e2e::run(
      "10 I = 1\n20 WHILE I <= 3\n30 PRINT I\n40 I = I + 1\n50 WEND\n"
      "60 PRINT \"DONE\"\n70 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 1 \n 2 \n 3 \nDONE\n");
}

TEST(stmt_nested_while) {
  auto r = e2e::run(
      "10 I=1\n20 WHILE I<=2\n30 J=1\n40 WHILE J<=2\n50 PRINT I*10+J\n"
      "60 J=J+1\n70 WEND\n80 I=I+1\n90 WEND\n100 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 11 \n 12 \n 21 \n 22 \n");
}

TEST(stmt_while_false_skips) {
  auto r = e2e::run(
      "10 I = 5\n20 WHILE I < 3\n30 PRINT \"NO\"\n40 WEND\n"
      "50 PRINT \"AFTER\"\n60 END");
  REQUIRE(r.ran);
  CHECK(r.output == "AFTER\n");
}

TEST(stmt_on_goto) {
  auto r = e2e::run(
      "10 X=2\n20 ON X GOTO 100,200\n30 END\n100 PRINT \"ONE\"\n110 END\n"
      "200 PRINT \"TWO\"\n210 END");
  REQUIRE(r.ran);
  CHECK(r.output == "TWO\n");
}

TEST(stmt_on_goto_out_of_range) {
  auto r = e2e::run(
      "10 X=9\n20 ON X GOTO 100,200\n30 PRINT \"FELLTHROUGH\"\n40 END\n"
      "100 PRINT \"ONE\"\n110 END\n200 PRINT \"TWO\"\n210 END");
  REQUIRE(r.ran);
  CHECK(r.output == "FELLTHROUGH\n");
}

TEST(stmt_on_gosub) {
  auto r = e2e::run(
      "10 X=1\n20 ON X GOSUB 100,200\n30 PRINT \"BACK\"\n40 END\n"
      "100 PRINT \"SUB1\"\n110 RETURN\n200 PRINT \"SUB2\"\n210 RETURN");
  REQUIRE(r.ran);
  CHECK(r.output == "SUB1\nBACK\n");
}

TEST(stmt_swap) {
  auto r = e2e::run("10 A=1\n20 B=2\n30 SWAP A,B\n40 PRINT A; B\n50 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 2  1 \n");
}

TEST(stmt_erase) {
  auto r = e2e::run(
      "10 DIM A(5)\n20 A(1)=9\n30 ERASE A\n40 DIM A(5)\n50 PRINT A(1)\n"
      "60 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 0 \n");
}

TEST(stmt_clear) {
  auto r = e2e::run("10 A = 5\n20 CLEAR\n30 PRINT A\n40 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 0 \n");
}

TEST(stmt_error_statement) {
  auto r = e2e::run("10 ERROR 5\n20 PRINT \"NO\"\n30 END");
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 5") != std::string::npos);
}

TEST(stmt_on_error_goto_zero_disables) {
  auto r = e2e::run(
      "10 ON ERROR GOTO 100\n20 ON ERROR GOTO 0\n30 X = 1/0\n40 END\n"
      "100 PRINT \"HANDLED\"\n110 END");
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 11") != std::string::npos);
}

TEST(stmt_mid_assign) {
  auto r = e2e::run(
      "10 A$=\"HELLO\"\n20 MID$(A$,2,3)=\"XYZ\"\n30 PRINT A$\n40 END");
  REQUIRE(r.ran);
  CHECK(r.output == "HXYZO\n");
}

TEST(stmt_mid_assign_no_length) {
  auto r = e2e::run(
      "10 A$=\"HELLO\"\n20 MID$(A$,3)=\"PP\"\n30 PRINT A$\n40 END");
  REQUIRE(r.ran);
  CHECK(r.output == "HEPPO\n");
}

TEST(stmt_lprint) {
  auto r = e2e::run("10 LPRINT \"PRINTED\"\n20 END");
  REQUIRE(r.ran);
  CHECK(r.output == "PRINTED\n");
}

TEST(stmt_screen_ops_parse) {
  auto r = e2e::run(
      "10 CLS\n20 LOCATE 1,1\n30 COLOR 7,0\n40 SCREEN 0\n50 WIDTH 80\n"
      "60 PRINT \"OK\"\n70 END");
  REQUIRE(r.ran);
  CHECK(r.output == "OK\n");
}

TEST(stmt_graphics_sound_stubs_parse) {
  auto r = e2e::run(
      "10 PSET (1,2)\n20 PRESET (3,4)\n30 CIRCLE (5,5),10\n"
      "40 PAINT (1,1)\n50 DRAW \"R10\"\n60 SOUND 440,10\n70 BEEP\n"
      "80 PALETTE 1,2\n90 WINDOW (0,0)-(1,1)\n100 VIEW (0,0)-(1,1)\n"
      "110 PRINT \"OK\"\n120 END");
  REQUIRE(r.ran);
  CHECK(r.output == "OK\n");
}

TEST(stmt_input_numeric) {
  ScriptedHost host({"21"});
  auto r = e2e::run_with_host(
      "10 INPUT X\n20 PRINT X * 2\n30 END", host);
  REQUIRE(r.ran);
  CHECK(r.output == "?  42 \n");
}

TEST(stmt_input_prompt) {
  ScriptedHost host({"Kevin"});
  auto r = e2e::run_with_host(
      "10 INPUT \"Enter name: \"; N$\n20 PRINT \"Hi \"; N$\n30 END", host);
  REQUIRE(r.ran);
  CHECK(r.output == "Enter name: Hi Kevin\n");
}

TEST(stmt_input_multiple) {
  ScriptedHost host({"3, 4"});
  auto r = e2e::run_with_host(
      "10 INPUT A, B\n20 PRINT A + B\n30 END", host);
  REQUIRE(r.ran);
  CHECK(r.output == "?  7 \n");
}

TEST(stmt_line_input) {
  ScriptedHost host({"hello, world"});
  auto r = e2e::run_with_host(
      "10 LINE INPUT A$\n20 PRINT \"[\" + A$ + \"]\"\n30 END", host);
  REQUIRE(r.ran);
  CHECK(r.output == "[hello, world]\n");
}

TEST(stmt_line_input_prompt) {
  ScriptedHost host({"hi there"});
  auto r = e2e::run_with_host(
      "10 LINE INPUT \"Say: \"; A$\n20 PRINT A$\n30 END", host);
  REQUIRE(r.ran);
  CHECK(r.output == "Say: hi there\n");
}

TEST(stmt_input_array_element) {
  ScriptedHost host({"42"});
  auto r = e2e::run_with_host(
      "10 DIM A(3)\n20 INPUT A(2)\n30 PRINT A(2)\n40 END", host);
  REQUIRE(r.ran);
  CHECK(r.output == "?  42 \n");
}

TEST(stmt_input_reprompt) {
  ScriptedHost host({"abc", "21"});
  auto r = e2e::run_with_host(
      "10 INPUT X\n20 PRINT X\n30 END", host);
  REQUIRE(r.ran);
  CHECK(r.output == "? ?Redo from start\n 21 \n");
}

TEST(stmt_input_reprompt_gives_up) {
  ScriptedHost host({"x", "y", "z", "w", "v", "u"});
  auto r = e2e::run_with_host(
      "10 INPUT X\n20 PRINT X\n30 END", host);
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 13") != std::string::npos);
}

// ---------- Random-access files ----------

TEST(stmt_random_file_roundtrip) {
  std::string path = "/tmp/lcbasic_random_test.dat";
  std::remove(path.c_str());
  auto r = e2e::run(
      "10 OPEN \"" + path + "\" FOR RANDOM AS #1 LEN=20\n"
      "20 FIELD #1, 10 AS N$, 10 AS V$\n"
      "30 LSET N$ = \"ALICE\"\n40 LSET V$ = \"42\"\n50 PUT #1, 1\n"
      "60 LSET N$ = \"BOB\"\n70 LSET V$ = \"99\"\n80 PUT #1, 2\n"
      "90 GET #1, 1\n100 PRINT \"[\" + N$ + \"][\" + V$ + \"]\"\n"
      "110 GET #1, 2\n120 PRINT \"[\" + N$ + \"][\" + V$ + \"]\"\n"
      "130 CLOSE #1\n140 END");
  std::remove(path.c_str());
  REQUIRE(r.ran);
  CHECK(r.output == "[ALICE     ][42        ]\n[BOB       ][99        ]\n");
}

TEST(stmt_lset_pads_left) {
  std::string path = "/tmp/lcbasic_lset_test.dat";
  std::remove(path.c_str());
  auto r = e2e::run(
      "10 OPEN \"" + path + "\" FOR RANDOM AS #1 LEN=10\n"
      "20 FIELD #1, 10 AS A$\n30 LSET A$ = \"HI\"\n"
      "40 PRINT \"[\" + A$ + \"]\"\n50 CLOSE #1\n60 END");
  std::remove(path.c_str());
  REQUIRE(r.ran);
  CHECK(r.output == "[HI        ]\n");
}

TEST(stmt_rset_pads_right) {
  std::string path = "/tmp/lcbasic_rset_test.dat";
  std::remove(path.c_str());
  auto r = e2e::run(
      "10 OPEN \"" + path + "\" FOR RANDOM AS #1 LEN=10\n"
      "20 FIELD #1, 10 AS A$\n30 RSET A$ = \"HI\"\n"
      "40 PRINT \"[\" + A$ + \"]\"\n50 CLOSE #1\n60 END");
  std::remove(path.c_str());
  REQUIRE(r.ran);
  CHECK(r.output == "[        HI]\n");
}
