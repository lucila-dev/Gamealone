# Gamealone

A desktop app where you play classic games against the computer. Everything runs offline — no accounts, no network.

## Tech stack

- **C++17**
- **SDL2** + **SDL2_ttf** for graphics and text
- **CMake** for building

## What it does

Opens with a home screen, then a library of eight games:

- Tic-Tac-Toe  
- Battleship  
- Connect Four  
- Checkers  
- Hangman  
- Memory Match  
- Rock Paper Scissors  
- Reversi  

Each game has Easy / Medium / Hard difficulty, a Rules panel, and dark/light theme.

## Build & run (macOS)

```bash
brew install cmake sdl2 sdl2_ttf
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="/opt/homebrew"
cmake --build build
./build/gamealone
```
