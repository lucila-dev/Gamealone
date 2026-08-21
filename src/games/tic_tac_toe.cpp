#include "game.hpp"
#include "theme.hpp"
#include "render.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <random>
#include <vector>

namespace {

class TicTacToe final : public Game {
 public:
  const char* title() const override { return "Tic-Tac-Toe"; }
  SDL_Color accent() const override { return colors().ticTacToe; }
  Difficulty difficulty() const override { return difficulty_; }
  GameOutcome outcome() const override { return outcome_; }

  void reset(Difficulty difficulty) override {
    difficulty_ = difficulty;
    board_.fill(0);
    playerTurn_ = true;
    outcome_ = GameOutcome::None;
    thinkTimer_ = 0;
    thinking_ = false;
  }

  void onEvent(const SDL_Event& e) override {
    if (outcome_ != GameOutcome::None || thinking_ || !playerTurn_) return;
    if (e.type != SDL_MOUSEBUTTONDOWN || e.button.button != SDL_BUTTON_LEFT) return;
    const int idx = cellAt(e.button.x, e.button.y);
    if (idx < 0 || board_[static_cast<size_t>(idx)] != 0) return;
    board_[static_cast<size_t>(idx)] = 1;
    evaluate();
    if (outcome_ == GameOutcome::None) {
      playerTurn_ = false;
      thinking_ = true;
      thinkTimer_ = 0.35;
    }
  }

  void update(double dt) override {
    if (!thinking_) return;
    thinkTimer_ -= dt;
    if (thinkTimer_ > 0) return;
    thinking_ = false;
    const int move = pickMove();
    if (move >= 0) board_[static_cast<size_t>(move)] = 2;
    evaluate();
    playerTurn_ = outcome_ == GameOutcome::None;
  }

  void draw(SDL_Renderer* renderer, TTF_Font* font, TTF_Font* fontLarge) override {
    (void)font;
    const SDL_Rect board = boardRect();
    ui::fillRoundRect(renderer, board, ui::S(18), colors().surface);
    ui::drawRoundRect(renderer, board, ui::S(18), colors().ticTacToe, std::max(2, ui::S(2)));
    const int gap = ui::S(12);
    const int cell = (board.w - 4 * gap) / 3;
    for (int i = 0; i < 9; ++i) {
      const int r = i / 3, c = i % 3;
      SDL_Rect rect{board.x + gap + c * (cell + gap), board.y + gap + r * (cell + gap), cell, cell};
      ui::fillRoundRect(renderer, rect, ui::S(14), colors().surfaceHigh);
      if (board_[static_cast<size_t>(i)] == 1) {
        ui::drawText(renderer, fontLarge, "X", rect.x + rect.w / 2, rect.y + rect.h / 2,
                     colors().ticTacToe, true);
      } else if (board_[static_cast<size_t>(i)] == 2) {
        ui::drawText(renderer, fontLarge, "O", rect.x + rect.w / 2, rect.y + rect.h / 2,
                     colors().memory, true);
      }
    }
  }

  std::string status() const override {
    if (outcome_ == GameOutcome::Win) return "You got three in a row.";
    if (outcome_ == GameOutcome::Lose) return "The computer got three in a row.";
    if (outcome_ == GameOutcome::Draw) return "The board is full.";
    if (thinking_ || !playerTurn_) return "Computer is thinking...";
    return "Your turn — you are X.";
  }

 private:
  static SDL_Rect boardRect() { return ui::SR(280, 150, 400, 400); }

  int cellAt(int x, int y) const {
    const SDL_Rect board = boardRect();
    if (!ui::pointInRect(x, y, board)) return -1;
    const int gap = ui::S(12);
    const int cell = (board.w - 4 * gap) / 3;
    const int lx = x - board.x - gap;
    const int ly = y - board.y - gap;
    if (lx < 0 || ly < 0) return -1;
    const int c = lx / (cell + gap);
    const int r = ly / (cell + gap);
    if (c < 0 || c > 2 || r < 0 || r > 2) return -1;
    const int ox = lx % (cell + gap);
    const int oy = ly % (cell + gap);
    if (ox >= cell || oy >= cell) return -1;
    return r * 3 + c;
  }

  void evaluate() {
    const int w = winnerOf(board_);
    if (w == 1) outcome_ = GameOutcome::Win;
    else if (w == 2) outcome_ = GameOutcome::Lose;
    else if (std::all_of(board_.begin(), board_.end(), [](int v) { return v != 0; }))
      outcome_ = GameOutcome::Draw;
  }

  static int winnerOf(const std::array<int, 9>& b) {
    static const int lines[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6},
                                    {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};
    for (const auto& line : lines) {
      const int a = b[static_cast<size_t>(line[0])];
      if (a && a == b[static_cast<size_t>(line[1])] && a == b[static_cast<size_t>(line[2])]) return a;
    }
    return 0;
  }

  int pickMove() {
    std::vector<int> empties;
    for (int i = 0; i < 9; ++i)
      if (board_[static_cast<size_t>(i)] == 0) empties.push_back(i);
    if (empties.empty()) return -1;

    std::uniform_real_distribution<double> dist(0.0, 1.0);
    if (difficulty_ == Difficulty::Easy && dist(rng_) < 0.7)
      return empties[static_cast<size_t>(std::uniform_int_distribution<int>(
          0, static_cast<int>(empties.size()) - 1)(rng_))];
    if (difficulty_ == Difficulty::Medium && dist(rng_) < 0.35)
      return empties[static_cast<size_t>(std::uniform_int_distribution<int>(
          0, static_cast<int>(empties.size()) - 1)(rng_))];
    return minimaxMove();
  }

  int minimaxMove() {
    int bestScore = -999;
    int bestMove = 0;
    for (int i = 0; i < 9; ++i) {
      if (board_[static_cast<size_t>(i)] != 0) continue;
      board_[static_cast<size_t>(i)] = 2;
      const int score = minimax(false);
      board_[static_cast<size_t>(i)] = 0;
      if (score > bestScore) {
        bestScore = score;
        bestMove = i;
      }
    }
    return bestMove;
  }

  int minimax(bool maximizing) {
    const int w = winnerOf(board_);
    if (w == 2) return 10;
    if (w == 1) return -10;
    if (std::all_of(board_.begin(), board_.end(), [](int v) { return v != 0; })) return 0;

    if (maximizing) {
      int best = -999;
      for (int i = 0; i < 9; ++i) {
        if (board_[static_cast<size_t>(i)] != 0) continue;
        board_[static_cast<size_t>(i)] = 2;
        best = std::max(best, minimax(false));
        board_[static_cast<size_t>(i)] = 0;
      }
      return best;
    }
    int best = 999;
    for (int i = 0; i < 9; ++i) {
      if (board_[static_cast<size_t>(i)] != 0) continue;
      board_[static_cast<size_t>(i)] = 1;
      best = std::min(best, minimax(true));
      board_[static_cast<size_t>(i)] = 0;
    }
    return best;
  }

  std::array<int, 9> board_{};
  Difficulty difficulty_ = Difficulty::Medium;
  bool playerTurn_ = true;
  bool thinking_ = false;
  double thinkTimer_ = 0;
  GameOutcome outcome_ = GameOutcome::None;
  std::mt19937 rng_{std::random_device{}()};
};

}  // namespace

std::unique_ptr<Game> makeTicTacToe() { return std::make_unique<TicTacToe>(); }
