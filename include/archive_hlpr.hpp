#pragma once
#include <string>

namespace faro::archive {
  int list_elements(std::string& path);
  int find_element(std::string& archive, const std::string path);
  int decompress_archive(std::string& archive, const std::string& path);
  int compress_folder(const std::string& path, const std::string& archive);
  int read_element(const std::string& path, const std::string& archive, std::string& content);
}
