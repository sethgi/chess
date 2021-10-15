#pragma once

#include <string>
#include <unordered_set>

#include "board/board.hh"
#include "board/board_utils.hh"
#include "move_generator/move_generator.hh"

namespace chess {


enum State {
  NORMAL,
  STALEMATE,
  WHITE_WINS,
  BLACK_WINS
};

struct Evaluation {
  State state;
  float value;

  std::string str() {
    std::string state_str;
    std::string value_str = std::to_string(value);
    switch(state) {
      case NORMAL:
        state_str = "NORMAL";
        break;
      case STALEMATE: 
        state_str = "STALEMATE";
        break; 
      case BLACK_WINS:
        state_str = "BLACK_WINS";
        break;
      case WHITE_WINS:
        state_str = "WHITE_WINS";
        break;
      default:
        state_str = fmt::format("INVALID STATE: {}", (int)state);
        break;
    }
    return fmt::format("{} ({})", state_str, value_str);
  }
};

class Evaluator {

 public:
  Evaluator(const Board& b): board_(b), move_gen_{b} {}
  
  Evaluation operator()(Color color){

    bool white_has_dark_bishop, white_has_light_bishop;
    bool black_has_dark_bishop, black_has_light_bishop;

    // bit 0 = none, bit 1 = pawn, and so on (follows enum)
    uint8_t white_has = 0;
    uint8_t black_has = 0;

    float value = 0;
    for(int file = 0; file < 8; ++file){
      for(int rank = 0; rank < 8; ++rank){

        bool is_dark_square = (file % 2) == (rank % 2);

        Piece p = board_.getPieceAt(file, rank);
        PieceType pt = getPieceType(p);

        bool is_white = board_.isColor(file, rank, Color::WHITE);
        bool is_black = board_.isColor(file, rank, Color::BLACK);
                
        if(is_white) {
          white_has = white_has | 1 << pt; 
          if (pt == PieceType::BISHOP) {
            if(is_dark_square) white_has_dark_bishop = true;
            else white_has_light_bishop = true;
          }
        } else if (is_black) {
          black_has = black_has |  1 << pt; 
          if(pt == PieceType::BISHOP) {
            if(is_dark_square) black_has_dark_bishop = true;
            else black_has_light_bishop = true;
          }
        }
                
        if(pt == PieceType::KING) continue;

        value += kPieceVals.at(getPieceType(p))
                  * board_.isColor(file, rank, color) ? 1 : -1;
      
      }
    }
    Evaluation result;
    result.value = value;

    const uint8_t king_only = 0b1 << PieceType::KING;

      
    // std::cerr << "white has" << fmt::format("{:b}",(size_t)white_has) << std::endl;
    // std::cerr << "black has" << fmt::format("{:b}",(size_t)black_has) << std::endl;
    
    // Get the board state
    if(isCheckmate(Color::WHITE)) {
      // std::cout << "black win checkmate" << std::endl;
      result.state = State::BLACK_WINS;
    } else if (isCheckmate(Color::BLACK)) {
      // std::cout << "white win checkmate" << std::endl;
      result.state = State::WHITE_WINS;
    } else if (!hasLegalMoves(color)) {
      // std::cout << "no move stalemate" << std::endl;
      result.state = State::STALEMATE;
    } else if (white_has == king_only && black_has == king_only) {
      // std::cout << "insufficient material" << std::endl;
      result.state = State::STALEMATE;
    }
    else if (white_has == king_only) {
      if((black_has_light_bishop && black_has_dark_bishop)
          || (black_has & 1 << PieceType::ROOK)
          || (black_has & 1 << PieceType::QUEEN)
          || (black_has & 1 << PieceType::BISHOP 
              && black_has & 1 << PieceType::KNIGHT))
      {
        // std::cout << "black has force" << std::endl;
        result.state = State::BLACK_WINS;
      } else {
        // std::cout << "inevitable insufficient material" << std::endl;
        result.state = State::STALEMATE;
      }
    }
    else if (black_has == king_only) {
      if((white_has_light_bishop && white_has_dark_bishop)
          || (white_has & 1 << PieceType::ROOK)
          || (white_has & 1 << PieceType::QUEEN)
          || (white_has & 1 << PieceType::BISHOP 
              && white_has & 1 << PieceType::KNIGHT))
      {
        // std::cerr << "white light?" << white_has_light_bishop << std::endl;
        // std::cerr << "white dark?" << white_has_dark_bishop << std::endl;
        // std::cerr << "white has force" << std::endl;
        result.state = State::WHITE_WINS;
      } else {
        // std::cout << "inevitable insufficient material" << std::endl;
        result.state = State::STALEMATE;
      }
    }
    else {
      result.state = State::NORMAL;
    }

    return result;
  }

 private:
  // If this player is IN checkmate (NOT applying)
  bool isCheckmate(Color color) {
    if(hasLegalMoves(color)) return false;

    return board_.inCheck(color);
  }

  bool hasLegalMoves(Color color) {
    return !move_gen_.getMovesForPlayer(color).empty();
  };

  const Board& board_;
  const MoveGenerator move_gen_;

};


}
