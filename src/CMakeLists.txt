cmake_minimum_required(VERSION 3.0.0)
project(ChessAI)

include_directories(.)
add_subdirectory(board)
add_subdirectory(game)
add_subdirectory(move_generator)
add_subdirectory(move_selector)
add_subdirectory(search)


