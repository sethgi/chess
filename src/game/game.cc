#include <fmt/format.h>
#include <boost/program_options.hpp>

#include "move_generator/move_generator.cc"

using namespace chess;
namespace po = boost::program_options;

int main(int argc, char** argv) {
  std::string fname;
  bool is_black{false};

  po::options_description desc{"Options"};
  desc.add_options()
    ("board-file", po::value<std::string>(&fname), "File with board desc"),
    ("start-black", po::bool_switch(&is_black), "Start with black move");

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
  po::notify(vm);

  Board b(fname);
  MoveGenerator move_gen(b);
  Color player = Color::WHITE;

  while(true) {
    std::string move;
    std::cout << "alg: ";
    std::cin >> move;

    Move m = b.moveFromAlgebraicNotation(move, player);

    std::cout << m.str() << std::endl;
    std::cout << (int)m.king_castle << std::endl;
    std::cout << (int)m.queen_castle << std::endl;
    fmt::print("{0:x}\n", m.en_passant_flags);
    /*
    auto moves = move_gen.getMovesForPlayer(player);

    std::cout << b << std::endl;
    fmt::print("{}'s turn\n", player==Color::WHITE? "White" : "Black");
    std::cout << b.formatMoveList(moves) << std::endl;

    int move_idx;
    std::cout << "Enter Move Index: ";
    std::cin >> move_idx;
    bool legal = b.doMove(moves[move_idx], player); 
    std::cout << "Legal: " << (legal ? "YES" : "NO") << std::endl;
    if(!legal) continue;
    
    player = static_cast<Color>(!player);
    */
  }
}
