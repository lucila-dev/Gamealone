#include "render.hpp"

#include <algorithm>
#include <cmath>

namespace ui {
namespace {
float gScale = 1.0f;
}

void setScale(float scale) { gScale = scale > 0.01f ? scale : 1.0f; }
float scale() { return gScale; }

bool pointInRect(int x, int y, const SDL_Rect& r) {
  return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

void fillRect(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color c) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  SDL_RenderFillRect(r, &rect);
}

void drawRect(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color c, int thickness) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  for (int i = 0; i < thickness; ++i) {
    SDL_Rect rr{rect.x + i, rect.y + i, rect.w - 2 * i, rect.h - 2 * i};
    SDL_RenderDrawRect(r, &rr);
  }
}

void fillCircle(SDL_Renderer* r, int cx, int cy, int radius, SDL_Color c) {
  if (radius <= 0) return;
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  for (int dy = -radius; dy <= radius; ++dy) {
    const int dx = static_cast<int>(std::sqrt(static_cast<double>(radius * radius - dy * dy)));
    SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
  }
}

void fillRoundRect(SDL_Renderer* r, const SDL_Rect& rect, int radius, SDL_Color c) {
  radius = std::min({radius, rect.w / 2, rect.h / 2});
  if (radius <= 0) {
    fillRect(r, rect, c);
    return;
  }
  fillRect(r, SDL_Rect{rect.x + radius, rect.y, rect.w - 2 * radius, rect.h}, c);
  fillRect(r, SDL_Rect{rect.x, rect.y + radius, rect.w, rect.h - 2 * radius}, c);
  fillCircle(r, rect.x + radius, rect.y + radius, radius, c);
  fillCircle(r, rect.x + rect.w - radius - 1, rect.y + radius, radius, c);
  fillCircle(r, rect.x + radius, rect.y + rect.h - radius - 1, radius, c);
  fillCircle(r, rect.x + rect.w - radius - 1, rect.y + rect.h - radius - 1, radius, c);
}

void drawRoundRect(SDL_Renderer* r, const SDL_Rect& rect, int radius, SDL_Color c, int thickness) {
  // Simple outline: rounded fill is enough for UI; border is a normal rect.
  (void)radius;
  drawRect(r, rect, c, thickness);
}

SDL_Rect textSize(TTF_Font* font, const std::string& text) {
  int w = 0, h = 0;
  TTF_SizeUTF8(font, text.c_str(), &w, &h);
  return SDL_Rect{0, 0, w, h};
}

void drawText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y,
              SDL_Color color, bool center) {
  if (text.empty()) return;
  SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
  if (!surface) return;
  SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_Rect dst{x, y, surface->w, surface->h};
  if (center) {
    dst.x -= surface->w / 2;
    dst.y -= surface->h / 2;
  }
  SDL_FreeSurface(surface);
  if (!texture) return;
  SDL_RenderCopy(renderer, texture, nullptr, &dst);
  SDL_DestroyTexture(texture);
}

void drawTextWrapped(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y,
                     int maxWidth, SDL_Color color, int lineHeight) {
  std::string line;
  int cy = y;
  auto flush = [&]() {
    if (line.empty()) return;
    drawText(renderer, font, line, x, cy, color, false);
    cy += lineHeight;
    line.clear();
  };

  size_t i = 0;
  while (i < text.size()) {
    if (text[i] == '\n') {
      flush();
      ++i;
      continue;
    }
    size_t j = i;
    while (j < text.size() && text[j] != ' ' && text[j] != '\n') ++j;
    const std::string word = text.substr(i, j - i);
    const std::string trial = line.empty() ? word : line + " " + word;
    if (textSize(font, trial).w > maxWidth && !line.empty()) {
      flush();
      line = word;
    } else {
      line = trial;
    }
    i = j;
    if (i < text.size() && text[i] == ' ') ++i;
  }
  flush();
}

void button(SDL_Renderer* renderer, TTF_Font* font, const SDL_Rect& rect, const std::string& label,
            SDL_Color accent, bool hover, bool selected) {
  const Palette& p = colors();
  SDL_Color bg = selected ? mix(accent, p.surface, 0.55f) : (hover ? p.surfaceHigh : p.surface);
  const int radius = std::max(8, S(10));
  fillRoundRect(renderer, rect, radius, bg);
  drawRoundRect(renderer, rect, radius, selected ? accent : (hover ? accent : p.border),
                selected || hover ? std::max(2, S(2)) : std::max(1, S(1)));
  const SDL_Color textColor = selected ? accent : p.text;
  drawText(renderer, font, label, rect.x + rect.w / 2, rect.y + rect.h / 2, textColor, true);
}

void card(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color accent, bool hover) {
  const Palette& p = colors();
  const int radius = std::max(12, S(16));
  fillRoundRect(renderer, SDL_Rect{rect.x + S(3), rect.y + S(5), rect.w, rect.h}, radius, p.shadow);
  fillRoundRect(renderer, rect, radius, hover ? p.surfaceHigh : p.surface);
  drawRoundRect(renderer, rect, radius, hover ? accent : p.border, hover ? std::max(2, S(2)) : 1);
  fillRect(renderer, SDL_Rect{rect.x, rect.y, rect.w, std::max(4, S(6))}, accent);
}

}  // namespace ui
