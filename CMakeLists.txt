cmake_minimum_required(VERSION 3.18)
project(lcbasic
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

enable_testing()

# Compiler warnings
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Catch2 dependency (downloaded via FetchContent)
include(FetchContent)
FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.3
)
FetchContent_MakeAvailable(catch2)

# Optional ncurses for the ANSI terminal backend
set(HAVE_CURSES FALSE)
find_package(Curses QUIET)
if(CURSES_FOUND AND CURSES_LIBRARY AND CURSES_INCLUDE_PATH)
  set(HAVE_CURSES TRUE)
  message(STATUS "Curses found: ${CURSES_LIBRARY}")
else()
  message(STATUS "Curses not found; building headless screen only (no ANSI backend)")
endif()

add_subdirectory(src/lcbasic_core)
add_subdirectory(src/lcbasic_screen)
add_subdirectory(src/lcbasic_debug)
add_subdirectory(src/lcbasic_dap)

option(BUILD_CLI "Build the lcbasic CLI binary" ON)
if(BUILD_CLI)
  add_subdirectory(src/lcbasic_cli)
endif()

add_subdirectory(tests)
