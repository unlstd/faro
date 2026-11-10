#pragma once
#include <string>
#include <vector>

namespace faro::posix {
  int exec_command(
    const std::vector<std::string>& args,
    const std::string& working_directory
);
}
