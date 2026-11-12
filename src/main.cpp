#include <iostream>
#include <string>
#include <vector>
#include "package.hpp"
#include "logger.hpp"

const std::string FARO_CACHE = "/var/faro/cache/";

namespace faro::cli {

    const char* VERSION = "0.3.0";

    void print_help() {
        std::cout
            << "Faro Package Manager " << VERSION << "\n\n"
            << "Usage:\n"
            << "  faro <command> [arguments]\n\n"
            << "Commands:\n"
            << "  build <archive>          Build a package\n"
            << "  install <archive>        Install a package\n"
            << "  build-install <archive>  Build and install a package\n\n"
            << "Options:\n"
            << "  -h, --help               Display this help message\n"
            << "  -v, --version            Display program version\n";
    }

    void print_version() {
        std::cout << "Faro Package Manager " << VERSION << '\n';
    }

    int build(const std::string& path) {
        faro::log::info("Using Faro to build a package.");

        faro::log::step("Reading package info.");

        faro::Package package = faro::get_package(path);

        if (package.name.empty()) {
            faro::log::error("Could not load package information.");
            return 1;
        }

        faro::log::success("Loaded " + package.name + " (" + package.version + ")");

        if (package.built) {
          std::cout << "\nThis package has already been built. Proceed anyway? [y/N]: ";
          char answer;
          if (!(std::cin >> answer) ||
              (answer != 'y' && answer != 'Y')) {
              faro::log::warning("Operation aborted by user.");
              return 1;
          }
        }

        if (faro::build_package(package) != 0) {
            faro::log::error("Package build failed.");
            return 1;
        }

        return 0;
    }

    int install(const std::string& path) {
        faro::log::info("Using Faro to install a package.");

        faro::log::step("Reading package info");

        faro::Package package = faro::get_package(path);

        if (package.name.empty()) {
            faro::log::error("Could not load package information.");
            return 1;
        }

        faro::log::success("Loaded " + package.name + " (" + package.version + ")");

        if (!package.built) {
            faro::log::error("This package has not been built yet. Run 'faro build' first.");
            return 1;
        }

        if (faro::install_package(package) != 0) {
            faro::log::error("Package installation failed.");
            return 1;
        }

        return 0;
    }

    int build_install(const std::string& path) {     
        faro::log::info("Using Faro to build and install a package.");
        faro::log::step("Reading package info.");

        faro::Package package = faro::get_package(path);

        if (package.name.empty()) {
            faro::log::error("Could not load package information.");
            return 1;
        }

        faro::log::success("Loaded " + package.name + " (" + package.version + ")");

        if (package.built) {
          std::cout << "\nThis package has already been built. Proceed anyway? [y/N]: ";
          char answer;
          if (!(std::cin >> answer) ||
              (answer != 'y' && answer != 'Y')) {
              faro::log::warning("Operation aborted by user.");
              return 1;
          }
        }

        if (faro::build_package(package) != 0) {
            faro::log::error("Package build failed.");
            return 1;
        }

        std::string package_path = FARO_CACHE + package.name + "-" + package.version + ".tar.xz";
        faro::log::step("Reading built info");
        package = faro::get_package(package_path);

        if (package.name.empty()) {
            faro::log::error("Could not load package information.");
            return 1;
        }

        faro::log::success("Loaded " + package.name + " (" + package.version + ")");

        if (faro::install_package(package) != 0) {
            faro::log::error("Package installation failed.");
            return 1;
        }
        
        return 0;
    }

    int remove(const std::string& name) {
        int result = faro::remove_package(name);
        switch (result) {
        case 2:
            faro::log::error(
            "Package removal failed. The package may be partially uninstalled. "
            "Check its installation state before retrying."
            );
            break;
        case 1:
            faro::log::error("There was an error while starting to remove package");
            break;
        default:
            break;
        }
        return result;
    }

    int list() {
        return faro::list_packages();
    }

    int run(const std::vector<std::string>& args) {
        if (args.empty()) {
            print_help();
            return 1;
        }

        const std::string command = args[0];

        if (command == "--help" || command == "-h" || command == "help") {
            print_help();
            return 0;
        }

        if (command == "--version" || command == "-v" || command == "version") {
            print_version();
            return 0;
        }

        if (command == "list") {
            return list();
        }

        if (command == "build" || command == "install" || command == "build-install" ||
            command == "remove") {
            if (args.size() != 2) { 
                faro::log::error("Invalid number of arguments for command '" + command + "'.");
                std::cout << "\n";
                print_help();
                return 1;
            }
            if (command == "build") {
                return build(args[1]);
            } else if (command == "install") {
                return install(args[1]);
            } else if (command == "build-install") {
                return build_install(args[1]);
            }
            return remove(args[1]);
        }
        
        faro::log::error("Unknown command: " + command);
        std::cout << "\n";
        print_help();

        return 1;
    }
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args;

    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    return faro::cli::run(args);
}

