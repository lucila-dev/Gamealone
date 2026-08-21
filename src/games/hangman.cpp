#include "game.hpp"
#include "theme.hpp"
#include "render.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

const std::vector<std::string> kEasy = {"CAT", "DOG", "SUN", "FISH", "BOOK", "TREE", "GAME",
                                        "STAR", "MOON", "BIRD", "CAKE", "SHIP", "FROG", "LAMP"};
const std::vector<std::string> kMedium = {"PUZZLE", "PLANET", "CASTLE", "GUITAR", "BRIDGE",
                                          "GARDEN", "ROCKET", "WINTER", "ISLAND", "DRAGON",
                                          "PIRATE", "HAMMER", "FOREST", "BANANA"};
const std::vector<std::string> kHard = {"LABYRINTH", "SYMPHONY", "NIGHTMARE", "ADVENTURE",
                                        "KNOWLEDGE", "MYSTERIOUS", "CROCODILE", "ASTRONAUT",
                                        "ALGORITHM", "CHAMPAGNE", "QUICKSAND"};

class Hangman final : public Game {
 public:
  const char* title() const override { return "Hangman"; }
  SDL_Color accent() const override { return colors().hangman; }
  Difficulty difficulty() const override { return difficulty_; }
  GameOutcome outcome() const override { return outcome_; }

  void reset(Difficulty difficulty) override {
    difficulty_ = difficulty;
    const auto& list = difficulty == Difficulty::Easy     ? kEasy
                       : difficulty == Difficulty::Medium ? kMedium
                                                          : kHard;
    word_ = list[static_cast<size_t>(
        std::uniform_int_distribution<int>(0, static_cast<int>(list.size()) - 1)(rng_))];
    guessed_.clear();
    outcome_ = GameOutcome::None;
  }

  void onEvent(const SDL_Event& e) override {
    if (outcome_ != GameOutcome::None) return;
    char letter = 0;
    if (e.type == SDL_KEYDOWN) {
      const SDL_Keycode key = e.key.keysym.sym;
      if (key >= SDLK_a && key <= SDLK_z) letter = static_cast<char>('A' + (key - SDLK_a));
    } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
      letter = keyAt(e.button.x, e.button.y);
    }
    if (!letter) return;
    if (guessed_.count(letter)) return;
    guessed_.insert(letter);
    if (std::all_of(word_.begin(), word_.end(), [&](char c) { return guessed_.count(c); }))
      outcome_ = GameOutcome::Win;
    else if (misses() >= 6)
      outcome_ = GameOutcome::Lose;
  }

  void update(double) override {}

  void draw(SDL_Renderer* renderer, TTF_Font* font, TTF_Font* fontLarge) override {
    drawGallows(renderer, misses());
    std::string masked;
    for (char c : word_) {
      masked.push_back(guessed_.count(c) ? c : '_');
      masked.push_back(' ');
    }
    ui::drawText(renderer, fontLarge, masked, ui::S(550), ui::S(280), colors().text, true);

    for (int i = 0; i < 26; ++i) {
      const char letter = static_cast<char>('A' + i);
      const SDL_Rect key = keyRect(i);
      const bool used = guessed_.count(letter);
      SDL_Color accent = colors().hangman;
      if (used) accent = word_.find(letter) != std::string::npos ? SDL_Color{22, 101, 52, 255}
                                                                : SDL_Color{127, 29, 29, 255};
      ui::button(renderer, font, key, std::string(1, letter), accent, false, used);
    }
  }

  std::string status() const override {
    if (outcome_ == GameOutcome::Win) return "You guessed it: " + word_;
    if (outcome_ == GameOutcome::Lose) return "The word was " + word_ + ".";
    return "Guess a letter — " + std::to_string(6 - misses()) + " misses left.";
  }

 private:
  int misses() const {
    int n = 0;
    for (char g : guessed_)
      if (word_.find(g) == std::string::npos) ++n;
    return n;
  }

  static SDL_Rect keyRect(int index) {
    const int cols = 13;
    const int row = index / cols;
    const int col = index % cols;
    return ui::SR(120 + col * 64, 520 + row * 58, 54, 46);
  }

  char keyAt(int x, int y) const {
    for (int i = 0; i < 26; ++i)
      if (ui::pointInRect(x, y, keyRect(i))) return static_cast<char>('A' + i);
    return 0;
  }

  void drawGallows(SDL_Renderer* renderer, int misses) {
    SDL_SetRenderDrawColor(renderer, colors().muted.r, colors().muted.g, colors().muted.b, 255);
    SDL_RenderDrawLine(renderer, ui::S(140), ui::S(450), ui::S(360), ui::S(450));
    SDL_RenderDrawLine(renderer, ui::S(180), ui::S(450), ui::S(180), ui::S(170));
    SDL_RenderDrawLine(renderer, ui::S(180), ui::S(170), ui::S(290), ui::S(170));
    SDL_RenderDrawLine(renderer, ui::S(290), ui::S(170), ui::S(290), ui::S(205));
    SDL_SetRenderDrawColor(renderer, colors().hangman.r, colors().hangman.g, colors().hangman.b, 255);
    if (misses >= 1) ui::fillCircle(renderer, ui::S(290), ui::S(230), ui::S(22), colors().hangman);
    if (misses >= 2) SDL_RenderDrawLine(renderer, ui::S(290), ui::S(252), ui::S(290), ui::S(330));
    if (misses >= 3) SDL_RenderDrawLine(renderer, ui::S(290), ui::S(275), ui::S(255), ui::S(310));
    if (misses >= 4) SDL_RenderDrawLine(renderer, ui::S(290), ui::S(275), ui::S(325), ui::S(310));
    if (misses >= 5) SDL_RenderDrawLine(renderer, ui::S(290), ui::S(330), ui::S(260), ui::S(390));
    if (misses >= 6) SDL_RenderDrawLine(renderer, ui::S(290), ui::S(330), ui::S(320), ui::S(390));
  }

  Difficulty difficulty_ = Difficulty::Medium;
  std::string word_;
  std::unordered_set<char> guessed_;
  GameOutcome outcome_ = GameOutcome::None;
  std::mt19937 rng_{std::random_device{}()};
};

}  // namespace

std::unique_ptr<Game> makeHangman() { return std::make_unique<Hangman>(); }
