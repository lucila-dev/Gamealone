#include "game.hpp"
#include "theme.hpp"
#include "render.hpp"

#include <memory>
#include <random>
#include <string>

namespace {

enum class Hand { Rock, Paper, Scissors };

const char* handName(Hand h) {
  switch (h) {
    case Hand::Rock: return "Rock";
    case Hand::Paper: return "Paper";
    case Hand::Scissors: return "Scissors";
  }
  return "?";
}

const char* handGlyph(Hand h) {
  switch (h) {
    case Hand::Rock: return "R";
    case Hand::Paper: return "P";
    case Hand::Scissors: return "S";
  }
  return "?";
}

int beats(Hand a, Hand b) {
  if (a == b) return 0;
  if ((a == Hand::Rock && b == Hand::Scissors) || (a == Hand::Paper && b == Hand::Rock) ||
      (a == Hand::Scissors && b == Hand::Paper))
    return 1;
  return -1;
}

class RockPaperScissors final : public Game {
 public:
  const char* title() const override { return "Rock Paper Scissors"; }
  SDL_Color accent() const override { return colors().rps; }
  Difficulty difficulty() const override { return difficulty_; }
  GameOutcome outcome() const override { return outcome_; }

  void reset(Difficulty difficulty) override {
    difficulty_ = difficulty;
    playerScore_ = cpuScore_ = 0;
    roundsToWin_ = difficulty == Difficulty::Easy ? 2 : difficulty == Difficulty::Medium ? 3 : 4;
    lastPlayer_ = lastCpu_ = Hand::Rock;
    hasLast_ = false;
    message_ = "Pick rock, paper, or scissors.";
    outcome_ = GameOutcome::None;
    thinking_ = false;
    thinkTimer_ = 0;
    pending_ = Hand::Rock;
  }

  void onEvent(const SDL_Event& e) override {
    if (outcome_ != GameOutcome::None || thinking_) return;
    if (e.type != SDL_MOUSEBUTTONDOWN || e.button.button != SDL_BUTTON_LEFT) return;
    for (int i = 0; i < 3; ++i) {
      if (ui::pointInRect(e.button.x, e.button.y, handRect(i))) {
        pending_ = static_cast<Hand>(i);
        thinking_ = true;
        thinkTimer_ = 0.35;
        message_ = "Computer is choosing...";
        return;
      }
    }
  }

  void update(double dt) override {
    if (!thinking_) return;
    thinkTimer_ -= dt;
    if (thinkTimer_ > 0) return;
    thinking_ = false;
    lastPlayer_ = pending_;
    lastCpu_ = pickCpu();
    hasLast_ = true;
    const int result = beats(lastPlayer_, lastCpu_);
    if (result > 0) {
      ++playerScore_;
      message_ = std::string("You win the round — ") + handName(lastPlayer_) + " beats " +
                 handName(lastCpu_) + ".";
    } else if (result < 0) {
      ++cpuScore_;
      message_ = std::string("Computer wins the round — ") + handName(lastCpu_) + " beats " +
                 handName(lastPlayer_) + ".";
    } else {
      message_ = "Tie round.";
    }
    if (playerScore_ >= roundsToWin_) {
      outcome_ = GameOutcome::Win;
      message_ = "You won the match!";
    } else if (cpuScore_ >= roundsToWin_) {
      outcome_ = GameOutcome::Lose;
      message_ = "Computer won the match.";
    }
  }

  void draw(SDL_Renderer* renderer, TTF_Font* font, TTF_Font* fontLarge) override {
    ui::drawText(renderer, fontLarge,
                 "First to " + std::to_string(roundsToWin_) + "  ·  You " +
                     std::to_string(playerScore_) + " — CPU " + std::to_string(cpuScore_),
                 ui::S(550), ui::S(160), colors().text, true);

    if (hasLast_) {
      ui::fillRoundRect(renderer, ui::SR(220, 210, 240, 150), ui::S(18), colors().surface);
      ui::fillRoundRect(renderer, ui::SR(640, 210, 240, 150), ui::S(18), colors().surface);
      ui::drawText(renderer, font, "You", ui::S(340), ui::S(235), colors().muted, true);
      ui::drawText(renderer, font, "CPU", ui::S(760), ui::S(235), colors().muted, true);
      ui::drawText(renderer, fontLarge, handGlyph(lastPlayer_), ui::S(340), ui::S(295),
                   colors().rps, true);
      ui::drawText(renderer, fontLarge, handGlyph(lastCpu_), ui::S(760), ui::S(295),
                   colors().primary, true);
      ui::drawText(renderer, font, handName(lastPlayer_), ui::S(340), ui::S(340), colors().text, true);
      ui::drawText(renderer, font, handName(lastCpu_), ui::S(760), ui::S(340), colors().text, true);
    }

    for (int i = 0; i < 3; ++i) {
      const SDL_Rect rect = handRect(i);
      ui::button(renderer, fontLarge, rect, handName(static_cast<Hand>(i)), colors().rps, false,
                 false);
    }
  }

  std::string status() const override { return message_; }

 private:
  static SDL_Rect handRect(int index) { return ui::SR(200 + index * 250, 520, 220, 72); }

  Hand pickCpu() {
    std::uniform_int_distribution<int> dist(0, 2);
    if (difficulty_ == Difficulty::Easy || !hasLast_) return static_cast<Hand>(dist(rng_));
    // Medium/Hard: sometimes counter the player's last move.
    std::uniform_real_distribution<double> chance(0.0, 1.0);
    const double bias = difficulty_ == Difficulty::Hard ? 0.55 : 0.3;
    if (chance(rng_) < bias) {
      if (lastPlayer_ == Hand::Rock) return Hand::Paper;
      if (lastPlayer_ == Hand::Paper) return Hand::Scissors;
      return Hand::Rock;
    }
    return static_cast<Hand>(dist(rng_));
  }

  Difficulty difficulty_ = Difficulty::Medium;
  int playerScore_ = 0;
  int cpuScore_ = 0;
  int roundsToWin_ = 3;
  Hand lastPlayer_ = Hand::Rock;
  Hand lastCpu_ = Hand::Rock;
  Hand pending_ = Hand::Rock;
  bool hasLast_ = false;
  bool thinking_ = false;
  double thinkTimer_ = 0;
  std::string message_;
  GameOutcome outcome_ = GameOutcome::None;
  std::mt19937 rng_{std::random_device{}()};
};

}  // namespace

std::unique_ptr<Game> makeRockPaperScissors() { return std::make_unique<RockPaperScissors>(); }
