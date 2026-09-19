#include <iostream>
#include <string>
#include <algorithm>
#include <span>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <limits>

#include "utils.h"
#include "Board/board.h"
#include "MoveGenerator/MoveGenerator.h"
#include "Engine/transpositionTable.h"


using namespace bitboard_utils;
using namespace constants;
using namespace zobrist;

using std::cout;
using std::endl;
using board::board_state;
using std::string;


namespace board
{
    board_state make_move(board_state board, unsigned int move)
    {
        // remove old piece location
        int source = move_source(move);
        int target = move_target(move);
        int piece = move_piece(move);
        int enemy = board.side == white ? black : white;
        pop_bit(board.bitboards[piece], source);
        pop_bit(board.occupancies[board.side], source);
        set_bit(board.occupancies[board.side], target);
        board.zobrist_hash ^= zobrist_pieces[piece][source];

        // update castling rights
        board.zobrist_hash ^= zobrist_castle[board.castle];
        if (piece == R)
        {
            if (source == h1) board.castle &= ~wk;
            else if (source == a1) board.castle &= ~wq;
        }
        else if (piece == r)
        {
            if (source == h8) board.castle &= ~bk;
            else if (source == a8) board.castle &= ~bq;
        }
        if (piece == K) board.castle &= 0b0011;
        else if (piece == k) board.castle &= 0b1100;
        if (move_capture(move) == R) {
            if (target == h1) board.castle &= ~wk;
            if (target == a1) board.castle &= ~wq;
        } else if (move_capture(move) == r) {
            if (target == h8) board.castle &= ~bk;
            if (target == a8) board.castle &= ~bq;
        }
        board.zobrist_hash ^= zobrist_castle[board.castle];
        board.halfmove_clock = (piece == P || piece == p || move_capture(move) != no_piece)
            ? 0 : board.halfmove_clock + 1;
        if (board.side == black) ++board.fullmove_number;

        // set piece at new location considering possible promotion
        int promotion = move_promotion(move);
        if (promotion == no_promotion)
        {
            set_bit(board.bitboards[piece], target);
            board.zobrist_hash ^= zobrist_pieces[piece][target];
        }
        else
        {
            if (promotion == promotion_queen)
            {
                set_bit(board.side == white ? board.bitboards[Q] : board.bitboards[q], target);
                board.zobrist_hash ^= zobrist_pieces[board.side == white ? Q : q][target];
            }
            else if (promotion == promotion_rook)
            {
                set_bit(board.side == white ? board.bitboards[R] : board.bitboards[r], target);
                board.zobrist_hash ^= zobrist_pieces[board.side == white ? R : r][target];
            }
            else if (promotion == promotion_bishop)
            {
                set_bit(board.side == white ? board.bitboards[B] : board.bitboards[b], target);
                board.zobrist_hash ^= zobrist_pieces[board.side == white ? B : b][target];
            }
            else if (promotion == promotion_knight)
            {
                set_bit(board.side == white ? board.bitboards[N] : board.bitboards[n], target);
                board.zobrist_hash ^= zobrist_pieces[board.side == white ? N : n][target];
            }
        }

        // if move was a capture, remove captured piece considering en passant
        int taken_piece = move_capture(move);
        if (taken_piece != no_piece)
        {
            int piece_location = move_enpassant(move) ? (board.side == white ? target + 8 : target - 8) : target;
            pop_bit(board.occupancies[enemy], piece_location);
            pop_bit(board.bitboards[taken_piece], piece_location);
            board.zobrist_hash ^= zobrist_pieces[taken_piece][piece_location];
        }

        // set en passant square if double push
        if (board.enpassant != no_square) board.zobrist_hash ^= zobrist_enpassant[board.enpassant];
        if (move_double_push(move))
        {
            board.enpassant = board.side == white ? target + 8 : target - 8;
            board.zobrist_hash ^= zobrist_enpassant[board.enpassant];
        }
        else
        {
            board.enpassant = no_square;
        }

        // if move was a castle, move rook
        if (move_castle(move))
        {
            if (board.side == white)
            {
                if (target == g1)
                {
                    pop_bit(board.bitboards[R], h1);
                    pop_bit(board.occupancies[board.side], h1);
                    set_bit(board.bitboards[R], f1);
                    set_bit(board.occupancies[board.side], f1);
                    board.zobrist_hash ^= zobrist_pieces[R][h1];
                    board.zobrist_hash ^= zobrist_pieces[R][f1];
                }
                else if (target == c1)
                {
                    pop_bit(board.bitboards[R], a1);
                    pop_bit(board.occupancies[board.side], a1);
                    set_bit(board.bitboards[R], d1);
                    set_bit(board.occupancies[board.side], d1);
                    board.zobrist_hash ^= zobrist_pieces[R][a1];
                    board.zobrist_hash ^= zobrist_pieces[R][d1];
                }
            }
            else
            {
                if (target == g8)
                {
                    pop_bit(board.bitboards[r], h8);
                    pop_bit(board.occupancies[board.side], h8);
                    set_bit(board.bitboards[r], f8);
                    set_bit(board.occupancies[board.side], f8);
                    board.zobrist_hash ^= zobrist_pieces[r][h8];
                    board.zobrist_hash ^= zobrist_pieces[r][f8];
                }
                else if (target == c8)
                {
                    pop_bit(board.bitboards[r], a8);
                    pop_bit(board.occupancies[board.side], a8);
                    set_bit(board.bitboards[r], d8);
                    set_bit(board.occupancies[board.side], d8);
                    board.zobrist_hash ^= zobrist_pieces[r][a8];
                    board.zobrist_hash ^= zobrist_pieces[r][d8];
                }
            }
        }

        board.occupancies[both] = board.occupancies[white] | board.occupancies[black];

        // update the player to move
        board.side = board.side == white ? black : white;
        board.zobrist_hash ^= zobrist_side;

        return board;
    }

    int find_captured_piece(board_state &board, int square)
    {
        int pawn = board.side == white ? 6 : 0;
        int king = board.side == white ? 11 : 5;
        for (int taken_piece = pawn; taken_piece <= king; taken_piece++)
        {
            if (get_bit(board.bitboards[taken_piece], square))
            {
                return taken_piece;
            }
        }
        return no_piece;
    }

    int find_piece(board_state &board, int square)
    {
        for (int piece = P; piece <= k; piece++)
        {
            if (get_bit(board.bitboards[piece], square))
            {
                return piece;
            }
        }
        return no_piece;
    }

    bool is_promoting(board_state &board)
    {
        U64 mask = board.side == white ? 0xFF00 : 0x00FF000000000000;
        U64 pawns = board.side == white ? board.bitboards[P] : board.bitboards[p];
        U64 promoting_pawns = mask & pawns;
        return promoting_pawns != 0;
    }

    unsigned int encode_move(int source, int target, int piece, int promotion, int capture, bool double_push, bool enpassant, bool castle)
    {
        unsigned int move = source | (target << 6) | (piece << 12) | (promotion << 16) | (capture << 20) | (double_push << 24) | (enpassant << 25) | (castle << 26);
        return move;
    }

    unsigned int encode_move(board_state &board, string move)
    {
        std::transform(move.begin(), move.end(), move.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::array<unsigned int, max_moves> buffer;
        for (auto legal : move_generator::generate_moves(board, buffer, false))
            if (move_to_string(legal) == move) return legal;
        return invalid_move;
    }
    board_state make_null_move(board_state position) {
        if (position.enpassant != no_square) position.zobrist_hash ^= zobrist_enpassant[position.enpassant];
        position.enpassant = no_square;
        position.side = !position.side;
        position.zobrist_hash ^= zobrist_side;
        return position;
    }

    int move_source(unsigned int move)
    {
        return move & 0b111111;
    }

    int move_target(unsigned int move)
    {
        return (move >> 6) & 0b111111;
    }

    int move_piece(unsigned int move)
    {
        return (move >> 12) & 0b1111;
    }

    int move_promotion(unsigned int move)
    {
        return (move >> 16) & 0b1111;
    }

    int move_capture(unsigned int move)
    {
        return (move >> 20) & 0b1111;
    }

    bool move_double_push(unsigned int move)
    {
        return (move >> 24) & 1;
    }

    bool move_enpassant(unsigned int move)
    {
        return (move >> 25) & 1;
    }

    bool move_castle(unsigned int move)
    {
        return (move >> 26) & 1;
    }

    string move_to_string(unsigned int move)
    {
        if (move == invalid_move) return "0000";
        string result = square_to_coordinates[move_source(move)] + square_to_coordinates[move_target(move)];
        if (move_promotion(move) != no_promotion)
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(promotion_to_string[move_promotion(move)])));
        return result;
    }

    bool is_promotion(int piece, int target) {
        return (piece == P && target / 8 == 0) || (piece == p && target / 8 == 7);
    }

}

namespace board_utils
{
    board_state parse_fen(string fen)
    {
        std::istringstream input(fen);
        string placement, side, castling, ep, half, full, extra;
        if (!(input >> placement >> side >> castling >> ep)) throw std::invalid_argument("FEN requires at least four fields");
        board_state state{};
        state.enpassant = no_square;
        int rank = 0, file = 0;
        for (char c : placement) {
            if (c == '/') {
                if (file != 8 || rank >= 7) throw std::invalid_argument("Invalid FEN rank");
                ++rank; file = 0;
            } else if (c >= '1' && c <= '8') {
                file += c - '0';
                if (file > 8) throw std::invalid_argument("Invalid FEN rank width");
            } else {
                auto it = string_to_piece.find(c);
                if (it == string_to_piece.end() || file >= 8) throw std::invalid_argument("Invalid FEN piece");
                set_bit(state.bitboards[it->second], rank * 8 + file++);
            }
        }
        if (rank != 7 || file != 8 || (side != "w" && side != "b")) throw std::invalid_argument("Invalid FEN board or side");
        if (count_bits(state.bitboards[K]) != 1 || count_bits(state.bitboards[k]) != 1) throw std::invalid_argument("FEN must contain one king per side");
        for (int base : {0, 6}) {
            int pawns = count_bits(state.bitboards[base]);
            int promoted = std::max(0, count_bits(state.bitboards[base + N]) - 2)
                         + std::max(0, count_bits(state.bitboards[base + B]) - 2)
                         + std::max(0, count_bits(state.bitboards[base + R]) - 2)
                         + std::max(0, count_bits(state.bitboards[base + Q]) - 1);
            if (pawns > 8 || promoted > 8 - pawns) throw std::invalid_argument("Impossible FEN material count");
        }
        if ((state.bitboards[P] | state.bitboards[p]) & 0xff000000000000ffULL) throw std::invalid_argument("Pawn on back rank");
        state.side = side == "w" ? white : black;
        if (castling != "-") for (char c : castling) {
            int right = c == 'K' ? wk : c == 'Q' ? wq : c == 'k' ? bk : c == 'q' ? bq : 0;
            if (!right || (state.castle & right)) throw std::invalid_argument("Invalid castling rights");
            state.castle |= right;
        }
        if (ep != "-") {
            state.enpassant = string_to_square(ep);
            if (state.enpassant == no_square || state.enpassant / 8 != (state.side == white ? 2 : 5)) throw std::invalid_argument("Invalid en passant square");
        }
        auto counter = [](const string& token, int minimum) {
            size_t end = 0; int value = std::stoi(token, &end);
            if (end != token.size() || value < minimum || value > std::numeric_limits<int>::max() - 1024) throw std::invalid_argument("Invalid FEN counter");
            return value;
        };
        if (input >> half) {
            if (!(input >> full) || input >> extra) throw std::invalid_argument("Invalid FEN fields");
            state.halfmove_clock = counter(half, 0);
            state.fullmove_number = counter(full, 1);
        }
        for (int piece = P; piece <= k; ++piece) state.occupancies[piece < 6 ? white : black] |= state.bitboards[piece];
        state.occupancies[both] = state.occupancies[white] | state.occupancies[black];
        if (state.enpassant != no_square) {
            int captured = state.enpassant + (state.side == white ? 8 : -8);
            if (get_bit(state.occupancies[both], state.enpassant) || !get_bit(state.bitboards[state.side == white ? p : P], captured))
                throw std::invalid_argument("Invalid en passant pawn");
        }
        // The side that just moved cannot have left its own king in check.
        auto previous = state;
        previous.side = !state.side;
        if (move_generator::is_square_attacked(least_significant_bit_index(previous.bitboards[previous.side == white ? K : k]), previous))
            throw std::invalid_argument("FEN leaves the nonmoving king in check");
        state.zobrist_hash = get_zobrist_hash(state);
        return state;
    }

    U64 repetition_key(board_state state) {
        if (state.enpassant == no_square) return state.zobrist_hash;
        // En passant affects repetition only when a legal capture exists.
        std::array<unsigned int, max_moves> buffer;
        for (auto move : move_generator::generate_moves(state, buffer, false))
            if (board::move_enpassant(move)) return state.zobrist_hash;
        return state.zobrist_hash ^ zobrist_enpassant[state.enpassant];
    }

    U64 get_zobrist_hash(board_state &board)
    {
        U64 zobrist_hash = 0ULL;
        for (int piece = P; piece <= k; piece++)
        {
            U64 bitboard = board.bitboards[piece];
            while (bitboard)
            {
                int square = least_significant_bit_index(bitboard);
                zobrist_hash ^= zobrist_pieces[piece][square];
                pop_bit(bitboard, square);
            }
        }
        zobrist_hash ^= zobrist_castle[board.castle];
        if (board.side == black) zobrist_hash ^= zobrist_side;
        if (board.enpassant != no_square) zobrist_hash ^= zobrist_enpassant[board.enpassant];
        return zobrist_hash;
    }

    void print_board(board_state &state)
    {
        cout << endl;
        for (int rank = 0; rank < 8; rank++)
        {
            cout << 8 - rank << "   ";
            for (int file = 0; file < 8; file++)
            {
                int square = rank * 8 + file;
                int current_piece = -1;
                for (int piece = P; piece <= k; piece++)
                {
                    if (get_bit(state.bitboards[piece], square))
                        current_piece = piece;
                }
                cout << ((current_piece == -1) ? '.' : piece_to_string[current_piece]) << ' ';
            }
            cout << endl;
        }
        cout << endl;
        cout << "    " << "a b c d e f g h" << endl << endl << endl;

        cout << "    Side:          " << (state.side ? "black" : "white") << endl;
        cout << "    Enpassant:     " << (state.enpassant != no_square ? square_to_coordinates[state.enpassant] : "-") << endl;
        cout << "    Castling:      " << ((state.castle & wk) ? 'K' : '-') << ((state.castle & wq) ? 'Q' : '-') << ((state.castle & bk) ? 'k' : '-') << ((state.castle & bq) ? 'q' : '-') << endl;
        cout << "    Hash key:      " << state.zobrist_hash << endl << endl << endl;
    }

    void print_move_list(std::span<unsigned int> move_list)
    {
        for (auto& move : move_list)
        {
            std::cout << board::move_to_string(move) << std::endl;
        }
    }

    int string_to_square(const string& square) {
        if (square.length() != 2) return no_square;
        char fileChar = square[0];
        char rankChar = square[1];
        int col = fileChar - 'a';
        int row = '8' - rankChar;
        if (col < 0 || col > 7 || row < 0 || row > 7) return no_square;
        return (row * 8) + col;
    }
}
