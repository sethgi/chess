#pragma once

#include <unordered_map>
#include <memory>

#include "board/board.hh"
#include "search/search.hh"
#include "search/cache_fwd.hh"

namespace chess {
  using CachePair = std::pair<Board, Color>;
}

// Same hash as for a Node
namespace std {

template <>
struct hash<chess::CachePair>
{
  size_t operator()(const chess::CachePair& key) const {
    // XOR with 31 bits followed by a 0 or 1
    // This guarantees that the hash is different for different colors 
    return std::hash<chess::Board>{}(key.first) ^ key.second;
  }
};

}

namespace chess {

struct CacheEntry {
  bool has_moves{false};
  MoveList legal_moves;

  bool has_check{false};
  bool in_check;
};

class Cache {
 public:
  Cache() = default;

  void insert(const Board& b, const Color c, const bool in_check); 
  bool getInCheck(const Board& b, const Color c, bool* result);

  void insert(const Board& b, const Color c, const MoveList& moves);
  bool getCacheEntry(const Board& b, const Color c, CacheEntry* result);
  bool getMoveList(const Board& b, const Color c, MoveList* result);
  bool contains(const Board& b, const Color c);

  void insert(const Node& n, const MoveList& moves);
  bool getCacheEntry(const Node& n, CacheEntry* result);
  bool getMoveList(const Node& n, MoveList* result);
  bool contains(const Node& n);

 private:
  std::unordered_map<CachePair, CacheEntry> cache_map_;
  size_t cache_hits_{0};
};

}

