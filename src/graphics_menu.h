#pragma once
#include <filesystem>
namespace rex { class Runtime; }
bool InstallGraphicsMenu(rex::Runtime& runtime, const std::filesystem::path& game_root,
                         const std::filesystem::path& profile);
