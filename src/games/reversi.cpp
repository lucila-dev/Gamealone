#include "game.hpp"
#include "theme.hpp"
#include "render.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kN = 8;

using Board = std::array<std::array<int, kN>, kN>;  // 0 empty, 1 player, 2 cpu

const int kDirs[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};

bool inside(int r, int c) { return r >= 0 && c >= 0 && r < kN && c < kN; }

bool wouldFlip(const Board& b, int r, int c, int who) {
  if (b[r][c] != 0) return false;
  const int enemy = 3 - who;
  for (const auto& d : kDirs) {
    int nr = r + d[0], nc = c + d[1];
    int seen = 0;
    while (inside(nr, nc) && b[nr][nc] == enemy) {
      ++seen;
      nr += d[0];
      nc += d[1];
    }
    if (seen > 0 && inside(nr, nc) && b[nr][nc] == who) return true;
  }
  return false;
}

std::vector<std::pair<int, int>> legalMoves(const Board& b, int who) {
  std::vector<std::pair<int, int>> moves;
  for (int r = 0; r < kN; ++r)
    for (int c = 0; c < kN; ++c)
      if (wouldFlip(b, r, c, who)) moves.push_back({r, c});
  return moves;
}

Board applyMove(Board b, int r, int c, int who) {
  b[r][c] = who;
  const int enemy = 3 - who;
  for (const auto& d : kDirs) {
    int nr = r + d[0], nc = c + d[1];
    std::vector<std::pair<int, int>> path;
    while (inside(nr, nc) && b[nr][nc] == enemy) {
      path.push_back({nr, nc});
      nr += d[0];
      nc += d[1];
    }
    if (!path.empty() && inside(nr, nc) && b[nr][nc] == who) {
      for (const auto& p : path) b[p.first][p.second] = who;
    }
  }
  return b;
}

int countPieces(const Board& b, int who) {
  int n = 0;
  for (const auto& row : b)
    for (int v : row)
      if (v == who) ++n;
  return n;
}

int evaluate(const Board& b) {
  static const int weight[8][8] = {
      {120, -20, 20, 5, 5, 20, -20, 120}, {-20, -40, -5, -5, -5, -5, -40, -20},
      {20, -5, 15, 3, 3, 15, -5, 20},     {5, -5, 3, 3, 3, 3, -5, 5},
      {5, -5, 3, 3, 3, 3, -5, 5},         {20, -5, 15, 3, 3, 15, -5, 20},
      {-20, -40, -5, -5, -5, -5, -40, -20}, {120, -20, 20, 5, 5, 20, -20, 120},
  };
  int score = 0;
  for (int r = 0; r < kN; ++r)
    for (int c = 0; c < kN; ++c) {
      if (b[r][c] == 2) score += weight[r][c];
      if (b[r][c] == 1) score -= weight[r][c];
    }
  score += static_cast<int>(legalMoves(b, 2).size()) * 3;
  score -= static_cast<int>(legalMoves(b, 1).size()) * 3;
  return score;
}

class Reversi final : public Game {
 public:
  const char* title() const override { return "Reversi"; }
  SDL_Color accent() const override { return colors().reversi; }
  Difficulty difficulty() const override { return difficulty_; }
  GameOutcome outcome() const override { return outcome_; }

  void reset(Difficulty difficulty) override {
    difficulty_ = difficulty;
    board_ = {};
    board_[3][3] = board_[4][4] = 2;
    board_[3][4] = board_[4][3] = 1;
    playerTurn_ = true;
    thinking_ = false;
    thinkTimer_ = 0;
    outcome_ = GameOutcome::None;
    message_ = "Your discs are teal. Place to flip the computer.";
  }

  void onEvent(const SDL_Event& e) override {
    if (outcome_ != GameOutcome::None || thinking_ || !playerTurn_) return;
    if (e.type != SDL_MOUSEBUTTONDOWN || e.button.button != SDL_BUTTON_LEFT) return;
    int r = 0, c = 0;
    if (!cellAt(e.button.x, e.button.y, r, c)) return;
    if (!wouldFlip(board_, r, c, 1)) return;
    board_ = applyMove(board_, r, c, 1);
    afterMove(true);
  }

  void update(double dt) override {
    if (!thinking_) return;
    thinkTimer_ -= dt;
    if (thinkTimer_ > 0) return;
    thinking_ = false;
    auto moves = legalMoves(board_, 2);
    if (!moves.empty()) {
      const auto choice = pickMove(moves);
      board_ = applyMove(board_, choice.first, choice.second, 2);
    }
    afterMove(false);
  }

  void draw(SDL_Renderer* renderer, TTF_Font* font, TTF_Font*) override {
    const SDL_Rect frame = boardRect();
    ui::fillRoundRect(renderer, frame, ui::S(12), SDL_Color{22, 101, 52, 255});
    const int cell = frame.w / kN;
    auto moves = playerTurn_ && !thinking_ ? legalMoves(board_, 1) : std::vector<std::pair<int, int>>{};
    for (int r = 0; r < kN; ++r) {
      for (int c = 0; c < kN; ++c) {
        SDL_Rect rect{frame.x + c * cell, frame.y + r * cell, cell - 1, cell - 1};
        ui::drawRect(renderer, rect, SDL_Color{21, 128, 61, 255}, 1);
        const bool legal =
            std::find(moves.begin(), moves.end(), std::make_pair(r, c)) != moves.end();
        if (legal) ui::fillCircle(renderer, rect.x + cell / 2, rect.y + cell / 2, ui::S(6),
                                  withAlpha(colors().reversi, 120));
        if (board_[r][c] == 1)
          ui::fillCircle(renderer, rect.x + cell / 2, rect.y + cell / 2, cell / 2 - ui::S(6),
                         colors().ticTacToe);
        if (board_[r][c] == 2)
          ui::fillCircle(renderer, rect.x + cell / 2, rect.y + cell / 2, cell / 2 - ui::S(6),
                         colors().text);
      }
    }
    ui::drawText(renderer, font,
                 "You " + std::to_string(countPieces(board_, 1)) + "  ·  CPU " +
                     std::to_string(countPieces(board_, 2)),
                 ui::S(480), ui::S(660), colors().muted, true);
  }

  std::string status() const override { return message_; }

 private:
  static SDL_Rect boardRect() { return ui::SR(300, 160, 500, 500); }

  bool cellAt(int x, int y, int& r, int& c) const {
    const SDL_Rect frame = boardRect();
    if (!ui::pointInRect(x, y, frame)) return false;
    const int cell = frame.w / kN;
    c = (x - frame.x) / cell;
    r = (y - frame.y) / cell;
    return inside(r, c);
  }

  void afterMove(bool afterPlayer) {
    const bool nextPlayer = !afterPlayer;
    auto nextMoves = legalMoves(board_, nextPlayer ? 1 : 2);
    auto otherMoves = legalMoves(board_, nextPlayer ? 2 : 1);
    if (nextMoves.empty() && otherMoves.empty()) {
      finish();
      return;
    }
    if (nextMoves.empty()) {
      // Skip turn.
      playerTurn_ = !nextPlayer;
      message_ = nextPlayer ? "No moves for you — computer continues."
                            : "Computer has no moves — your turn.";
      if (!playerTurn_) {
        thinking_ = true;
        thinkTimer_ = 0.35;
      }
      return;
    }
    playerTurn_ = nextPlayer;
    if (playerTurn_) {
      message_ = "Your turn — tap a highlighted square.";
    } else {
      message_ = "Computer is thinking...";
      thinking_ = true;
      thinkTimer_ = 0.4;
    }
  }

  void finish() {
    const int you = countPieces(board_, 1);
    const int cpu = countPieces(board_, 2);
    if (you > cpu) {
      outcome_ = GameOutcome::Win;
      message_ = "Board full — you have more discs.";
    } else if (cpu > you) {
      outcome_ = GameOutcome::Lose;
      message_ = "Board full — computer has more discs.";
    } else {
      outcome_ = GameOutcome::Draw;
      message_ = "Board full — draw.";
    }
    playerTurn_ = true;
  }

  std::pair<int, int> pickMove(const std::vector<std::pair<int, int>>& moves) {
    std::uniform_real_distribution<double> chance(0.0, 1.0);
    if (difficulty_ == Difficulty::Easy && chance(rng_) < 0.55) {
      return moves[static_cast<size_t>(
          std::uniform_int_distribution<int>(0, static_cast<int>(moves.size()) - 1)(rng_))];
    }
    const int depth = difficulty_ == Difficulty::Hard ? 3 : 2;
    int best = -100000;
    std::vector<std::pair<int, int>> choices = {moves.front()};
    for (const auto& m : moves) {
      Board next = applyMove(board_, m.first, m.second, 2);
      const int score = minimax(next, depth - 1, -100000, 100000, false);
      if (score > best) {
        best = score;
        choices = {m};
      } else if (score == best) {
        choices.push_back(m);
      }
    }
    return choices[static_cast<size_t>(
        std::uniform_int_distribution<int>(0, static_cast<int>(choices.size()) - 1)(rng_))];
  }

  int minimax(Board board, int depth, int alpha, int beta, bool maximizing) {
    auto cpuMoves = legalMoves(board, 2);
    auto playerMoves = legalMoves(board, 1);
    if (depth == 0 || (cpuMoves.empty() && playerMoves.empty())) return evaluate(board);

    if (maximizing) {
      if (cpuMoves.empty()) return minimax(board, depth - 1, alpha, beta, false);
      int value = -100000;
      for (const auto& m : cpuMoves) {
        value = std::max(value, minimax(applyMove(board, m.first, m.second, 2), depth - 1, alpha,
                                        beta, false));
        alpha = std::max(alpha, value);
        if (alpha >= beta) break;
      }
      return value;
    }
    if (playerMoves.empty()) return minimax(board, depth - 1, alpha, beta, true);
    int value = 100000;
    for (const auto& m : playerMoves) {
      value = std::min(value, minimax(applyMove(board, m.first, m.second, 1), depth - 1, alpha, beta,
                                      true));
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
  std::string message_;
  std::mt19937 rng_{std::random_device{}()};
};

}  // namespace

std::unique_ptr<Game> makeReversi() { return std::make_unique<Reversi>(); }
