#pragma once

#include <unordered_map>

#include "board/board.hh"

namespace chess {


const std::unordered_map<std::string, Piece> string_to_piece{
  {"__", Piece::NONE},
  {"WP", Piece::WHITE_PAWN},
  {"WR", Piece::WHITE_ROOK},
  {"WB", Piece::WHITE_BISHOP},
  {"WN", Piece::WHITE_KNIGHT},
  {"WQ", Piece::WHITE_QUEEN},
  {"WK", Piece::WHITE_KING},
  {"BP", Piece::BLACK_PAWN},
  {"BR", Piece::BLACK_ROOK},
  {"BB", Piece::BLACK_BISHOP},
  {"BN", Piece::BLACK_KNIGHT},
  {"BQ", Piece::BLACK_QUEEN},
  {"BK", Piece::BLACK_KING}
};

const std::unordered_map<std::string, PieceType> string_to_piece_type{
  {"_", PieceType::NONE_TYPE},
  {"P", PieceType::PAWN},
  {"R", PieceType::ROOK},
  {"B", PieceType::BISHOP},
  {"N", PieceType::KNIGHT},
  {"Q", PieceType::QUEEN},
  {"K", PieceType::KING},
};       
       
const std::unordered_map<PieceType, std::string> piece_type_to_string{
  {PieceType::NONE_TYPE, "_"},
  {PieceType::PAWN, "P"},
  {PieceType::ROOK, "R"},
  {PieceType::BISHOP, "B"},
  {PieceType::KNIGHT, "N"},
  {PieceType::QUEEN, "Q"},
  {PieceType::KING, "K"},
};

const std::unordered_map<Piece, std::string> piece_to_str{
  {Piece::NONE, "__"},
  {Piece::WHITE_PAWN, "WP"},
  {Piece::WHITE_ROOK, "WR"},
  {Piece::WHITE_BISHOP, "WB"},
  {Piece::WHITE_KNIGHT, "WN"},
  {Piece::WHITE_QUEEN, "WQ"},
  {Piece::WHITE_KING, "WK"},
  {Piece::BLACK_PAWN, "BP"},
  {Piece::BLACK_ROOK, "BR"},
  {Piece::BLACK_BISHOP, "BB"},
  {Piece::BLACK_KNIGHT, "BN"},
  {Piece::BLACK_QUEEN, "BQ"},
  {Piece::BLACK_KING, "BK"},
};

static std::string getStrFromPiece(Piece p) {
  return piece_to_str.at(p);
}

static Piece getPieceFromStr(std::string str) {
  return string_to_piece.at(str);
}

static std::string getStrFromType(PieceType p) {
  return piece_type_to_string.at(p);
}

static PieceType getTypeFromStr(std::string str) {
  return string_to_piece_type.at(str);
}

static Piece buildPiece(PieceType type, Color color) {
  return static_cast<Piece>((color << 3) | type);
}

constexpr PieceType getPieceType(Piece piece) {
  // Get rid of the color bit
  return static_cast<PieceType>(piece & 0b11110111);
}

constexpr Color getPieceColor(Piece piece) {
  return static_cast<Color>((piece >> 3) & 0b1);
}

}

/*
=================================
|BP  WR  BN  WW      BR  WB  BQ |
|WK  WP  BB  WN  BK  BP  WR  BN |
|WW      BR  WB  BQ  WK  WP  BB |
|WN  BK  BP  WR  BN  WW      BR |
|WB  BQ  WK  WP  BB  WN  BK  BP |
|WR  BN  WW      BR  WB  BQ  WK |
|WP  BB  WN  BK  BP  WR  BN  WW |
|    BR  WB  BQ  WK  WP  BB  WN |
=================================
*/
