#pragma once

#include "board/board.hh"
#include <vector>
#include <string>
#include <chrono>
#include <unordered_map>
#include <array>

namespace chess {


struct Node {
  
  Board board;
  const Color player;
  const Move last_move;

  Node* parent = nullptr;

  size_t expand_count{0};
  float value{0};

  MoveList unexplored_children;
  std::vector<Node> children;

  Node(const Board b, const Color p, const Move m):
    board(b),
    player(p),
    last_move(m)
  {}

  void printStats() {
    fmt::print("Tree Size: {} Nodes\n", treeSize());
    fmt::print("Tree Depth: {}\n", treeDepth());
  }

  size_t treeDepth() {
    return treeDepthHelper() - 1;
  }

  size_t treeDepthHelper() {
    size_t max_child_depth = 0;
    for(auto& c : children) {
      size_t child_depth = c.treeDepthHelper();
      if(child_depth > max_child_depth)
        max_child_depth = child_depth;
    }

    return 1 + max_child_depth;
  }

  size_t treeSize() {
    size_t count = 1;

    for(auto& c : children) {
      count += c.treeSize();
    }

    return count;
  }

  using TimeMap = std::unordered_map<size_t, size_t>;

  void compareHashes();

  void compareHashesHelper(TimeMap& sdbm_time, size_t& sdbm_collisions,
                           TimeMap& djb2_time, size_t& djb2_collisions);


  void generateDotFile(std::string out_fname, int max_depth = -1);

  std::vector<std::string> generateDotHelper(int max_depth, int& node_idx, float uct_val) const;
};


class MCTS {

 public:

  MCTS(int time_limit_ms);

  Move uctSearch(const Board& board, const Color player);

  Node* treePolicy(Node* n);

  Node* expand(Node* n);

  Node* bestChild(Node* n);

  float defaultPolicy(Node* n);

  void backPropagate(Node* n, const float value);

 private:
  int time_limit_ms_;
};


}


namespace std {

template <>
struct hash<chess::Node>
{
  size_t operator()(const chess::Node& node) const {
    // XOR with 31 bits followed by a 0 or 1
    // This guarantees that the hash is different for different colors 
    return std::hash<chess::Board>{}(node.board) ^ node.player;
  }
};

}
