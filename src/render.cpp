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
  // Soft outline via inset fillRoundRect trick (avoids jagged circle outlines).
  radius = std::min({radius, rect.w / 2, rect.h / 2});
  thickness = std::max(1, thickness);
  for (int i = 0; i < thickness; ++i) {
    SDL_Rect rr{rect.x + i, rect.y + i, rect.w - 2 * i, rect.h - 2 * i};
    if (rr.w <= 0 || rr.h <= 0) break;
    // Approximate: draw as thin ring using four sides + corners already in fill
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    // Top / bottom
    SDL_RenderDrawLine(r, rr.x + radius, rr.y, rr.x + rr.w - radius - 1, rr.y);
    SDL_RenderDrawLine(r, rr.x + radius, rr.y + rr.h - 1, rr.x + rr.w - radius - 1, rr.y + rr.h - 1);
    // Left / right
    SDL_RenderDrawLine(r, rr.x, rr.y + radius, rr.x, rr.y + rr.h - radius - 1);
    SDL_RenderDrawLine(r, rr.x + rr.w - 1, rr.y + radius, rr.x + rr.w - 1, rr.y + rr.h - radius - 1);
    // Corner dots (filled tiny arcs)
    const int rad = std::max(1, radius - i);
    for (int a = 0; a <= 90; a += 3) {
      const double radA = a * 3.141592653589793 / 180.0;
      const int dx = static_cast<int>(std::lround(rad * std::cos(radA)));
      const int dy = static_cast<int>(std::lround(rad * std::sin(radA)));
      SDL_RenderDrawPoint(r, rr.x + radius - dx, rr.y + radius - dy);
      SDL_RenderDrawPoint(r, rr.x + rr.w - radius - 1 + dx, rr.y + radius - dy);
      SDL_RenderDrawPoint(r, rr.x + radius - dx, rr.y + rr.h - radius - 1 + dy);
      SDL_RenderDrawPoint(r, rr.x + rr.w - radius - 1 + dx, rr.y + rr.h - radius - 1 + dy);
    }
  }
}

void drawRoundRectOn(SDL_Renderer* r, const SDL_Rect& rect, int radius, SDL_Color border,
                     SDL_Color fill, int thickness) {
  fillRoundRect(r, rect, radius, fill);
  drawRoundRect(r, rect, radius, border, thickness);
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

int measureTextWrapped(TTF_Font* font, const std::string& text, int maxWidth, int lineHeight) {
  if (text.empty()) return 0;
  std::string line;
  int lines = 0;
  auto flush = [&]() {
    if (line.empty()) return;
    ++lines;
    line.clear();
  };

  size_t i = 0;
  while (i < text.size()) {
    if (text[i] == '\n') {
      if (line.empty()) ++lines;  // blank line still advances
      else flush();
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
  if (lines <= 0) return 0;
  // Last line occupies font height; prior lines advance by lineHeight.
  const int fontH = std::max(lineHeight, textSize(font, "Ag").h);
  return (lines - 1) * lineHeight + fontH;
}

void button(SDL_Renderer* renderer, TTF_Font* font, const SDL_Rect& rect, const std::string& label,
            SDL_Color accent, bool hover, bool selected) {
  const Palette& p = colors();
  // Template pills: soft blush fill, thin rose border, selected = stronger tint
  SDL_Color bg = selected ? mix(accent, p.surface, 0.55f)
                          : (hover ? p.surfaceHigh : withAlpha(p.surface, 230));
  if (!selected && !hover) bg = p.surface;
  const int radius = std::max(rect.h / 2 - 1, S(12));
  fillRoundRect(renderer, rect, radius, bg);
  drawRoundRect(renderer, rect, radius, selected ? accent : p.border,
                selected ? std::max(2, S(2)) : 1);
  drawText(renderer, font, label, rect.x + rect.w / 2, rect.y + rect.h / 2,
           selected ? accent : p.text, true);
}

void card(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color accent, bool hover) {
  const Palette& p = colors();
  const int radius = std::max(16, S(18));

  // Soft drop shadow
  fillRoundRect(renderer, SDL_Rect{rect.x + S(2), rect.y + S(4), rect.w, rect.h}, radius, p.shadow);

  // White card body
  fillRoundRect(renderer, rect, radius, hover ? p.surfaceHigh : p.surface);
  drawRoundRect(renderer, rect, radius, hover ? mix(accent, p.border, 0.35f) : p.border,
                hover ? 2 : 1);

  // Decorative corner orbs (template)
  const SDL_Color orb = withAlpha(mix(accent, p.bgGlow, 0.45f), 70);
  const int orbR = S(28);
  fillCircle(renderer, rect.x + S(8), rect.y + S(8), orbR, orb);
  fillCircle(renderer, rect.x + rect.w - S(8), rect.y + S(8), orbR, orb);
  fillCircle(renderer, rect.x + S(8), rect.y + rect.h - S(8), orbR, orb);
  fillCircle(renderer, rect.x + rect.w - S(8), rect.y + rect.h - S(8), orbR, orb);

  // Left accent bar
  fillRoundRect(renderer,
                SDL_Rect{rect.x + S(14), rect.y + S(18), std::max(3, S(4)), rect.h - S(36)}, S(2),
                accent);
}

void panel(SDL_Renderer* renderer, const SDL_Rect& rect, int radius) {
  const Palette& p = colors();
  fillRoundRect(renderer, SDL_Rect{rect.x + S(2), rect.y + S(3), rect.w, rect.h}, radius, p.shadow);
  fillRoundRect(renderer, rect, radius, p.surface);
  drawRoundRect(renderer, rect, radius, p.border, 1);
}

}  // namespace ui
