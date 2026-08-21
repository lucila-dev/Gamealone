#include "theme.hpp"

namespace {

ThemeMode gMode = ThemeMode::Dark;

const Palette kDark{
    {10, 12, 22, 255},      // bg
    {24, 32, 58, 255},      // bgGlow
    {20, 26, 42, 255},      // surface
    {32, 42, 66, 255},      // surfaceHigh
    {240, 244, 252, 255},   // text
    {148, 163, 184, 255},   // muted
    {58, 72, 104, 255},     // border
    {129, 168, 255, 255},   // primary
    {45, 212, 191, 255},    // ticTacToe
    {56, 189, 248, 255},    // battleship
    {251, 191, 36, 255},    // connectFour
    {248, 113, 113, 255},   // checkers
    {167, 139, 250, 255},   // hangman
    {251, 113, 133, 255},   // memory
    {52, 211, 153, 255},    // rps
    {250, 204, 21, 255},    // reversi
    {0, 0, 0, 90},          // shadow
};

const Palette kLight{
    {244, 247, 251, 255},   // bg
    {226, 234, 248, 255},   // bgGlow
    {255, 255, 255, 255},   // surface
    {228, 235, 245, 255},   // surfaceHigh
    {15, 23, 42, 255},      // text
    {100, 116, 139, 255},   // muted
    {203, 213, 225, 255},   // border
    {37, 99, 235, 255},     // primary
    {13, 148, 136, 255},    // ticTacToe
    {2, 132, 199, 255},     // battleship
    {217, 119, 6, 255},     // connectFour
    {220, 38, 38, 255},     // checkers
    {124, 58, 237, 255},    // hangman
    {225, 29, 72, 255},     // memory
    {5, 150, 105, 255},     // rps
    {161, 98, 7, 255},      // reversi
    {15, 23, 42, 28},       // shadow
};

}  // namespace

void setThemeMode(ThemeMode mode) { gMode = mode; }
ThemeMode themeMode() { return gMode; }
void toggleThemeMode() { gMode = (gMode == ThemeMode::Dark) ? ThemeMode::Light : ThemeMode::Dark; }
const Palette& colors() { return gMode == ThemeMode::Dark ? kDark : kLight; }
