add_executable(lcbasic_tests
  test_main.cpp
  test_lexer.cpp
  test_headless.cpp
  test_e2e.cpp
  test_fstok.cpp
  test_debugger.cpp
  test_dap.cpp
  test_bugs.cpp
  test_statements.cpp
  test_functions.cpp
)

if(HAVE_CURSES)
  target_link_libraries(lcbasic_tests PRIVATE lcbasic_core lcbasic_screen lcbasic_fstok lcbasic_debug lcbasic_dap ${CURSES_LIBRARY})
else()
  target_link_libraries(lcbasic_tests PRIVATE lcbasic_core lcbasic_screen lcbasic_fstok lcbasic_debug lcbasic_dap)
endif()

add_test(NAME lcbasic_tests COMMAND lcbasic_tests)
