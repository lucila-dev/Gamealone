#include "game.hpp"
#include "theme.hpp"
#include "render.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kRows = 6;
constexpr int kCols = 7;

class ConnectFour final : public Game {
 public:
  const char* title() const override { return "Connect Four"; }
  SDL_Color accent() const override { return colors().connectFour; }
  Difficulty difficulty() const override { return difficulty_; }
  GameOutcome outcome() const override { return outcome_; }

  void reset(Difficulty difficulty) override {
    difficulty_ = difficulty;
    for (auto& row : board_) row.fill(0);
    playerTurn_ = true;
    thinking_ = false;
    thinkTimer_ = 0;
    outcome_ = GameOutcome::None;
  }

  void onEvent(const SDL_Event& e) override {
    if (outcome_ != GameOutcome::None || thinking_ || !playerTurn_) return;
    if (e.type != SDL_MOUSEBUTTONDOWN || e.button.button != SDL_BUTTON_LEFT) return;
    const int col = colAt(e.button.x, e.button.y);
    if (col < 0) return;
    if (!drop(col, 1)) return;
    evaluate();
    if (outcome_ == GameOutcome::None) {
      playerTurn_ = false;
      thinking_ = true;
      thinkTimer_ = 0.3;
    }
  }

  void update(double dt) override {
    if (!thinking_) return;
    thinkTimer_ -= dt;
    if (thinkTimer_ > 0) return;
    thinking_ = false;
    const int col = pickColumn();
    drop(col, 2);
    evaluate();
    playerTurn_ = outcome_ == GameOutcome::None;
  }

  void draw(SDL_Renderer* renderer, TTF_Font*, TTF_Font*) override {
    const SDL_Rect frame = boardRect();
    ui::fillRect(renderer, frame, SDL_Color{29, 78, 216, 255});
    ui::drawRect(renderer, frame, colors().connectFour, 2);
    const float cw = frame.w / static_cast<float>(kCols);
    const float rh = frame.h / static_cast<float>(kRows);
    for (int r = 0; r < kRows; ++r) {
      for (int c = 0; c < kCols; ++c) {
        const int cx = frame.x + static_cast<int>((c + 0.5f) * cw);
        const int cy = frame.y + static_cast<int>((kRows - 1 - r + 0.5f) * rh);
        const int v = board_[static_cast<size_t>(r)][static_cast<size_t>(c)];
        SDL_Color color = colors().bg;
        if (v == 1) color = colors().connectFour;
        if (v == 2) color = SDL_Color{251, 113, 133, 255};
        ui::fillCircle(renderer, cx, cy, static_cast<int>(std::min(cw, rh) * 0.38f), color);
      }
    }
  }

  std::string status() const override {
    if (outcome_ == GameOutcome::Win) return "You connected four.";
    if (outcome_ == GameOutcome::Lose) return "The computer connected four.";
    if (outcome_ == GameOutcome::Draw) return "The grid is full.";
    if (thinking_ || !playerTurn_) return "Computer is dropping a disc...";
    return "Your turn — tap a column.";
  }

 private:
  using Board = std::array<std::array<int, kCols>, kRows>;

  static SDL_Rect boardRect() { return ui::SR(250, 160, 600, 460); }

  int colAt(int x, int y) const {
    const SDL_Rect frame = boardRect();
    if (!ui::pointInRect(x, y, frame)) return -1;
    return (x - frame.x) * kCols / frame.w;
  }

  bool drop(int col, int who) {
    if (col < 0 || col >= kCols) return false;
    for (int r = 0; r < kRows; ++r) {
      if (board_[static_cast<size_t>(r)][static_cast<size_t>(col)] == 0) {
        board_[static_cast<size_t>(r)][static_cast<size_t>(col)] = who;
        return true;
      }
    }
    return false;
  }

  static int dropRow(const Board& b, int col) {
    for (int r = 0; r < kRows; ++r)
      if (b[static_cast<size_t>(r)][static_cast<size_t>(col)] == 0) return r;
    return -1;
  }

  static std::vector<int> legal(const Board& b) {
    std::vector<int> cols;
    for (int c = 0; c < kCols; ++c)
      if (b[kRows - 1][static_cast<size_t>(c)] == 0) cols.push_back(c);
    return cols;
  }

  static int winnerOf(const Board& b) {
    auto at = [&](int r, int c) {
      if (r < 0 || c < 0 || r >= kRows || c >= kCols) return 0;
      return b[static_cast<size_t>(r)][static_cast<size_t>(c)];
    };
    for (int r = 0; r < kRows; ++r) {
      for (int c = 0; c < kCols; ++c) {
        const int who = at(r, c);
        if (!who) continue;
        if (at(r, c + 1) == who && at(r, c + 2) == who && at(r, c + 3) == who) return who;
        if (at(r + 1, c) == who && at(r + 2, c) == who && at(r + 3, c) == who) return who;
        if (at(r + 1, c + 1) == who && at(r + 2, c + 2) == who && at(r + 3, c + 3) == who) return who;
        if (at(r - 1, c + 1) == who && at(r - 2, c + 2) == who && at(r - 3, c + 3) == who) return who;
      }
    }
    return 0;
  }

  void evaluate() {
    const int w = winnerOf(board_);
    if (w == 1) outcome_ = GameOutcome::Win;
    else if (w == 2) outcome_ = GameOutcome::Lose;
    else if (legal(board_).empty()) outcome_ = GameOutcome::Draw;
  }

  int pickColumn() {
    auto cols = legal(board_);
    if (cols.empty()) return 0;
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    const int depth = difficulty_ == Difficulty::Easy ? 2 : difficulty_ == Difficulty::Medium ? 4 : 5;
    if (difficulty_ == Difficulty::Easy && dist(rng_) < 0.55) {
      return cols[static_cast<size_t>(
          std::uniform_int_distribution<int>(0, static_cast<int>(cols.size()) - 1)(rng_))];
    }
    int bestScore = -100000;
    std::vector<int> best = {cols.front()};
    for (int col : cols) {
      const int row = dropRow(board_, col);
      board_[static_cast<size_t>(row)][static_cast<size_t>(col)] = 2;
      const int score = minimax(depth - 1, -100000, 100000, false);
      board_[static_cast<size_t>(row)][static_cast<size_t>(col)] = 0;
      if (score > bestScore) {
        bestScore = score;
        best = {col};
      } else if (score == bestScore) {
        best.push_back(col);
      }
    }
    return best[static_cast<size_t>(
        std::uniform_int_distribution<int>(0, static_cast<int>(best.size()) - 1)(rng_))];
  }

  int minimax(int depth, int alpha, int beta, bool maximizing) {
    const int w = winnerOf(board_);
    if (w == 2) return 10000 + depth;
    if (w == 1) return -10000 - depth;
    auto cols = legal(board_);
    if (depth == 0 || cols.empty()) return evaluateBoard();

    if (maximizing) {
      int value = -100000;
      for (int col : cols) {
        const int row = dropRow(board_, col);
        board_[static_cast<size_t>(row)][static_cast<size_t>(col)] = 2;
        value = std::max(value, minimax(depth - 1, alpha, beta, false));
        board_[static_cast<size_t>(row)][static_cast<size_t>(col)] = 0;
        alpha = std::max(alpha, value);
        if (alpha >= beta) break;
      }
      return value;
    }
    int value = 100000;
    for (int col : cols) {
      const int row = dropRow(board_, col);
      board_[static_cast<size_t>(row)][static_cast<size_t>(col)] = 1;
      value = std::min(value, minimax(depth - 1, alpha, beta, true));
      board_[static_cast<size_t>(row)][static_cast<size_t>(col)] = 0;
      beta = std::min(beta, value);
      if (alpha >= beta) break;
    }
    return value;
  }

  int evaluateBoard() const {
    int score = 0;
    for (int r = 0; r < kRows; ++r) {
      if (board_[static_cast<size_t>(r)][3] == 2) score += 3;
      if (board_[static_cast<size_t>(r)][3] == 1) score -= 3;
    }
    return score + windows(2) - windows(1);
  }

  int windows(int who) const {
    int score = 0;
    auto scoreLine = [&](int a, int b, int c, int d) {
      const int vals[4] = {a, b, c, d};
      int count = 0, empty = 0, enemy = 0;
      for (int v : vals) {
        if (v == who) ++count;
        else if (v == 0) ++empty;
        else ++enemy;
      }
      if (enemy) return;
      if (count == 3 && empty == 1) score += 80;
      if (count == 2 && empty == 2) score += 8;
      if (count == 1 && empty == 3) score += 1;
    };
    for (int r = 0; r < kRows; ++r)
      for (int c = 0; c < kCols - 3; ++c)
        scoreLine(board_[r][c], board_[r][c + 1], board_[r][c + 2], board_[r][c + 3]);
    for (int c = 0; c < kCols; ++c)
      for (int r = 0; r < kRows - 3; ++r)
        scoreLine(board_[r][c], board_[r + 1][c], board_[r + 2][c], board_[r + 3][c]);
    for (int r = 0; r < kRows - 3; ++r)
      for (int c = 0; c < kCols - 3; ++c) {
        scoreLine(board_[r][c], board_[r + 1][c + 1], board_[r + 2][c + 2], board_[r + 3][c + 3]);
        scoreLine(board_[r + 3][c], board_[r + 2][c + 1], board_[r + 1][c + 2], board_[r][c + 3]);
      }
    return score;
  }

  Board board_{};
  Difficulty difficulty_ = Difficulty::Medium;
  bool playerTurn_ = true;
  bool thinking_ = false;
  double thinkTimer_ = 0;
  GameOutcome outcome_ = GameOutcome::None;
  std::mt19937 rng_{std::random_device{}()};
};

}  // namespace

std::unique_ptr<Game> makeConnectFour() { return std::make_unique<ConnectFour>(); }
