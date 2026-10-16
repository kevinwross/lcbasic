set(HAVE_CURSES FALSE)
find_package(Curses QUIET)
if(CURSES_FOUND AND CURSES_LIBRARY AND CURSES_INCLUDE_PATH)
  set(HAVE_CURSES TRUE)
endif()

if(HAVE_CURSES)
  add_library(lcbasic_screen STATIC
    headless_screen.cpp
    stdout_screen.cpp
    ansi_screen.cpp
  )
  target_link_libraries(lcbasic_screen PUBLIC ${CURSES_LIBRARY})
else()
  message(STATUS "Curses not found; building headless screen only (no ANSI backend)")
  add_library(lcbasic_screen STATIC
    headless_screen.cpp
    stdout_screen.cpp
  )
endif()

target_include_directories(lcbasic_screen PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/src/lcbasic_core
)

if(HAVE_CURSES)
  target_include_directories(lcbasic_screen PUBLIC ${CURSES_INCLUDE_PATH})
endif()
