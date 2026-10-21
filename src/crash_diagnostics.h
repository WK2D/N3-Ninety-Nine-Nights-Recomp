#pragma once

#include <filesystem>

// Install once before guest threads start. Reports stay in the selected local
// profile; diagnostic setup failures must not prevent the game from starting.
bool InstallCrashDiagnostics(const std::filesystem::path& log_directory) noexcept;
