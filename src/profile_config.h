#pragma once
#include <filesystem>

void SaveProfileConfig(const std::filesystem::path& path);
bool RepairLegacyProfileConfig(const std::filesystem::path& path);
