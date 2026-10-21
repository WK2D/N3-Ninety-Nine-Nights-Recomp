#include "import_paths.h"
#include <stdexcept>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace fs = std::filesystem;
namespace n3::data {
void CheckImportPath(const fs::path& path) {
  for (auto current = fs::absolute(path); !current.empty();) {
#ifdef _WIN32
    const auto attributes = GetFileAttributesW(current.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
      const auto error = GetLastError();
      if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)
        throw fs::filesystem_error("Cannot inspect import destination", current,
                                   std::error_code(error, std::system_category()));
    } else if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
      throw std::runtime_error("Import destination must not contain symbolic links or junctions");
    }
#else
    std::error_code error;
    const auto status = fs::symlink_status(current, error);
    if (error && error != std::errc::no_such_file_or_directory)
      throw fs::filesystem_error("Cannot inspect import destination", current, error);
    if (fs::is_symlink(status))
      throw std::runtime_error("Import destination must not contain symbolic links");
#endif
    const auto parent = current.parent_path();
    if (current == parent) break;
    current = parent;
  }
}
}
