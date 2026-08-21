# Gamealone

University C++ project: an offline desktop game hub built with **SDL2**.

Play classic games against a simple computer opponent.

## Games

1. Tic-Tac-Toe  
2. Battleship  
3. Connect Four  
4. Checkers  
5. Hangman  
6. Memory Match  
7. Rock Paper Scissors  
8. Reversi  

Each game has Easy / Medium / Hard and a Rules panel.

## Requirements (macOS)

```bash
brew install cmake sdl2 sdl2_ttf
```

## Build & run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="/opt/homebrew"
cmake --build build
./build/gamealone
```

## Project layout

```
src/
  main.cpp          entry point
  app.cpp / .hpp    screens (home, library, game UI)
  render.cpp        simple drawing helpers
  theme.cpp         dark / light colours
  games/            one file per game
```

## Controls

- **Play** → choose a game  
- **Rules** → how to play  
- **Esc** back · **R** reset · **1/2/3** difficulty · **T** theme  
