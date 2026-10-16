add_executable(lcbasic_tests
  test_main.cpp
  test_lexer.cpp
  test_headless.cpp
  test_e2e.cpp
)

if(HAVE_CURSES)
  target_link_libraries(lcbasic_tests PRIVATE lcbasic_core lcbasic_screen ${CURSES_LIBRARY})
else()
  target_link_libraries(lcbasic_tests PRIVATE lcbasic_core lcbasic_screen)
endif()

add_test(NAME lcbasic_tests COMMAND lcbasic_tests)
