#pragma once

#include <iostream>
#include <string>
#include <vector>

// Minimal test harness so tests run without an external framework.
#define TEST(name)                                                  \
  static void test_##name();                                        \
  struct TestRegistrar_##name {                                     \
    TestRegistrar_##name() {                                        \
      ::test_registry().push_back({#name, test_##name});            \
    }                                                               \
  };                                                                \
  static TestRegistrar_##name registrar_##name;                     \
  static void test_##name()

#define CHECK(cond)                                                 \
  do {                                                              \
    if (!(cond)) {                                                  \
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__           \
                << ": " #cond "\n";                                 \
      ++::test_failures();                                          \
      return;                                                       \
    }                                                               \
  } while (0)

#define CHECK_FALSE(cond) CHECK(!(cond))

#define REQUIRE(cond) CHECK(cond)

struct TestEntry { const char* name; void (*fn)(); };

inline std::vector<TestEntry>& test_registry() {
  static std::vector<TestEntry> registry;
  return registry;
}

inline int& test_failures() {
  static int failures = 0;
  return failures;
}

inline int run_all_tests() {
  int total_failures = 0;
  for (const auto& entry : test_registry()) {
    test_failures() = 0;  // reset per test
    std::cout << "RUN " << entry.name << std::flush;
    entry.fn();
    if (test_failures() > 0) {
      total_failures += test_failures();
      std::cout << " FAIL\n";
    } else {
      std::cout << " ok\n";
    }
  }
  return total_failures == 0 ? 0 : 1;
}
