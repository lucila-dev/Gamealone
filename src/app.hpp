#pragma once

#include "game.hpp"

#include <memory>
#include <string>
#include <vector>

class App {
 public:
  bool init();
  void run();
  void shutdown();

 private:
  enum class Screen { Landing, Hub, Rules, Game };

  struct HubItem {
    const char* name;
    const char* subtitle;
    const char* rules;
    SDL_Color (*accent)();
    std::unique_ptr<Game> (*factory)();
  };

  bool refreshScaleAndFonts();
  void loadWindowIcon();
  void handleEvent(const SDL_Event& e);
  void update(double dt);
  void draw();
  void drawBackground();
  void drawLanding();
  void drawHub();
  void drawRulesScreen();
  void drawGameChrome();
  void selectGame(int index);
  void startSelectedGame();
  void backToHub();
  void mapMouse(SDL_Event& e);
  void clampRulesScroll();

  SDL_Rect themeButtonRect() const;
  SDL_Rect backButtonRect() const;
  SDL_Rect resetButtonRect() const;
  SDL_Rect difficultyChipRect(int index) const;
  SDL_Rect playButtonRect() const;
  SDL_Rect homeButtonRect() const;
  SDL_Rect hubCardRect(int index) const;
  SDL_Rect rulesStartRect() const;
  SDL_Rect rulesBackRect() const;

  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  TTF_Font* font_ = nullptr;
  TTF_Font* fontLarge_ = nullptr;
  TTF_Font* fontTitle_ = nullptr;
  TTF_Font* fontHero_ = nullptr;
  SDL_Surface* iconSurface_ = nullptr;

  Screen screen_ = Screen::Landing;
  Difficulty difficulty_ = Difficulty::Medium;
  std::unique_ptr<Game> game_;
  std::vector<HubItem> items_;
  int activeGameIndex_ = -1;
  int rulesScroll_ = 0;
  int rulesContentH_ = 0;
  bool running_ = true;
  int mouseX_ = 0;
  int mouseY_ = 0;
  float dpiScale_ = 1.0f;
  int outputW_ = 1100;
  int outputH_ = 820;
};
