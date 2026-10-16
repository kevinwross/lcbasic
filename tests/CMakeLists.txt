add_executable(lcbasic_tests
  test_main.cpp
  test_lexer.cpp
  test_headless.cpp
  test_e2e.cpp
  test_fstok.cpp
)

if(HAVE_CURSES)
  target_link_libraries(lcbasic_tests PRIVATE lcbasic_core lcbasic_screen lcbasic_fstok ${CURSES_LIBRARY})
else()
  target_link_libraries(lcbasic_tests PRIVATE lcbasic_core lcbasic_screen lcbasic_fstok)
endif()

add_test(NAME lcbasic_tests COMMAND lcbasic_tests)
