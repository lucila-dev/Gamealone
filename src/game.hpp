#pragma once

#include "theme.hpp"

#include <SDL.h>
#include <SDL_ttf.h>

#include <memory>
#include <string>

enum class Difficulty { Easy, Medium, Hard };
enum class GameOutcome { None, Win, Lose, Draw };

inline const char* difficultyLabel(Difficulty d) {
  switch (d) {
    case Difficulty::Easy: return "Easy";
    case Difficulty::Medium: return "Medium";
    case Difficulty::Hard: return "Hard";
  }
  return "Medium";
}

inline const char* outcomeLabel(GameOutcome o) {
  switch (o) {
    case GameOutcome::Win: return "You win!";
    case GameOutcome::Lose: return "Computer wins";
    case GameOutcome::Draw: return "Draw";
    case GameOutcome::None: return "";
  }
  return "";
}

class Game {
 public:
  virtual ~Game() = default;
  virtual const char* title() const = 0;
  virtual SDL_Color accent() const = 0;
  virtual void reset(Difficulty difficulty) = 0;
  virtual void onEvent(const SDL_Event& e) = 0;
  virtual void update(double dt) = 0;
  virtual void draw(SDL_Renderer* renderer, TTF_Font* font, TTF_Font* fontLarge) = 0;
  virtual std::string status() const = 0;
  virtual GameOutcome outcome() const = 0;
  virtual Difficulty difficulty() const = 0;
  virtual bool requestMenu() const { return false; }
  virtual void clearMenuRequest() {}
};

std::unique_ptr<Game> makeTicTacToe();
std::unique_ptr<Game> makeBattleship();
std::unique_ptr<Game> makeConnectFour();
std::unique_ptr<Game> makeCheckers();
std::unique_ptr<Game> makeHangman();
std::unique_ptr<Game> makeMemory();
std::unique_ptr<Game> makeRockPaperScissors();
std::unique_ptr<Game> makeReversi();
