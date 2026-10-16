add_library(lcbasic_debug STATIC
  debugger.cpp
)

target_include_directories(lcbasic_debug PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(lcbasic_debug PUBLIC lcbasic_core)
