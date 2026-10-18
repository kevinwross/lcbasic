# Local development build for lcbasic (g++ / make).
# CI uses CMake; this Makefile is a convenience for rapid iteration.

CXX ?= g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -g -O0
LDFLAGS := -lncurses

CORE_SRC := src/lcbasic_core/value.cpp \
            src/lcbasic_core/host.cpp \
            src/lcbasic_core/number_format.cpp \
            src/lcbasic_core/builtins.cpp \
            src/lcbasic_core/lexer.cpp \
            src/lcbasic_core/parser.cpp \
            src/lcbasic_core/compiler.cpp \
            src/lcbasic_core/vm.cpp \
            src/lcbasic_fstok/tokenizer.cpp \
            src/lcbasic_fstok/basic_format.cpp

SCREEN_SRC := src/lcbasic_screen/headless_screen.cpp \
              src/lcbasic_screen/stdout_screen.cpp \
              src/lcbasic_screen/ansi_screen.cpp

CORE_INC := -Isrc/lcbasic_core
SCREEN_INC := -Isrc/lcbasic_screen

.PHONY: all clean test run

all: build/lcbasic build/lcbasic_tests

build/lcbasic: $(CORE_SRC) $(SCREEN_SRC) src/lcbasic_cli/main.cpp | build
	$(CXX) $(CXXFLAGS) $(CORE_INC) $(SCREEN_INC) \
		$(CORE_SRC) $(SCREEN_SRC) src/lcbasic_cli/main.cpp \
		-o $@ $(LDFLAGS)

build/lcbasic_tests: $(CORE_SRC) $(SCREEN_SRC) tests/test_lexer.cpp \
                     tests/test_headless.cpp tests/test_e2e.cpp | build
	$(CXX) $(CXXFLAGS) $(CORE_INC) $(SCREEN_INC) \
		$(CORE_SRC) $(SCREEN_SRC) \
		tests/test_lexer.cpp tests/test_headless.cpp tests/test_e2e.cpp \
		-o $@ $(LDFLAGS)

build:
	mkdir -p build

test: build/lcbasic_tests
	./build/lcbasic_tests

clean:
	rm -rf build
