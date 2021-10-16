#include "move_selector/move_selection.hh"
#include "move_generator/move_generator.hh"

#include <algorithm>
#include <unordered_map>

namespace chess {

MoveSelection::MoveSelection(const Board& board):
  move_gen_(board),
  random_gen_(rd_())
{
  std::random_device rd;
  
}

bool MoveSelection::getMoveForPlayer(Color player, Move* move) {
  auto moves = move_gen_.getMovesForPlayer(player, cache_);

  std::vector<float> weights;
  weights.assign(moves.size(), 1);
  size_t idx;
  bool result = weightedSelectMove(moves, weights, &idx);
  if(!result) return false;

  *move = moves[idx];

  return true;
}

bool MoveSelection::getMoveForPlayer(Color player, MoveList moves, size_t* move_idx) {
  std::vector<float> weights;
  weights.assign(moves.size(), 1);
  
  size_t idx;
  bool result = weightedSelectMove(moves, weights, &idx);
  
  if(!result) return false;

  *move_idx = idx;

  return true;
}

// Note that this edits weights but i don't care
bool MoveSelection::weightedSelectMove(MoveList moves,
        std::vector<float> weights, 
        size_t* index) {

  float sum = 0;

  // Normalize the vector
  for(auto w : weights) {
    sum += w;
  }

  for(auto& w : weights)
    w /= sum;
  
  float rand_num = dist_(random_gen_);

  float run_weight_sum = 0;
  size_t i = 0;
  for(auto w : weights) {
    run_weight_sum += w;
    if(run_weight_sum > rand_num) {
      *index = i;
      return true;
    }
    ++i;
  }
  return false;
}

}

/*

<test of uniformity>

int main() {

  chess::Board b("./board/config/starting_position");
  chess::MoveSelection move_select(b);

  chess::Move m;
  std::unordered_map<std::string, int> move_freq;

  for(int i = 0; i < 1000000; ++i) {
    move_select.getMoveForPlayer(chess::Color::WHITE, &m);
    std::string mstr = b.moveToAlgebraicNotation(m);
    if(move_freq.find(mstr) == move_freq.end()) {
      move_freq.emplace(mstr, 0);
    }

    move_freq.at(mstr) += 1;
  }

  chess::MoveGenerator move_gen(b);
  
  auto list = move_gen.getMovesForPlayer(chess::Color::WHITE);
  
  for(auto& m : list) {
    std::string mstr = b.moveToAlgebraicNotation(m);
    std::cout << mstr << ": " 
              << move_freq.at(mstr) << std::endl;
  
  }
  

}

*/
