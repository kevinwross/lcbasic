add_library(lcbasic_dap STATIC
  json.cpp
  dap_server.cpp
)

target_include_directories(lcbasic_dap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(lcbasic_dap PUBLIC
  lcbasic_core
  lcbasic_debug
  lcbasic_screen
)

add_executable(lcbasic-dap dap_main.cpp)
target_link_libraries(lcbasic-dap PRIVATE lcbasic_dap)
