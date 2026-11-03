#include <iostream>

#include "cli.hpp"

int main(int argc, char** argv) {
  try {
    return rzm::cli::run(argc, argv);
  } catch (const std::exception& error) {
    std::cerr << "fatal: " << error.what() << '\n';
    return 1;
  }
}
