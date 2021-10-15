#pragma once

#include "move_generator/move_generator.hh"
#include "board/board.hh"
#include <random>

namespace chess {

class MoveSelection {

 public:
  MoveSelection(const Board& board);

  bool getMoveForPlayer(Color player, Move* move);
  bool getMoveForPlayer(Color player, MoveList moves, size_t* move_idx);

 protected:

  bool weightedSelectMove(MoveList moves, std::vector<float> weights, size_t* move); 

  const MoveGenerator& move_gen_;

  // random nonsense
  std::random_device rd_;
  std::mt19937 random_gen_;
  std::uniform_real_distribution<float> dist_;
};
}
