#include "theme.hpp"

namespace {

ThemeMode gMode = ThemeMode::Light;

const Palette kDark{
    {42, 28, 38, 255},
    {72, 42, 58, 255},
    {58, 36, 48, 255},
    {78, 48, 64, 255},
    {255, 240, 246, 255},
    {232, 180, 198, 255},
    {140, 90, 110, 255},
    {255, 143, 171, 255},
    {255, 170, 190, 255},
    {186, 170, 255, 255},
    {255, 200, 140, 255},
    {255, 150, 170, 255},
    {220, 170, 255, 255},
    {255, 160, 200, 255},
    {170, 230, 200, 255},
    {255, 210, 150, 255},
    {0, 0, 0, 70},
};

// Soft pink hub template
const Palette kLight{
    {255, 240, 246, 255},   // bg
    {255, 214, 228, 255},   // bgGlow
    {255, 252, 254, 255},   // surface (card white)
    {255, 232, 240, 255},   // surfaceHigh
    {90, 40, 60, 255},      // text
    {180, 120, 140, 255},   // muted
    {255, 200, 218, 255},   // border
    {255, 120, 160, 255},   // primary
    {255, 140, 170, 255},   // ticTacToe
    {170, 150, 255, 255},   // battleship
    {255, 180, 120, 255},   // connectFour
    {255, 120, 150, 255},   // checkers
    {200, 150, 255, 255},   // hangman
    {255, 130, 180, 255},   // memory
    {120, 200, 170, 255},   // rps
    {255, 170, 100, 255},   // reversi
    {255, 160, 190, 45},    // shadow
};

}  // namespace

void setThemeMode(ThemeMode mode) { gMode = mode; }
ThemeMode themeMode() { return gMode; }
void toggleThemeMode() { gMode = (gMode == ThemeMode::Dark) ? ThemeMode::Light : ThemeMode::Dark; }
const Palette& colors() { return gMode == ThemeMode::Dark ? kDark : kLight; }
