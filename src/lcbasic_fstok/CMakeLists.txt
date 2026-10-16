add_library(lcbasic_fstok STATIC
  tokenizer.cpp
  basic_format.cpp
)

target_include_directories(lcbasic_fstok PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
