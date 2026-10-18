#include "test_runner.h"
#include "e2e_util.h"

// ---------- List 1 correctness bugs ----------

TEST(bug_string_var_defaults_empty) {
  auto r = e2e::run("10 PRINT \"[\" + A$ + \"]\"\n20 END");
  REQUIRE(r.ran);
  CHECK(r.output == "[]\n");
}

TEST(bug_string_array_defaults_empty) {
  auto r = e2e::run("10 DIM A$(3)\n20 PRINT \"[\" + A$(1) + \"]\"\n30 END");
  REQUIRE(r.ran);
  CHECK(r.output == "[]\n");
}

TEST(bug_array_subscript_out_of_range) {
  auto r = e2e::run("10 DIM A(3)\n20 A(10) = 5\n30 PRINT A(10)\n40 END");
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 9") != std::string::npos);
}

TEST(bug_array_negative_subscript) {
  auto r = e2e::run("10 DIM A(3)\n20 PRINT A(-1)\n30 END");
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 9") != std::string::npos);
}

TEST(bug_division_by_zero) {
  auto r = e2e::run("10 PRINT 1 / 0\n20 END");
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 11") != std::string::npos);
}

TEST(bug_read_into_array) {
  auto r = e2e::run(
      "10 DIM A(3)\n20 DATA 7, 8\n30 READ A(1)\n40 READ A(2)\n"
      "50 PRINT A(1); A(2)\n60 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 7  8 \n");
}

TEST(bug_string_array_read) {
  auto r = e2e::run(
      "10 DIM A$(2)\n20 DATA \"hi\"\n30 READ A$(1)\n40 PRINT A$(1)\n50 END");
  REQUIRE(r.ran);
  CHECK(r.output == "hi\n");
}

TEST(bug_type_mismatch_store) {
  auto r = e2e::run("10 A$ = 5\n20 END");
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 13") != std::string::npos);
}

TEST(bug_type_mismatch_add) {
  auto r = e2e::run("10 PRINT \"A\" + 5\n20 END");
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 13") != std::string::npos);
}

TEST(bug_type_mismatch_compare) {
  auto r = e2e::run("10 IF \"A\" = 5 THEN 30\n20 END\n30 END");
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 13") != std::string::npos);
}

TEST(bug_integer_overflow) {
  auto r = e2e::run("10 X% = 40000\n20 END");
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 6") != std::string::npos);
}

TEST(bug_long_overflow) {
  auto r = e2e::run("10 X& = 3000000000\n20 END");
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 6") != std::string::npos);
}

TEST(bug_integer_array_coercion) {
  auto r = e2e::run(
      "10 DIM A%(2)\n20 A%(1) = 3.7\n30 PRINT A%(1)\n40 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 4 \n");
}

TEST(bug_array_type_mismatch) {
  auto r = e2e::run("10 DIM A%(2)\n20 A%(1) = \"x\"\n30 END");
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 13") != std::string::npos);
}

TEST(bug_division_by_zero_float) {
  auto r = e2e::run("10 X = 0\n20 PRINT 5 / X\n30 END");
  CHECK_FALSE(r.ran);
  CHECK(r.error.find("error 11") != std::string::npos);
}
