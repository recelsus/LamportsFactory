#include <iostream>

int main() {
  try {
    std::cout << "Server is starting...\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}

