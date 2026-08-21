#include "app.hpp"

#include <iostream>

int main(int, char**) {
  App app;
  if (!app.init()) {
    std::cerr << "Failed to start Gamealone\n";
    return 1;
  }
  app.run();
  app.shutdown();
  return 0;
}
