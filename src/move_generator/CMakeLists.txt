add_library(move_gen move_generator.cc)
target_link_libraries(move_gen 
  board
  fmt)
