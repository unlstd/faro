#include "logger.hpp"
#include <iostream>

void faro::log::info(const std::string &message) {
  std::cout << faro::log::color::CYAN; 
  std::cout << "[INFO] ";
  std::cout << faro::log::color::RESET;
  std::cout << message << '\n';  
}

void faro::log::step(const std::string &message) {
  std::cout << faro::log::color::BLUE; 
  std::cout << "==> ";
  std::cout << message << '\n';  
  std::cout << faro::log::color::RESET;
}

void faro::log::debug(const std::string &message) {
  std::cout << faro::log::color::MAGENTA; 
  std::cout << "[DEBUG] ";
  std::cout << faro::log::color::RESET;
  std::cout << message << '\n';  
}

void faro::log::success(const std::string &message) {
  std::cout << faro::log::color::GREEN; 
  std::cout << "[ OK ] ";
  std::cout << message << '\n';  
  std::cout << faro::log::color::RESET;
}

void faro::log::status(const std::string &message) {
  std::cout << faro::log::color::YELLOW; 
  std::cout << "[ RUN ] ";
  std::cout << message << '\n';  
  std::cout << faro::log::color::RESET;
}

void faro::log::error(const std::string &message) {
  std::cout << faro::log::color::RED; 
  std::cout << "[ERR] ";
  std::cout << message << '\n';  
  std::cout << faro::log::color::RESET;
}

void faro::log::warning(const std::string &message) {
  std::cout << faro::log::color::YELLOW; 
  std::cout << "[WARN] ";
  std::cout << message << '\n';  
  std::cout << faro::log::color::RESET;
}
