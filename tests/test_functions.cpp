#include "test_runner.h"
#include "e2e_util.h"

// ---------- Built-in functions ----------

TEST(fn_abs_sgn) {
  auto r = e2e::run(
      "10 PRINT ABS(-5)\n20 PRINT SGN(-9)\n30 PRINT SGN(0)\n"
      "40 PRINT SGN(9)\n50 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 5 \n-1 \n 0 \n 1 \n");
}

TEST(fn_int_fix) {
  auto r = e2e::run(
      "10 PRINT INT(3.7)\n20 PRINT INT(-3.7)\n30 PRINT FIX(3.7)\n"
      "40 PRINT FIX(-3.7)\n50 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 3 \n-4 \n 3 \n-3 \n");
}

TEST(fn_sqr) {
  auto r = e2e::run("10 PRINT SQR(16)\n20 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 4 \n");
}

TEST(fn_sqr_negative_errors) {
  auto r = e2e::run("10 PRINT SQR(-1)\n20 END");
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 5") != std::string::npos);
}

TEST(fn_trig) {
  auto r = e2e::run(
      "10 PRINT SIN(0)\n20 PRINT COS(0)\n30 PRINT ATN(0)\n40 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 0 \n 1 \n 0 \n");
}

TEST(fn_log_exp) {
  auto r = e2e::run("10 PRINT LOG(1)\n20 PRINT EXP(0)\n30 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 0 \n 1 \n");
}

TEST(fn_log_domain_errors) {
  auto r = e2e::run("10 PRINT LOG(0)\n20 END");
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 5") != std::string::npos);
}

TEST(fn_cint_csng_cdbl) {
  auto r = e2e::run(
      "10 PRINT CINT(3.7)\n20 PRINT CINT(-3.7)\n30 PRINT CDBL(5)\n40 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 4 \n-4 \n 5 \n");
}

TEST(fn_cint_overflow) {
  auto r = e2e::run("10 PRINT CINT(40000)\n20 END");
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 6") != std::string::npos);
}

TEST(fn_len) {
  auto r = e2e::run("10 PRINT LEN(\"HELLO\")\n20 PRINT LEN(\"\")\n30 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 5 \n 0 \n");
}

TEST(fn_left_right_mid) {
  auto r = e2e::run(
      "10 PRINT LEFT$(\"HELLO\",2)\n20 PRINT RIGHT$(\"HELLO\",2)\n"
      "30 PRINT MID$(\"HELLO\",2,3)\n40 PRINT MID$(\"HELLO\",3)\n50 END");
  REQUIRE(r.ran);
  CHECK(r.output == "HE\nLO\nELL\nLLO\n");
}

TEST(fn_instr) {
  auto r = e2e::run(
      "10 PRINT INSTR(\"HELLO\",\"LL\")\n"
      "20 PRINT INSTR(3,\"ABCABC\",\"B\")\n"
      "30 PRINT INSTR(\"ABC\",\"Z\")\n40 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 3 \n 5 \n 0 \n");
}

TEST(fn_val_str) {
  auto r = e2e::run(
      "10 PRINT VAL(\"12.5xyz\")\n20 PRINT VAL(\"  -7.25\")\n"
      "30 PRINT STR$(42)\n40 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 12.5 \n-7.25 \n 42\n");
}

TEST(fn_chr_asc) {
  auto r = e2e::run(
      "10 PRINT CHR$(65)\n20 PRINT ASC(\"A\")\n30 PRINT ASC(\"\")\n40 END");
  REQUIRE(r.ran);
  CHECK(r.output == "A\n 65 \n 0 \n");
}

TEST(fn_space_string) {
  auto r = e2e::run(
      "10 PRINT \"[\" + SPACE$(3) + \"]\"\n"
      "20 PRINT \"[\" + STRING$(3,\"*\") + \"]\"\n"
      "30 PRINT \"[\" + STRING$(2,65) + \"]\"\n40 END");
  REQUIRE(r.ran);
  CHECK(r.output == "[   ]\n[***]\n[AA]\n");
}

TEST(fn_case_hex_oct) {
  auto r = e2e::run(
      "10 PRINT UCASE$(\"abc\")\n20 PRINT LCASE$(\"ABC\")\n"
      "30 PRINT HEX$(255)\n40 PRINT OCT$(8)\n50 END");
  REQUIRE(r.ran);
  CHECK(r.output == "ABC\nabc\nFF\n10\n");
}

TEST(fn_spc) {
  auto r = e2e::run("10 PRINT \"[\" + SPC(3) + \"]\"\n20 END");
  REQUIRE(r.ran);
  CHECK(r.output == "[   ]\n");
}

TEST(fn_tab) {
  // TAB(5) at column 1 emits 4 spaces, then "X" lands at column 5.
  auto r = e2e::run("10 PRINT TAB(5); \"X\"\n20 END");
  REQUIRE(r.ran);
  CHECK(r.output == "    X\n");
}

TEST(fn_fre_pos_csrlin) {
  auto r = e2e::run(
      "10 PRINT POS(0)\n20 PRINT CSRLIN\n30 PRINT FRE(0) > 0\n40 END");
  REQUIRE(r.ran);
  // POS/CSRLIN are 0 at the start; FRE(0) > 0 is -1.
  CHECK(r.output == " 0 \n 0 \n-1 \n");
}

namespace {
class FixedRndHost : public RealHost {
public:
  double rnd(double seed) override { return seed < 0 ? 0.25 : 0.75; }
};
}  // namespace

TEST(fn_rnd) {
  FixedRndHost host;
  auto r = e2e::run_with_host(
      "10 PRINT RND(1)\n20 PRINT RND(0)\n30 PRINT RND(-1)\n40 END", host);
  REQUIRE(r.ran);
  CHECK(r.output == " .75 \n .75 \n .25 \n");
}

TEST(fn_nested) {
  auto r = e2e::run(
      "10 PRINT LEN(LEFT$(\"HELLO\",3))\n"
      "20 PRINT ABS(INT(-3.7))\n30 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 3 \n 4 \n");
}
