#include "game.hpp"
#include "theme.hpp"
#include "render.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

enum class Piece : uint8_t { Empty, PlayerMan, PlayerKing, CpuMan, CpuKing };

struct Pos {
  int r = 0;
  int c = 0;
  bool operator==(const Pos& o) const { return r == o.r && c == o.c; }
};

struct Move {
  Pos from{};
  Pos to{};
  std::vector<Pos> captures;
  bool isCapture() const { return !captures.empty(); }
};

constexpr int kSize = 8;

bool isPlayer(Piece p) { return p == Piece::PlayerMan || p == Piece::PlayerKing; }
bool isCpu(Piece p) { return p == Piece::CpuMan || p == Piece::CpuKing; }
bool isKing(Piece p) { return p == Piece::PlayerKing || p == Piece::CpuKing; }
bool darkSquare(int r, int c) { return ((r + c) & 1) == 1; }
bool inside(Pos p) { return p.r >= 0 && p.c >= 0 && p.r < kSize && p.c < kSize; }

using Board = std::array<std::array<Piece, kSize>, kSize>;

Board initialBoard() {
  Board b{};
  for (int r = 0; r < kSize; ++r)
    for (int c = 0; c < kSize; ++c) {
      if (!darkSquare(r, c)) continue;
      if (r <= 2) b[r][c] = Piece::CpuMan;
      if (r >= 5) b[r][c] = Piece::PlayerMan;
    }
  return b;
}

bool wouldPromote(Piece piece, Pos to) {
  return (piece == Piece::PlayerMan && to.r == 0) || (piece == Piece::CpuMan && to.r == 7);
}

std::vector<std::pair<int, int>> dirsFor(Piece piece) {
  if (piece == Piece::PlayerMan) return {{-1, -1}, {-1, 1}};
  if (piece == Piece::CpuMan) return {{1, -1}, {1, 1}};
  return {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
}

void searchCaptures(Board& board, Pos origin, Pos current, Piece piece, std::vector<Pos> captured,
                    std::vector<Move>& out) {
  bool found = false;
  for (const auto& d : dirsFor(piece)) {
    Pos mid{current.r + d.first, current.c + d.second};
    Pos to{current.r + 2 * d.first, current.c + 2 * d.second};
    if (!inside(to) || !inside(mid)) continue;
    if (board[to.r][to.c] != Piece::Empty) continue;
    const Piece victim = board[mid.r][mid.c];
    if (victim == Piece::Empty) continue;
    if (isPlayer(piece) && !isCpu(victim)) continue;
    if (isCpu(piece) && !isPlayer(victim)) continue;
    if (std::find(captured.begin(), captured.end(), mid) != captured.end()) continue;

    found = true;
    auto nextCaptured = captured;
    nextCaptured.push_back(mid);
    if (wouldPromote(piece, to)) {
      out.push_back(Move{origin, to, nextCaptured});
      continue;
    }
    const Piece savedCurrent = board[current.r][current.c];
    const Piece savedMid = board[mid.r][mid.c];
    board[current.r][current.c] = Piece::Empty;
    board[mid.r][mid.c] = Piece::Empty;
    board[to.r][to.c] = piece;
    searchCaptures(board, origin, to, piece, nextCaptured, out);
    board[current.r][current.c] = savedCurrent;
    board[mid.r][mid.c] = savedMid;
    board[to.r][to.c] = Piece::Empty;
  }
  if (!found && !captured.empty()) out.push_back(Move{origin, current, captured});
}

std::vector<Move> legalMoves(Board board, bool player) {
  std::vector<Move> captures, steps;
  for (int r = 0; r < kSize; ++r) {
    for (int c = 0; c < kSize; ++c) {
      const Piece piece = board[r][c];
      if (piece == Piece::Empty) continue;
      if (player && !isPlayer(piece)) continue;
      if (!player && !isCpu(piece)) continue;
      searchCaptures(board, {r, c}, {r, c}, piece, {}, captures);
      for (const auto& d : dirsFor(piece)) {
        Pos to{r + d.first, c + d.second};
        if (inside(to) && board[to.r][to.c] == Piece::Empty)
          steps.push_back(Move{{r, c}, to, {}});
      }
    }
  }
  return captures.empty() ? steps : captures;
}

Board applyMove(Board board, const Move& move) {
  Piece piece = board[move.from.r][move.from.c];
  board[move.from.r][move.from.c] = Piece::Empty;
  for (const auto& cap : move.captures) board[cap.r][cap.c] = Piece::Empty;
  if (wouldPromote(piece, move.to))
    piece = isPlayer(piece) ? Piece::PlayerKing : Piece::CpuKing;
  board[move.to.r][move.to.c] = piece;
  return board;
}

int evaluate(const Board& board) {
  int score = 0;
  for (int r = 0; r < kSize; ++r)
    for (int c = 0; c < kSize; ++c) {
      switch (board[r][c]) {
        case Piece::CpuMan: score += 10 + r; break;
        case Piece::CpuKing: score += 18; break;
        case Piece::PlayerMan: score -= 10 + (7 - r); break;
        case Piece::PlayerKing: score -= 18; break;
        default: break;
      }
    }
  return score;
}

enum class End { None, Player, Cpu };

End winner(const Board& board, bool playerToMove) {
  if (legalMoves(board, playerToMove).empty()) return playerToMove ? End::Cpu : End::Player;
  int playerPieces = 0, cpuPieces = 0;
  for (const auto& row : board)
    for (const auto p : row) {
      if (isPlayer(p)) ++playerPieces;
      if (isCpu(p)) ++cpuPieces;
    }
  if (playerPieces == 0) return End::Cpu;
  if (cpuPieces == 0) return End::Player;
  return End::None;
}

class Checkers final : public Game {
 public:
  const char* title() const override { return "Checkers"; }
  SDL_Color accent() const override { return colors().checkers; }
  Difficulty difficulty() const override { return difficulty_; }
  GameOutcome outcome() const override { return outcome_; }

  void reset(Difficulty difficulty) override {
    difficulty_ = difficulty;
    board_ = initialBoard();
    playerTurn_ = true;
    thinking_ = false;
    thinkTimer_ = 0;
    outcome_ = GameOutcome::None;
    hasSelected_ = false;
  }

  void onEvent(const SDL_Event& e) override {
    if (outcome_ != GameOutcome::None || thinking_ || !playerTurn_) return;
    if (e.type != SDL_MOUSEBUTTONDOWN || e.button.button != SDL_BUTTON_LEFT) return;
    Pos pos;
    if (!posAt(e.button.x, e.button.y, pos)) return;
    auto moves = legalMoves(board_, true);

    if (hasSelected_) {
      for (const auto& m : moves) {
        if (m.from == selected_ && m.to == pos) {
          board_ = applyMove(board_, m);
          hasSelected_ = false;
          evaluateOutcome(true);
          if (outcome_ == GameOutcome::None) {
            playerTurn_ = false;
            thinking_ = true;
            thinkTimer_ = 0.4;
          }
          return;
        }
      }
    }
    if (isPlayer(board_[pos.r][pos.c]) &&
        std::any_of(moves.begin(), moves.end(), [&](const Move& m) { return m.from == pos; })) {
      selected_ = pos;
      hasSelected_ = true;
    } else {
      hasSelected_ = false;
    }
  }

  void update(double dt) override {
    if (!thinking_) return;
    thinkTimer_ -= dt;
    if (thinkTimer_ > 0) return;
    thinking_ = false;
    auto moves = legalMoves(board_, false);
    if (!moves.empty()) board_ = applyMove(board_, pick(moves));
    evaluateOutcome(false);
    playerTurn_ = outcome_ == GameOutcome::None;
  }

  void draw(SDL_Renderer* renderer, TTF_Font*, TTF_Font*) override {
    const SDL_Rect frame = boardRect();
    const int cell = frame.w / kSize;
    auto moves = legalMoves(board_, playerTurn_);
    std::vector<Pos> dests;
    if (hasSelected_)
      for (const auto& m : moves)
        if (m.from == selected_) dests.push_back(m.to);

    for (int r = 0; r < kSize; ++r) {
      for (int c = 0; c < kSize; ++c) {
        SDL_Rect rect{frame.x + c * cell, frame.y + r * cell, cell, cell};
        SDL_Color color = darkSquare(r, c) ? SDL_Color{63, 46, 34, 255} : SDL_Color{231, 211, 176, 255};
        if (hasSelected_ && selected_ == Pos{r, c})
          color = SDL_Color{180, 70, 70, 255};
        if (std::find(dests.begin(), dests.end(), Pos{r, c}) != dests.end())
          color = SDL_Color{74, 180, 100, 255};
        ui::fillRect(renderer, rect, color);
        const Piece p = board_[r][c];
        if (p == Piece::Empty) continue;
        const SDL_Color pieceColor = isPlayer(p) ? SDL_Color{239, 68, 68, 255} : SDL_Color{17, 24, 39, 255};
        ui::fillCircle(renderer, rect.x + cell / 2, rect.y + cell / 2, cell / 2 - ui::S(8), pieceColor);
        if (isKing(p))
          ui::drawRect(renderer, SDL_Rect{rect.x + ui::S(12), rect.y + ui::S(12), cell - ui::S(24), cell - ui::S(24)},
                       colors().connectFour, 3);
      }
    }
  }

  std::string status() const override {
    if (outcome_ == GameOutcome::Win) return "You win.";
    if (outcome_ == GameOutcome::Lose) return "Computer wins.";
    if (thinking_ || !playerTurn_) return "Computer is moving...";
    if (hasSelected_) return "Tap a highlighted square.";
    auto moves = legalMoves(board_, true);
    if (std::any_of(moves.begin(), moves.end(), [](const Move& m) { return m.isCapture(); }))
      return "Your turn — a capture is required.";
    return "Your turn — tap a red piece.";
  }

 private:
  static SDL_Rect boardRect() { return ui::SR(240, 130, 480, 480); }

  bool posAt(int x, int y, Pos& out) const {
    const SDL_Rect frame = boardRect();
    if (!ui::pointInRect(x, y, frame)) return false;
    const int cell = frame.w / kSize;
    out.c = (x - frame.x) / cell;
    out.r = (y - frame.y) / cell;
    return inside(out);
  }

  void evaluateOutcome(bool afterPlayer) {
    const End end = winner(board_, !afterPlayer);
    if (end == End::Player) outcome_ = GameOutcome::Win;
    if (end == End::Cpu) outcome_ = GameOutcome::Lose;
  }

  Move pick(const std::vector<Move>& moves) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    if (difficulty_ == Difficulty::Easy && dist(rng_) < 0.6) {
      return moves[static_cast<size_t>(
          std::uniform_int_distribution<int>(0, static_cast<int>(moves.size()) - 1)(rng_))];
    }
    const int depth = difficulty_ == Difficulty::Easy ? 1 : difficulty_ == Difficulty::Medium ? 2 : 3;
    int best = -100000;
    std::vector<Move> choices = {moves.front()};
    for (const auto& move : moves) {
      Board next = applyMove(board_, move);
      const int score = minimax(next, depth - 1, -100000, 100000, false);
      if (score > best) {
        best = score;
        choices = {move};
      } else if (score == best) {
        choices.push_back(move);
      }
    }
    return choices[static_cast<size_t>(
        std::uniform_int_distribution<int>(0, static_cast<int>(choices.size()) - 1)(rng_))];
  }

  int minimax(Board board, int depth, int alpha, int beta, bool playerTurn) {
    const End end = winner(board, playerTurn);
    if (end == End::Cpu) return 1000 + depth;
    if (end == End::Player) return -1000 - depth;
    if (depth == 0) return evaluate(board);
    auto moves = legalMoves(board, playerTurn);
    if (moves.empty()) return playerTurn ? -1000 : 1000;

    if (!playerTurn) {
      int value = -100000;
      for (const auto& move : moves) {
        value = std::max(value, minimax(applyMove(board, move), depth - 1, alpha, beta, true));
        alpha = std::max(alpha, value);
        if (alpha >= beta) break;
      }
      return value;
    }
    int value = 100000;
    for (const auto& move : moves) {
      value = std::min(value, minimax(applyMove(board, move), depth - 1, alpha, beta, false));
      beta = std::min(beta, value);
      if (alpha >= beta) break;
    }
    return value;
  }

  Board board_{};
  Difficulty difficulty_ = Difficulty::Medium;
  bool playerTurn_ = true;
  bool thinking_ = false;
  double thinkTimer_ = 0;
  GameOutcome outcome_ = GameOutcome::None;
  bool hasSelected_ = false;
  Pos selected_{};
  std::mt19937 rng_{std::random_device{}()};
};

}  // namespace

std::unique_ptr<Game> makeCheckers() { return std::make_unique<Checkers>(); }
