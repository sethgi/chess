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
bool do_assert = false;
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
    size_t t = entry.second.second;
    if(t > sdbm_max_time)
      sdbm_max_time = t;
    if(t < sdbm_min_time)
      sdbm_min_time = t;
    sdbm_total_time += t;
  }
  
  size_t djb2_min_time{std::numeric_limits<size_t>::max()}, djb2_max_time{0}, djb2_total_time{0};
  for(auto& entry : djb2_time) {
    size_t t = entry.second.second;
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

  const size_t num_tries = 1000;
  
  size_t hash;

  // Up First: SDBM

  // Time it (1000x should do)
  auto start = std::chrono::steady_clock::now();
  for(int i = 0; i < num_tries; ++i) {
    hash = board.computeSDBMHash(); 
  }
  auto end = std::chrono::steady_clock::now();


  size_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count()/num_tries;
  
  if(sdbm_time.count(hash) != 0) {
    if(sdbm_time.at(hash).first != *this)
      ++sdbm_collisions;
  } else {
    sdbm_time.emplace(hash, std::pair<Node, size_t>(*this, ns));
  }

  start = std::chrono::steady_clock::now();
  for(int i = 0; i < num_tries; ++i) {
    hash = board.computeDJB2Hash(); 
  }
  end = std::chrono::steady_clock::now();
  
  ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count()/num_tries;
  if(djb2_time.count(hash) != 0) {
    if(djb2_time.at(hash).first != *this)
      ++djb2_collisions;
  } else {
    djb2_time.emplace(hash, std::pair<Node, size_t>(*this,ns));
  }

  for(auto& c : children) {
    c.compareHashesHelper(sdbm_time, sdbm_collisions, djb2_time, djb2_collisions);
  }
}

MCTS::MCTS(int time_limit_ms) : time_limit_ms_(time_limit_ms)
{}

Move MCTS::uctSearch(const Board& board, const Color player) {
  Move root_move;
  root_move.is_null = true;
  Node root_node(board, player, root_move);
  
  Node* current_node = &root_node;

  cache_ = std::make_shared<Cache>();
  
  MoveGenerator move_gen(board);
  move_gen.setCache(cache_);
  current_node->unexplored_children = move_gen.getMovesForPlayer(player);
 
  auto start = std::chrono::system_clock::now();
  auto end = std::chrono::system_clock::now();
  while(std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() 
        < time_limit_ms_) {

    if(do_debug) {
      fmt::print("Loop: {} of {}\n",
          std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count(),
          time_limit_ms_);
    }

    current_node = treePolicy(&root_node);

    // TODO: detect the case where there's actually nothing left to explore
    if(current_node == nullptr) {
      end = std::chrono::system_clock::now();
      if(do_assert) assert(current_node);
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
  Node* result;
  while(current_node->children.size() > 0) {
    if(current_node->unexplored_children.size() > 0) {
      result = expand(current_node);
      if(do_assert) assert(result);
      return result;
    }
    current_node = bestChild(current_node);
    if(do_assert) assert(current_node);
  }
  result = expand(current_node);
  if(do_assert) assert(result);
  return result;
}

Node* MCTS::expand(Node* n) {
  Color player = n->player;
  if(do_debug)
    std::cerr << "expand" << std::endl;
  MoveSelection selector(n->board);
  selector.setCache(cache_);
  
  if(n->unexplored_children.size() == 0) {
    std::cerr << "Warning: tried to expand node without unexplored children" << std::endl;
    std::cerr << n->children.size() << std::endl;
    std::cerr << n->board << std::endl;
    return nullptr;
  }

  size_t move_idx;
  if(!selector.getMoveForPlayer(player, n->unexplored_children, &move_idx)) {
    std::cerr << "Warning: failed to get move for player" << std::endl;
    return nullptr;
  }
  
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
  move_gen.setCache(cache_);
  MoveList moves = move_gen.getMovesForPlayer(child.player);
  assert(moves.size() > 0);
  child.unexplored_children = std::move(moves);
  

  Node* result = &(n->children.back());
  if(do_assert) assert(result);
  return result;
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

  if(do_assert) {
    assert(max_node != n);
    assert(max_node != nullptr);
  }
  return max_node;

}

float MCTS::defaultPolicy(Node* n) {
  if(do_debug)
    std::cerr << "default policy" << std::endl;
  Board current_board = n->board;

  MoveSelection selector(current_board);
  selector.setCache(cache_);
  Evaluator eval(current_board, cache_);

  Color current_player = n->player;

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

// Run through many iterations to build a huge tree, mainly for evaluating hashes
// returns heap-allocated because I'm lazy
Node buildBigTree(Board start_board, size_t num_runs, CachePtr cache) {
  Node root(start_board, Color::WHITE, Move(0,0,0,0));
  
  // Do one move num_runs times
  for(size_t i = 0; i < num_runs; ++i) {
    Board local_board = start_board;
    MoveSelection selector(local_board);
    selector.setCache(cache);
    Move m;
    if(do_assert) {
      assert(selector.getMoveForPlayer(Color::WHITE, &m));
    }
    local_board.doMove(m, Color::WHITE);
    root.children.emplace_back(local_board, static_cast<Color>(!Color::WHITE), m);
    root.children.back().parent = &root;
  }
  

  // run each of those to the end
  for(size_t i = 0; i < num_runs; ++i) {
    Node* node = &root.children.at(i);

    Board board = node->board;
    MoveSelection selector(board);
    selector.setCache(cache);
    Evaluator eval(board, cache);

    Color current_player = node->player;

    auto evaluation = eval(current_player);

    while(evaluation.state == State::NORMAL) {
      Move m;

      if(!selector.getMoveForPlayer(current_player, &m)) break;

      board.doMove(m, current_player);

      current_player = static_cast<Color>(!current_player);

      evaluation = eval(current_player);

      node->children.emplace_back(board, current_player, m);
      Node& child = node->children.back();
      child.parent = node;
      node = &child;
    }
  }
  return root;
}

} // namespace chess

namespace po = boost::program_options;

int main(int argc, char** argv) {
  std::string fname;
  bool is_black{false};
  int time_limit_ms = 1000;

  po::options_description desc{"Options"};
  desc.add_options()
    ("board-file,b", po::value<std::string>(&fname)->required(), "File with board desc")
    ("exploration,c", po::value<float>(&exploration_constant), "Exploration constant")
    ("time,t", po::value<int>(&time_limit_ms), "Time Limit (ms)")
    ("verbose,v", po::bool_switch(&format_verbose), "If set, dot graph is verbose w./ stats")
    ("debug,d", po::bool_switch(&do_debug), "If set, prints debugs")
    ("assert,a", po::bool_switch(&do_assert), "If set, asserts sanity checks")
    ("start-black", po::bool_switch(&is_black), "Start with black move");

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
  po::notify(vm);

  chess::Board starting_board(fname);

  chess::MCTS mcts(time_limit_ms);

  // auto node = chess::buildBigTree(starting_board, time_limit_ms);

  // node.generateDotFile("graph.dot");
  // node.printStats();
  // node.compareHashes();
 
  auto result = mcts.uctSearch(starting_board, chess::Color::WHITE);
  std::cerr << result.str() << std::endl;
  return 0;
}

