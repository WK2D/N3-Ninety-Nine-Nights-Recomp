#pragma once
#include <filesystem>

namespace n3::data {
// Reject links in the destination and its ancestors, including Windows reparse
// points. Missing components are allowed because import creates a fresh tree.
void CheckImportPath(const std::filesystem::path& path);
}
