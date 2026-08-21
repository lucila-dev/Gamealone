#include "app.hpp"

#include "render.hpp"
#include "theme.hpp"

#include <algorithm>
#include <iostream>

namespace {

constexpr int kWidth = 1000;
constexpr int kHeight = 740;

const char* kFontPath = "/System/Library/Fonts/Supplemental/Arial Unicode.ttf";
const char* kFontFallback = "/System/Library/Fonts/Supplemental/Arial.ttf";

SDL_Color accentTicTacToe() { return colors().ticTacToe; }
SDL_Color accentBattleship() { return colors().battleship; }
SDL_Color accentConnectFour() { return colors().connectFour; }
SDL_Color accentCheckers() { return colors().checkers; }
SDL_Color accentHangman() { return colors().hangman; }
SDL_Color accentMemory() { return colors().memory; }
SDL_Color accentRps() { return colors().rps; }
SDL_Color accentReversi() { return colors().reversi; }

TTF_Font* openFont(const char* path, int pt) {
  TTF_Font* font = TTF_OpenFont(path, pt);
  if (!font) font = TTF_OpenFont(kFontFallback, pt);
  return font;
}

}  // namespace

bool App::refreshScaleAndFonts() {
  int windowW = 0, windowH = 0, outputW = 0, outputH = 0;
  SDL_GetWindowSize(window_, &windowW, &windowH);
  SDL_GetRendererOutputSize(renderer_, &outputW, &outputH);
  if (windowW <= 0 || windowH <= 0) return false;

  dpiScale_ = static_cast<float>(outputW) / static_cast<float>(windowW);
  if (dpiScale_ < 1.0f) dpiScale_ = 1.0f;
  ui::setScale(dpiScale_);
  outputW_ = outputW;
  outputH_ = outputH;

  if (fontHero_) TTF_CloseFont(fontHero_);
  if (fontTitle_) TTF_CloseFont(fontTitle_);
  if (fontLarge_) TTF_CloseFont(fontLarge_);
  if (font_) TTF_CloseFont(font_);

  font_ = openFont(kFontPath, std::max(14, static_cast<int>(std::lround(17.0 * dpiScale_))));
  fontLarge_ = openFont(kFontPath, std::max(20, static_cast<int>(std::lround(26.0 * dpiScale_))));
  fontTitle_ = openFont(kFontPath, std::max(28, static_cast<int>(std::lround(40.0 * dpiScale_))));
  fontHero_ = openFont(kFontPath, std::max(36, static_cast<int>(std::lround(64.0 * dpiScale_))));
  return font_ && fontLarge_ && fontTitle_ && fontHero_;
}

bool App::init() {
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
    return false;
  }
  if (TTF_Init() != 0) {
    std::cerr << "TTF_Init failed: " << TTF_GetError() << '\n';
    return false;
  }

  window_ = SDL_CreateWindow("Gamealone", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kWidth,
                             kHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
  if (!window_) {
    std::cerr << "CreateWindow failed: " << SDL_GetError() << '\n';
    return false;
  }

  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer_) {
    std::cerr << "CreateRenderer failed: " << SDL_GetError() << '\n';
    return false;
  }
  SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

  if (!refreshScaleAndFonts()) {
    std::cerr << "OpenFont failed: " << TTF_GetError() << '\n';
    return false;
  }

  items_ = {
      {"Tic-Tac-Toe", "3 in a row",
       "Goal: get three of your marks in a row.\n\n"
       "You are X. The computer is O.\n"
       "Take turns placing a mark on an empty square.\n"
       "Win with any row, column, or diagonal of three.\n"
       "Hard mode uses unbeatable minimax play.",
       accentTicTacToe, makeTicTacToe},
      {"Battleship", "Sink the fleet",
       "Goal: sink every computer ship before yours are sunk.\n\n"
       "1) Place all five ships on your grid (or tap Random).\n"
       "2) Press Start battle.\n"
       "3) Fire on the computer grid — hits mark ships, misses splash.\n"
       "Ships: Carrier 5, Battleship 4, Cruiser 3, Submarine 3, Destroyer 2.\n"
       "Harder difficulties hunt smarter after a hit.",
       accentBattleship, makeBattleship},
      {"Connect Four", "Drop four",
       "Goal: connect four of your discs in a line.\n\n"
       "Click a column to drop your disc.\n"
       "Discs stack from the bottom.\n"
       "Win with four in a row horizontally, vertically, or diagonally.\n"
       "Harder settings look more moves ahead.",
       accentConnectFour, makeConnectFour},
      {"Checkers", "Jump to win",
       "Goal: capture all computer pieces or leave it with no moves.\n\n"
       "You move the red pieces upward.\n"
       "Men move diagonally forward; kings move either way.\n"
       "Jumps are required when available — multi-jumps continue.\n"
       "Reach the far row to crown a king.",
       accentCheckers, makeCheckers},
      {"Hangman", "Guess the word",
       "Goal: reveal the hidden word before the figure is complete.\n\n"
       "The computer picks a word by difficulty.\n"
       "Click letters (or type A–Z) to guess.\n"
       "Correct letters fill in; wrong guesses add a body part.\n"
       "Six wrong guesses and you lose.",
       accentHangman, makeHangman},
      {"Memory Match", "Find pairs",
       "Goal: collect more matching pairs than the computer.\n\n"
       "Flip two cards each turn.\n"
       "A match stays open and scores a point; then you go again.\n"
       "A miss flips them back and ends your turn.\n"
       "Harder AI remembers more cards it has seen.",
       accentMemory, makeMemory},
      {"Rock Paper Scissors", "Best of rounds",
       "Goal: win enough rounds before the computer does.\n\n"
       "Rock beats Scissors, Scissors beats Paper, Paper beats Rock.\n"
       "Easy is first to 2, Medium to 3, Hard to 4.\n"
       "Harder AI sometimes counters your previous choice.",
       accentRps, makeRockPaperScissors},
      {"Reversi", "Flip the board",
       "Goal: have more discs than the computer when the board fills.\n\n"
       "You place teal discs; computer places light discs.\n"
       "A legal move must sandwich enemy discs in a straight line.\n"
       "Those discs flip to your color.\n"
       "If you have no move, your turn is skipped.",
       accentReversi, makeReversi},
  };
  return true;
}

void App::run() {
  Uint64 prev = SDL_GetPerformanceCounter();
  while (running_) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_WINDOWEVENT &&
          (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
           e.window.event == SDL_WINDOWEVENT_RESIZED)) {
        refreshScaleAndFonts();
      }
      mapMouse(e);
      handleEvent(e);
    }
    const Uint64 now = SDL_GetPerformanceCounter();
    const double dt =
        static_cast<double>(now - prev) / static_cast<double>(SDL_GetPerformanceFrequency());
    prev = now;
    update(dt);
    draw();
  }
}

void App::shutdown() {
  game_.reset();
  if (fontHero_) TTF_CloseFont(fontHero_);
  if (fontTitle_) TTF_CloseFont(fontTitle_);
  if (fontLarge_) TTF_CloseFont(fontLarge_);
  if (font_) TTF_CloseFont(font_);
  if (renderer_) SDL_DestroyRenderer(renderer_);
  if (window_) SDL_DestroyWindow(window_);
  TTF_Quit();
  SDL_Quit();
}

void App::mapMouse(SDL_Event& e) {
  if (e.type == SDL_MOUSEMOTION) {
    e.motion.x = static_cast<Sint32>(std::lround(e.motion.x * dpiScale_));
    e.motion.y = static_cast<Sint32>(std::lround(e.motion.y * dpiScale_));
    mouseX_ = e.motion.x;
    mouseY_ = e.motion.y;
  } else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
    e.button.x = static_cast<Sint32>(std::lround(e.button.x * dpiScale_));
    e.button.y = static_cast<Sint32>(std::lround(e.button.y * dpiScale_));
  }
}

SDL_Rect App::themeButtonRect() const { return ui::SR(kWidth - 150, 18, 130, 40); }
SDL_Rect App::backButtonRect() const { return ui::SR(24, 18, 100, 40); }
SDL_Rect App::resetButtonRect() const { return ui::SR(kWidth - 300, 18, 100, 40); }
SDL_Rect App::rulesButtonRect() const { return ui::SR(kWidth - 420, 18, 100, 40); }
SDL_Rect App::difficultyChipRect(int index) const {
  return ui::SR(kWidth / 2 - 170 + index * 115, 72, 100, 34);
}
SDL_Rect App::playButtonRect() const { return ui::SR(kWidth / 2 - 120, 390, 240, 64); }
SDL_Rect App::homeButtonRect() const { return ui::SR(24, 18, 110, 40); }
SDL_Rect App::rulesCloseRect() const { return ui::SR(kWidth / 2 + 210, 150, 40, 40); }

SDL_Rect App::hubCardRect(int index) const {
  const int cols = 4;
  const int cardW = 220;
  const int cardH = 150;
  const int gap = 16;
  const int startX = (kWidth - (cols * cardW + (cols - 1) * gap)) / 2;
  const int startY = 150;
  const int col = index % cols;
  const int row = index / cols;
  return ui::SR(startX + col * (cardW + gap), startY + row * (cardH + gap), cardW, cardH);
}

SDL_Rect App::hubRulesChipRect(int index) const {
  const SDL_Rect card = hubCardRect(index);
  return SDL_Rect{card.x + card.w - ui::S(70), card.y + ui::S(12), ui::S(58), ui::S(28)};
}

void App::openGame(int index) {
  if (index < 0 || index >= static_cast<int>(items_.size())) return;
  activeGameIndex_ = index;
  game_ = items_[static_cast<size_t>(index)].factory();
  game_->reset(difficulty_);
  showRules_ = false;
  screen_ = Screen::Game;
}

void App::openRules(int index) {
  if (index < 0 || index >= static_cast<int>(items_.size())) return;
  rulesIndex_ = index;
  showRules_ = true;
}

void App::backToHub() {
  game_.reset();
  activeGameIndex_ = -1;
  showRules_ = false;
  screen_ = Screen::Hub;
}

void App::handleEvent(const SDL_Event& e) {
  if (e.type == SDL_QUIT) {
    running_ = false;
    return;
  }

  if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_t) {
    toggleThemeMode();
    return;
  }

  if (showRules_) {
    if (e.type == SDL_KEYDOWN &&
        (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_RETURN)) {
      showRules_ = false;
      return;
    }
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
      const SDL_Rect panel = ui::SR(180, 140, 640, 460);
      if (ui::pointInRect(e.button.x, e.button.y, rulesCloseRect()) ||
          !ui::pointInRect(e.button.x, e.button.y, panel)) {
        showRules_ = false;
      }
    }
    return;
  }

  if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
    if (ui::pointInRect(e.button.x, e.button.y, themeButtonRect())) {
      toggleThemeMode();
      return;
    }
  }

  if (screen_ == Screen::Landing) {
    if (e.type == SDL_KEYDOWN) {
      if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_SPACE) {
        screen_ = Screen::Hub;
        return;
      }
      if (e.key.keysym.sym == SDLK_ESCAPE) {
        running_ = false;
        return;
      }
    }
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
      if (ui::pointInRect(e.button.x, e.button.y, playButtonRect())) screen_ = Screen::Hub;
    }
    return;
  }

  if (screen_ == Screen::Hub) {
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
      screen_ = Screen::Landing;
      return;
    }
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
      if (ui::pointInRect(e.button.x, e.button.y, homeButtonRect())) {
        screen_ = Screen::Landing;
        return;
      }
      for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        if (ui::pointInRect(e.button.x, e.button.y, hubRulesChipRect(i))) {
          openRules(i);
          return;
        }
        if (ui::pointInRect(e.button.x, e.button.y, hubCardRect(i))) {
          openGame(i);
          return;
        }
      }
    }
    return;
  }

  // Game screen
  if (e.type == SDL_KEYDOWN) {
    switch (e.key.keysym.sym) {
      case SDLK_ESCAPE:
        backToHub();
        return;
      case SDLK_h:
        if (activeGameIndex_ >= 0) openRules(activeGameIndex_);
        return;
      case SDLK_r:
        if (game_) game_->reset(difficulty_);
        return;
      case SDLK_1:
        difficulty_ = Difficulty::Easy;
        if (game_) game_->reset(difficulty_);
        return;
      case SDLK_2:
        difficulty_ = Difficulty::Medium;
        if (game_) game_->reset(difficulty_);
        return;
      case SDLK_3:
        difficulty_ = Difficulty::Hard;
        if (game_) game_->reset(difficulty_);
        return;
      default:
        break;
    }
  }

  if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
    if (ui::pointInRect(e.button.x, e.button.y, backButtonRect())) {
      backToHub();
      return;
    }
    if (ui::pointInRect(e.button.x, e.button.y, rulesButtonRect()) && activeGameIndex_ >= 0) {
      openRules(activeGameIndex_);
      return;
    }
    const Difficulty levels[3] = {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard};
    for (int i = 0; i < 3; ++i) {
      if (ui::pointInRect(e.button.x, e.button.y, difficultyChipRect(i))) {
        difficulty_ = levels[i];
        if (game_) game_->reset(difficulty_);
        return;
      }
    }
    if (ui::pointInRect(e.button.x, e.button.y, resetButtonRect())) {
      if (game_) game_->reset(difficulty_);
      return;
    }
  }

  if (game_) {
    game_->onEvent(e);
    if (game_->requestMenu()) {
      game_->clearMenuRequest();
      backToHub();
    }
  }
}

void App::update(double dt) {
  if (screen_ == Screen::Game && game_ && !showRules_) game_->update(dt);
}

void App::drawBackground() {
  const Palette& p = colors();
  ui::fillRect(renderer_, SDL_Rect{0, 0, outputW_, outputH_}, p.bg);
  // Soft modern glow panels
  ui::fillRoundRect(renderer_, ui::SR(-80, -60, 420, 280), ui::S(80),
                    withAlpha(p.bgGlow, themeMode() == ThemeMode::Dark ? 255 : 255));
  ui::fillRoundRect(renderer_, ui::SR(680, 480, 420, 320), ui::S(90), withAlpha(p.primary, 28));
  ui::fillRoundRect(renderer_, ui::SR(720, -40, 360, 220), ui::S(70), withAlpha(p.ticTacToe, 24));
}

void App::draw() {
  drawBackground();
  switch (screen_) {
    case Screen::Landing:
      drawLanding();
      break;
    case Screen::Hub:
      drawHub();
      break;
    case Screen::Game:
      drawGameChrome();
      if (game_) game_->draw(renderer_, font_, fontLarge_);
      break;
  }

  const bool themeHover = ui::pointInRect(mouseX_, mouseY_, themeButtonRect());
  const char* themeLabel = themeMode() == ThemeMode::Dark ? "Light mode" : "Dark mode";
  ui::button(renderer_, font_, themeButtonRect(), themeLabel, colors().primary, themeHover, false);

  if (showRules_) drawRulesOverlay();
  SDL_RenderPresent(renderer_);
}

void App::drawLanding() {
  const Palette& p = colors();

  ui::fillRoundRect(renderer_, ui::SR(120, 120, 760, 480), ui::S(28), p.surface);
  ui::drawRoundRect(renderer_, ui::SR(120, 120, 760, 480), ui::S(28), p.primary, std::max(2, ui::S(2)));

  ui::drawText(renderer_, font_, "OFFLINE GAME CLUB", ui::S(kWidth / 2), ui::S(170), p.muted, true);
  ui::drawText(renderer_, fontHero_, "Gamealone", ui::S(kWidth / 2), ui::S(250), p.text, true);
  ui::drawText(renderer_, fontLarge_, "Eight classic games. You versus the computer.",
               ui::S(kWidth / 2), ui::S(320), p.muted, true);

  const bool playHover = ui::pointInRect(mouseX_, mouseY_, playButtonRect());
  SDL_Rect play = playButtonRect();
  ui::fillRoundRect(renderer_, play, ui::S(16), playHover ? mix(p.primary, p.text, 0.15f) : p.primary);
  ui::drawText(renderer_, fontLarge_, "Play", play.x + play.w / 2, play.y + play.h / 2,
               themeMode() == ThemeMode::Dark ? SDL_Color{10, 12, 22, 255} : SDL_Color{255, 255, 255, 255},
               true);

  ui::drawText(renderer_, font_, "Enter / Space to start  ·  T theme  ·  Esc quit",
               ui::S(kWidth / 2), ui::S(500), p.muted, true);
}

void App::drawHub() {
  const Palette& p = colors();
  ui::button(renderer_, font_, homeButtonRect(), "Home", p.primary,
             ui::pointInRect(mouseX_, mouseY_, homeButtonRect()), false);
  ui::drawText(renderer_, fontTitle_, "Choose a game", ui::S(kWidth / 2), ui::S(40), p.text, true);
  ui::drawText(renderer_, font_, "Tap a card to play  ·  Rules for how each game works",
               ui::S(kWidth / 2), ui::S(88), p.muted, true);

  for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
    const SDL_Rect rect = hubCardRect(i);
    const bool hover = ui::pointInRect(mouseX_, mouseY_, rect);
    const SDL_Color accent = items_[static_cast<size_t>(i)].accent();
    ui::card(renderer_, rect, accent, hover);
    ui::fillRoundRect(renderer_,
                      SDL_Rect{rect.x + ui::S(16), rect.y + ui::S(18), ui::S(40), ui::S(40)},
                      ui::S(12), mix(accent, p.surface, 0.65f));
    ui::drawText(renderer_, fontLarge_, items_[static_cast<size_t>(i)].name, rect.x + ui::S(16),
                 rect.y + ui::S(78), p.text, false);
    ui::drawText(renderer_, font_, items_[static_cast<size_t>(i)].subtitle, rect.x + ui::S(16),
                 rect.y + ui::S(112), p.muted, false);

    const SDL_Rect rulesChip = hubRulesChipRect(i);
    const bool rulesHover = ui::pointInRect(mouseX_, mouseY_, rulesChip);
    ui::button(renderer_, font_, rulesChip, "Rules", accent, rulesHover, false);
  }

  ui::drawText(renderer_, font_, std::to_string(items_.size()) + " games  ·  Esc back to home",
               ui::S(kWidth / 2), ui::S(kHeight - 28), p.muted, true);
}

void App::drawGameChrome() {
  if (!game_) return;
  const Palette& p = colors();
  const SDL_Color accent = game_->accent();

  ui::button(renderer_, font_, backButtonRect(), "Back", accent,
             ui::pointInRect(mouseX_, mouseY_, backButtonRect()), false);
  ui::button(renderer_, font_, rulesButtonRect(), "Rules", accent,
             ui::pointInRect(mouseX_, mouseY_, rulesButtonRect()), false);
  ui::button(renderer_, font_, resetButtonRect(), "Reset", accent,
             ui::pointInRect(mouseX_, mouseY_, resetButtonRect()), false);

  ui::drawText(renderer_, fontLarge_, game_->title(), ui::S(kWidth / 2), ui::S(36), accent, true);

  const Difficulty levels[3] = {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard};
  for (int i = 0; i < 3; ++i) {
    const SDL_Rect chip = difficultyChipRect(i);
    ui::button(renderer_, font_, chip, difficultyLabel(levels[i]), accent,
               ui::pointInRect(mouseX_, mouseY_, chip), difficulty_ == levels[i]);
  }

  ui::drawText(renderer_, font_, game_->status(), ui::S(kWidth / 2), ui::S(120), p.muted, true);

  if (game_->outcome() != GameOutcome::None) {
    SDL_Rect banner = ui::SR(kWidth / 2 - 190, kHeight - 78, 380, 50);
    ui::fillRoundRect(renderer_, banner, ui::S(14), p.surface);
    ui::drawRoundRect(renderer_, banner, ui::S(14), accent, std::max(2, ui::S(2)));
    ui::drawText(renderer_, fontLarge_, outcomeLabel(game_->outcome()), ui::S(kWidth / 2),
                 ui::S(kHeight - 53), accent, true);
  }
}

void App::drawRulesOverlay() {
  if (rulesIndex_ < 0 || rulesIndex_ >= static_cast<int>(items_.size())) return;
  const auto& item = items_[static_cast<size_t>(rulesIndex_)];
  const Palette& p = colors();
  const SDL_Color accent = item.accent();

  ui::fillRect(renderer_, SDL_Rect{0, 0, outputW_, outputH_}, SDL_Color{0, 0, 0, 150});
  const SDL_Rect panel = ui::SR(180, 140, 640, 460);
  ui::fillRoundRect(renderer_, panel, ui::S(22), p.surface);
  ui::drawRoundRect(renderer_, panel, ui::S(22), accent, std::max(2, ui::S(2)));

  ui::drawText(renderer_, fontLarge_, item.name, panel.x + ui::S(28), panel.y + ui::S(28), accent,
               false);
  ui::drawText(renderer_, font_, "How to play", panel.x + ui::S(28), panel.y + ui::S(68), p.muted,
               false);
  ui::drawTextWrapped(renderer_, font_, item.rules, panel.x + ui::S(28), panel.y + ui::S(110),
                      panel.w - ui::S(56), p.text, ui::S(26));

  ui::button(renderer_, font_, rulesCloseRect(), "X", accent,
             ui::pointInRect(mouseX_, mouseY_, rulesCloseRect()), false);
  ui::drawText(renderer_, font_, "Esc or click outside to close", panel.x + panel.w / 2,
               panel.y + panel.h - ui::S(28), p.muted, true);
}
