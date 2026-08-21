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

constexpr char kFaces[8] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'};

class Memory final : public Game {
 public:
  const char* title() const override { return "Memory Match"; }
  SDL_Color accent() const override { return colors().memory; }
  Difficulty difficulty() const override { return difficulty_; }
  GameOutcome outcome() const override { return outcome_; }

  void reset(Difficulty difficulty) override {
    difficulty_ = difficulty;
    cards_.clear();
    for (char f : kFaces) {
      cards_.push_back(f);
      cards_.push_back(f);
    }
    std::shuffle(cards_.begin(), cards_.end(), rng_);
    matched_.fill(false);
    revealed_.fill(false);
    playerTurn_ = true;
    busy_ = false;
    timer_ = 0;
    phase_ = Phase::Idle;
    firstPick_ = -1;
    playerScore_ = cpuScore_ = 0;
    outcome_ = GameOutcome::None;
    memory_.clear();
    cpuPick_ = {-1, -1};
  }

  void onEvent(const SDL_Event& e) override {
    if (busy_ || outcome_ != GameOutcome::None || !playerTurn_) return;
    if (e.type != SDL_MOUSEBUTTONDOWN || e.button.button != SDL_BUTTON_LEFT) return;
    const int index = cardAt(e.button.x, e.button.y);
    if (index < 0 || matched_[static_cast<size_t>(index)] || revealed_[static_cast<size_t>(index)])
      return;
    revealed_[static_cast<size_t>(index)] = true;
    remember(index);
    if (firstPick_ < 0) {
      firstPick_ = index;
      return;
    }
    busy_ = true;
    phase_ = Phase::PlayerResolve;
    timer_ = 0.65;
    secondPick_ = index;
  }

  void update(double dt) override {
    if (!busy_) return;
    timer_ -= dt;
    if (timer_ > 0) return;

    if (phase_ == Phase::PlayerResolve) {
      resolvePair(firstPick_, secondPick_, true);
      firstPick_ = -1;
      if (outcome_ != GameOutcome::None) {
        busy_ = false;
        phase_ = Phase::Idle;
        return;
      }
      if (!playerTurn_) {
        phase_ = Phase::CpuFirst;
        timer_ = 0.35;
        cpuPick_ = pickCpu();
        return;
      }
      busy_ = false;
      phase_ = Phase::Idle;
      return;
    }

    if (phase_ == Phase::CpuFirst) {
      if (cpuPick_.first >= 0) {
        revealed_[static_cast<size_t>(cpuPick_.first)] = true;
        remember(cpuPick_.first);
      }
      phase_ = Phase::CpuSecond;
      timer_ = 0.45;
      return;
    }

    if (phase_ == Phase::CpuSecond) {
      if (cpuPick_.second >= 0) {
        revealed_[static_cast<size_t>(cpuPick_.second)] = true;
        remember(cpuPick_.second);
      }
      phase_ = Phase::CpuResolve;
      timer_ = 0.65;
      return;
    }

    if (phase_ == Phase::CpuResolve) {
      resolvePair(cpuPick_.first, cpuPick_.second, false);
      if (outcome_ != GameOutcome::None) {
        busy_ = false;
        phase_ = Phase::Idle;
        return;
      }
      if (!playerTurn_) {
        phase_ = Phase::CpuFirst;
        timer_ = 0.35;
        cpuPick_ = pickCpu();
        return;
      }
      busy_ = false;
      phase_ = Phase::Idle;
    }
  }

  void draw(SDL_Renderer* renderer, TTF_Font*, TTF_Font* fontLarge) override {
    const SDL_Rect frame = boardRect();
    const int gap = ui::S(10);
    const int cw = (frame.w - 3 * gap) / 4;
    const int ch = (frame.h - 3 * gap) / 4;
    for (int i = 0; i < 16; ++i) {
      const int r = i / 4, c = i % 4;
      SDL_Rect card{frame.x + c * (cw + gap), frame.y + r * (ch + gap), cw, ch};
      const bool open = revealed_[static_cast<size_t>(i)] || matched_[static_cast<size_t>(i)];
      SDL_Color bg = open ? colors().surfaceHigh : mix(colors().memory, colors().surface, 0.75f);
      if (matched_[static_cast<size_t>(i)]) bg = SDL_Color{80, 30, 50, 255};
      ui::fillRect(renderer, card, bg);
      ui::drawRect(renderer, card, colors().memory, 1);
      if (open) {
        ui::drawText(renderer, fontLarge, std::string(1, cards_[static_cast<size_t>(i)]),
                     card.x + card.w / 2, card.y + card.h / 2, colors().text, true);
      } else {
        ui::drawText(renderer, fontLarge, "?", card.x + card.w / 2, card.y + card.h / 2,
                     colors().memory, true);
      }
    }
  }

  std::string status() const override {
    if (outcome_ == GameOutcome::Win)
      return "You found more pairs (" + std::to_string(playerScore_) + "-" +
             std::to_string(cpuScore_) + ").";
    if (outcome_ == GameOutcome::Lose)
      return "Computer found more pairs (" + std::to_string(cpuScore_) + "-" +
             std::to_string(playerScore_) + ").";
    if (outcome_ == GameOutcome::Draw)
      return "Even pairs (" + std::to_string(playerScore_) + "-" + std::to_string(cpuScore_) + ").";
    if (!playerTurn_)
      return "Computer is matching...  You " + std::to_string(playerScore_) + " · CPU " +
             std::to_string(cpuScore_);
    return "Your turn — find a pair.  You " + std::to_string(playerScore_) + " · CPU " +
           std::to_string(cpuScore_);
  }

 private:
  enum class Phase { Idle, PlayerResolve, CpuFirst, CpuSecond, CpuResolve };

  static SDL_Rect boardRect() { return ui::SR(290, 160, 520, 520); }

  int cardAt(int x, int y) const {
    const SDL_Rect frame = boardRect();
    if (!ui::pointInRect(x, y, frame)) return -1;
    const int gap = ui::S(10);
    const int cw = (frame.w - 3 * gap) / 4;
    const int ch = (frame.h - 3 * gap) / 4;
    for (int i = 0; i < 16; ++i) {
      const int r = i / 4, c = i % 4;
      SDL_Rect card{frame.x + c * (cw + gap), frame.y + r * (ch + gap), cw, ch};
      if (ui::pointInRect(x, y, card)) return i;
    }
    return -1;
  }

  void remember(int index) {
    memory_.erase(std::remove_if(memory_.begin(), memory_.end(),
                                 [&](const auto& e) { return e.first == index; }),
                  memory_.end());
    memory_.push_back({index, cards_[static_cast<size_t>(index)]});
    const int cap = difficulty_ == Difficulty::Easy ? 2 : difficulty_ == Difficulty::Medium ? 6 : 16;
    while (static_cast<int>(memory_.size()) > cap) memory_.erase(memory_.begin());
  }

  void forget(int a, int b) {
    memory_.erase(std::remove_if(memory_.begin(), memory_.end(),
                                 [&](const auto& e) { return e.first == a || e.first == b; }),
                  memory_.end());
  }

  void resolvePair(int a, int b, bool player) {
    if (a < 0 || b < 0) return;
    if (a != b && cards_[static_cast<size_t>(a)] == cards_[static_cast<size_t>(b)]) {
      matched_[static_cast<size_t>(a)] = true;
      matched_[static_cast<size_t>(b)] = true;
      if (player) ++playerScore_;
      else ++cpuScore_;
      forget(a, b);
      if (!player) playerTurn_ = false;  // cpu continues
      else playerTurn_ = true;
    } else {
      revealed_[static_cast<size_t>(a)] = false;
      revealed_[static_cast<size_t>(b)] = false;
      playerTurn_ = !player;
    }
    if (std::all_of(matched_.begin(), matched_.end(), [](bool m) { return m; })) {
      if (playerScore_ > cpuScore_) outcome_ = GameOutcome::Win;
      else if (cpuScore_ > playerScore_) outcome_ = GameOutcome::Lose;
      else outcome_ = GameOutcome::Draw;
      playerTurn_ = true;
    }
  }

  std::pair<int, int> pickCpu() {
    std::vector<int> hidden;
    for (int i = 0; i < 16; ++i)
      if (!matched_[static_cast<size_t>(i)] && !revealed_[static_cast<size_t>(i)]) hidden.push_back(i);

    // Find known pair from memory.
    for (size_t i = 0; i < memory_.size(); ++i) {
      for (size_t j = i + 1; j < memory_.size(); ++j) {
        if (memory_[i].second == memory_[j].second && memory_[i].first != memory_[j].first &&
            !matched_[static_cast<size_t>(memory_[i].first)] &&
            !matched_[static_cast<size_t>(memory_[j].first)])
          return {memory_[i].first, memory_[j].first};
      }
    }

    std::shuffle(hidden.begin(), hidden.end(), rng_);
    if (hidden.empty()) return {-1, -1};
    const int first = hidden.front();
    if (difficulty_ != Difficulty::Easy) {
      for (const auto& entry : memory_) {
        if (entry.second == cards_[static_cast<size_t>(first)] && entry.first != first &&
            !matched_[static_cast<size_t>(entry.first)])
          return {first, entry.first};
      }
    }
    int second = first;
    for (int h : hidden) {
      if (h != first) {
        second = h;
        break;
      }
    }
    return {first, second};
  }

  std::vector<char> cards_;
  std::array<bool, 16> matched_{};
  std::array<bool, 16> revealed_{};
  Difficulty difficulty_ = Difficulty::Medium;
  bool playerTurn_ = true;
  bool busy_ = false;
  double timer_ = 0;
  Phase phase_ = Phase::Idle;
  int firstPick_ = -1;
  int secondPick_ = -1;
  int playerScore_ = 0;
  int cpuScore_ = 0;
  GameOutcome outcome_ = GameOutcome::None;
  std::vector<std::pair<int, char>> memory_;
  std::pair<int, int> cpuPick_{-1, -1};
  std::mt19937 rng_{std::random_device{}()};
};

}  // namespace

std::unique_ptr<Game> makeMemory() { return std::make_unique<Memory>(); }
