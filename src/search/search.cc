#include <chrono>
#include <cmath>
#include <boost/program_options.hpp>
#include <iostream>
#include <cassert>
#include <fstream>
#include <thread>

#include "search/search.hh"
#include "evaluator/evaluate.hh"
#include "move_selector/move_selection.hh"

bool do_debug = false;
bool format_verbose = false;

float exploration_constant = 0;

namespace chess {

void Node::generateDotFile(std::string out_fname, int max_depth)
{
  int node_idx = 0;
  std::vector<std::string> contents = generateDotHelper(max_depth, node_idx, -1);

  std::ofstream output_stream(out_fname);
  output_stream << "digraph search_tree {" << std::endl;
  for(const auto& s : contents)
    output_stream << "  " << s << std::endl;
  output_stream << "}" << std::endl;
  output_stream.close();
}

void Node::compareHashes() {
  TimeMap sdbm_time;
  TimeMap djb2_time;

  size_t sdbm_collisions{0};
  size_t djb2_collisions{0};

  compareHashesHelper(sdbm_time, sdbm_collisions, djb2_time, djb2_collisions);

  fmt::print("SDBM Collisions: {}, DJB2 Collisions: {}\n",
              sdbm_collisions, djb2_collisions);

  size_t sdbm_min_time{std::numeric_limits<size_t>::max()}, sdbm_max_time{0}, sdbm_total_time{0};
  for(auto& entry : sdbm_time) {
    size_t t = entry.second;
    if(t > sdbm_max_time)
      sdbm_max_time = t;
    if(t < sdbm_min_time)
      sdbm_min_time = t;
    sdbm_total_time += t;
  }
  
  size_t djb2_min_time{std::numeric_limits<size_t>::max()}, djb2_max_time{0}, djb2_total_time{0};
  for(auto& entry : djb2_time) {
    size_t t = entry.second;
    if(t > djb2_max_time)
      djb2_max_time = t;
    if(t < djb2_min_time)
      djb2_min_time = t;
    djb2_total_time += t;
  }

  fmt::print("SBDM Time -> Min: {}, Max: {}, Mean: {}\n",
             sdbm_min_time, sdbm_max_time, sdbm_total_time/sdbm_time.size());
  fmt::print("DJB2 Time -> Min: {}, Max: {}, Mean: {}\n",
             djb2_min_time, djb2_max_time, djb2_total_time/djb2_time.size());
}

void Node::compareHashesHelper(TimeMap& sdbm_time, size_t& sdbm_collisions,
                               TimeMap& djb2_time, size_t& djb2_collisions){

  const size_t num_tries = 10000;
  
  size_t hash;

  // Up First: SDBM

  // Time it (10,000x should do)
  auto start = std::chrono::steady_clock::now();
  for(int i = 0; i < num_tries; ++i) {
    hash = board.computeSDBMHash(); 
  }
  auto end = std::chrono::steady_clock::now();


  size_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count()/num_tries;
  
  if(sdbm_time.count(hash) != 0) {
    ++sdbm_collisions;
  } else {
    sdbm_time.emplace(hash, ns);
  }

  start = std::chrono::steady_clock::now();
  for(int i = 0; i < num_tries; ++i) {
    hash = board.computeDJB2Hash(); 
  }
  end = std::chrono::steady_clock::now();
  
  ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count()/num_tries;
  if(djb2_time.count(hash) != 0) {
    ++djb2_collisions;
  } else {
    djb2_time.emplace(hash, ns);
  }

  for(auto& c : children) {
    c.compareHashesHelper(sdbm_time, sdbm_collisions, djb2_time, djb2_collisions);
  }
}

// node_idx is the index of the node currently being expanded.
// We copy it for our use, then increment once per child created.
std::vector<std::string> Node::generateDotHelper(int max_depth, int& node_idx, float uct_val) const {
  if(max_depth == 0) return {}; // covers case where depth is passed as -1
  
  if(max_depth > 0) --max_depth; // don't decrement -1, that's just dumb

  int this_node_idx = node_idx;
  
  std::vector<std::string> local_list;
  
  std::string last_move_str = node_idx == 0? "ROOT" : parent->board.moveToAlgebraicNotation(last_move);
  
  std::string label = format_verbose ? fmt::format("{} (Count: {}) \n Val: {}, UCT: {}",
                                        last_move_str, expand_count, value,
                                        uct_val == -1? "inf" : std::to_string(uct_val))
                                     : last_move_str;

  std::string node_str = fmt::format("{} [label=\"{}\"]", this_node_idx, label);
  
  local_list.push_back(node_str);
 
  // Main base case is when node has no children
  for(const auto& c : children) {
    float val = c.value / c.expand_count + 
      exploration_constant * std::sqrt(2 * std::log(expand_count) / c.expand_count);
    
    ++node_idx;

    local_list.push_back(fmt::format("{}->{}", this_node_idx, node_idx));
    
    std::vector<std::string> child_list = c.generateDotHelper(max_depth, node_idx, val);
    
    local_list.insert(local_list.end(), child_list.begin(), child_list.end());
  }
  return local_list;
}

MCTS::MCTS(int time_limit_ms) : time_limit_ms_(time_limit_ms)
{}

Move MCTS::uctSearch(const Board& board, const Color player) {
  Move root_move;
  root_move.is_null = true;
  Node root_node(board, player, root_move);
  
  Node* current_node = &root_node;
  
  MoveGenerator move_gen(board);
  current_node->unexplored_children = move_gen.getMovesForPlayer(player);
 
  auto start = std::chrono::system_clock::now();
  auto end = std::chrono::system_clock::now();
  while(std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() 
        < time_limit_ms_) {

    if(do_debug)
      std::cerr << "loop" << std::endl;
  
    current_node = treePolicy(&root_node);

    // TODO: detect the case where there's actually nothing left to explore
    if(current_node == nullptr) {
      continue;
    }
    
    float value = defaultPolicy(current_node);
    backPropagate(current_node, value);
    end = std::chrono::system_clock::now();
  }
  if(do_debug)
    std::cerr << "exit" << std::endl;

  root_node.generateDotFile("graph.dot");
  root_node.printStats();
  root_node.compareHashes();

  // TODO: if current_node is nullptr, this is a problem
  Node* best_child = bestChild(&root_node);
  return best_child->last_move;
}

Node* MCTS::treePolicy(Node* n) {
  if(do_debug)
    std::cerr << "tree policy" << std::endl;
 
  Node* current_node = n;
  Node* prev_node = n;
  while(current_node->children.size() > 0 
        && current_node->unexplored_children.size() == 0) {
  
    if(do_debug)
      std::cerr << "traverse" << std::endl;
    current_node = bestChild(current_node);

    assert(current_node != prev_node);
    prev_node = current_node;
  }
  
  return expand(current_node);
}

Node* MCTS::expand(Node* n) {
  Color player = n->player;
  if(do_debug)
    std::cerr << "expand" << std::endl;
  MoveSelection selector(n->board);
  
  if(n->unexplored_children.size() == 0) return nullptr;

  size_t move_idx;
  if(!selector.getMoveForPlayer(player, n->unexplored_children, &move_idx)) return nullptr;
  
  Move m = n->unexplored_children[move_idx];
  
  n->unexplored_children.erase(n->unexplored_children.begin() + move_idx);
  Board new_board;
  
  if(!n->board.doMove(m, player, &new_board)) {
    std::cerr << "Move was illegal" << std::endl;
    std::cerr << "Requested Move: " << m.str() << std::endl;
    
    std::cerr << "COLOR: " << (player == Color::WHITE? "White" : "Black") << std::endl;

    try {
      std::cerr << "Requested Move Alg: " << n->board.moveToAlgebraicNotation(m) << std::endl;
    } catch (...) {
      std::cerr << "<can't format>" << std::endl;
    }
  
    std::cout << "Board: " << n->board << std::endl;

    throw std::runtime_error("Failed to do move");
  }
  
  n->children.emplace_back(new_board, static_cast<Color>(!player), m);
  Node& child = n->children.back();
  child.parent = n;

  // Pre-compute the possible moves
  MoveGenerator move_gen(new_board);
  MoveList moves = move_gen.getMovesForPlayer(child.player);
  child.unexplored_children = std::move(moves);

  return &n->children.back();
}

Node* MCTS::bestChild(Node* n) {
  if(do_debug)
    std::cerr << "best child" << std::endl; 
  size_t idx = 0;
  float max_val = -1*std::numeric_limits<float>::infinity();
  size_t max_idx = 0;
  for(auto& c : n->children) {
    // std::cerr << "child board: " << std::endl << c.board << std::endl;
    float val = c.value / c.expand_count + 
     exploration_constant * std::sqrt(2 * std::log(n->expand_count) / c.expand_count);
    if (val > max_val) {
      max_val = val;
      max_idx = idx;
    }
    ++idx;
  }

  Node* max_node = &(n->children[max_idx]);
  assert(max_node != n);
  assert(max_node != nullptr);

  return max_node;

}

float MCTS::defaultPolicy(Node* n) {
  if(do_debug)
    std::cerr << "default policy" << std::endl;
  Board current_board = n->board;

  MoveSelection selector(current_board);
  Evaluator eval(current_board);

  Color current_player = n->player;

  std::string nothing;
  auto evaluation = eval(current_player);

  while(evaluation.state == State::NORMAL) {
   
    Move m;

    if(!selector.getMoveForPlayer(current_player, &m)) break;
    // fmt::print("{} does: {}\n", current_player == Color::WHITE? "White" : "Black",
    //                                       current_board.moveToAlgebraicNotation(m));

    current_board.doMove(m, current_player);

    current_player = static_cast<Color>(!current_player);
    // std::cout << current_board << std::endl;

    evaluation = eval(current_player);
    current_board.checkForInvalidPawns();
  }

  return eval(n->player).value;
}

void MCTS::backPropagate(Node* n, const float value) {
  if(do_debug)
    std::cerr << "back prop" << std::endl;
  Node* current_node = n;

  while(current_node != nullptr) {
    current_node->expand_count += 1;
    current_node->value += value;
    current_node = current_node->parent;
  }
}

}

namespace po = boost::program_options;

int main(int argc, char** argv) {
  std::string fname;
  bool is_black{false};
  int time_limit_ms = 1000;

  po::options_description desc{"Options"};
  desc.add_options()
    ("board-file", po::value<std::string>(&fname)->required(), "File with board desc")
    ("c", po::value<float>(&exploration_constant), "Exploration constant")
    ("t", po::value<int>(&time_limit_ms), "Time Limit (ms)")
    ("start-black", po::bool_switch(&is_black), "Start with black move");

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
  po::notify(vm);

  chess::Board starting_board(fname);

  chess::MCTS mcts(time_limit_ms);

  auto result = mcts.uctSearch(starting_board, chess::Color::WHITE);
  std::cerr << result.str() << std::endl;
  return 0;
}

