cmake_minimum_required(VERSION 3.16)
project(gamealone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

add_executable(gamealone
  src/main.cpp
  src/app.cpp
  src/render.cpp
  src/theme.cpp
  src/games/tic_tac_toe.cpp
  src/games/battleship.cpp
  src/games/connect_four.cpp
  src/games/checkers.cpp
  src/games/hangman.cpp
  src/games/memory.cpp
  src/games/rock_paper_scissors.cpp
  src/games/reversi.cpp
)

target_include_directories(gamealone PRIVATE src)

if(TARGET SDL2::SDL2)
  target_link_libraries(gamealone PRIVATE SDL2::SDL2)
else()
  target_include_directories(gamealone PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(gamealone PRIVATE ${SDL2_LIBRARIES})
endif()

if(TARGET SDL2_ttf::SDL2_ttf)
  target_link_libraries(gamealone PRIVATE SDL2_ttf::SDL2_ttf)
else()
  target_include_directories(gamealone PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
  target_link_libraries(gamealone PRIVATE ${SDL2_TTF_LIBRARIES})
endif()

if(APPLE)
  target_link_libraries(gamealone PRIVATE "-framework Cocoa")
endif()
