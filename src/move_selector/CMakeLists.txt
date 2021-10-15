add_library(move_selection move_selection.cc)
target_link_libraries(move_selection
  board
  move_gen)


