#include <iostream>
#include "package.hpp"
#include "logger.hpp"

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
    std::string path = argv[2];
    faro::log::info("Using faro to build the package.");
    faro::log::step("Fetching info about the package...");
    faro::log::success("Package fetched");
    faro::Package package = faro::get_package(path);
    if (package.built == true) {
      std::cout << std::endl;
      std::cout << "This package was built, do you want to proceed? [y/n]: ";
      char answer; 
      std::cin >> answer;
      if (answer != 'y') {
        faro::log::warning("Operation aborted by user.");
        return 1;
      }
    }
    if (faro::build_package(package) != 0) {
      faro::log::error("Package build failed.");
      return -1;
    }
  } else if (std::string(argv[1]) == "install") {
    if (argc != 3) {
      std::cerr << "Usage: faro install <archive>\n";
      return 1;
    }
    std::string path = argv[2];  
    faro::log::info("Using faro to install the package.");
    faro::log::step("Fetching info about the package...");
    faro::Package package = faro::get_package(path);
    faro::log::success("Package fetched");
    if (package.built == false) {
      std::cout << std::endl;
      faro::log::warning("This package has not been built yet. Run faro build first.");
    }
    if (faro::install_package(package) != 0) {
      faro::log::error("Package installation failed.");
      return -1;
    }
  }  
  return 0;
}
