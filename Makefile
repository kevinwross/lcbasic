# Local development build for lcbasic (g++ / make).
# CI and the canonical build use CMake; this Makefile is a convenience.
# It builds into build-make/ so it never collides with the CMake build/.

CXX ?= g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -g -O0
LDFLAGS := -lncurses

INCLUDES := -Isrc/lcbasic_core -Isrc/lcbasic_screen \
            -Isrc/lcbasic_debug -Isrc/lcbasic_dap

CORE_SRC := src/lcbasic_core/value.cpp \
            src/lcbasic_core/host.cpp \
            src/lcbasic_core/number_format.cpp \
            src/lcbasic_core/builtins.cpp \
            src/lcbasic_core/program_loader.cpp \
            src/lcbasic_core/lexer.cpp \
            src/lcbasic_core/parser.cpp \
            src/lcbasic_core/compiler.cpp \
            src/lcbasic_core/vm.cpp

SCREEN_SRC := src/lcbasic_screen/headless_screen.cpp \
              src/lcbasic_screen/stdout_screen.cpp \
              src/lcbasic_screen/ansi_screen.cpp

DEBUG_SRC := src/lcbasic_debug/debugger.cpp

DAP_SRC := src/lcbasic_dap/json.cpp \
           src/lcbasic_dap/dap_server.cpp

TEST_SRC := tests/test_main.cpp \
            tests/test_lexer.cpp \
            tests/test_headless.cpp \
            tests/test_e2e.cpp \
            tests/test_debugger.cpp \
            tests/test_dap.cpp \
            tests/test_bugs.cpp \
            tests/test_statements.cpp \
            tests/test_functions.cpp \
            tests/test_operators.cpp

LIB_SRC := $(CORE_SRC) $(SCREEN_SRC) $(DEBUG_SRC) $(DAP_SRC)

OUT := build-make

.PHONY: all clean test

all: $(OUT)/lcbasic $(OUT)/lcbasic-dap $(OUT)/lcbasic_tests

$(OUT)/lcbasic: $(LIB_SRC) src/lcbasic_cli/main.cpp | $(OUT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIB_SRC) src/lcbasic_cli/main.cpp \
		-o $@ $(LDFLAGS)

$(OUT)/lcbasic-dap: $(LIB_SRC) src/lcbasic_dap/dap_main.cpp | $(OUT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIB_SRC) src/lcbasic_dap/dap_main.cpp \
		-o $@ $(LDFLAGS)

$(OUT)/lcbasic_tests: $(LIB_SRC) $(TEST_SRC) | $(OUT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIB_SRC) $(TEST_SRC) -o $@ $(LDFLAGS)

$(OUT):
	mkdir -p $(OUT)

test: $(OUT)/lcbasic_tests
	./$(OUT)/lcbasic_tests

clean:
	rm -rf $(OUT)
