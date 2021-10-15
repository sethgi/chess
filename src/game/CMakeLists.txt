FIND_PACKAGE(Boost COMPONENTS program_options REQUIRED)
INCLUDE_DIRECTORIES (${Boost_INCLUDE_DIR})

add_executable(game game.cc)
target_link_libraries(game
  move_gen
  board
  fmt
  ${Boost_LIBRARIES})
