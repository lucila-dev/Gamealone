#pragma once

#include "theme.hpp"

#include <SDL.h>
#include <SDL_ttf.h>

#include <cmath>
#include <string>

namespace ui {

void setScale(float scale);
float scale();
inline int S(int v) { return static_cast<int>(std::lround(static_cast<double>(v) * scale())); }
inline int S(float v) { return static_cast<int>(std::lround(static_cast<double>(v) * scale())); }
inline SDL_Rect SR(int x, int y, int w, int h) { return SDL_Rect{S(x), S(y), S(w), S(h)}; }

bool pointInRect(int x, int y, const SDL_Rect& r);
void fillRect(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color c);
void drawRect(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color c, int thickness = 1);
void fillRoundRect(SDL_Renderer* r, const SDL_Rect& rect, int radius, SDL_Color c);
void drawRoundRect(SDL_Renderer* r, const SDL_Rect& rect, int radius, SDL_Color c, int thickness = 1);
void drawRoundRectOn(SDL_Renderer* r, const SDL_Rect& rect, int radius, SDL_Color border,
                     SDL_Color fill, int thickness = 1);
void fillCircle(SDL_Renderer* r, int cx, int cy, int radius, SDL_Color c);
void drawText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y,
              SDL_Color color, bool center = false);
void drawTextWrapped(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y,
                     int maxWidth, SDL_Color color, int lineHeight);
int measureTextWrapped(TTF_Font* font, const std::string& text, int maxWidth, int lineHeight);
SDL_Rect textSize(TTF_Font* font, const std::string& text);
void button(SDL_Renderer* renderer, TTF_Font* font, const SDL_Rect& rect, const std::string& label,
            SDL_Color accent, bool hover, bool selected = false);
void card(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color accent, bool hover);
void panel(SDL_Renderer* renderer, const SDL_Rect& rect, int radius);

}  // namespace ui
