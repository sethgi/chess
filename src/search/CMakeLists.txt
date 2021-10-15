add_executable(search search.cc)
FIND_PACKAGE(Boost COMPONENTS program_options REQUIRED)
INCLUDE_DIRECTORIES (${Boost_INCLUDE_DIR})

target_link_libraries(search
  board
  move_gen
  move_selection
  ${Boost_LIBRARIES})


