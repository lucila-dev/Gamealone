#include "app.hpp"

#include "render.hpp"
#include "theme.hpp"

#include <algorithm>
#include <iostream>
#include <string>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace {

constexpr int kWidth = 1100;
constexpr int kHeight = 820;

// Clear, high-legibility sans fonts (no decorative serif titles).
const char* kUiFontPaths[] = {
    "/System/Library/Fonts/SFNS.ttf",
    "/System/Library/Fonts/SFCompact.ttf",
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
    "/System/Library/Fonts/Helvetica.ttc",
};
const char* kUiBoldPaths[] = {
    "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
    "/System/Library/Fonts/SFNS.ttf",
    "/System/Library/Fonts/SFCompact.ttf",
    "/System/Library/Fonts/Supplemental/Arial.ttf",
};

SDL_Color accentTicTacToe() { return colors().ticTacToe; }
SDL_Color accentBattleship() { return colors().battleship; }
SDL_Color accentConnectFour() { return colors().connectFour; }
SDL_Color accentCheckers() { return colors().checkers; }
SDL_Color accentHangman() { return colors().hangman; }
SDL_Color accentMemory() { return colors().memory; }
SDL_Color accentRps() { return colors().rps; }
SDL_Color accentReversi() { return colors().reversi; }

TTF_Font* openFontFrom(const char* const* paths, int count, int pt) {
  for (int i = 0; i < count; ++i) {
    if (TTF_Font* font = TTF_OpenFont(paths[i], pt)) return font;
  }
  return nullptr;
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

  const int nUi = static_cast<int>(sizeof(kUiFontPaths) / sizeof(kUiFontPaths[0]));
  const int nBold = static_cast<int>(sizeof(kUiBoldPaths) / sizeof(kUiBoldPaths[0]));
  // Slightly larger sizes for easier reading on Retina.
  font_ = openFontFrom(kUiFontPaths, nUi, std::max(16, static_cast<int>(std::lround(18.0 * dpiScale_))));
  fontLarge_ =
      openFontFrom(kUiBoldPaths, nBold, std::max(20, static_cast<int>(std::lround(24.0 * dpiScale_))));
  fontTitle_ =
      openFontFrom(kUiBoldPaths, nBold, std::max(32, static_cast<int>(std::lround(40.0 * dpiScale_))));
  fontHero_ =
      openFontFrom(kUiBoldPaths, nBold, std::max(44, static_cast<int>(std::lround(60.0 * dpiScale_))));
  if (fontLarge_) TTF_SetFontStyle(fontLarge_, TTF_STYLE_BOLD);
  if (fontTitle_) TTF_SetFontStyle(fontTitle_, TTF_STYLE_BOLD);
  if (fontHero_) TTF_SetFontStyle(fontHero_, TTF_STYLE_BOLD);
  return font_ && fontLarge_ && fontTitle_ && fontHero_;
}

void App::loadWindowIcon() {
  const char* paths[] = {
      "assets/icon.bmp",
      "../assets/icon.bmp",
      "../Resources/icon.bmp",
      "Gamealone.app/Contents/Resources/icon.bmp",
      "build/Gamealone.app/Contents/Resources/icon.bmp",
  };
  for (const char* path : paths) {
    iconSurface_ = SDL_LoadBMP(path);
    if (iconSurface_) break;
  }

#ifdef __APPLE__
  if (!iconSurface_) {
    char exePath[4096];
    uint32_t size = sizeof(exePath);
    if (_NSGetExecutablePath(exePath, &size) == 0) {
      std::string base(exePath);
      const auto slash = base.find_last_of('/');
      if (slash != std::string::npos) base.resize(slash);
      const std::string candidate = base + "/../Resources/icon.bmp";
      iconSurface_ = SDL_LoadBMP(candidate.c_str());
    }
  }
#endif

  if (iconSurface_ && window_) SDL_SetWindowIcon(window_, iconSurface_);
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
  loadWindowIcon();

  if (!refreshScaleAndFonts()) {
    std::cerr << "OpenFont failed: " << TTF_GetError() << '\n';
    return false;
  }

  items_ = {
      {"Tic-Tac-Toe", "3 in a row",
       "How to play\n"
       "You are X. The computer is O. Take turns placing one mark on an empty square.\n\n"
       "How to win\n"
       "Get three of your marks in a straight line — row, column, or diagonal.\n"
       "If the board fills with no three in a row, the game is a draw.\n\n"
       "Controls\n"
       "Click an empty square to place your mark.\n\n"
       "Difficulty\n"
       "Easy: computer often plays randomly.\n"
       "Medium: mixes random moves with smart play.\n"
       "Hard: plays optimally and is very hard to beat.",
       accentTicTacToe, makeTicTacToe},
      {"Battleship", "Sink the fleet",
       "How to play\n"
       "First place your five ships on your board, then take turns firing at the computer.\n\n"
       "Setup\n"
       "Drag a ship from the Fleet list onto the board. Keep the mouse button held while you "
       "move it, then release to place.\n"
       "While holding the ship, press Space (or scroll the mouse wheel) to rotate between "
       "horizontal and vertical. You can also use the Horizontal / Vertical buttons before "
       "you pick a ship up.\n"
       "To remove a ship after placing it, drag it off the board (or back to the Fleet list), "
       "or click its Fleet slot marked \"on board\".\n"
       "Use Random to auto-place the fleet. When all ships are placed, press Start battle.\n\n"
       "Fleet sizes\n"
       "Carrier 5, Battleship 4, Cruiser 3, Submarine 3, Destroyer 2.\n\n"
       "Battle\n"
       "Click a cell on the computer board to fire. Hits and misses are marked.\n"
       "Sink every computer ship before yours are all sunk.\n\n"
       "Difficulty\n"
       "Harder settings make the computer hunt more accurately after a hit.",
       accentBattleship, makeBattleship},
      {"Connect Four", "Drop four",
       "How to play\n"
       "Players take turns dropping one disc into a column of a 7x6 grid.\n"
       "Discs fall to the lowest empty space in that column.\n\n"
       "How to win\n"
       "Connect four of your discs in a row horizontally, vertically, or diagonally.\n"
       "If the grid fills with no winner, the game is a draw.\n\n"
       "Controls\n"
       "Click a column to drop your disc.\n\n"
       "Difficulty\n"
       "Harder settings look further ahead when choosing a move.",
       accentConnectFour, makeConnectFour},
      {"Checkers", "Jump to win",
       "How to play\n"
       "You control the red pieces at the bottom and move upward on dark squares only.\n"
       "Men move one step diagonally forward. Kings move diagonally in any direction.\n\n"
       "Captures\n"
       "Jump over an adjacent enemy piece onto an empty square beyond it.\n"
       "If a capture is available, you must take it. Multi-jumps continue in one turn.\n\n"
       "Kings\n"
       "Reach the opposite end of the board to crown a king.\n\n"
       "How to win\n"
       "Capture all computer pieces, or leave the computer with no legal moves.\n\n"
       "Controls\n"
       "Click one of your pieces, then click a highlighted destination square.",
       accentCheckers, makeCheckers},
      {"Hangman", "Guess the word",
       "How to play\n"
       "The computer secretly picks a word based on the difficulty.\n"
       "Guess one letter at a time by clicking the on-screen keyboard or typing A–Z.\n\n"
       "Correct guesses\n"
       "Every matching letter in the word is revealed.\n\n"
       "Wrong guesses\n"
       "A body part is added to the hangman figure. You may miss up to 6 times.\n\n"
       "How to win\n"
       "Reveal the full word before the figure is complete.\n"
       "If you reach 6 misses, you lose and the word is shown.\n\n"
       "Difficulty\n"
       "Easy uses short words, Hard uses longer ones.",
       accentHangman, makeHangman},
      {"Memory Match", "Find pairs",
       "How to play\n"
       "The board has 16 face-down cards: 8 pairs.\n"
       "On your turn, flip two cards.\n\n"
       "Matching\n"
       "If they match, they stay open and you score 1 point, then take another turn.\n"
       "If they do not match, they flip back and it becomes the computer's turn.\n\n"
       "How to win\n"
       "When all pairs are found, the higher score wins. Equal scores are a draw.\n\n"
       "Difficulty\n"
       "Harder AI remembers more cards it has already seen.",
       accentMemory, makeMemory},
      {"Rock Paper Scissors", "Best of rounds",
       "How to play\n"
       "Each round, choose Rock, Paper, or Scissors.\n"
       "The computer chooses at the same time.\n\n"
       "Rules\n"
       "Rock beats Scissors.\n"
       "Scissors beats Paper.\n"
       "Paper beats Rock.\n"
       "Same choice is a tie round (no point).\n\n"
       "How to win\n"
       "First to the target number of round wins takes the match.\n"
       "Easy: first to 2. Medium: first to 3. Hard: first to 4.\n\n"
       "Difficulty\n"
       "Harder AI sometimes counters your previous choice.",
       accentRps, makeRockPaperScissors},
      {"Reversi", "Flip the board",
       "How to play\n"
       "You place teal discs. The computer places light discs on an 8x8 board.\n"
       "A move is legal only if it traps one or more enemy discs in a straight line "
       "(horizontal, vertical, or diagonal) between your new disc and another of yours.\n\n"
       "Flipping\n"
       "All trapped enemy discs flip to your color.\n"
       "Legal empty squares are highlighted. If you have no legal move, your turn is skipped.\n\n"
       "How to win\n"
       "When neither side can move (usually when the board is full), the player with more "
       "discs wins. Equal counts are a draw.\n\n"
       "Controls\n"
       "Click a highlighted square to place a disc.",
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
  if (iconSurface_) SDL_FreeSurface(iconSurface_);
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
SDL_Rect App::resetButtonRect() const { return ui::SR(kWidth - 280, 18, 110, 40); }
SDL_Rect App::difficultyChipRect(int index) const {
  return ui::SR(kWidth / 2 - 170 + index * 115, 64, 100, 32);
}
SDL_Rect App::playButtonRect() const { return ui::SR(kWidth / 2 - 120, 400, 240, 64); }
SDL_Rect App::homeButtonRect() const { return ui::SR(24, 18, 110, 40); }
SDL_Rect App::rulesStartRect() const { return ui::SR(kWidth / 2 + 24, 698, 210, 50); }
SDL_Rect App::rulesBackRect() const { return ui::SR(kWidth / 2 - 234, 698, 200, 50); }

SDL_Rect App::hubCardRect(int index) const {
  const int cols = 3;
  const int cardW = 320;
  const int cardH = 190;
  const int gap = 20;
  const int startX = (kWidth - (cols * cardW + (cols - 1) * gap)) / 2;
  const int startY = 130;
  const int col = index % cols;
  const int row = index / cols;
  return ui::SR(startX + col * (cardW + gap), startY + row * (cardH + gap), cardW, cardH);
}

void App::selectGame(int index) {
  if (index < 0 || index >= static_cast<int>(items_.size())) return;
  activeGameIndex_ = index;
  rulesScroll_ = 0;
  rulesContentH_ = 0;
  game_.reset();
  screen_ = Screen::Rules;
}

void App::clampRulesScroll() {
  const int viewH = ui::S(430);
  const int maxScroll = std::max(0, rulesContentH_ - viewH);
  if (rulesScroll_ < 0) rulesScroll_ = 0;
  if (rulesScroll_ > maxScroll) rulesScroll_ = maxScroll;
}

void App::startSelectedGame() {
  if (activeGameIndex_ < 0 || activeGameIndex_ >= static_cast<int>(items_.size())) return;
  game_ = items_[static_cast<size_t>(activeGameIndex_)].factory();
  game_->reset(difficulty_);
  screen_ = Screen::Game;
}

void App::backToHub() {
  game_.reset();
  activeGameIndex_ = -1;
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
        if (ui::pointInRect(e.button.x, e.button.y, hubCardRect(i))) {
          selectGame(i);
          return;
        }
      }
    }
    return;
  }

  if (screen_ == Screen::Rules) {
    if (e.type == SDL_MOUSEWHEEL) {
      rulesScroll_ -= e.wheel.y * ui::S(40);
      clampRulesScroll();
      return;
    }
    if (e.type == SDL_KEYDOWN) {
      if (e.key.keysym.sym == SDLK_ESCAPE) {
        backToHub();
        return;
      }
      if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_SPACE) {
        startSelectedGame();
        return;
      }
    }
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
      if (ui::pointInRect(e.button.x, e.button.y, rulesBackRect()) ||
          ui::pointInRect(e.button.x, e.button.y, homeButtonRect())) {
        backToHub();
        return;
      }
      if (ui::pointInRect(e.button.x, e.button.y, rulesStartRect())) {
        startSelectedGame();
        return;
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
  if (screen_ == Screen::Game && game_) game_->update(dt);
}

void App::drawBackground() {
  const Palette& p = colors();
  ui::fillRect(renderer_, SDL_Rect{0, 0, outputW_, outputH_}, p.bg);
  // Large overlapping blush circles — hub template
  ui::fillCircle(renderer_, ui::S(-40), ui::S(80), ui::S(220), withAlpha(p.bgGlow, 160));
  ui::fillCircle(renderer_, ui::S(200), ui::S(-60), ui::S(260), withAlpha(p.bgGlow, 140));
  ui::fillCircle(renderer_, ui::S(980), ui::S(120), ui::S(280), withAlpha(p.primary, 35));
  ui::fillCircle(renderer_, ui::S(900), ui::S(700), ui::S(240), withAlpha(p.bgGlow, 150));
  ui::fillCircle(renderer_, ui::S(80), ui::S(720), ui::S(200), withAlpha(p.primary, 28));
  ui::fillCircle(renderer_, ui::S(560), ui::S(820), ui::S(180), withAlpha(p.bgGlow, 120));
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
    case Screen::Rules:
      drawRulesScreen();
      break;
    case Screen::Game:
      drawGameChrome();
      if (game_) game_->draw(renderer_, font_, fontLarge_);
      break;
  }

  const bool themeHover = ui::pointInRect(mouseX_, mouseY_, themeButtonRect());
  const char* themeLabel = themeMode() == ThemeMode::Dark ? "Light mode" : "Dark mode";
  ui::button(renderer_, font_, themeButtonRect(), themeLabel, colors().primary, themeHover, false);
  SDL_RenderPresent(renderer_);
}

void App::drawLanding() {
  const Palette& p = colors();

  ui::panel(renderer_, ui::SR(170, 150, 760, 460), ui::S(24));

  ui::drawText(renderer_, fontHero_, "Gamealone", ui::S(kWidth / 2), ui::S(260), p.text, true);
  ui::drawText(renderer_, fontLarge_, "Eight classic games. Play against the computer.",
               ui::S(kWidth / 2), ui::S(330), p.muted, true);

  const bool playHover = ui::pointInRect(mouseX_, mouseY_, playButtonRect());
  SDL_Rect play = playButtonRect();
  ui::fillRoundRect(renderer_, play, play.h / 2,
                    playHover ? mix(p.primary, p.text, 0.1f) : p.primary);
  ui::drawText(renderer_, fontLarge_, "Play", play.x + play.w / 2, play.y + play.h / 2,
               SDL_Color{255, 255, 255, 255}, true);

  ui::drawText(renderer_, font_, "Enter / Space to start  ·  T theme  ·  Esc quit",
               ui::S(kWidth / 2), ui::S(520), p.muted, true);
}

void App::drawHub() {
  const Palette& p = colors();
  ui::button(renderer_, font_, homeButtonRect(), "Home", p.primary,
             ui::pointInRect(mouseX_, mouseY_, homeButtonRect()), false);
  ui::drawText(renderer_, fontTitle_, "Choose a game", ui::S(kWidth / 2), ui::S(40), p.text, true);
  ui::drawText(renderer_, font_, "Select a game to read the rules, then start playing",
               ui::S(kWidth / 2), ui::S(88), p.muted, true);

  for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
    SDL_Rect rect = hubCardRect(i);
    const bool hover = ui::pointInRect(mouseX_, mouseY_, rect);
    const SDL_Color accent = items_[static_cast<size_t>(i)].accent();

    if (hover) {
      rect.x -= ui::S(4);
      rect.y -= ui::S(6);
      rect.w += ui::S(8);
      rect.h += ui::S(8);
    }
    ui::card(renderer_, rect, accent, hover);

    // Icon spot — soft ring + solid center (template)
    ui::fillCircle(renderer_, rect.x + ui::S(48), rect.y + ui::S(48), ui::S(22),
                   mix(accent, p.surface, 0.55f));
    ui::fillCircle(renderer_, rect.x + ui::S(48), rect.y + ui::S(48), ui::S(11), accent);

    ui::drawText(renderer_, fontLarge_, items_[static_cast<size_t>(i)].name, rect.x + ui::S(28),
                 rect.y + ui::S(90), p.text, false);
    ui::drawText(renderer_, font_, items_[static_cast<size_t>(i)].subtitle, rect.x + ui::S(28),
                 rect.y + ui::S(128), p.muted, false);
  }

  ui::drawText(renderer_, font_, std::to_string(items_.size()) + " games  ·  Esc back to home",
               ui::S(kWidth / 2), ui::S(kHeight - 28), p.muted, true);
}

void App::drawRulesScreen() {
  if (activeGameIndex_ < 0 || activeGameIndex_ >= static_cast<int>(items_.size())) return;
  const auto& item = items_[static_cast<size_t>(activeGameIndex_)];
  const Palette& p = colors();
  const SDL_Color accent = item.accent();

  ui::button(renderer_, font_, homeButtonRect(), "Home", accent,
             ui::pointInRect(mouseX_, mouseY_, homeButtonRect()), false);

  const SDL_Rect panel = ui::SR(70, 48, 960, 720);
  ui::panel(renderer_, panel, ui::S(26));

  const int headerH = ui::S(110);
  ui::fillRoundRect(renderer_,
                    SDL_Rect{panel.x + ui::S(20), panel.y + ui::S(20), panel.w - ui::S(40), headerH},
                    ui::S(18), mix(accent, p.surface, 0.82f));

  const int iconCx = panel.x + ui::S(72);
  const int iconCy = panel.y + ui::S(20) + headerH / 2;
  ui::fillCircle(renderer_, iconCx, iconCy, ui::S(26), mix(accent, p.surface, 0.45f));
  ui::fillCircle(renderer_, iconCx, iconCy, ui::S(13), accent);

  ui::drawText(renderer_, fontTitle_, item.name, panel.x + ui::S(118), panel.y + ui::S(40), p.text,
               false);
  ui::drawText(renderer_, fontLarge_, item.subtitle, panel.x + ui::S(118), panel.y + ui::S(84),
               mix(p.text, p.muted, 0.2f), false);

  const SDL_Rect clip{panel.x + ui::S(24), panel.y + headerH + ui::S(36), panel.w - ui::S(48),
                      panel.h - headerH - ui::S(140)};
  SDL_RenderSetClipRect(renderer_, &clip);

  const int padX = ui::S(24);
  const int padY = ui::S(22);
  const int textInset = ui::S(18);
  const int contentLeft = clip.x + ui::S(4);
  const int contentWidth = clip.w - ui::S(8);
  const int textWidth = contentWidth - padX * 2 - textInset;
  const int lineH = ui::S(28);
  const int gap = ui::S(14);

  struct Section {
    std::string heading;
    std::string body;
    int height = 0;
  };
  std::vector<Section> sections;

  const std::string rules = item.rules;
  size_t pos = 0;
  while (pos < rules.size()) {
    size_t end = rules.find("\n\n", pos);
    if (end == std::string::npos) end = rules.size();
    std::string block = rules.substr(pos, end - pos);
    while (!block.empty() && (block.back() == '\n' || block.back() == ' ')) block.pop_back();
    pos = (end == rules.size()) ? end : end + 2;
    if (block.empty()) continue;

    Section sec;
    sec.body = block;
    const size_t nl = block.find('\n');
    if (nl != std::string::npos) {
      const std::string first = block.substr(0, nl);
      if (first.size() <= 28 && first.find('.') == std::string::npos && !first.empty()) {
        sec.heading = first;
        sec.body = block.substr(nl + 1);
        while (!sec.body.empty() && (sec.body.front() == '\n' || sec.body.front() == ' '))
          sec.body.erase(sec.body.begin());
      }
    } else if (block.size() <= 28 && block.find('.') == std::string::npos) {
      sec.heading = block;
      sec.body.clear();
    }

    const int headingH =
        sec.heading.empty() ? 0 : std::max(ui::S(28), ui::textSize(fontLarge_, sec.heading).h);
    const int bodyH =
        sec.body.empty() ? 0 : ui::measureTextWrapped(font_, sec.body, textWidth, lineH);
    const int innerGap = (!sec.heading.empty() && !sec.body.empty()) ? ui::S(14) : 0;
    sec.height = padY + headingH + innerGap + bodyH + padY;
    sections.push_back(std::move(sec));
  }

  rulesContentH_ = 0;
  for (size_t i = 0; i < sections.size(); ++i) {
    rulesContentH_ += sections[i].height;
    if (i + 1 < sections.size()) rulesContentH_ += gap;
  }
  clampRulesScroll();

  int cy = clip.y - rulesScroll_;
  for (const auto& sec : sections) {
    const SDL_Rect card{contentLeft, cy, contentWidth, sec.height};
    if (card.y + card.h >= clip.y && card.y <= clip.y + clip.h) {
      ui::fillRoundRect(renderer_, card, ui::S(16), p.surfaceHigh);
      ui::drawRoundRect(renderer_, card, ui::S(16), mix(accent, p.border, 0.5f), 1);
      ui::fillRoundRect(renderer_,
                        SDL_Rect{card.x + ui::S(14), card.y + padY, ui::S(4),
                                 std::max(ui::S(8), card.h - padY * 2)},
                        ui::S(2), accent);

      int ty = card.y + padY;
      if (!sec.heading.empty()) {
        ui::drawText(renderer_, fontLarge_, sec.heading, card.x + padX + textInset, ty, p.text,
                     false);
        ty += std::max(ui::S(28), ui::textSize(fontLarge_, sec.heading).h);
        if (!sec.body.empty()) ty += ui::S(14);
      }
      if (!sec.body.empty()) {
        ui::drawTextWrapped(renderer_, font_, sec.body, card.x + padX + textInset, ty, textWidth,
                            p.text, lineH);
      }
    }
    cy += sec.height + gap;
  }

  SDL_RenderSetClipRect(renderer_, nullptr);

  if (rulesContentH_ > clip.h) {
    ui::drawText(renderer_, font_, "Scroll for more", panel.x + panel.w / 2,
                 clip.y + clip.h + ui::S(6), p.muted, true);
  }

  const SDL_Rect footer{panel.x + ui::S(20), panel.y + panel.h - ui::S(88), panel.w - ui::S(40),
                        ui::S(68)};
  ui::fillRoundRect(renderer_, footer, ui::S(16), mix(p.surfaceHigh, p.surface, 0.35f));
  ui::drawText(renderer_, font_, "Ready when you are", footer.x + ui::S(28),
               footer.y + footer.h / 2, p.muted, false);

  ui::button(renderer_, fontLarge_, rulesBackRect(), "Back", accent,
             ui::pointInRect(mouseX_, mouseY_, rulesBackRect()), false);
  ui::button(renderer_, fontLarge_, rulesStartRect(), "Start game", accent,
             ui::pointInRect(mouseX_, mouseY_, rulesStartRect()), true);
}

void App::drawGameChrome() {
  if (!game_) return;
  const Palette& p = colors();
  const SDL_Color accent = game_->accent();

  ui::button(renderer_, font_, backButtonRect(), "Back", accent,
             ui::pointInRect(mouseX_, mouseY_, backButtonRect()), false);
  ui::button(renderer_, font_, resetButtonRect(), "Reset", accent,
             ui::pointInRect(mouseX_, mouseY_, resetButtonRect()), false);

  ui::drawText(renderer_, fontTitle_, game_->title(), ui::S(kWidth / 2), ui::S(36), p.text, true);

  const Difficulty levels[3] = {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard};
  for (int i = 0; i < 3; ++i) {
    const SDL_Rect chip = difficultyChipRect(i);
    ui::button(renderer_, font_, chip, difficultyLabel(levels[i]), accent,
               ui::pointInRect(mouseX_, mouseY_, chip), difficulty_ == levels[i]);
  }

  ui::drawText(renderer_, font_, game_->status(), ui::S(kWidth / 2), ui::S(112), p.text, true);

  if (game_->outcome() != GameOutcome::None) {
    SDL_Rect banner = ui::SR(kWidth / 2 - 190, kHeight - 78, 380, 50);
    ui::panel(renderer_, banner, ui::S(16));
    ui::drawText(renderer_, fontLarge_, outcomeLabel(game_->outcome()), ui::S(kWidth / 2),
                 ui::S(kHeight - 53), accent, true);
  }
}

