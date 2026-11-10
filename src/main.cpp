#include <iostream>
#include "package.hpp"

int main (int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: faro <command> [arguments]\n";
    return 1;
  }

  if (std::string(argv[1]) == "build") {
    if (argc != 3) {
        std::cerr << "Usage: faro build <archive>\n";
        return 1;
    }
    std::string path = argv[2];  std::string temp_folder = "/home/unlstd/INF/CPP/test";
    std::cout << "Using faro to build the package. \n";
    std::cout << "Fetching info about the package... \n";
    faro::Package package = faro::get_package(path);
    if (package.built == true) {
      std::cout << std::endl;
      std::cout << "This package was built, do you want to proceed? [y/n]: ";
      char answer; 
      std::cin >> answer;
      if (answer != 'y') {
        std::cout << "Aborted. \n"; // It doesn't really matter what user type as long as it isn't y
        return 1;
      }
    }
    if (faro::build_package(package) != 0) {
      std::cerr << "[ERR] build_package() did not return 0 \n";
      return -1;
    }
  } else if (std::string(argv[1]) == "install") {
    if (argc != 3) {
      std::cerr << "Usage: faro install <archive>\n";
      return 1;
    }
    std::string path = argv[2];  std::string temp_folder = "/home/unlstd/INF/CPP/test";
    std::cout << "Using faro to install the package. \n";
    std::cout << "Fetching info about the package... \n";
    faro::Package package = faro::get_package(path);
    if (package.built == false) {
      std::cout << std::endl;
      std::cout << "This package has not been built yet. Run faro build to build it first.";
    }
    if (faro::install_package(package) != 0) {
      std::cerr << "[ERR] install_package() did not return 0 \n";
      return -1;
    }
  }  
  return 0;
}
