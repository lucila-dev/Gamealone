#pragma once

#include <SDL.h>

enum class ThemeMode { Dark, Light };

struct Palette {
  SDL_Color bg;
  SDL_Color bgGlow;
  SDL_Color surface;
  SDL_Color surfaceHigh;
  SDL_Color text;
  SDL_Color muted;
  SDL_Color border;
  SDL_Color primary;
  SDL_Color ticTacToe;
  SDL_Color battleship;
  SDL_Color connectFour;
  SDL_Color checkers;
  SDL_Color hangman;
  SDL_Color memory;
  SDL_Color rps;
  SDL_Color reversi;
  SDL_Color shadow;
};

void setThemeMode(ThemeMode mode);
ThemeMode themeMode();
void toggleThemeMode();
const Palette& colors();

inline SDL_Color withAlpha(SDL_Color c, Uint8 a) {
  c.a = a;
  return c;
}

inline SDL_Color mix(SDL_Color a, SDL_Color b, float t) {
  const auto lerp = [t](Uint8 x, Uint8 y) {
    return static_cast<Uint8>(x + (y - x) * t);
  };
  return SDL_Color{lerp(a.r, b.r), lerp(a.g, b.g), lerp(a.b, b.b), 255};
}
