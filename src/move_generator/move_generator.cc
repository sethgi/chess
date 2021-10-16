#include <iterator>
#include <cmath>
#include <boost/program_options.hpp>

#include "move_generator/move_generator.hh"
#include "board/board_utils.hh"

// TODO: 
// - Check for legal moves
//  - Trace back from king in all directions
// - Special moves (require knowledge of prev moves)
//  - Castling
//    - Can't go through check or out of check
//    - Can't have moved involved pieces
//    - Both directions
//  - En passant
//    - Opponent has to have just moved the piece

namespace chess {

const Directions kRookDirs{{0,1}, {0,-1}, {1,0}, {-1,0}};
const Directions kBishopDirs{{-1,1}, {1,1}, {1,-1}, {-1,-1}};
const Directions kKnightDirs{{-2,1}, {-1,2}, {1,2}, {2,1},
                             {2,-1}, {1,-2}, {-1,-2},{-2,-1}};
const Directions kQueenDirs{{0,1}, {0,-1}, {1,0}, {-1,0},
                            {-1,1}, {1,1}, {1,-1}, {-1,-1}};

MoveGenerator::MoveGenerator(const Board& b) : board_(b)
{}

MoveList MoveGenerator::getMovesForPiece(uint8_t file, uint8_t rank) const
{
  auto piece = board_.getPieceAt(file, rank);
  const PieceType type = getPieceType(piece);
  const Color color = getPieceColor(piece);
 
  MoveList moves;

  switch(type) {
    case PieceType::PAWN:
      moves = getMovesForPawn(file, rank, color);
      break;
    case PieceType::ROOK:
      moves = getMovesForDirs(file, rank, kRookDirs, color);
      break;
    case PieceType::BISHOP:
      moves = getMovesForDirs(file, rank, kBishopDirs, color);
      break;
    case PieceType::KNIGHT:
      moves = getMovesForDirs(file, rank, kKnightDirs, color, true);
      break;
    case PieceType::QUEEN:
      moves = getMovesForDirs(file, rank, kQueenDirs, color);
      break;
    case PieceType::KING:
      moves = getMovesForDirs(file, rank, kQueenDirs, color, true);
      break;
    defult:
      throw std::runtime_error(fmt::format("Invalid piece {0:x}", piece));
  }
  return moves;
}

MoveList MoveGenerator::getMovesForPlayer(Color color) const {
  return getMovesForPlayer(color, cache_);
}

MoveList MoveGenerator::getMovesForPlayer(Color color, CachePtr cache) const {

  MoveList result;
  
  if(cache && cache->getMoveList(board_, color, &result)) {
    return result;
  }

  for(int file = 0; file < kBoardDim; ++file) {
    for(int rank = 0; rank < kBoardDim; ++rank) {
      if(!board_.isColor(file, rank, color)) continue;
      auto moves = getMovesForPiece(file, rank);
      result.insert(result.end(),
                    std::make_move_iterator(moves.begin()),
                    std::make_move_iterator(moves.end()));
    }
  }

  // Castling (only works for 8x8 board).
  // To simplify downstream stuff, make sure that this castle is legit (none of the
  // pieces the king goes through are under attack).
  if(color == Color::WHITE) {
    // Queen
    if ((board_.special_move_flags_ & kWhiteQueenCastleMask)
        && board_.isEmpty(1,0) && board_.isEmpty(2,0) && board_.isEmpty(3,0)
        && !(board_.posAttacked(2,0, color) || board_.posAttacked(3,0, color) || board_.posAttacked(4,0,color))
        && board_.getPieceAt(0,0) == Piece::WHITE_ROOK
        && board_.getPieceAt(4,0) == Piece::WHITE_KING) {

      result.emplace_back(0,0,0,0);
      result.at(result.size() - 1).queen_castle = 1;
    } else if ((board_.special_move_flags_ & kWhiteKingCastleMask)
        && board_.isEmpty(5,0) && board_.isEmpty(6,0)
        && !(board_.posAttacked(4,0,color) || board_.posAttacked(5,0,color) || board_.posAttacked(6,0,color))
        && board_.getPieceAt(7,0) == Piece::WHITE_ROOK
        && board_.getPieceAt(4,0) == Piece::WHITE_KING) {
      
      result.emplace_back(0,0,0,0);
      result.at(result.size() - 1).king_castle = 1;
    }
  } else {
    if ((board_.special_move_flags_ & kBlackQueenCastleMask)
        && board_.isEmpty(1,7) && board_.isEmpty(2,7) && board_.isEmpty(3,7)
        && !(board_.posAttacked(2,0,color) || board_.posAttacked(3,0,color) || board_.posAttacked(4,0,color))
        && board_.getPieceAt(0,7) == Piece::BLACK_ROOK
        && board_.getPieceAt(4,7) == Piece::BLACK_KING) {
    
      result.emplace_back(0,0,0,0);
      result.at(result.size() - 1).queen_castle = 1;
    } else if ((board_.special_move_flags_ & kBlackKingCastleMask)
        && board_.isEmpty(5,7) && board_.isEmpty(6,7)
        && !(board_.posAttacked(4,7,color) || board_.posAttacked(5,7,color) || board_.posAttacked(6,7,color))
        && board_.getPieceAt(7,7) == Piece::BLACK_ROOK
        && board_.getPieceAt(4,7) == Piece::BLACK_KING) {
      
      result.emplace_back(0,0,0,0);
      result.at(result.size() - 1).king_castle = 1;
    }
    
  }
  
  auto to_return = filterLegalMoves(result, color, cache);
  if(cache) {
    cache->insert(board_, color, to_return);
  }
  return to_return;
}

MoveList MoveGenerator::filterLegalMoves(MoveList& in_list, Color color, CachePtr cache) const {
  auto is_illegal = [&](Move& m) {
    Board temp_board = board_;
    return !temp_board.doMove(m, color, cache);
  };

  auto new_end = std::remove_if(in_list.begin(), in_list.end(), is_illegal);
  in_list.erase(new_end, in_list.end());
  return in_list;
}

MoveList MoveGenerator::getMovesForPawn(uint8_t file, uint8_t rank, Color color) const {
  int8_t dir, double_rank, promote_rank, ep_rank;

  if(color == Color::WHITE) {
    dir = 1;
    double_rank = 1;
    promote_rank = kBoardDim - 1;
    ep_rank = kBoardDim - 4;
  } else {
    dir = -1;
    double_rank = kBoardDim - 2;
    promote_rank = 0;
    ep_rank = 3;
  }
  
  MoveList result;
  // If we can move forward
  if(board_.isEmpty(file, rank + dir)) {
    // Double move. Ignore the stupid edge case where board is 4 long and it can promote.
    // Also in general assume that promoting a pawn doesn't go off end of board.
    if(rank == double_rank && board_.isEmpty(file, rank + 2*dir)) {
      Move new_move(file, rank, file, rank+2*dir);
      new_move.en_passant_flags = 0b1000 | file;
      result.push_back(new_move);
    }
    if (rank + dir == promote_rank) {
      result.emplace_back(file, rank, file, rank + dir, PieceType::QUEEN);
      result.emplace_back(file, rank, file, rank + dir, PieceType::ROOK);
      result.emplace_back(file, rank, file, rank + dir, PieceType::KNIGHT);
      result.emplace_back(file, rank, file, rank + dir, PieceType::BISHOP);
    } else {
      result.emplace_back(file, rank, file, rank+dir);
    }
  }

  // Capture diagonal: ++file
  if(file + 1 < kBoardDim && board_.isOtherColor(file + 1, rank + dir, color)) {
    if(rank + dir == promote_rank) {
      result.emplace_back(file, rank, file + 1, rank + dir, PieceType::QUEEN);
      result.emplace_back(file, rank, file + 1, rank + dir, PieceType::ROOK);
      result.emplace_back(file, rank, file + 1, rank + dir, PieceType::KNIGHT);
      result.emplace_back(file, rank, file + 1, rank + dir, PieceType::BISHOP);
    } else {
      result.emplace_back(file, rank, file + 1, rank + dir);
    }
  }

  // Capture diagnoal: --file
  if(file - 1 >= 0 && board_.isOtherColor(file - 1, rank + dir, color)) {
    if(rank + dir == promote_rank) {
      result.emplace_back(file, rank, file - 1, rank + dir, PieceType::QUEEN);
      result.emplace_back(file, rank, file - 1, rank + dir, PieceType::ROOK);
      result.emplace_back(file, rank, file - 1, rank + dir, PieceType::KNIGHT);
      result.emplace_back(file, rank, file - 1, rank + dir, PieceType::BISHOP);
    } else {
      result.emplace_back(file, rank, file - 1, rank + dir);
    }
  }

  // En Passant
  if(rank == ep_rank 
      && (board_.special_move_flags_ & kCanEnPassantMask)) {
    uint8_t ep_file = (board_.special_move_flags_ & kEnPassantFileMask) >> 4;

    if(board_.isEmpty(ep_file, ep_rank + dir)) {
      if((ep_file > 0 && ep_file - 1 == file)
          || (ep_file < kBoardDim - 1 && ep_file + 1 == file)) {
          
        Move new_move(file, rank, ep_file, rank+dir);
        new_move.is_en_passant = true;
        result.push_back(new_move);
      }
    }
  }

  return result;
}

MoveList MoveGenerator::getMovesForDirs(uint8_t file,
                                        uint8_t rank,
                                        Directions dirs,
                                        Color color,
                                        bool one_step) const {
  MoveList result; 

  for(const auto& d : dirs) {
    int steps = 1;
    while (true) {
      int8_t new_file = file + d.first * steps;
      int8_t new_rank = rank + d.second * steps;

      if(new_file < 0 || new_file >= kBoardDim 
         || new_rank < 0 || new_rank >= kBoardDim)
        break;

      // If same color, this dir is done
      if(board_.isColor(new_file, new_rank, color)) break;

      // Otherwise, we can go at least this many steps
      result.emplace_back(file, rank, new_file, new_rank);
      
      // If we run into any piece, all done
      if (one_step || board_.isOtherColor(new_file, new_rank, color)) break;
      ++steps;
    }
  }
  return result;
}


}

/*
using namespace chess;
namespace po = boost::program_options;

int main(int argc, char** argv) {
  std::string fname;
  bool is_black;

  po::options_description desc{"Options"};
  desc.add_options()
    ("board_file", po::value<std::string>(&fname), "File with board desc")
    ("black", po::bool_switch(&is_black)->default_value(false), "Set to true to be black pieces");

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
  po::notify(vm);
  Board b(fname);
  std::cout << b << std::endl;
  MoveGenerator move_gen(b);
  const Color color = is_black ? Color::BLACK : Color::WHITE;
  auto moves = move_gen.getMovesForPlayer(color);
  std::cout << moves.str() << std::endl;

  int move_idx;
  std::cout << "Enter Move Index: ";
  std::cin >> move_idx;
  std::cout << "Legal: " << (b.doMove(moves[move_idx], color) ? "YES" : "NO") << std::endl;
  std::cout << b << std::endl;
} */
