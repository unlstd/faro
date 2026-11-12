#include "archive_hlpr.hpp"
#include "posix_hlpr.hpp"
#include "package.hpp"
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <unistd.h>
#include <unordered_set>
#include "logger.hpp"
#include <iomanip>

const std::string FARO_CACHE = "/var/faro/cache/";
const std::string FARO_PREFIX = "/usr/";

// -------------------------------------------------------------
// The following functions are helpers for package manipulations
// -------------------------------------------------------------

faro::Package faro::get_package(std::string path){
  faro::Package package;
  const std::string faro_metadata = "faro_metadata.json";  
  const std::string faro_recipe = "faro_recipe.json";

  // Check is path is path or just package name.
  std::string real_path = path;
  if (!std::filesystem::exists(path)) {
    real_path = FARO_CACHE;
    real_path += path + ".tar.xz"; 
  }
  
  if (faro::archive::find_element(real_path, faro_metadata) == 0){   
    nlohmann::json metadata;
    std::string sfile;
   
    if (faro::archive::read_element(faro_metadata, real_path, sfile) != 0) {
        faro::log::error("Cannot read metadata");
        return package;
    }
    try {
        metadata = nlohmann::json::parse(sfile);
    } catch (const nlohmann::json::parse_error&) {
        faro::log::error("Incorrect metadata format.");
        return package;
    }

    if (!metadata.is_object() ||
        !metadata.contains("name") ||
        !metadata.contains("version") ||
        !metadata.contains("description") ||
        !metadata.contains("dependencies")) {

        
        faro::log::error("Incorrect metadata format.");
        return package;
    }
    
    package.name = metadata["name"];
    package.version = metadata["version"];
    package.description = metadata["description"];
    package.archive_path = real_path;
    package.built = true;
    package.dependencies = metadata["dependencies"];
    package.libs = metadata["libs"];
    
  } else if (faro::archive::find_element(real_path, faro_recipe) != 0){
    faro::log::error("Archive is not a faro package.");
    return package;
  } else if (faro::archive::find_element(real_path, faro_recipe) == 0){    
    nlohmann::json recipe;
    std::string sfile;

    if (faro::archive::read_element(faro_recipe, real_path, sfile) != 0) {
        faro::log::error("Cannot read metadata.");
        return package;
    }
    try {
        recipe = nlohmann::json::parse(sfile);
    } catch (const nlohmann::json::parse_error&) {
        faro::log::error("Incorrect recipe format.");
        return package;
    }

    if (!recipe.is_object() ||
        !recipe.contains("name") ||
        !recipe.contains("version") ||
        !recipe.contains("description") ||
        !recipe.contains("dependencies")) {

        faro::log::error("Incorrect recipe format.");
        return package;
    }
    package.name = recipe["name"];
    package.version = recipe["version"];
    package.description = recipe["description"];
    package.dependencies = recipe["dependencies"];
    package.archive_path = real_path;
    package.built = false;
    package.build = recipe["build"];
    package.install = recipe["install"];
  } 

  return package;
}

// This function is defiend later on but needs to be used in the next function
// that is why we make dummy definition here.
static std::vector<std::string> get_shared_libraries( const std::string& binary_path);
// -----------------------------------------------------------------------------------

int faro::build_package(Package &package) {
  // Check privilages
  if (geteuid() != 0) {
    faro::log::error("Building package requires root privileges. Run with sudo.");
    return 1;
  }
  
  std::string tmp_path = "/tmp/faro/";
  tmp_path.append(package.name);                 // TODO: sanitize the name first

  faro::log::step("Preparing build environment");
  
  // Create temp directory for build
  std::string build_path = tmp_path;
  build_path.append("/build/");
  if (!std::filesystem::exists(build_path)) {
    if (!std::filesystem::create_directories(build_path)){
      faro::log::error("Couldn't create " + build_path);
      return -1;
    }
  }

  faro::log::status("Extracting source archive...");
  
  // Decompress archive into newly created directory
  if (faro::archive::decompress_archive(package.archive_path, build_path) != 0) {
    faro::log::error("Faro was unable to extract your archive.");
    faro::log::status("Cleaning after broken archive decompression...");
    if (std::filesystem::remove_all(build_path) != 0) {
      faro::log::error("Couldn't delete temporary archive location");
    }
    return -1;
  }
  if (package.build.empty()) {
    faro::log::error("No build commands specified in recipe file.");
    return -1;
  }

  faro::log::success("Source archive extracted.");
  faro::log::step("Building package");
  
  // Execute every command in the build section in faro_recipe.json
  for (auto command : package.build) {
    std::string command_with_args;
    for (auto argument : command) {
      command_with_args += argument + " ";
    }
    faro::log::status(command_with_args);
    if (faro::posix::exec_command(command, build_path) != 0) {
      faro::log::error("Could not execute build command");
      return -1;
    }
    faro::log::success("Task completed");
  }

  faro::log::step("Installing package into temporary directory");
  
  // The build not only builds the package but installs it into the temp folder
  // it does so that it can generate the faro_metadata.faro and make install faster.
  std::string destdir_path = tmp_path;
  destdir_path.append("/destdir"); 
  
  // TODO: replace destdir with more reliable method of installation.
  std::string destdir_argument = "DESTDIR=";
  destdir_argument.append(destdir_path);
  if (!std::filesystem::exists(destdir_path)) {
    std::filesystem::remove_all(destdir_path); // Start with clean workspace
    if (!std::filesystem::create_directories(destdir_path)) {
      faro::log::error("An error occured while creating " + destdir_path);
      return -1;
    }
  }
  if (package.install.empty()) {
    faro::log::error("No install commands specified in faro recipe");
    return -1;
  }
  
  // This assumes that make install is the first one being executed
  // as for now if we stick to this convenction it's good enough. TODO: Change it
  package.install.at(0).push_back(destdir_argument);
  for (auto command : package.install) {
    std::string command_with_args;
    for (auto argument : command) {
      command_with_args += argument + " ";
    }
    faro::log::status(command_with_args);
    if (faro::posix::exec_command(command, build_path) != 0) {
      faro::log::error("Could not execute install command");
      return -1;
    }
    faro::log::success("Command executed");
  }

  faro::log::step("Generating metadata");

  // At the end build function should also generate faro_metadata.json file so it can be
  // later installed into the system. The following code is the implementation of that
  // assumption. 
  nlohmann::json metadata;
  std::string metadata_file_path = destdir_path;
  metadata_file_path.append("/faro_metadata.json");
  
  // First set all the variables that can be set without additional steps
  metadata["name"] = package.name;
  metadata["version"] = package.version;
  metadata["built"] = true;
  metadata["description"] = package.description;
  // for successfull build package dependencies must have already been check
  //  that means we don't need to do this again ;)
  metadata["dependencies"] = package.dependencies;

  
  // Now check for the used libs
  std::string binary_file = destdir_path;
  binary_file.append("/usr/bin/");
  binary_file.append(package.name); // we assume that binary name is the same as package name
  metadata["libs"] = get_shared_libraries(binary_file);

  // TODO: Check for every installed file, and add it to files section in
  // metadata.json this way we can safetly update or uninstall program

  std::string usr_destdir_path = destdir_path;
  usr_destdir_path.append("/usr");
  std::vector<std::string> files;
  for (const auto & entry : std::filesystem::recursive_directory_iterator(usr_destdir_path)) {
    if (entry.is_regular_file()) {
      // Use realative instead of absolute file path
      files.push_back( std::filesystem::relative(entry.path(), destdir_path).string() );
    }
  }
  metadata["files"] = files;
  
  std::string compiled_archive_path = FARO_CACHE;
  compiled_archive_path += package.name + "-" + package.version + ".tar.xz";

  // Add new archive path to the metadata and save the file in the archive.
  metadata["archive_path"] = compiled_archive_path;
  std::ofstream metadata_file(metadata_file_path);
  metadata_file << metadata.dump(4);
  metadata_file.close();

  faro::log::success("Generated faro_metadata.json file");
  faro::log::step("Creating package archive");
  
  // Now we compress the whole destdir folder into compiled tarball so it can be later
  // easily installed into the system.
  try {  
    std::filesystem::create_directories(FARO_CACHE);
  } catch (std::filesystem::filesystem_error& e) {
    faro::log::error("There was an error while creating folder: " + (std::string)e.what());
    return 1;
  }
  
  if (faro::archive::compress_folder(destdir_path, compiled_archive_path) != 0) {
    faro::log::error("Could not compress folder");
    return 1;
  }
  faro::log::success("Package archive created");

  faro::log::success("Package built successfully!");
  return 0;
};

// Installing packages in this case is just copied precompiled .tar.xz into filesystem
int faro::install_package(Package &package) {
  if (package.built == false) {
    faro::log::error("Not built package cannot be installed!");
    return 1;
  }

  // Check privilages
  if (geteuid() != 0) {
    faro::log::error("Installation requires root privileges. Run with sudo.");
    return 1;
  }

  faro::log::step("Installing " + package.name + " (" + package.version + ")");

  std::string temp_path = "/tmp/faro/";
  temp_path += package.name + "/" + "temp";
  if (!std::filesystem::exists(temp_path)) {
    std::filesystem::remove_all(temp_path); // Start with clean workspace
    if (!std::filesystem::create_directories(temp_path)) {
      faro::log::error("Faro couldn't create temp file for an archive");
      return -1;
    }
  }
  
  // So we don't have the function to extract only specific directory or file
  // from an archive so we decompress everything and copy only selected folder
  // TODO: Implement an option to decompress only selected folders.
  if (faro::archive::decompress_archive(package.archive_path, temp_path) != 0) {
    faro::log::error("Couldn't extract archive to a temp directory");
    return 1;
  }

  std::string usr_in_path = temp_path;
  usr_in_path += "/usr/";
  try {
    std::filesystem::copy(usr_in_path, FARO_PREFIX, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
  } catch (std::filesystem::filesystem_error& e){
    faro::log::error("Fatal error occured while copying the package:" + (std::string)e.what());
    return 1;
  }
  

  // Store metadata into safe place
  std::string installed_db_dir = "/var/lib/faro/installed/";
  std::filesystem::create_directories(installed_db_dir);
  
  std::string db_file_path = installed_db_dir + package.name + ".json";
  
  std::string metadata_content;
  if (faro::archive::read_element("faro_metadata.json", package.archive_path, metadata_content) == 0) {
      std::ofstream db_file(db_file_path);
      if (!db_file) {
        faro::log::error("Could not save faro_metadata.json file!");
        return 1;
      }
      db_file << metadata_content;
      db_file.close();
  } else {
      faro::log::warning("Package installed, but failed to save metadata database.");
      return 1;
  }

  faro::log::success("Package " + package.name + " successfully installed!");
  std::filesystem::remove_all(temp_path);
  return 0;

}

// This function has been stolen from archive_hlpr.cpp
static std::filesystem::path sanitize_path(const std::filesystem::path& base, const std::filesystem::path& entry) {
    if (entry.is_absolute() || entry.has_root_name())
        throw std::runtime_error("[FAILED] Absolute archive path rejected");

    auto normalized = entry.lexically_normal();

    for (const auto& component : normalized) {
        if (component == "..")
            throw std::runtime_error("[FAILED] Archive path traversal rejected");
    }

    return (base / normalized).lexically_normal();
}


int faro::remove_package(const std::string &package_name) {
  nlohmann::json metadata;
  std::filesystem::path metadata_path = "/var/lib/faro/installed/" + package_name + ".json";
  std::ifstream file(metadata_path);
  
  if (!file.is_open()) {
      faro::log::error(package_name + " is not installed");
      return 1;
  }
  try {
    file >> metadata;
  } catch (const nlohmann::json::exception& e) {
    faro::log::error(std::string("Invalid JSON: ") + e.what());
    return 1;
  }
  // Metadata checks ------------------------------------------------
  if (!metadata.contains("files") || !metadata["files"].is_array() ||
      !metadata.contains("name")  || !metadata.contains("version"))
  {
    faro::log::error("Invalid package metadata");
    return 1;
  }
  for (const auto& entry : metadata["files"]) {
    if (!entry.is_string()) {
        faro::log::error("Invalid file entry in package metadata");
        return 1;
    }
  }
  // -----------------------------------------------------------------
  
  faro::log::step("Checking package dependencies");
  
  std::unordered_set<std::string> shared;

  // The idea here is to iterate by every json in the directory to later
  // check if another package is using the same library as the package your about
  // to uninstall, if it's the case we skip this package and don't remove it to avoid problems.
  for (const auto& another_package : std::filesystem::directory_iterator("/var/lib/faro/installed")) {
    if (another_package.path() == metadata_path) {
      continue; 
    }

    // Grab the contents of the json we're currently analizing
    std::ifstream another_file(another_package.path());
    if (!another_file.is_open()) {
      faro::log::error("Couldn't open " + another_package.path().string());
      return 1;
    }
    
    nlohmann::json another_metadata;
    try {
      another_file >> another_metadata;
    } catch (const nlohmann::json::exception& e) {
      faro::log::error(std::string("Invalid JSON: ") + e.what());
      return 1;
    }
    
    // Quick validation -------------------------------------------------------------
    if (!another_metadata.contains("files") || !another_metadata["files"].is_array()) {
      faro::log::error("Invalid package metadata in file: " + another_package.path().string());
      faro::log::error("All metadata files in /var/lib/faro/installed/ must be correct!");
      return 1;
    }
    for (const auto& entry : another_metadata["files"]) {
      if (!entry.is_string()) {
          faro::log::error("Invalid file entry in package metadata");
          return 1;
      }
    }
    // -------------------------------------------------------------------------------
    
    // Now for every single file entry in metadata file, we go and analize if it's
    // already mentioned in the current file we're about to remove
    for (const auto& another_metadata_file : another_metadata["files"]) {
      for (const auto& current_metadata_file : metadata["files"]) {
        // If there is a shared library add it to shared vector and display warning
        if (another_metadata_file == current_metadata_file) {
          faro::log::warning("Keeping: " + current_metadata_file.get<std::string>() + " (owned by another package) ");
          shared.insert(current_metadata_file.get<std::string>());
        }
      }
    } 
  }
  faro::log::success("Shared file check completed");
  faro::log::step("Removing package files");

  // Now what's left is to remove every single file in metadata["files"] except
  // of those in shared vector.
  size_t package_count = 0;
  for (const auto& file : metadata["files"]) {
    if (!shared.contains(file.get<std::string>())) {
      // Sanitize path still doesn't support symlinks!!!
      // TODO: Add symlink protection
      std::filesystem::path target = sanitize_path("/",file.get<std::string>());
      
      if (!std::filesystem::remove(target)) {
        faro::log::error("Couldn't remove " + file.get<std::string>());
        return 2; // Error code 2: package removal was interrupted mid work
      }
      faro::log::success("Removed: /" + file.get<std::string>());
      package_count++;
    }
  }
  
  faro::log::success("Removed " + std::to_string(package_count) + " files");
  faro::log::step("Removing package metadata");
  
  // Remember to remove metadata json to avoid future errors
  if (!std::filesystem::remove(metadata_path)) {
    faro::log::error("Couldn't remove " + metadata_path.string() + " please do it manually to avoid errors");
    return 2; 
  }
  
  faro::log::success("Removed metadata");
  std::cout << "\n";
  faro::log::success("Package " + metadata["name"].get<std::string>() + "-" + metadata["version"].get<std::string>() + " successfully uninstalled");
  return 0;
}

int faro::list_packages() {
  nlohmann::json metadata;
  std::filesystem::path metadata_path = "/var/lib/faro/installed/";
  std::cout << "Installed packages: \n\n";
  std::cout << std::left
          << std::setw(15) << "NAME"
          << std::setw(12) << "VERSION"
          << "DESCRIPTION\n";

  size_t packages_count{0};
  for (auto& metadata_path : std::filesystem::directory_iterator(metadata_path)) {
    std::ifstream file(metadata_path.path());
  
    if (!file.is_open()) {
        faro::log::warning(metadata_path.path().string() + " can't be opened");
        continue;
    }
    try {
      file >> metadata;
    } catch (const nlohmann::json::exception& e) {
      faro::log::error(std::string("Invalid JSON: ") + e.what());
      return 1;
    }
    // Metadata checks ------------------------------------------------
    if (!metadata.contains("name")  || !metadata.contains("version") ||
        !metadata.contains("description")) 
    {
      faro::log::error("Invalid package metadata");
      return 1;
    }
    packages_count++;
    std::cout << std::left
          << std::setw(15) << metadata["name"].get<std::string>()
          << std::setw(12) << metadata["version"].get<std::string>()
          << metadata["description"].get<std::string>()
          << "\n";
  }
  
  std::cout << "\nA total of " + std::to_string(packages_count) << " packages installed \n";
  return 0;
}

// Function only used in this file
static std::vector<std::string> get_shared_libraries( const std::string& binary_path) {
    std::vector<std::string> libraries;

    std::string command = "ldd \"" + binary_path + "\" 2>/dev/null";

    FILE* pipe = popen(command.c_str(), "r");
  
    if (pipe == nullptr) {
        return libraries;
    }

    std::array<char, 512> buffer;

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        std::string line = buffer.data();

        // So we skip the linux built-in libraries
        if (line.find("linux-vdso") != std::string::npos ||
            line.find("ld-linux") != std::string::npos) {
            continue;
        }

        // We search for the start of library path
        std::size_t start = line.find("=> ");

        if (start == std::string::npos) {
            continue;
        }

        start += 3; // We point at the start of library path

        // find first space after the start space results in finding end of library path
        std::size_t end = line.find(" ", start); 

        std::string path = line.substr(start, end - start);

        // Fetch only file name from the path
        std::filesystem::path library_path(path);
        libraries.push_back(library_path.filename().string());
    }

    pclose(pipe);

    return libraries;
}
