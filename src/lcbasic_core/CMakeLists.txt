add_library(lcbasic_core STATIC
  value.cpp
  host.cpp
  lexer.cpp
  parser.cpp
  compiler.cpp
  vm.cpp
)

target_include_directories(lcbasic_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(lcbasic_core PRIVATE lcbasic_screen lcbasic_fstok)
