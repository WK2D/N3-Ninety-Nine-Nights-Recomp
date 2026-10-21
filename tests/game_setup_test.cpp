#include "game_setup.h"
#include <rex/system/flags.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class Function> void Reject(Function fn) {
  bool rejected = false;
  try { fn(); } catch (const std::exception&) { rejected = true; }
  Require(rejected, "Invalid setup state was accepted");
}
int wmain(int argc, wchar_t** argv) {
  const auto base = fs::temp_directory_path();
  const auto root = base / ("n3-setup-test-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
  if (!fs::create_directory(root)) return 1;
  int result = 0;
  try {
    REXCVAR_SET(headless, true);
    rex::PathConfig paths{};
    paths.user_data_root = root;
    Reject([&] { ResolveGameData(paths); });
    std::ofstream(root / "game-path.txt") << "missing-game-folder";
    Reject([&] { ResolveGameData(paths); });
    paths.game_data_root = root;
    Reject([&] { ResolveGameData(paths); });
    if (argc == 2) {
      const auto game = fs::absolute(argv[1]);
      paths.game_data_root = game;
      Require(ResolveGameData(paths)->game_data_root == game, "Explicit matching folder failed");
      const auto text = game.generic_u8string();
      std::ofstream out(root / "game-path.txt", std::ios::binary | std::ios::trunc);
      out.write(reinterpret_cast<const char*>(text.data()), text.size());
      out.close();
      paths.game_data_root.clear();
      Require(ResolveGameData(paths)->game_data_root == game, "Remembered matching folder failed");
      paths.game_data_root = root;
      Reject([&] { ResolveGameData(paths); });
      std::cout << "Explicit and remembered game-folder verification passed\n";
    }
    std::cout << "Missing-data, stale-link, and invalid-override checks passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; result = 1; }
  if (fs::weakly_canonical(root).parent_path() == fs::weakly_canonical(base) &&
      root.filename().string().starts_with("n3-setup-test-")) fs::remove_all(root);
  return result;
}
