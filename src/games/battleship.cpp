#include "game.hpp"
#include "theme.hpp"
#include "render.hpp"

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

struct ShipSpec {
  const char* name;
  int length;
};

constexpr ShipSpec kFleet[] = {
    {"Carrier", 5}, {"Battleship", 4}, {"Cruiser", 3}, {"Submarine", 3}, {"Destroyer", 2},
};

enum class Shot : uint8_t { None, Miss, Hit, Sunk };

struct Point {
  int x = 0;
  int y = 0;
  bool operator==(const Point& o) const { return x == o.x && y == o.y; }
};

struct PlacedShip {
  ShipSpec spec{};
  std::vector<Point> cells;
  std::vector<Point> hits;
  bool sunk() const { return static_cast<int>(hits.size()) >= spec.length; }
};

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
    selectedShip_ = 0;
    playerTurn_ = true;
    thinking_ = false;
    thinkTimer_ = 0;
    outcome_ = GameOutcome::None;
    lastEvent_ = "Place your ships, then start battle.";
    clearGrid(playerOcc_);
    clearGrid(cpuOcc_);
    clearShots(playerShots_);
    clearShots(cpuShots_);
    playerShips_.clear();
    cpuShips_.clear();
    targets_.clear();
    lastHit_ = originHit_ = dir_ = Point{-1, -1};
    placeFleet(cpuOcc_, cpuShips_);
  }

  void onEvent(const SDL_Event& e) override {
    if (e.type == SDL_KEYDOWN) {
      if (e.key.keysym.sym == SDLK_SPACE && placing_) horizontal_ = !horizontal_;
      if (e.key.keysym.sym == SDLK_a && placing_) randomizePlayer();
      if (e.key.keysym.sym == SDLK_RETURN && placing_ &&
          static_cast<int>(playerShips_.size()) == 5) {
        placing_ = false;
        lastEvent_ = "Fire on the computer's waters.";
      }
      return;
    }
    if (e.type != SDL_MOUSEBUTTONDOWN || e.button.button != SDL_BUTTON_LEFT) return;

    if (placing_) {
      // UI buttons
      if (ui::pointInRect(e.button.x, e.button.y, ui::SR(40, 640, 140, 36))) {
        horizontal_ = !horizontal_;
        return;
      }
      if (ui::pointInRect(e.button.x, e.button.y, ui::SR(200, 640, 140, 36))) {
        randomizePlayer();
        return;
      }
      if (ui::pointInRect(e.button.x, e.button.y, ui::SR(360, 640, 140, 36)) &&
          static_cast<int>(playerShips_.size()) == 5) {
        placing_ = false;
        lastEvent_ = "Fire on the computer's waters.";
        return;
      }
      for (int i = 0; i < 5; ++i) {
        if (ui::pointInRect(e.button.x, e.button.y, ui::SR(40 + i * 170, 580, 160, 34))) {
          selectedShip_ = i;
          return;
        }
      }
      Point cell;
      if (cellAt(playerBoardRect(), e.button.x, e.button.y, cell)) {
        if (playerOcc_[cell.y][cell.x] >= 0) removeShipAt(cell);
        else placeSelected(cell);
      }
      return;
    }

    if (outcome_ != GameOutcome::None || thinking_ || !playerTurn_) return;
    Point cell;
    if (!cellAt(cpuBoardRect(), e.button.x, e.button.y, cell)) return;
    if (cpuShots_[cell.y][cell.x] != Shot::None) return;
    const Shot result = applyShot(cell, cpuOcc_, cpuShips_, cpuShots_);
    lastEvent_ = shotLine("You", result);
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
    lastEvent_ = shotLine("Computer", result);
    checkOutcome();
    playerTurn_ = outcome_ == GameOutcome::None;
  }

  void draw(SDL_Renderer* renderer, TTF_Font* font, TTF_Font*) override {
    if (placing_) {
      drawGrid(renderer, font, playerBoardRect(), playerShots_, true, playerOcc_, "Your waters");
      for (int i = 0; i < 5; ++i) {
        const bool placed = shipPlaced(i);
        const bool selected = selectedShip_ == i;
        SDL_Rect chip = ui::SR(40 + i * 170, 580, 160, 34);
        ui::button(renderer, font, chip,
                   std::string(kFleet[i].name) + " " + std::to_string(kFleet[i].length),
                   colors().battleship, false, selected || placed);
      }
      ui::button(renderer, font, ui::SR(40, 640, 140, 36),
                 horizontal_ ? "Horizontal" : "Vertical", colors().battleship, false, false);
      ui::button(renderer, font, ui::SR(200, 640, 140, 36), "Random", colors().battleship, false,
                 false);
      ui::button(renderer, font, ui::SR(360, 640, 140, 36), "Start battle", colors().battleship,
                 false, static_cast<int>(playerShips_.size()) == 5);
      return;
    }
    drawGrid(renderer, font, cpuBoardRect(), cpuShots_, false, cpuOcc_, "Computer");
    drawGrid(renderer, font, playerBoardRectBattle(), playerShots_, true, playerOcc_, "You");
  }

  std::string status() const override {
    if (outcome_ == GameOutcome::Win) return "You sunk the computer's fleet.";
    if (outcome_ == GameOutcome::Lose) return "Your fleet is gone.";
    if (placing_) {
      if (static_cast<int>(playerShips_.size()) == 5) return "Fleet ready. Start battle when ready.";
      return std::string("Place the ") + kFleet[selectedShip_].name + " (" +
             std::to_string(kFleet[selectedShip_].length) + "). Space=rotate, A=random.";
    }
    return lastEvent_;
  }

 private:
  using Occ = std::array<std::array<int, kSize>, kSize>;
  using Shots = std::array<std::array<Shot, kSize>, kSize>;

  static SDL_Rect playerBoardRect() { return ui::SR(230, 130, 500, 500); }
  static SDL_Rect cpuBoardRect() { return ui::SR(40, 140, 420, 420); }
  static SDL_Rect playerBoardRectBattle() { return ui::SR(500, 140, 420, 420); }

  static void clearGrid(Occ& g) {
    for (auto& row : g) row.fill(-1);
  }
  static void clearShots(Shots& g) {
    for (auto& row : g) row.fill(Shot::None);
  }

  bool shipPlaced(int index) const {
    for (const auto& s : playerShips_)
      if (s.spec.name == kFleet[index].name && s.spec.length == kFleet[index].length) return true;
    // Compare by length+name via index match
    return std::any_of(playerShips_.begin(), playerShips_.end(), [&](const PlacedShip& s) {
      return std::string(s.spec.name) == kFleet[index].name;
    });
  }

  bool cellAt(const SDL_Rect& board, int mx, int my, Point& out) const {
    if (!ui::pointInRect(mx, my, board)) return false;
    out.x = (mx - board.x) * kSize / board.w;
    out.y = (my - board.y) * kSize / board.h;
    return out.x >= 0 && out.y >= 0 && out.x < kSize && out.y < kSize;
  }

  void drawGrid(SDL_Renderer* renderer, TTF_Font* font, const SDL_Rect& board, const Shots& shots,
                bool showShips, const Occ& occ, const char* label) {
    ui::drawText(renderer, font, label, board.x + board.w / 2, board.y - ui::S(18), colors().text, true);
    const int cw = board.w / kSize;
    const int ch = board.h / kSize;
    for (int y = 0; y < kSize; ++y) {
      for (int x = 0; x < kSize; ++x) {
        SDL_Rect cell{board.x + x * cw, board.y + y * ch, cw - 1, ch - 1};
        const Shot shot = shots[y][x];
        const bool hasShip = showShips && occ[y][x] >= 0;
        SDL_Color color = themeMode() == ThemeMode::Dark ? SDL_Color{29, 78, 137, 255}
                                                         : SDL_Color{147, 197, 253, 255};
        if (hasShip) color = themeMode() == ThemeMode::Dark ? SDL_Color{100, 116, 139, 255}
                                                            : SDL_Color{71, 85, 105, 255};
        if (shot == Shot::Miss) color = themeMode() == ThemeMode::Dark ? SDL_Color{30, 58, 95, 255}
                                                                       : SDL_Color{191, 219, 254, 255};
        if (shot == Shot::Hit) color = SDL_Color{251, 113, 133, 255};
        if (shot == Shot::Sunk) color = SDL_Color{185, 28, 28, 255};
        ui::fillRect(renderer, cell, color);
        if (shot == Shot::Miss)
          ui::fillCircle(renderer, cell.x + cell.w / 2, cell.y + cell.h / 2, 3, colors().muted);
        if (shot == Shot::Hit || shot == Shot::Sunk)
          ui::drawText(renderer, font, "X", cell.x + cell.w / 2, cell.y + cell.h / 2, colors().text,
                       true);
      }
    }
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
    for (const auto& c : cells)
      if (occ[c.y][c.x] >= 0) return false;
    return !cells.empty();
  }

  void placeFleet(Occ& occ, std::vector<PlacedShip>& ships) {
    for (const auto& spec : kFleet) {
      for (int attempt = 0; attempt < 300; ++attempt) {
        const bool horiz = std::uniform_int_distribution<int>(0, 1)(rng_) == 1;
        const int x = std::uniform_int_distribution<int>(0, kSize - 1)(rng_);
        const int y = std::uniform_int_distribution<int>(0, kSize - 1)(rng_);
        auto cells = cellsFor(spec.length, x, y, horiz);
        if (!fits(occ, cells)) continue;
        PlacedShip ship;
        ship.spec = spec;
        ship.cells = cells;
        ships.push_back(ship);
        for (const auto& c : cells) occ[c.y][c.x] = static_cast<int>(ships.size()) - 1;
        break;
      }
    }
  }

  void randomizePlayer() {
    clearGrid(playerOcc_);
    playerShips_.clear();
    placeFleet(playerOcc_, playerShips_);
    selectedShip_ = 0;
  }

  void placeSelected(Point start) {
    if (shipPlaced(selectedShip_)) return;
    auto cells = cellsFor(kFleet[selectedShip_].length, start.x, start.y, horizontal_);
    if (!fits(playerOcc_, cells)) return;
    PlacedShip ship;
    ship.spec = kFleet[selectedShip_];
    ship.cells = cells;
    playerShips_.push_back(ship);
    for (const auto& c : cells) playerOcc_[c.y][c.x] = static_cast<int>(playerShips_.size()) - 1;
    for (int i = 0; i < 5; ++i) {
      if (!shipPlaced(i)) {
        selectedShip_ = i;
        break;
      }
    }
  }

  void removeShipAt(Point cell) {
    const int index = playerOcc_[cell.y][cell.x];
    if (index < 0) return;
    const auto ship = playerShips_[static_cast<size_t>(index)];
    for (const auto& c : ship.cells) playerOcc_[c.y][c.x] = -1;
    playerShips_.erase(playerShips_.begin() + index);
    for (int y = 0; y < kSize; ++y)
      for (int x = 0; x < kSize; ++x)
        if (playerOcc_[y][x] > index) --playerOcc_[y][x];
    for (int i = 0; i < 5; ++i) {
      if (std::string(kFleet[i].name) == ship.spec.name) {
        selectedShip_ = i;
        break;
      }
    }
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
  int selectedShip_ = 0;
  bool playerTurn_ = true;
  bool thinking_ = false;
  double thinkTimer_ = 0;
  GameOutcome outcome_ = GameOutcome::None;
  std::string lastEvent_;
  Occ playerOcc_{}, cpuOcc_{};
  Shots playerShots_{}, cpuShots_{};
  std::vector<PlacedShip> playerShips_, cpuShips_;
  std::vector<Point> targets_;
  Point lastHit_{-1, -1}, originHit_{-1, -1}, dir_{-1, -1};
  std::mt19937 rng_{std::random_device{}()};
};

}  // namespace

std::unique_ptr<Game> makeBattleship() { return std::make_unique<Battleship>(); }
