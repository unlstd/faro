#include "archive_hlpr.hpp"
#include <archive.h>
#include <archive_entry.h>
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <fstream>
#include "logger.hpp"

// ---------------------------------------------------
// This file provides simple interface with libarchive
// ---------------------------------------------------

// -------------------------------------------------------------------
// The following functions are static helpers for rest of the functions
// --------------------------------------------------------------------
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

static int copy_data(struct archive *ar, struct archive *aw) {
    const void *buffer;
    size_t size;
    la_int64_t offset;

    for (;;) {
        int r = archive_read_data_block(ar, &buffer, &size, &offset);

        if (r == ARCHIVE_EOF)
            return ARCHIVE_OK;

        if (r != ARCHIVE_OK)
            return r;

        r = archive_write_data_block(aw, buffer, size, offset);

        if (r != ARCHIVE_OK)
            return r;
    }
}



// --------------
// End of helpers
// --------------

int faro::archive::list_elements(std::string& path) {
  // Libarchive uses two structs
  // - Archive
  // - Entry
  // where archive is currently used archive and entry is file we're working on
  struct archive *a;
  struct archive_entry *entry;
  int r;
  int result{0}; // We don't want to return 1 every time we encounter an error
  //  because it would cause us a lot of memory leaks
  //  instead we set up one result int and return it at the end.
  
  // Setup archive for further operations,
  //   enable all known formats for example tar or zip,
  //   and enable all formats for example .xz or .gz
  a = archive_read_new();
  archive_read_support_filter_all(a);
  archive_read_support_format_all(a);
  
  r = archive_read_open_filename(a, path.c_str(), 10240);
  if (r != ARCHIVE_OK)
    result = 1;
  
  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    std::cout << archive_entry_pathname(entry) << "\n";
    archive_read_data_skip(a);
  }

  // Remember to free alocated memory
  r = archive_read_free(a);
  if (r != ARCHIVE_OK) result = 1;

  return result;
}

// All futher functions use the same concept as explained before.

int faro::archive::find_element(std::string& path, const std::string element) {
  struct archive *a;
  struct archive_entry *entry;
  int r;
  int result{1};

  a = archive_read_new();
  archive_read_support_filter_all(a);
  archive_read_support_format_all(a);
  
  r = archive_read_open_filename(a, path.c_str(), 10240);
  if (r != ARCHIVE_OK)
    result = 1;
  
  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    std::string entry_path = archive_entry_pathname(entry);
    while (entry_path.starts_with("./")) {
        entry_path.erase(0, 2);
    }

    if (entry_path == element) {
      result = 0;
      break;
    } else {
      archive_read_data_skip(a);
    }
  }
  
  r = archive_read_free(a);
  if (r != ARCHIVE_OK) result = 1;
  return result;
}

int faro::archive::decompress_archive(std::string &archive, const std::string &path) {
  struct archive *a;
  struct archive *ext;
  struct archive_entry *entry;
  int flags;
  int result{0};
  int r;

  // Select which metadata will be copied over to new destination
  flags = ARCHIVE_EXTRACT_TIME;
  flags |= ARCHIVE_EXTRACT_PERM;
  flags |= ARCHIVE_EXTRACT_ACL;
  flags |= ARCHIVE_EXTRACT_FFLAGS;

  a = archive_read_new();
  archive_read_support_format_all(a);
  archive_read_support_filter_all(a);

  ext = archive_write_disk_new();
  archive_write_disk_set_options(ext, flags);
  archive_write_disk_set_standard_lookup(ext);  

  
  if ((r = archive_read_open_filename(a, archive.c_str(), 10240)))
    result = 1;
  for (;;) {
    r = archive_read_next_header(a, &entry);

    if (r == ARCHIVE_EOF)
      break;
    
    // Set path for file extraction
    // Use sanitize_path() function to get rid of potential backdoor.
    try {
      auto output_path = sanitize_path(path, archive_entry_pathname(entry));
      archive_entry_set_pathname(entry, output_path.c_str());
    } catch (const std::exception& e) {
      faro::log::error((std::string)e.what());
      return 1;
    }
    
    if (r < ARCHIVE_OK)
      faro::log::error(archive_error_string(a));
    if (r < ARCHIVE_WARN)
      result = 1;
    
    r = archive_write_header(ext, entry);
    if (r < ARCHIVE_OK)
      faro::log::error(archive_error_string(ext));
    else if (archive_entry_size(entry) > 0) {
      r = copy_data(a, ext);
      if (r < ARCHIVE_OK)
        faro::log::error(archive_error_string(ext));
      if (r < ARCHIVE_WARN)
        result = 1;
    }
    r = archive_write_finish_entry(ext);
    if (r < ARCHIVE_OK)
       faro::log::error(archive_error_string(ext));
    if (r < ARCHIVE_WARN)
      result = 1;
  }
  archive_read_close(a);
  archive_read_free(a);
  archive_write_close(ext);
  archive_write_free(ext);
  return result;
}

int faro::archive::compress_folder(const std::string &path, const std::string &archive) {
  // Compressing is the reverse of decompressing:
  // - a is the archive we WRITE to
  // - disk is a helper that READS file info (size, permissions, type...) from disk
  // - entry describes the file we're currently adding
  struct archive *a;
  struct archive *disk;
  struct archive_entry *entry;
  int r;
  int result{0};

  std::filesystem::path base(path);
  if (!std::filesystem::is_directory(base)) {
    std::cerr << "[FAILED] Not a directory: " << path << "\n";
    return 1;
  }

  // Setup output archive: tar format, compressed with xz (.tar.xz)
  a = archive_write_new();
  archive_write_add_filter_xz(a);
  archive_write_set_format_pax_restricted(a);

  disk = archive_read_disk_new();
  archive_read_disk_set_standard_lookup(disk);

  r = archive_write_open_filename(a, archive.c_str());
  if (r != ARCHIVE_OK) {
    faro::log::error(archive_error_string(a));
    archive_write_free(a);
    archive_read_free(disk);
    return 1;
  }

  entry = archive_entry_new();

  // Go through every file and subfolder inside of the folder
  for (const auto& item : std::filesystem::recursive_directory_iterator(base)) {
    archive_entry_clear(entry); // that way we don't need to create new entry every iteration
  
    archive_entry_copy_sourcepath(entry, item.path().c_str());
    archive_entry_set_pathname(entry, std::filesystem::relative(item.path(), base).c_str());

    // We copy metadata from file
    r = archive_read_disk_entry_from_file(disk, entry, -1, NULL);
    if (r < ARCHIVE_OK)
      faro::log::error(archive_error_string(a));
    if (r < ARCHIVE_WARN) {
      result = 1;
      break;
    }

    r = archive_write_header(a, entry);
    if (r < ARCHIVE_OK)
      faro::log::error(archive_error_string(a));
    if (r < ARCHIVE_WARN) {
      result = 1;
      break;
    }

    // Only regular files have data, folders and symlinks are just the header
    if (archive_entry_filetype(entry) == AE_IFREG) {
      std::ifstream file(item.path(), std::ios::binary);
      char buffer[8192];

      // The last chunk is usually smaller than the buffer,
      // so we also check gcount() to not lose it
      while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        if (archive_write_data(a, buffer, file.gcount()) < 0) {
          faro::log::error(archive_error_string(a));
          result = 1;
          break;
        }
      }
      if (result == 1)
        break;
    }
  }

  // Free everything. Closing the archive also flushes the gzip data,
  // so it's worth checking if it succeeded.
  archive_entry_free(entry);
  archive_read_free(disk);
  r = archive_write_close(a);
  if (r < ARCHIVE_OK) {
    faro::log::error(archive_error_string(a));
    result = 1;
  }
  archive_write_free(a);

  return result;
}

int faro::archive::read_element(const std::string& path, const std::string& archive, std::string& content) {
  struct archive *a;
  struct archive_entry *entry;
  int r;
  // Assume failure, we change it to 0 only after element was fully read
  int result = 1;

  a = archive_read_new();
  archive_read_support_filter_all(a);
  archive_read_support_format_all(a);

  r = archive_read_open_filename(a, archive.c_str(), 10240);
  if (r != ARCHIVE_OK) {
    archive_read_free(a);
    return 1;
  }

  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    std::string entry_path = archive_entry_pathname(entry);

    while (entry_path.starts_with("./")) {
        entry_path.erase(0, 2);
    }
    if (entry_path == path) {
      content.clear();

      // Read data in chunks and append it to the string
      char buffer[8192];
      la_ssize_t n;
      while ((n = archive_read_data(a, buffer, sizeof(buffer))) > 0)
        content.append(buffer, n);

      // n == 0 means end of the file, n < 0 means error
      if (n < 0) {
        faro::log::error(archive_error_string(a));
        content.clear();
      } else {
        result = 0;
      } 
      break;
    } else {
      archive_read_data_skip(a);
    }
  }

  archive_read_free(a);
  return result;
}
