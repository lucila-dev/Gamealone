#include "game.hpp"
#include "render.hpp"
#include "theme.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kSize = 10;
constexpr int kFleetCount = 5;

struct ShipSpec {
  const char* name;
  int length;
};

constexpr ShipSpec kFleet[kFleetCount] = {
    {"Carrier", 5}, {"Battleship", 4}, {"Cruiser", 3}, {"Submarine", 3}, {"Destroyer", 2},
};

enum class Shot : uint8_t { None, Miss, Hit, Sunk };

struct Point {
  int x = 0;
  int y = 0;
  bool operator==(const Point& o) const { return x == o.x && y == o.y; }
};

struct PlacedShip {
  int fleetIndex = 0;
  ShipSpec spec{};
  std::vector<Point> cells;
  bool horizontal = true;
  std::vector<Point> hits;
  bool sunk() const { return static_cast<int>(hits.size()) >= spec.length; }
};

// Draw a simple ship silhouette fully inside `bounds`.
void drawShipSprite(SDL_Renderer* renderer, SDL_Rect bounds, bool horizontal, SDL_Color hull,
                    bool ghost) {
  if (bounds.w < 4 || bounds.h < 4) return;
  const Uint8 alpha = ghost ? 160 : 255;
  hull.a = alpha;
  SDL_Color deck = mix(hull, SDL_Color{255, 255, 255, 255}, 0.25f);
  deck.a = alpha;
  SDL_Color accent = mix(hull, SDL_Color{0, 0, 0, 255}, 0.25f);
  accent.a = alpha;

  // Inset so nothing draws outside the slot/cell.
  bounds.x += 2;
  bounds.y += 2;
  bounds.w -= 4;
  bounds.h -= 4;
  if (bounds.w < 4 || bounds.h < 4) return;

  if (horizontal) {
    const int midY = bounds.y + bounds.h / 2;
    const int bodyH = std::max(8, bounds.h * 82 / 100);
    const int bodyY = midY - bodyH / 2;
    const int bow = std::max(8, bounds.w / 7);
    ui::fillRoundRect(renderer, SDL_Rect{bounds.x + bow / 2, bodyY, bounds.w - bow, bodyH},
                      bodyH / 2, hull);
    const int tipX = bounds.x + bounds.w - 1;
    for (int i = 0; i < bow; ++i) {
      const int t = bow - i;
      const int half = (bodyH * t) / (2 * bow);
      ui::fillRect(renderer, SDL_Rect{tipX - i, midY - half, 1, std::max(1, half * 2)}, hull);
    }
    const int cabinW = std::max(6, bounds.w / 6);
    const int cabinH = std::min(bounds.h - 2, std::max(8, bodyH * 90 / 100));
    ui::fillRoundRect(renderer,
                      SDL_Rect{bounds.x + bounds.w / 3, midY - cabinH / 2, cabinW, cabinH}, 3, deck);
    const int stackH = std::max(4, bodyH / 2);
    ui::fillRect(renderer,
                 SDL_Rect{bounds.x + bounds.w / 3 + cabinW / 2 - 2, bodyY, 4, stackH}, accent);
    for (int i = 0; i < 3; ++i) {
      const int px = bounds.x + bow + (i + 1) * (bounds.w - bow) / 5;
      ui::fillCircle(renderer, px, midY, std::max(2, bodyH / 7), accent);
    }
  } else {
    const int midX = bounds.x + bounds.w / 2;
    const int bodyW = std::max(8, bounds.w * 82 / 100);
    const int bodyX = midX - bodyW / 2;
    const int bow = std::max(8, bounds.h / 7);
    ui::fillRoundRect(renderer, SDL_Rect{bodyX, bounds.y + bow / 2, bodyW, bounds.h - bow},
                      bodyW / 2, hull);
    for (int i = 0; i < bow; ++i) {
      const int t = bow - i;
      const int half = (bodyW * t) / (2 * bow);
      ui::fillRect(renderer, SDL_Rect{midX - half, bounds.y + i, std::max(1, half * 2), 1}, hull);
    }
    const int cabinH = std::max(6, bounds.h / 6);
    const int cabinW = std::min(bounds.w - 2, std::max(8, bodyW * 90 / 100));
    ui::fillRoundRect(renderer,
                      SDL_Rect{midX - cabinW / 2, bounds.y + bounds.h / 3, cabinW, cabinH}, 3, deck);
    const int stackW = std::max(4, bodyW / 2);
    ui::fillRect(renderer,
                 SDL_Rect{bodyX, bounds.y + bounds.h / 3 + cabinH / 2 - 2, stackW, 4}, accent);
    for (int i = 0; i < 3; ++i) {
      const int py = bounds.y + bow + (i + 1) * (bounds.h - bow) / 5;
      ui::fillCircle(renderer, midX, py, std::max(2, bodyW / 7), accent);
    }
  }
}

class Battleship final : public Game {
 public:
  const char* title() const override { return "Battleship"; }
  SDL_Color accent() const override { return colors().battleship; }
  Difficulty difficulty() const override { return difficulty_; }
  GameOutcome outcome() const override { return outcome_; }

  void reset(Difficulty difficulty) override {
    difficulty_ = difficulty;
    placing_ = true;
    horizontal_ = true;
    playerTurn_ = true;
    thinking_ = false;
    thinkTimer_ = 0;
    outcome_ = GameOutcome::None;
    lastEvent_ = "Hold-drag ships onto the board. Keep holding and press Space to rotate.";
    log_.clear();
    pushLog("Place your fleet to begin.", false);
    clearGrid(playerOcc_);
    clearGrid(cpuOcc_);
    clearShots(playerShots_);
    clearShots(cpuShots_);
    playerShips_.clear();
    cpuShips_.clear();
    targets_.clear();
    lastHit_ = originHit_ = dir_ = Point{-1, -1};
    dragging_ = false;
    dragFleet_ = -1;
    dragHadBoard_ = false;
    mouseX_ = mouseY_ = 0;
    for (int i = 0; i < kFleetCount; ++i) {
      onBoard_[i] = false;
      horiz_[i] = true;
      origin_[i] = Point{-1, -1};
    }
    placeFleet(cpuOcc_, cpuShips_);
  }

  void onEvent(const SDL_Event& e) override {
    if (placing_) {
      handlePlacementEvent(e);
      return;
    }

    if (e.type != SDL_MOUSEBUTTONDOWN || e.button.button != SDL_BUTTON_LEFT) return;
    if (outcome_ != GameOutcome::None || thinking_ || !playerTurn_) return;
    Point cell;
    if (!cellAt(cpuBoardRect(), e.button.x, e.button.y, cell)) return;
    if (cpuShots_[cell.y][cell.x] != Shot::None) return;
    const Shot result = applyShot(cell, cpuOcc_, cpuShips_, cpuShots_);
    pushLog(shotLine("You", result), result != Shot::Miss);
    checkOutcome();
    if (outcome_ != GameOutcome::None) return;
    playerTurn_ = false;
    thinking_ = true;
    thinkTimer_ = 0.45;
  }

  void update(double dt) override {
    if (!thinking_) return;
    thinkTimer_ -= dt;
    if (thinkTimer_ > 0) return;
    thinking_ = false;
    const Point target = pickShot();
    const Shot result = applyShot(target, playerOcc_, playerShips_, playerShots_);
    notifyShot(target, result);
    pushLog(shotLine("Computer", result), result != Shot::Miss);
    checkOutcome();
    playerTurn_ = outcome_ == GameOutcome::None;
  }

  void draw(SDL_Renderer* renderer, TTF_Font* font, TTF_Font* fontLarge) override {
    if (placing_) {
      drawPlacement(renderer, font);
      return;
    }
    drawBattle(renderer, font, fontLarge);
  }

  std::string status() const override {
    if (outcome_ == GameOutcome::Win) return "You sunk the computer's fleet.";
    if (outcome_ == GameOutcome::Lose) return "Your fleet is gone.";
    if (placing_) {
      if (allPlayerShipsPlaced()) return "Fleet ready — press Start battle.";
      if (dragging_) {
        return std::string(kFleet[dragFleet_].name) +
               (horizontal_ ? " · horizontal" : " · vertical") + " · Space rotates · release to place";
      }
      return "Hold-drag ships · Space rotates · drag off board to remove";
    }
    return lastEvent_;
  }

 private:
  using Occ = std::array<std::array<int, kSize>, kSize>;
  using Shots = std::array<std::array<Shot, kSize>, kSize>;

  static SDL_Rect playerBoardRect() { return ui::SR(420, 175, 360, 360); }
  static SDL_Rect cpuBoardRect() { return ui::SR(580, 145, 360, 360); }
  static SDL_Rect playerBoardRectBattle() { return ui::SR(100, 145, 360, 360); }
  static SDL_Rect fleetPanelRect() { return ui::SR(40, 540, 300, 210); }
  static SDL_Rect logPanelRect() { return ui::SR(360, 540, 360, 210); }
  static SDL_Rect turnPanelRect() { return ui::SR(740, 540, 320, 210); }
  static SDL_Rect dockRect(int index) {
    return ui::SR(40, 180 + index * 76, 280, 64);
  }
  static SDL_Rect randomRect() { return ui::SR(360, 640, 120, 40); }
  static SDL_Rect horizRect() { return ui::SR(495, 640, 130, 40); }
  static SDL_Rect vertRect() { return ui::SR(640, 640, 120, 40); }
  static SDL_Rect startRect() { return ui::SR(780, 640, 150, 40); }

  void setOrientation(bool horiz) {
    horizontal_ = horiz;
    if (dragging_ && dragFleet_ >= 0) horiz_[dragFleet_] = horiz;
  }

  void toggleOrientation() { setOrientation(!horizontal_); }

  void handlePlacementEvent(const SDL_Event& e) {
    if (e.type == SDL_MOUSEMOTION) {
      mouseX_ = e.motion.x;
      mouseY_ = e.motion.y;
      return;
    }

    // Scroll wheel rotates without releasing the ship.
    if (e.type == SDL_MOUSEWHEEL) {
      toggleOrientation();
      return;
    }

    if (e.type == SDL_KEYDOWN && !e.key.repeat) {
      if (e.key.keysym.sym == SDLK_SPACE || e.key.keysym.sym == SDLK_v) {
        toggleOrientation();
        return;
      }
      if (e.key.keysym.sym == SDLK_h) {
        setOrientation(true);
        return;
      }
      if (e.key.keysym.sym == SDLK_a) {
        randomizePlayer();
        return;
      }
      if (e.key.keysym.sym == SDLK_RETURN && allPlayerShipsPlaced()) beginBattle();
      return;
    }

    // Right-click: rotate sticky orientation, or rotate a placed ship in place.
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {
      mouseX_ = e.button.x;
      mouseY_ = e.button.y;
      if (dragging_) {
        toggleOrientation();
        return;
      }
      Point cell;
      if (cellAt(playerBoardRect(), e.button.x, e.button.y, cell)) {
        const int idx = playerOcc_[cell.y][cell.x];
        if (idx >= 0) {
          rotateShipInPlace(playerShips_[static_cast<size_t>(idx)].fleetIndex);
          return;
        }
      }
      toggleOrientation();
      return;
    }

    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
      mouseX_ = e.button.x;
      mouseY_ = e.button.y;
      if (ui::pointInRect(e.button.x, e.button.y, horizRect())) {
        setOrientation(true);
        return;
      }
      if (ui::pointInRect(e.button.x, e.button.y, vertRect())) {
        setOrientation(false);
        return;
      }
      if (ui::pointInRect(e.button.x, e.button.y, randomRect())) {
        randomizePlayer();
        return;
      }
      if (ui::pointInRect(e.button.x, e.button.y, startRect()) && allPlayerShipsPlaced()) {
        beginBattle();
        return;
      }

      // Pick up from dock — or remove a placed ship by clicking its fleet slot.
      for (int i = 0; i < kFleetCount; ++i) {
        if (ui::pointInRect(e.button.x, e.button.y, dockRect(i))) {
          if (onBoard_[i]) {
            removeFleetFromBoard(i);
            return;
          }
          startDrag(i, false, Point{-1, -1}, horizontal_);
          return;
        }
      }

      // Pick up from board (drag to move, or drag off board to remove)
      Point cell;
      if (cellAt(playerBoardRect(), e.button.x, e.button.y, cell)) {
        const int idx = playerOcc_[cell.y][cell.x];
        if (idx >= 0) {
          const auto& ship = playerShips_[static_cast<size_t>(idx)];
          const int fleet = ship.fleetIndex;
          const Point prev = origin_[fleet];
          const bool prevH = ship.horizontal;
          removeFleetFromBoard(fleet);
          startDrag(fleet, true, prev, prevH);
          return;
        }
      }
      return;
    }

    if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && dragging_) {
      mouseX_ = e.button.x;
      mouseY_ = e.button.y;
      tryDrop(e.button.x, e.button.y);
      dragging_ = false;
      dragFleet_ = -1;
    }
  }

  void startDrag(int fleetIndex, bool hadBoardPos, Point prevOrigin, bool prevHoriz) {
    dragging_ = true;
    dragFleet_ = fleetIndex;
    dragHadBoard_ = hadBoardPos;
    dragPrevOrigin_ = prevOrigin;
    dragPrevHoriz_ = prevHoriz;
    // Dock pickups use sticky orientation; board pickups keep the ship's facing.
    horizontal_ = hadBoardPos ? prevHoriz : horizontal_;
    horiz_[fleetIndex] = horizontal_;
  }

  void rotateShipInPlace(int fleetIndex) {
    if (!onBoard_[fleetIndex]) {
      toggleOrientation();
      return;
    }
    const Point o = origin_[fleetIndex];
    const bool nextHoriz = !horiz_[fleetIndex];
    removeFleetFromBoard(fleetIndex);
    auto cells = cellsFor(kFleet[fleetIndex].length, o.x, o.y, nextHoriz);
    if (fits(playerOcc_, cells)) {
      placeFleetShip(fleetIndex, o, nextHoriz);
      setOrientation(nextHoriz);
    } else {
      // Try centering rotation around the middle cell.
      const int len = kFleet[fleetIndex].length;
      const int mid = len / 2;
      Point pivot = nextHoriz ? Point{o.x - mid, o.y} : Point{o.x, o.y - mid};
      cells = cellsFor(len, pivot.x, pivot.y, nextHoriz);
      if (fits(playerOcc_, cells)) {
        placeFleetShip(fleetIndex, pivot, nextHoriz);
        setOrientation(nextHoriz);
      } else {
        placeFleetShip(fleetIndex, o, !nextHoriz);  // restore
      }
    }
  }

  void tryDrop(int mx, int my) {
    if (dragFleet_ < 0) return;

    // Dropped back on the fleet panel → remove from board.
    for (int i = 0; i < kFleetCount; ++i) {
      if (ui::pointInRect(mx, my, dockRect(i))) {
        // Already off the board while dragging; leave it in the dock.
        return;
      }
    }
    if (ui::pointInRect(mx, my, ui::SR(28, 155, 300, 460))) return;

    Point cell;
    if (cellAt(playerBoardRect(), mx, my, cell)) {
      auto cells = cellsFor(kFleet[dragFleet_].length, cell.x, cell.y, horizontal_);
      if (fits(playerOcc_, cells)) {
        placeFleetShip(dragFleet_, cell, horizontal_);
        return;
      }
      // On board but blocked: put it back where it was, if it came from the board.
      if (dragHadBoard_) {
        placeFleetShip(dragFleet_, dragPrevOrigin_, dragPrevHoriz_);
        horizontal_ = dragPrevHoriz_;
      }
      return;
    }

    // Released off the board → remove (do not snap back).
  }

  void beginBattle() {
    placing_ = false;
    dragging_ = false;
    pushLog("Battle started — fire on enemy waters.", false);
  }

  void pushLog(const std::string& text, bool hit) {
    lastEvent_ = text;
    log_.push_back(LogEntry{text, hit});
    if (log_.size() > 8) log_.erase(log_.begin());
  }

  bool allPlayerShipsPlaced() const {
    for (bool b : onBoard_)
      if (!b) return false;
    return true;
  }

  void drawPlacement(SDL_Renderer* renderer, TTF_Font* font) {
    const Palette& p = colors();
    ui::panel(renderer, ui::SR(28, 155, 300, 460), ui::S(20));
    ui::drawText(renderer, font, "Fleet", ui::S(178), ui::S(175), p.text, true);

    for (int i = 0; i < kFleetCount; ++i) {
      const SDL_Rect slot = dockRect(i);
      ui::fillRoundRect(renderer, slot, ui::S(14), p.surfaceHigh);
      ui::drawRoundRect(renderer, slot, ui::S(14), p.border, 1);

      const int labelX = slot.x + ui::S(14);
      const int labelY = slot.y + (slot.h - ui::S(16)) / 2;
      ui::drawText(renderer, font, kFleet[i].name, labelX, labelY, p.text, false);

      // Ship sits in a padded right lane so it never clips the rounded edge.
      const SDL_Rect shipArea{slot.x + ui::S(112), slot.y + ui::S(16), slot.w - ui::S(128),
                              slot.h - ui::S(32)};

      if (!onBoard_[i] && !(dragging_ && dragFleet_ == i)) {
        const int shipW =
            std::min(shipArea.w - ui::S(4), std::max(ui::S(40), shipArea.w * kFleet[i].length / 6));
        const SDL_Rect shipBox{shipArea.x, shipArea.y, shipW, shipArea.h};
        drawShipSprite(renderer, shipBox, true, shipColor(i), false);
      } else if (onBoard_[i]) {
        ui::drawText(renderer, font, "click to remove", shipArea.x + shipArea.w / 2,
                     shipArea.y + shipArea.h / 2, p.muted, true);
      }
    }

    drawGridBase(renderer, font, playerBoardRect(), playerShots_, false, "Your waters");
    drawPlacedShips(renderer, playerBoardRect(), playerShips_, false);

    if (dragging_ && dragFleet_ >= 0) {
      Point cell;
      const bool overBoard = cellAt(playerBoardRect(), mouseX_, mouseY_, cell);
      auto cells = overBoard ? cellsFor(kFleet[dragFleet_].length, cell.x, cell.y, horizontal_)
                             : std::vector<Point>{};
      const bool valid = overBoard && fits(playerOcc_, cells);
      if (overBoard && !cells.empty()) {
        const SDL_Rect ghost = shipPixelRect(playerBoardRect(), cells, horizontal_);
        drawShipSprite(renderer, ghost, horizontal_,
                       valid ? SDL_Color{74, 222, 128, 255} : SDL_Color{248, 113, 113, 255}, true);
      } else {
        const int cellPx = playerBoardRect().w / kSize;
        const int len = kFleet[dragFleet_].length;
        SDL_Rect follow =
            horizontal_ ? SDL_Rect{mouseX_ - (len * cellPx) / 2, mouseY_ - cellPx / 2, len * cellPx,
                                   cellPx}
                        : SDL_Rect{mouseX_ - cellPx / 2, mouseY_ - (len * cellPx) / 2, cellPx,
                                   len * cellPx};
        drawShipSprite(renderer, follow, horizontal_, shipColor(dragFleet_), true);
      }
    }

    ui::button(renderer, font, randomRect(), "Random", colors().battleship, false, false);
    ui::button(renderer, font, horizRect(), "Horizontal", colors().battleship, false, horizontal_);
    ui::button(renderer, font, vertRect(), "Vertical", colors().battleship, false, !horizontal_);
    ui::button(renderer, font, startRect(), "Start battle", colors().battleship, false,
               allPlayerShipsPlaced());

    ui::drawText(renderer, font, "Hold to drag · Space to rotate · drag off board to remove",
                 ui::S(580), ui::S(700), p.text, true);
  }

  static SDL_Color shipColor(int fleetIndex) {
    static const SDL_Color palette[kFleetCount] = {
        {170, 150, 255, 255}, {255, 140, 170, 255}, {255, 170, 100, 255},
        {200, 150, 255, 255}, {255, 130, 180, 255},
    };
    return palette[fleetIndex];
  }

  void drawLabeledBoard(SDL_Renderer* renderer, TTF_Font* font, const SDL_Rect& board,
                        const char* title) {
    const Palette& p = colors();
    const SDL_Rect frame{board.x - ui::S(32), board.y - ui::S(36), board.w + ui::S(52),
                         board.h + ui::S(54)};
    ui::panel(renderer, frame, ui::S(20));
    // Darker title for contrast on white panels
    ui::drawText(renderer, font, title, board.x + board.w / 2, board.y - ui::S(18), p.text, true);

    const int cw = board.w / kSize;
    const int ch = board.h / kSize;
    const SDL_Color line = p.border;
    const SDL_Color water = mix(p.surfaceHigh, p.surface, 0.35f);
    const SDL_Color label = mix(p.text, p.muted, 0.35f);

    for (int y = 0; y < kSize; ++y) {
      for (int x = 0; x < kSize; ++x) {
        SDL_Rect cell{board.x + x * cw, board.y + y * ch, cw, ch};
        ui::fillRect(renderer, cell, water);
        ui::drawRect(renderer, cell, line, 1);
      }
    }

    for (int i = 0; i < kSize; ++i) {
      ui::drawText(renderer, font, std::to_string(i + 1), board.x + i * cw + cw / 2,
                   board.y + board.h + ui::S(14), label, true);
      const char row[2] = {static_cast<char>('A' + i), '\0'};
      ui::drawText(renderer, font, row, board.x - ui::S(16), board.y + i * ch + ch / 2, label,
                   true);
    }
  }

  void drawBattle(SDL_Renderer* renderer, TTF_Font* font, TTF_Font* fontLarge) {
    const Palette& p = colors();
    drawLabeledBoard(renderer, font, playerBoardRectBattle(), "Your fleet");
    drawLabeledBoard(renderer, font, cpuBoardRect(), "Enemy waters");
    drawPlacedShips(renderer, playerBoardRectBattle(), playerShips_, true);
    drawShotsOverlay(renderer, font, cpuBoardRect(), cpuShots_);
    drawShotsOverlay(renderer, font, playerBoardRectBattle(), playerShots_);

    const SDL_Rect fleet = fleetPanelRect();
    const SDL_Rect log = logPanelRect();
    const SDL_Rect turn = turnPanelRect();
    ui::panel(renderer, fleet, ui::S(18));
    ui::panel(renderer, log, ui::S(18));
    ui::panel(renderer, turn, ui::S(18));

    ui::drawText(renderer, font, "Your fleet", fleet.x + ui::S(18), fleet.y + ui::S(16), p.muted,
                 false);
    for (int i = 0; i < kFleetCount; ++i) {
      const int yy = fleet.y + ui::S(44) + i * ui::S(28);
      bool sunk = false;
      for (const auto& s : playerShips_) {
        if (s.fleetIndex == i) {
          sunk = s.sunk();
          break;
        }
      }
      ui::fillCircle(renderer, fleet.x + ui::S(22), yy + ui::S(8), ui::S(6),
                     sunk ? p.border : shipColor(i));
      const std::string label =
          std::string(kFleet[i].name) + " (" + std::to_string(kFleet[i].length) + ")";
      ui::drawText(renderer, font, label, fleet.x + ui::S(40), yy, sunk ? p.muted : p.text, false);
    }

    ui::drawText(renderer, font, "Game log", log.x + ui::S(18), log.y + ui::S(16), p.muted, false);
    int ly = log.y + ui::S(48);
    for (int i = static_cast<int>(log_.size()) - 1; i >= 0 && ly < log.y + log.h - ui::S(16); --i) {
      const auto& entry = log_[static_cast<size_t>(i)];
      ui::fillCircle(renderer, log.x + ui::S(22), ly + ui::S(8), ui::S(5),
                     entry.hit ? p.primary : p.border);
      ui::drawText(renderer, font, entry.text, log.x + ui::S(40), ly, p.text, false);
      ly += ui::S(26);
    }

    ui::drawText(renderer, font, "Action", turn.x + ui::S(18), turn.y + ui::S(16), p.muted, false);
    if (outcome_ != GameOutcome::None) {
      ui::drawText(renderer, fontLarge, outcome_ == GameOutcome::Win ? "Victory" : "Defeated",
                   turn.x + turn.w / 2, turn.y + ui::S(90), p.primary, true);
    } else if (thinking_ || !playerTurn_) {
      ui::drawText(renderer, fontLarge, "Enemy turn", turn.x + turn.w / 2, turn.y + ui::S(80),
                   p.text, true);
      ui::drawText(renderer, font, "Computer is aiming…", turn.x + turn.w / 2, turn.y + ui::S(120),
                   p.muted, true);
    } else {
      ui::drawText(renderer, fontLarge, "Your turn", turn.x + turn.w / 2, turn.y + ui::S(80), p.text,
                   true);
      ui::drawText(renderer, font, "Select a cell on the enemy grid.", turn.x + turn.w / 2,
                   turn.y + ui::S(120), p.muted, true);
    }
  }

  void drawGridBase(SDL_Renderer* renderer, TTF_Font* font, const SDL_Rect& board,
                    const Shots& /*shots*/, bool /*unused*/, const char* label) {
    drawLabeledBoard(renderer, font, board, label);
  }

  void drawPlacedShips(SDL_Renderer* renderer, const SDL_Rect& board,
                       const std::vector<PlacedShip>& ships, bool /*hideIfSunkOnly*/) {
    for (const auto& ship : ships) {
      if (ship.cells.empty()) continue;
      const SDL_Rect box = shipPixelRect(board, ship.cells, ship.horizontal);
      drawShipSprite(renderer, box, ship.horizontal, shipColor(ship.fleetIndex), false);
    }
  }

  void drawShotsOverlay(SDL_Renderer* renderer, TTF_Font* font, const SDL_Rect& board,
                        const Shots& shots) {
    const Palette& p = colors();
    const int cw = board.w / kSize;
    const int ch = board.h / kSize;
    for (int y = 0; y < kSize; ++y) {
      for (int x = 0; x < kSize; ++x) {
        SDL_Rect cell{board.x + x * cw, board.y + y * ch, cw - 1, ch - 1};
        const Shot shot = shots[y][x];
        if (shot == Shot::Miss)
          ui::fillCircle(renderer, cell.x + cell.w / 2, cell.y + cell.h / 2, std::max(4, cw / 5),
                         mix(p.border, p.surface, 0.15f));
        if (shot == Shot::Hit || shot == Shot::Sunk) {
          ui::drawText(renderer, font, "X", cell.x + cell.w / 2, cell.y + cell.h / 2,
                       shot == Shot::Sunk ? p.primary : mix(p.primary, p.text, 0.25f), true);
        }
      }
    }
  }

  static SDL_Rect shipPixelRect(const SDL_Rect& board, const std::vector<Point>& cells,
                                bool horizontal) {
    int minX = cells.front().x, maxX = cells.front().x;
    int minY = cells.front().y, maxY = cells.front().y;
    for (const auto& c : cells) {
      minX = std::min(minX, c.x);
      maxX = std::max(maxX, c.x);
      minY = std::min(minY, c.y);
      maxY = std::max(maxY, c.y);
    }
    const int cw = board.w / kSize;
    const int ch = board.h / kSize;
    // Fill almost the full cell strip so ships look solid, not skinny bars.
    const int pad = std::max(2, std::min(cw, ch) / 10);
    if (horizontal) {
      return SDL_Rect{board.x + minX * cw + pad, board.y + minY * ch + pad,
                      (maxX - minX + 1) * cw - 2 * pad, ch - 2 * pad};
    }
    return SDL_Rect{board.x + minX * cw + pad, board.y + minY * ch + pad, cw - 2 * pad,
                    (maxY - minY + 1) * ch - 2 * pad};
  }

  bool cellAt(const SDL_Rect& board, int mx, int my, Point& out) const {
    if (!ui::pointInRect(mx, my, board)) return false;
    out.x = (mx - board.x) * kSize / board.w;
    out.y = (my - board.y) * kSize / board.h;
    return out.x >= 0 && out.y >= 0 && out.x < kSize && out.y < kSize;
  }

  std::vector<Point> cellsFor(int length, int x, int y, bool horiz) const {
    std::vector<Point> cells;
    for (int i = 0; i < length; ++i) {
      const int cx = horiz ? x + i : x;
      const int cy = horiz ? y : y + i;
      if (cx < 0 || cy < 0 || cx >= kSize || cy >= kSize) return {};
      cells.push_back({cx, cy});
    }
    return cells;
  }

  bool fits(const Occ& occ, const std::vector<Point>& cells) const {
    if (cells.empty()) return false;
    for (const auto& c : cells)
      if (occ[c.y][c.x] >= 0) return false;
    return true;
  }

  void rebuildPlayerFromState() {
    clearGrid(playerOcc_);
    playerShips_.clear();
    for (int i = 0; i < kFleetCount; ++i) {
      if (!onBoard_[i]) continue;
      auto cells = cellsFor(kFleet[i].length, origin_[i].x, origin_[i].y, horiz_[i]);
      PlacedShip ship;
      ship.fleetIndex = i;
      ship.spec = kFleet[i];
      ship.cells = cells;
      ship.horizontal = horiz_[i];
      playerShips_.push_back(ship);
      for (const auto& c : cells) playerOcc_[c.y][c.x] = static_cast<int>(playerShips_.size()) - 1;
    }
  }

  void placeFleetShip(int fleetIndex, Point origin, bool horiz) {
    auto cells = cellsFor(kFleet[fleetIndex].length, origin.x, origin.y, horiz);
    if (!fits(playerOcc_, cells)) return;
    onBoard_[fleetIndex] = true;
    origin_[fleetIndex] = origin;
    horiz_[fleetIndex] = horiz;
    rebuildPlayerFromState();
  }

  void removeFleetFromBoard(int fleetIndex) {
    onBoard_[fleetIndex] = false;
    origin_[fleetIndex] = Point{-1, -1};
    rebuildPlayerFromState();
  }

  static void clearGrid(Occ& g) {
    for (auto& row : g) row.fill(-1);
  }
  static void clearShots(Shots& g) {
    for (auto& row : g) row.fill(Shot::None);
  }

  void placeFleet(Occ& occ, std::vector<PlacedShip>& ships) {
    for (int fi = 0; fi < kFleetCount; ++fi) {
      const auto& spec = kFleet[fi];
      for (int attempt = 0; attempt < 300; ++attempt) {
        const bool horiz = std::uniform_int_distribution<int>(0, 1)(rng_) == 1;
        const int x = std::uniform_int_distribution<int>(0, kSize - 1)(rng_);
        const int y = std::uniform_int_distribution<int>(0, kSize - 1)(rng_);
        auto cells = cellsFor(spec.length, x, y, horiz);
        if (!fits(occ, cells)) continue;
        PlacedShip ship;
        ship.fleetIndex = fi;
        ship.spec = spec;
        ship.cells = cells;
        ship.horizontal = horiz;
        ships.push_back(ship);
        for (const auto& c : cells) occ[c.y][c.x] = static_cast<int>(ships.size()) - 1;
        break;
      }
    }
  }

  void randomizePlayer() {
    dragging_ = false;
    dragFleet_ = -1;
    clearGrid(playerOcc_);
    playerShips_.clear();
    for (int i = 0; i < kFleetCount; ++i) onBoard_[i] = false;
    placeFleet(playerOcc_, playerShips_);
    // Sync state arrays from playerShips_
    for (const auto& ship : playerShips_) {
      onBoard_[ship.fleetIndex] = true;
      horiz_[ship.fleetIndex] = ship.horizontal;
      origin_[ship.fleetIndex] = ship.cells.front();
      // Ensure origin is min corner
      Point o = ship.cells.front();
      for (const auto& c : ship.cells) {
        if (c.x < o.x || c.y < o.y) o = c;
      }
      // For horizontal, origin is leftmost; for vertical, topmost — cellsFor uses start as first
      if (ship.horizontal) {
        o = ship.cells.front();
        for (const auto& c : ship.cells)
          if (c.x < o.x) o = c;
      } else {
        o = ship.cells.front();
        for (const auto& c : ship.cells)
          if (c.y < o.y) o = c;
      }
      origin_[ship.fleetIndex] = o;
    }
    rebuildPlayerFromState();
  }

  Shot applyShot(Point cell, Occ& occ, std::vector<PlacedShip>& ships, Shots& shots) {
    const int idx = occ[cell.y][cell.x];
    if (idx < 0) {
      shots[cell.y][cell.x] = Shot::Miss;
      return Shot::Miss;
    }
    auto& ship = ships[static_cast<size_t>(idx)];
    if (std::find(ship.hits.begin(), ship.hits.end(), cell) == ship.hits.end())
      ship.hits.push_back(cell);
    if (ship.sunk()) {
      for (const auto& c : ship.cells) shots[c.y][c.x] = Shot::Sunk;
      return Shot::Sunk;
    }
    shots[cell.y][cell.x] = Shot::Hit;
    return Shot::Hit;
  }

  void checkOutcome() {
    const bool cpuDead =
        !cpuShips_.empty() && std::all_of(cpuShips_.begin(), cpuShips_.end(),
                                          [](const PlacedShip& s) { return s.sunk(); });
    const bool playerDead =
        !playerShips_.empty() && std::all_of(playerShips_.begin(), playerShips_.end(),
                                             [](const PlacedShip& s) { return s.sunk(); });
    if (cpuDead) outcome_ = GameOutcome::Win;
    else if (playerDead) outcome_ = GameOutcome::Lose;
  }

  std::string shotLine(const char* who, Shot result) const {
    if (result == Shot::Miss) return std::string(who) + " missed.";
    if (result == Shot::Hit) return std::string(who) + " hit a ship!";
    if (result == Shot::Sunk) return std::string(who) + " sunk a ship!";
    return lastEvent_;
  }

  void notifyShot(Point cell, Shot result) {
    if (result == Shot::Miss) {
      if (dir_.x != -1 || dir_.y != -1) {
        dir_ = Point{-dir_.x, -dir_.y};
        lastHit_ = originHit_;
      }
      return;
    }
    if (result == Shot::Sunk) {
      targets_.clear();
      lastHit_ = originHit_ = dir_ = Point{-1, -1};
      return;
    }
    lastHit_ = cell;
    if (originHit_.x < 0) originHit_ = cell;
    if (originHit_.x >= 0 && !(cell == originHit_) && dir_.x == -1 && dir_.y == -1)
      dir_ = Point{cell.x - originHit_.x, cell.y - originHit_.y};
    if (difficulty_ != Difficulty::Easy) {
      const Point dirs[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
      for (const auto& d : dirs) {
        Point n{cell.x + d.x, cell.y + d.y};
        if (n.x >= 0 && n.y >= 0 && n.x < kSize && n.y < kSize) targets_.push_back(n);
      }
    }
  }

  Point pickShot() {
    if (difficulty_ != Difficulty::Easy) {
      while (!targets_.empty()) {
        Point next = targets_.back();
        targets_.pop_back();
        if (playerShots_[next.y][next.x] == Shot::None) return next;
      }
      if (lastHit_.x >= 0 && (dir_.x != -1 || dir_.y != -1)) {
        Point next{lastHit_.x + dir_.x, lastHit_.y + dir_.y};
        if (next.x >= 0 && next.y >= 0 && next.x < kSize && next.y < kSize &&
            playerShots_[next.y][next.x] == Shot::None)
          return next;
      }
    }
    std::vector<Point> candidates;
    for (int y = 0; y < kSize; ++y)
      for (int x = 0; x < kSize; ++x) {
        if (playerShots_[y][x] != Shot::None) continue;
        if (difficulty_ == Difficulty::Hard && ((x + y) & 1)) continue;
        candidates.push_back({x, y});
      }
    if (candidates.empty()) {
      for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x)
          if (playerShots_[y][x] == Shot::None) candidates.push_back({x, y});
    }
    return candidates[static_cast<size_t>(
        std::uniform_int_distribution<int>(0, static_cast<int>(candidates.size()) - 1)(rng_))];
  }

  Difficulty difficulty_ = Difficulty::Medium;
  bool placing_ = true;
  bool horizontal_ = true;
  bool playerTurn_ = true;
  bool thinking_ = false;
  double thinkTimer_ = 0;
  GameOutcome outcome_ = GameOutcome::None;
  std::string lastEvent_;
  struct LogEntry {
    std::string text;
    bool hit = false;
  };
  std::vector<LogEntry> log_;
  Occ playerOcc_{}, cpuOcc_{};
  Shots playerShots_{}, cpuShots_{};
  std::vector<PlacedShip> playerShips_, cpuShips_;
  std::vector<Point> targets_;
  Point lastHit_{-1, -1}, originHit_{-1, -1}, dir_{-1, -1};
  std::mt19937 rng_{std::random_device{}()};

  bool dragging_ = false;
  int dragFleet_ = -1;
  bool dragHadBoard_ = false;
  Point dragPrevOrigin_{-1, -1};
  bool dragPrevHoriz_ = true;
  int mouseX_ = 0;
  int mouseY_ = 0;
  std::array<bool, kFleetCount> onBoard_{};
  std::array<bool, kFleetCount> horiz_{};
  std::array<Point, kFleetCount> origin_{};
};

}  // namespace

std::unique_ptr<Game> makeBattleship() { return std::make_unique<Battleship>(); }
