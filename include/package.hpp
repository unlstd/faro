#pragma once

#include <string>
#include <vector>

namespace faro {

    struct Package {
        std::string name;
        std::string version;
        std::string description;

        std::string archive_path;
        bool built;
        std::vector<std::string> libs;
        std::vector<std::string> dependencies;
        std::vector<std::string> files;
        std::vector<std::vector<std::string>> build;        
        std::vector<std::vector<std::string>> install;
    };

    Package get_package(std::string path);
    int install_package(Package& package);
    int build_package(Package& package);
    int remove_package(const std::string& package_name);
    int list_packages();
    int package_info(Package& package);
}
