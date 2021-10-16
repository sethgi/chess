#include "search/cache.hh"
#include <cassert>

namespace chess {

void Cache::insert(const Board& b, const Color c, const bool in_check) {
  CachePair key(b,c);
  if(cache_map_.count(key) == 0) {
    CacheEntry entry;
    entry.has_check = true;
    entry.in_check = in_check;
    cache_map_[CachePair(b,c)] = entry;
  } else if (!cache_map_[key].has_check) {
    cache_map_[key].has_check = true;
    cache_map_[key].in_check = in_check;
  }
}

void Cache::insert(const Board& b, const Color c, const MoveList& moves){
  CachePair key(b,c);
  if(cache_map_.count(key) == 0) {
    CacheEntry entry;
    entry.has_moves = true;
    entry.legal_moves = moves;
    cache_map_[CachePair(b,c)] = entry;
  } else if (!cache_map_[key].has_moves) {
    cache_map_[key].has_moves = true;
    cache_map_[key].legal_moves = moves;
  }
}

bool Cache::getCacheEntry(const Board& b, const Color c, CacheEntry* result){
  CachePair key(b,c);
  if(cache_map_.count(key) == 0) return false;
  result = &cache_map_.at(key);
  cache_hits_++;
  return true;
}

bool Cache::getMoveList(const Board& b, const Color c, MoveList* result){
  CachePair key(b,c);
  if(cache_map_.count(key) == 0 || !cache_map_[key].has_moves) {
    return false;
  }
  *result = cache_map_[key].legal_moves;
  cache_hits_++;
  return true;
}

bool Cache::getInCheck(const Board& b, const Color c, bool* result) {
  CachePair key(b,c);
  if(cache_map_.count(key) == 0 || !cache_map_[key].has_check) {
    return false;
  }
  *result = cache_map_[key].in_check;
  cache_hits_++;
  return true;
}

bool Cache::contains(const Board& b, const Color c){
  CachePair key(b,c);
  return cache_map_.count(key) != 0;
}

void Cache::insert(const Node& n, const MoveList& moves){
  insert(n.board, n.player, moves);
}  

bool Cache::getCacheEntry(const Node& n, CacheEntry* result){
  return getCacheEntry(n.board, n.player, result);
}  

bool Cache::getMoveList(const Node& n, MoveList* result){
  return getMoveList(n.board, n.player, result);
}  

bool Cache::contains(const Node& n){
  return contains(n.board, n.player);
}  

}
