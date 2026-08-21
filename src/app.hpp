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
  enum class Screen { Landing, Hub, Game };

  struct HubItem {
    const char* name;
    const char* subtitle;
    const char* rules;
    SDL_Color (*accent)();
    std::unique_ptr<Game> (*factory)();
  };

  bool refreshScaleAndFonts();
  void handleEvent(const SDL_Event& e);
  void update(double dt);
  void draw();
  void drawBackground();
  void drawLanding();
  void drawHub();
  void drawGameChrome();
  void drawRulesOverlay();
  void openGame(int index);
  void openRules(int index);
  void backToHub();
  void mapMouse(SDL_Event& e);

  SDL_Rect themeButtonRect() const;
  SDL_Rect backButtonRect() const;
  SDL_Rect resetButtonRect() const;
  SDL_Rect rulesButtonRect() const;
  SDL_Rect difficultyChipRect(int index) const;
  SDL_Rect playButtonRect() const;
  SDL_Rect homeButtonRect() const;
  SDL_Rect hubCardRect(int index) const;
  SDL_Rect hubRulesChipRect(int index) const;
  SDL_Rect rulesCloseRect() const;

  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  TTF_Font* font_ = nullptr;
  TTF_Font* fontLarge_ = nullptr;
  TTF_Font* fontTitle_ = nullptr;
  TTF_Font* fontHero_ = nullptr;

  Screen screen_ = Screen::Landing;
  Difficulty difficulty_ = Difficulty::Medium;
  std::unique_ptr<Game> game_;
  std::vector<HubItem> items_;
  int activeGameIndex_ = -1;
  bool showRules_ = false;
  int rulesIndex_ = -1;
  bool running_ = true;
  int mouseX_ = 0;
  int mouseY_ = 0;
  float dpiScale_ = 1.0f;
  int outputW_ = 960;
  int outputH_ = 720;
};
