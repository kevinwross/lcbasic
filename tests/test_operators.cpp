#include "test_runner.h"
#include "e2e_util.h"

// ---------- Logical / bitwise operators ----------

TEST(op_and_or) {
  auto r = e2e::run(
      "10 PRINT 1 AND 2\n20 PRINT 1 OR 2\n30 PRINT 6 AND 3\n"
      "40 PRINT 6 OR 3\n50 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 0 \n 3 \n 2 \n 7 \n");
}

TEST(op_not) {
  auto r = e2e::run("10 PRINT NOT 0\n20 PRINT NOT -1\n30 END");
  REQUIRE(r.ran);
  CHECK(r.output == "-1 \n 0 \n");
}

TEST(op_xor_eqv_imp) {
  auto r = e2e::run(
      "10 PRINT 5 XOR 3\n20 PRINT 5 EQV 3\n30 PRINT 5 IMP 3\n40 END");
  REQUIRE(r.ran);
  CHECK(r.output == " 6 \n-7 \n-5 \n");
}

TEST(op_precedence) {
  // AND binds tighter than OR; NOT binds tighter than AND.
  auto r = e2e::run(
      "10 PRINT 1 OR 2 AND 3\n20 PRINT NOT 0 AND 1\n"
      "30 PRINT NOT 1 = 2\n40 END");
  REQUIRE(r.ran);
  // 1 OR (2 AND 3) = 3 ; (NOT 0) AND 1 = 1 ; NOT (1 = 2) = -1
  CHECK(r.output == " 3 \n 1 \n-1 \n");
}

TEST(op_in_if) {
  auto r = e2e::run(
      "10 IF 3 > 2 AND 1 < 2 THEN 40\n20 PRINT \"NO\"\n30 END\n"
      "40 PRINT \"YES\"\n50 END");
  REQUIRE(r.ran);
  CHECK(r.output == "YES\n");
}
