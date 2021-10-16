#pragma once

#include <vector>
#include <fmt/format.h>
#include <algorithm>

#include "board/board.hh"
#include "board/board_utils.hh"
#include "search/cache.hh"

namespace chess {


// file inc, rank inc
using Directions = std::vector<std::pair<int8_t, int8_t>>;


class MoveGenerator {

 public:
  MoveGenerator() = delete;
  MoveGenerator(const Board& b);
  
  MoveList getMovesForPiece(uint8_t file, uint8_t rank) const;
  MoveList getMovesForPlayer(Color color) const;
  MoveList getMovesForPlayer(Color color, CachePtr cache) const;

  void setCache(CachePtr cache) {
    cache_ = cache;
  }

 private:
  
  MoveList getMovesForPawn(uint8_t file, uint8_t rank, Color color) const;
  MoveList getMovesForDirs(uint8_t file, uint8_t rank, Directions dirs, Color color, bool one_step = false) const;

  MoveList filterLegalMoves(MoveList& in_list, Color color, CachePtr cache) const;
  const Board& board_;
  CachePtr cache_{nullptr};
};


}
