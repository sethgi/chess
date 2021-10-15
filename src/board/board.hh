#pragma once

#include <bitset>
#include <array>
#include <cmath>
#include <string>
#include <iostream>
#include <vector>
#include <fmt/format.h>
#include <unordered_map>

// file inc, rank inc
using Directions = std::vector<std::pair<int8_t, int8_t>>;

constexpr size_t kBitsPerPiece = 4; // 0->6 = 0b000 -> 0b110, plus color in front
constexpr size_t kBoardDim = 8;
constexpr size_t kNumBits = kBitsPerPiece * kBoardDim * kBoardDim;
constexpr size_t kNumBytes = std::ceil(static_cast<float>(kNumBits) / 8);

const Directions kRookDirs{{0,1}, {0,-1}, {1,0}, {-1,0}};
const Directions kBishopDirs{{-1,1}, {1,1}, {1,-1}, {-1,-1}};
const Directions kKnightDirs{{-2,1}, {-1,2}, {1,2}, {2,1},
                             {2,-1}, {1,-2}, {-1,-2},{-2,-1}};
const Directions kQueenDirs{{0,1}, {0,-1}, {1,0}, {-1,0},
                            {-1,1}, {1,1}, {1,-1}, {-1,-1}};

const std::array<std::string, 8> kFileNames{"a","b","c","d","e","f","g","h"};
const std::array<std::string, 8> kRankNames{"1","2","3","4","5","6","7","8"};

// The special move byte is organized as followed:
// <can_ep> <ep_file [0..2]> <black_king_castle> <black_queen_castle> <white_king_castle> <white_queen_castle>
// Where <dir>_castle is 1 by default, and switches to 0 if a move ever prohibits castling
// <can_ep> indicates whether the msot recent move double-moved a pawn, enabling en passant
// <ep_file> is the file where a pawn was just double-moved on.
constexpr uint8_t kWhiteQueenCastleMask = 0b00000001;
constexpr uint8_t kWhiteKingCastleMask  = 0b00000010;
constexpr uint8_t kBlackQueenCastleMask = 0b00000100;
constexpr uint8_t kBlackKingCastleMask  = 0b00001000;
constexpr uint8_t kEnPassantFileMask    = 0b01110000;
constexpr uint8_t kCanEnPassantMask     = 0b10000000;

namespace chess {

enum PieceType : uint8_t {
  NONE_TYPE = 0,
  PAWN      = 0b001,
  ROOK      = 0b010,
  BISHOP    = 0b011,
  KNIGHT    = 0b100,
  QUEEN     = 0b101,
  KING      = 0b110
};

constexpr std::array<uint8_t, 6> kPieceVals = {0, 1, 5, 3, 3, 9};

const std::unordered_map<char, PieceType> kTypeFromChar{
  {'R',PieceType::ROOK},
  {'B',PieceType::BISHOP},
  {'N',PieceType::KNIGHT},
  {'Q',PieceType::QUEEN},
  {'K',PieceType::KING}};

enum Color : uint8_t {
  WHITE = 0b0,
  BLACK = 0b1
};

// This is repetitive, but saves bits
// First bit color, next three type
enum Piece : uint8_t {
  NONE          = 0,
  WHITE_PAWN    = 0b0001,
  WHITE_ROOK    = 0b0010,
  WHITE_BISHOP  = 0b0011,
  WHITE_KNIGHT  = 0b0100,
  WHITE_QUEEN   = 0b0101,
  WHITE_KING    = 0b0110,
  BLACK_PAWN    = 0b1001,
  BLACK_ROOK    = 0b1010,
  BLACK_BISHOP  = 0b1011,
  BLACK_KNIGHT  = 0b1100,
  BLACK_QUEEN   = 0b1101,
  BLACK_KING    = 0b1110
};

static std::vector kAllPieces{NONE, WHITE_PAWN, WHITE_ROOK,
  WHITE_BISHOP, WHITE_KNIGHT, WHITE_QUEEN, WHITE_KING, BLACK_PAWN, 
  BLACK_ROOK, BLACK_BISHOP, BLACK_KNIGHT, BLACK_QUEEN, BLACK_KING};


struct Move {
  uint8_t start_rank;
  uint8_t start_file;
  uint8_t end_rank;
  uint8_t end_file;

  uint8_t en_passant_flags = 0x00;

  bool king_castle{false};
  bool queen_castle{false};

  bool is_en_passant{false};
  
  bool is_null{false};

  PieceType promotes_to{PieceType::NONE_TYPE};

  Move() = default;
  ~Move() {};

  Move(uint8_t sf, uint8_t sr, uint8_t ef, uint8_t er) {
    start_rank = sr;
    start_file = sf;
    end_rank = er;
    end_file = ef;
  }

  Move(uint8_t sf, uint8_t sr, uint8_t ef, uint8_t er, PieceType p) {
    start_rank = sr;
    start_file = sf;
    end_rank = er;
    end_file = ef;
    promotes_to = p;
  }

  std::string str() const;
};


class MoveList : public std::vector<Move>{
 public:
  std::string str() {
    std::vector<std::string> out; 
    out.resize(size());
    int i = 0;
    for(auto m : *this) {
      out.at(i) = (fmt::format("{}: {}", i, m.str()));
      ++i;
    }
    return fmt::format("{}", fmt::join(out, "\n"));
  }

};

class Board{

public:
  Board();
  Board(const std::string& fname);

  Piece getPieceAt(uint8_t file, uint8_t rank) const;
  void setPieceAt(uint8_t file, uint8_t rank, Piece piece);

  std::string formatBoard() const;
  void setBoard(const std::string& board_string);
  void setBoardFromFile(const std::string& fname);

  void writeToFile(const std::string& fname = "board.txt");
 
  // Naive: Doesn't check legality or capture. Just overwrites end pos with start piece.
  void movePiece(uint8_t start_file, uint8_t start_rank, uint8_t end_file, uint8_t end_rank);
  void movePiece(Move move, Color color);

  bool isEmpty(uint8_t file, uint8_t rank) const {return getPieceAt(file, rank) == Piece::NONE;}
  
  bool isColor(uint8_t file, uint8_t rank, const Color color) const;
  bool isOtherColor(uint8_t file, uint8_t rank, const Color color) const;
  
  friend std::ostream& operator<<(std::ostream& os, const Board& b){
    return os << b.formatBoard() << std::endl;
  }

  // If attacked_by is set, only look for that type. Else, check all
  // if attacking_pieces isn't nullptr, return a vec of the pieces which attack the square
  bool inCheck(const Color color) const;

  // If is_enemy is true, then we're building adversarial moves for the bad guy checking
  // where they can attack. If it's false, we're building moves for good guy and looking for 
  // spaces where we can move to
  bool posAttacked(uint8_t file, uint8_t rank, const Color color, 
                   const PieceType attacked_by = PieceType::NONE_TYPE,
                   std::vector<std::pair<uint8_t, uint8_t>>* attacking_pieces = nullptr,
                   bool is_enemy = true) const;

  // if result is nullptr, update this instance. Otherwise, return a new board
  // Precond: The move makes sense. Basically, it's legal except for check. 
  // cap_value of whatever piece gets captured (only used if set)
  bool doMove(Move move, Color color, Board* result = nullptr, int* cap_value = nullptr);

  std::string moveToAlgebraicNotation(const Move m) const;
  Move moveFromAlgebraicNotation(const std::string s, Color color) const;

  std::string formatMoveList(const MoveList m) const;
  
  void checkForInvalidPawns() const;

  size_t computeHash() const;

  size_t computeSDBMHash() const;
  size_t computeDJB2Hash() const;

  uint8_t special_move_flags_{0x0F};
private:

  // List of locations of all the matching pieces that can reach the given square.
  // Used for disambiguation in algebraic notation.
  std::vector<std::pair<uint8_t, uint8_t>> canBeReachedBy(uint8_t file, uint8_t rank, Piece piece);
  
  // Column-major. 0,0 is A1, etc. 
  std::array<uint8_t, kNumBytes> data_;
};

}; // namespace chess

namespace std {

template <>
struct hash<chess::Board>
{
  size_t operator()(const chess::Board& board) const {
    return board.computeHash();
  }
};

}


