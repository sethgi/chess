#include <sstream>
#include <fstream>
#include <algorithm>

#include "board/board.hh"
#include "board/board_utils.hh"

// Comment out all but one of these
#define USE_HASH_DJB2
// #define USE_HASH_SDBM

namespace chess {

static constexpr uint8_t kBitMask = 0xF0;


std::string Move::str() const {
    if(king_castle) return ("K Castle");
    if(queen_castle) return ("Q Castle");

    std::string promote_str = promotes_to == PieceType::NONE_TYPE
                                ? ""
                                : "+"+getStrFromType(promotes_to);
    
    return fmt::format("({},{})->({},{}){}", start_file, start_rank,
                         end_file, end_rank, promote_str);
}

Board::Board() {
  for(auto& b : data_) b = 0;
}

Board::Board(const std::string& fname) {
  setBoardFromFile(fname);
}

void Board::setBoard(const std::string& board_string) {
  std::istringstream board_stream(board_string);
  std::string line;
  size_t line_count = 0;
  while(std::getline(board_stream, line)) {
    ++line_count;
    if(line_count == 1 || line_count == kBoardDim+2) continue;
    std::istringstream line_stream(line);
    std::string token;
    size_t token_count = 0;
    while(std::getline(line_stream, token, ' ')){
      if(token == "") continue;
      ++token_count;
      if(token_count == 1 || token_count == kBoardDim + 2) continue;
      setPieceAt(token_count - 2, kBoardDim - (line_count-1), getPieceFromStr(token));
    }
  }
}

void Board::setBoardFromFile(const std::string& fname) {
  std::ifstream board_str(fname);
  std::stringstream buffer;
  buffer << board_str.rdbuf();
  setBoard(buffer.str());
}

Piece Board::getPieceAt(uint8_t file, uint8_t rank) const {
  uint8_t bit_index = (file*kBoardDim + rank)*kBitsPerPiece;

  /*
   * Get array as column-major. First entry first. So A1..A8, B1..B8...
   * Left shift to get the desired entry as the first three bytes.
   *
   * Then mask with 0b111 to get the value.
   *
   * Then shift back right to get the correct result.
  */
  return Piece(((data_[bit_index / 8] << (bit_index % 8) ) & kBitMask) >> 4);
}

void Board::setPieceAt(uint8_t file, uint8_t rank, Piece piece) {
  uint8_t bit_index = (file*kBoardDim + rank)*kBitsPerPiece;

  uint8_t& byte = data_[bit_index / 8];
  uint8_t shift_amount = bit_index % 8;

  byte &= ~(kBitMask >> shift_amount);
  byte |= (kBitMask & (piece << 4)) >> shift_amount;
}

void Board::movePiece(uint8_t start_file, uint8_t start_rank, uint8_t end_file, uint8_t end_rank){
  setPieceAt(end_file, end_rank, getPieceAt(start_file, start_rank));
  setPieceAt(start_file, start_rank, Piece::NONE);
}

void Board::movePiece(Move move, Color color) {
  Piece end_piece;
  if(move.promotes_to != PieceType::NONE_TYPE) {
    end_piece = buildPiece(move.promotes_to, color);
  } else {
    end_piece = getPieceAt(move.start_file, move.start_rank);
  }

  setPieceAt(move.end_file, move.end_rank, end_piece);
  setPieceAt(move.start_file, move.start_rank, Piece::NONE);

  if(move.is_en_passant) {
    int pawn_dir = color == Color::WHITE ? 1 : -1;
    setPieceAt(move.end_file, move.end_rank - pawn_dir, Piece::NONE);
  }
}

// Puts A1 in bottom left
std::string Board::formatBoard() const {
  const size_t format_size = 3;
  std::string result;

  std::string header_footer((format_size+1)*kBoardDim + 2, '=');
  header_footer.append("\n");
  result.append(header_footer);
  // start at top left (A8) and go to bottom right (H1)
  for(int rank = kBoardDim - 1; rank >= 0; --rank) {
    result.append("| ");
    for(int file = 0; file < kBoardDim; ++file) {
      auto piece_str = getStrFromPiece(getPieceAt(file, rank));
      result.append(piece_str);
      result.append(std::string(format_size - piece_str.length(), ' '));
      if(file != kBoardDim - 1) result.append(" ");
    }
    result.append("|\n");
  }
  result.append(header_footer);

  result.append(fmt::format("Castle: {}{}{}{}{}",
        (special_move_flags_ & kWhiteKingCastleMask)  ? "WK " : "",
        (special_move_flags_ & kWhiteQueenCastleMask) ? "WQ " : "",
        (special_move_flags_ & kBlackKingCastleMask)  ? "BK " : "",
        (special_move_flags_ & kBlackQueenCastleMask) ? "BQ " : "",
        !(special_move_flags_ & kCanEnPassantMask)
          ? ""
          : fmt::format("EP: {:d}", (special_move_flags_ & kEnPassantFileMask) >> 4)
        ));
  return result;
}

bool Board::isColor(uint8_t file, uint8_t rank, Color color) const {
  return !isEmpty(file, rank) && getPieceColor(getPieceAt(file, rank)) == color;
}

bool Board::isOtherColor(uint8_t file, uint8_t rank, Color color) const {
  return !isEmpty(file, rank) && getPieceColor(getPieceAt(file, rank)) != color;
}

void Board::writeToFile(const std::string& fname) {
  std::ofstream output(fname);
  output << formatBoard() << std::endl;
  output.close();
}

bool Board::inCheck(const Color color) const {
  uint8_t file, rank;
  for(uint8_t f = 0; f < kBoardDim; ++f) {
    for(uint8_t r = 0; r < kBoardDim; ++r) {
      if (getPieceAt(f, r) == buildPiece(PieceType::KING, color)) {
        file = f;
        rank = r;
      }
    }
  }
  return posAttacked(file, rank, color);
}

#define ADD_OR_RETURN(file, rank)  {\
  if(return_pieces) {attacking_pieces->emplace_back(file,rank);} \
  else {return true;} }

bool Board::posAttacked(uint8_t file, uint8_t rank, const Color color,
    const PieceType attacked_by,
    std::vector<std::pair<uint8_t, uint8_t>>* attacking_pieces,
    bool is_enemy) const {

  Color attacker_color;
  if(is_enemy)
    attacker_color = static_cast<Color>(!color);
  else
    attacker_color = color;

  Piece king_piece = buildPiece(PieceType::KING ,attacker_color); 
  Piece queen_piece = buildPiece(PieceType::QUEEN ,attacker_color); 
  Piece rook_piece = buildPiece(PieceType::ROOK ,attacker_color); 
  Piece bishop_piece = buildPiece(PieceType::BISHOP ,attacker_color); 
  Piece knight_piece = buildPiece(PieceType::KNIGHT ,attacker_color); 
  Piece pawn_piece = buildPiece(PieceType::PAWN ,attacker_color); 

  bool pawn_attack = attacked_by == PieceType::PAWN || attacked_by == PieceType::NONE_TYPE;
  bool knight_attack = attacked_by == PieceType::KNIGHT || attacked_by == PieceType::NONE_TYPE;
  bool bishop_attack = attacked_by == PieceType::BISHOP || attacked_by == PieceType::NONE_TYPE;
  bool rook_attack = attacked_by == PieceType::ROOK || attacked_by == PieceType::NONE_TYPE;
  bool queen_attack = attacked_by == PieceType::QUEEN || attacked_by == PieceType::NONE_TYPE;
  bool king_attack = attacked_by == PieceType::KING || attacked_by == PieceType::NONE_TYPE;
 
  bool return_pieces = attacking_pieces != nullptr;

  if(pawn_attack) {
    int8_t pawn_dir = attacker_color == Color::WHITE ? 1 : -1;
    
    // If looking at whether a square is under attack, or if the current square is the other color,
    // then check for diagnoal attacks
    if(is_enemy || isOtherColor(file, rank, attacker_color)) {
      // Check for attacking pawns
      // --file
      if(file > 0 && getPieceAt(file - 1, rank - pawn_dir) == pawn_piece)
        ADD_OR_RETURN(file-1, rank-pawn_dir);
      // ++file
      if(file < kBoardDim - 1 && getPieceAt(file + 1, rank - pawn_dir) == pawn_piece)
        ADD_OR_RETURN(file+1, rank-pawn_dir);
    }
    // We look for en passant if checking whether the square above/below (depending on color) is 
    // the other color, then check EP criteria
    if(isOtherColor(file, rank-pawn_dir, attacker_color)) {
      // En passant logic:
      //  If this file is the EP one, check back and either side for an opposing pawn. 
      //  If it exists, then there's EP
      if(special_move_flags_ & kCanEnPassantMask) {
        uint8_t ep_file = (special_move_flags_ & kEnPassantFileMask) >> 4;

        // The rank a pawn goes TO during EP capture
        uint8_t ep_rank = attacker_color == Color::WHITE? 5 : 2;

        if(file > 0 && getPieceAt(file-1, ep_rank-pawn_dir) == pawn_piece) {
          ADD_OR_RETURN(file-1, ep_rank-pawn_dir);
        }
        if(file < kBoardDim - 1 && getPieceAt(file+1, ep_rank-pawn_dir) == pawn_piece) {
          ADD_OR_RETURN(file+1, ep_rank-pawn_dir);
        }
      }
    }

    // If we're making moves for good guy 
    if(!is_enemy) {
      if(getPieceAt(file, rank-pawn_dir) == pawn_piece) {
        ADD_OR_RETURN(file, rank-pawn_dir);
      }
      else if(getPieceAt(file, rank-2*pawn_dir) == pawn_piece) { // can't be both
        ADD_OR_RETURN(file, rank-2*pawn_dir);
      }
    }
  }

  if(knight_attack) {
    // Now knights
    for(const auto& dir : kKnightDirs) {
      int8_t new_file = file + dir.first;
      int8_t new_rank = rank + dir.second;

      if(new_file < 0 || new_file >= kBoardDim 
         || new_rank < 0 || new_rank >= kBoardDim)
        continue;
      
      if(getPieceAt(new_file, new_rank) == knight_piece)
        ADD_OR_RETURN(new_file, new_rank);
    }
   }
  

  if(queen_attack || rook_attack) {
    for(const auto& dir : kRookDirs) {
      int step = 1;
      while(true) {
        int8_t new_file = file + dir.first * step;
        int8_t new_rank = rank + dir.second * step;

        if(new_file < 0 || new_file >= kBoardDim 
           || new_rank < 0 || new_rank >= kBoardDim)
          break;

        if((getPieceAt(new_file, new_rank) == queen_piece && queen_attack)
           || (getPieceAt(new_file, new_rank) == rook_piece && rook_attack)) {
          ADD_OR_RETURN(new_file, new_rank);

        }
        
        // If we find a blocker, stop
        if(getPieceAt(new_file, new_rank) != Piece::NONE) break;
        ++step;
      }
    }
  }

  if(queen_attack || bishop_attack) {
    // Diagonal movers (bishop + queen)
    for(const auto& dir : kBishopDirs) {
      int step = 1;
      while(true) {
        int8_t new_file = file + dir.first * step;
        int8_t new_rank = rank + dir.second * step;

        if(new_file < 0 || new_file >= kBoardDim 
           || new_rank < 0 || new_rank >= kBoardDim)
          break;
        
        if(getPieceAt(new_file, new_rank) == queen_piece && queen_attack
           || getPieceAt(new_file, new_rank) == bishop_piece && bishop_attack)

          ADD_OR_RETURN(new_file, new_rank);
        
        // If we find a blocker, stop
        if(getPieceAt(new_file, new_rank) != Piece::NONE) break;
        ++step;
      }
    }
  }

  if(king_attack) {
    for(int file_step = -1; file_step <= 1; ++file_step) {
      for(int rank_step = -1; rank_step <= 1; ++rank_step) {
        uint8_t new_file = file + file_step;
        uint8_t new_rank = rank + rank_step;

        if(new_file < 0 || new_file >= kBoardDim
            || new_rank < 0 || new_rank >= kBoardDim)
          continue;

        if(getPieceAt(new_file, new_rank) == king_piece)
          ADD_OR_RETURN(new_file, new_rank);
      }
    }
  }

  if(attacking_pieces == nullptr)
    return false;

  else return !attacking_pieces->empty();
}

#undef ADD_OR_RETURN

bool Board::doMove(Move move, Color color, Board* result, int* cap_value) {
  Board tmp_board = *this;

  bool just_castled = false;

  int back_rank = color == Color::WHITE? 0 : 7;
    

  const int castle_mask_shift = color == Color::WHITE? 0 : 2;

  // Case 1: Castle. We verified before that none of the positions are in check or occupied.
  if(move.queen_castle) {
    tmp_board.movePiece(4, back_rank, 2, back_rank); 
    tmp_board.movePiece(0, back_rank, 3, back_rank);
    just_castled = true;
  } else if (move.king_castle) {
    tmp_board.movePiece(4, back_rank, 6, back_rank); 
    tmp_board.movePiece(7, back_rank, 5, back_rank);
    just_castled = true;
  } 

  // Not a castle
  else {
    PieceType captured_type = getPieceType(getPieceAt(move.end_file, move.end_rank));
    if(cap_value != nullptr) {
      *cap_value = kPieceVals[captured_type];
    }
   
    // If we're about to move a king, no more castling.
    if(getPieceType(getPieceAt(move.start_file, move.start_rank)) == PieceType::KING) {
      tmp_board.special_move_flags_ = tmp_board.special_move_flags_ & ~(0b11 << castle_mask_shift);
    }

    
    // If we're about to move the H rook, no more king-side castles.
    // No need to check if it's a rook since moving any piece there means we disabled at some point.
    if(move.start_file == 7 && move.start_rank == back_rank){
      tmp_board.special_move_flags_ = tmp_board.special_move_flags_ & ~(0b10 << castle_mask_shift); 
    } else if (move.start_file == 0 && move.start_rank == back_rank) { // same for queen
      tmp_board.special_move_flags_ = tmp_board.special_move_flags_ & ~(0b1 << castle_mask_shift); 
    }

    tmp_board.movePiece(move, color);
   
    // Set en passant flags
    // Zero out left 4, then set.
    tmp_board.special_move_flags_ = tmp_board.special_move_flags_ & 0x0F;
    tmp_board.special_move_flags_ = tmp_board.special_move_flags_ | (move.en_passant_flags << 4);
  }

  if(just_castled) {
    if(cap_value != nullptr) *cap_value = 0;
    // no more castling
    tmp_board.special_move_flags_ = tmp_board.special_move_flags_ & ~(0b11 << castle_mask_shift);

    // next guy can't en passant after a castle
    tmp_board.special_move_flags_ = tmp_board.special_move_flags_ & 0x0F;
  }

  if(tmp_board.inCheck(color)) {
    return false;
  }

  if(result == nullptr) {
    *this = tmp_board;
  } else {
    *result = tmp_board;
  }
  return true;
}


std::string Board::moveToAlgebraicNotation(const Move move) const {
  
  if(move.king_castle)
    return "0-0";
  else if(move.queen_castle)
    return "0-0-0";

  // Get all the start locations that can end in end location by the same piece (color and type)
  Piece piece = getPieceAt(move.start_file, move.start_rank);
  Color color = getPieceColor(piece);
  PieceType type = getPieceType(piece);
  std::vector<std::pair<uint8_t, uint8_t>> attackers;
 
  posAttacked(move.end_file, move.end_rank, getPieceColor(piece), getPieceType(piece),
      &attackers, false);

  if(attackers.size() == 0) {
    std::cerr << "error in: " << move.str() << std::endl;
    throw std::runtime_error("move seems impossible?");
  }
 
  std::array<std::string, 7> piece_names{"NONE", "", "R", "B", "N", "Q", "K"};

  bool captures = getPieceType(getPieceAt(move.end_file, move.end_rank)) != PieceType::NONE_TYPE;

  std::string start, end, connector, suffix;

  connector = captures? "x" : "";
  
  /*
  if(attackers.size() > 1) {
    fmt::print("Looking for {}\n", move.str());
    for(auto a : attackers){
      fmt::print("{},{}\n",a.first,a.second);
    }
  }
  */

  // No ambiguity
  if (attackers.size() == 1) {
    // Special case: when a pawn captures, we use the file they left from.
    if(type == PieceType::PAWN && captures) {
      start = kFileNames[move.start_file];
    }
    else start = piece_names[type];
  }
  // Single ambiguity: differentiate by file OR rank
  else if (attackers.size() == 2) {
    // case 1: files are different
    if(attackers[0].first != attackers[1].first) {
      start = piece_names[type] + kFileNames[move.start_file];
    } else { // cant be on same rank AND file
      start = piece_names[type] + kRankNames[move.start_rank];
    }
  } 
  // Multiple ambiguities: differentiate by one or both
  else {
    int identical_file_count = 0;
    int identical_rank_count = 0;

    // Count how many pieces are on the same rank or file
    for(const auto loc : attackers) {
      if(loc.first == move.start_file) ++identical_file_count;
      if(loc.second == move.start_rank) ++ identical_rank_count;
    }

    // Case 1: Only one piece on this file, it disambiguates enough
    if(identical_file_count == 1) {
      start = piece_names[type] + kFileNames[move.start_file];
    } else if (identical_rank_count == 1) { // Case 2: rank is enough
      start = piece_names[type] + kRankNames[move.start_rank];
    } else { // case 3: need both
      start = piece_names[type] + kFileNames[move.end_file] + kRankNames[move.start_rank];
    }
  }
  
  end = kFileNames[move.end_file] + kRankNames[move.end_rank];
 
  // Do the move to look for check
  Board tmp_board = *this;
  tmp_board.doMove(move, color);
  suffix = tmp_board.inCheck(static_cast<Color>(!color)) ? "+" : "";

  std::string promote = move.promotes_to == PieceType::NONE_TYPE ? 
    "" : getStrFromType(move.promotes_to); 

  return start + connector + end + suffix;
}

Move Board::moveFromAlgebraicNotation(const std::string str, Color color) const {
  if(str.length() < 2) throw std::runtime_error("length must be >= 2");
  
  if(str == "0-0") {
    Move result(0,0,0,0);
    result.king_castle = 0;
    return result;
  }

  if(str == "0-0-0") {
    Move result(0,0,0,0);
    result.queen_castle = 0;
    return result;
  }
  
  std::string s = str;

  // Remove suffix.
  if(s.back() == '+' || s.back() == '#')
    s.pop_back();

  // Get target location
  uint8_t end_rank = s.back() - '0' - 1; // ascii to int
  s.pop_back();
  uint8_t end_file = s.back() - 'a'; 
  s.pop_back();

  bool has_start_rank{false}, has_start_file{false};
  uint8_t start_rank, start_file;
  
  // maybe pop suffix
  if(!s.empty() && s.back() == 'x')
    s.pop_back();

  bool has_piece_type{false};
  PieceType piece_type = PieceType::PAWN;

  // Maybe get start things (pawns don't need)
  if(!s.empty()) {
    if(s.back() >= 'a'){ // is this a file?
      has_start_file = true;
      start_file = s.back() - 'a';
    } else if (s.back() >= 'A') { // Is this a piece?
      has_piece_type = true;
      piece_type = kTypeFromChar.at(s.back());
    } else { // must be a rank
      has_start_rank = true;
      start_rank = s.back() - '0' - 1;
    }
    s.pop_back();
  }

  if(!s.empty() && !has_start_file && !has_piece_type) { 
    has_start_rank = true;
    if(s.back() < 'a') {
      has_piece_type = true;
      piece_type = kTypeFromChar.at(s.back());
    } else {
      start_file = s.back() - 'a';
      has_start_file = true;
    }
    s.pop_back();
  }

  if(!s.empty()) { // if anything is left, it's a piece
    has_piece_type = true;
    piece_type = kTypeFromChar.at(s.back());
  }


  // If we have to deduce the start position
  if(!(has_start_rank && has_start_file)) {
    std::vector<std::pair<uint8_t, uint8_t>> attacking_pieces;

    // Get the list of pieces that can move there
    posAttacked(end_file, end_rank, color, piece_type, &attacking_pieces, false);
    
    auto matches_criteria = [&](std::pair<uint8_t, uint8_t> candidate) -> bool {
      return ((!has_start_file || candidate.first == start_file)
          && (!has_start_rank  || candidate.second == start_rank));
    };

    std::remove_if(attacking_pieces.begin(), attacking_pieces.end(), matches_criteria);
    
    // for(auto p : attacking_pieces) {
    //   fmt::print("Option: ({},{})\n", p.first, p.second);
    // }

    if(attacking_pieces.size() == 0)
      throw std::runtime_error("No matching move");
    else if(attacking_pieces.size() > 1)
      throw std::runtime_error("Ambiguous move");
    else {
      start_file = attacking_pieces.at(0).first;
      start_rank = attacking_pieces.at(0).second;
    }
  }


  Move result(start_file, start_rank, end_file, end_rank);

  // Check if en passant
  if(piece_type == PieceType::PAWN
      && start_file != end_file
      && isEmpty(end_file, end_rank))
  {
    result.is_en_passant = true;
  }

  // Check if enables en passant
  if(piece_type == PieceType::PAWN
      && std::abs((int)end_rank - (int)start_rank) == 2) {
    result.en_passant_flags = (1 << 3) | end_file;
  }

  return result;
}

std::string Board::formatMoveList(const MoveList moves) const {
  std::vector<std::string> out; 
  out.resize(moves.size());
  int i = 0;
  for(auto m : moves) {
    out.at(i) = (fmt::format("{}: {} [{}]", i, moveToAlgebraicNotation(m), m.str()));
    ++i;
  }
  return fmt::format("{}", fmt::join(out, "\n"));
}

void Board::checkForInvalidPawns() const {
  for(int i = 0; i < 8; ++i) {
    if(getPieceType(getPieceAt(i, 0)) == PieceType::PAWN 
        || getPieceType(getPieceAt(i, 7)) == PieceType::PAWN) {
      
      std::cerr << *this << std::endl;
      throw std::runtime_error("Pawn is dumb");
  
    }
  }
}

// http://www.cse.yorku.ca/~oz/hash.html
size_t Board::computeDJB2Hash() const {
  size_t hash = 5381;

  #pragma unroll
  for(int i = 0; i < 64; ++i) {
    hash = ((hash << 5) + hash) + data_[i]; // hash * 33 + c
  }

  // also the flags
  hash = ((hash << 5) + hash) + special_move_flags_;

  return hash;
}


// http://www.cse.yorku.ca/~oz/hash.html
size_t Board::computeSDBMHash() const {
  size_t hash = 0;
  
  #pragma unroll
  for(int i = 0; i < 64; ++i) {
    hash = data_[i] + (hash << 6) + (hash << 16) - hash;
  }

  hash = special_move_flags_ + (hash << 6) + (hash << 16) - hash;
  return hash;
}


// An unusually great post: https://softwareengineering.stackexchange.com/questions/49550/which-hashing-algorithm-is-best-for-uniqueness-and-speed
// TODO: consider murmur
size_t Board::computeHash() const {

#ifdef USE_HASH_DJB2
  return computeDJB2Hash(); 
#endif

#ifdef USE_HASH_SDBM  
  return computeSDBMHash(); 
#endif

}

} // namespace chess


