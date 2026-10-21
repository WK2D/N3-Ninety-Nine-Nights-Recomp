#include "game_setup.h"
#include "game_data.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <rex/cvar.h>
#include <rex/system/flags.h>

#include <fstream>

namespace fs = std::filesystem;
namespace {
bool Matches(const fs::path& folder) {
  try { n3::data::VerifyGameFolder(folder); return true; } catch (const std::exception&) { return false; }
}
fs::path ReadLink(const fs::path& filename) {
  std::ifstream in(filename, std::ios::binary);
  std::string line;
  std::getline(in, line);
  if (line.size() > 32768 || line.empty()) return {};
  return fs::path(std::u8string(reinterpret_cast<const char8_t*>(line.data()), line.size()));
}
void SaveLink(const fs::path& filename, const fs::path& folder) {
  fs::create_directories(filename.parent_path());
  const auto temp = fs::path(filename.wstring() + L".tmp");
  const auto utf8 = fs::absolute(folder).generic_u8string();
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out.write(reinterpret_cast<const char*>(utf8.data()), utf8.size())) throw std::runtime_error("Cannot remember the game folder");
    out.close();
    if (!out) throw std::runtime_error("Cannot finish saving the game folder link");
  }
  if (!MoveFileExW(temp.c_str(), filename.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    throw std::runtime_error("Cannot save the game folder link");
  }
}
fs::path PickSource() {
  wchar_t filename[32768]{};
  OPENFILENAMEW picker{};
  picker.lStructSize = sizeof(picker);
  picker.hwndOwner = GetActiveWindow();
  picker.lpstrTitle = L"N3 Recompiled - Select your own disc image or extracted default.xex";
  picker.lpstrFilter = L"Xbox 360 disc image or executable\0*.iso;*.xex\0Disc image (*.iso)\0*.iso\0Executable (default.xex)\0*.xex\0\0";
  picker.lpstrFile = filename;
  picker.nMaxFile = static_cast<DWORD>(std::size(filename));
  picker.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
  if (GetOpenFileNameW(&picker)) return fs::path(filename);
  if (CommDlgExtendedError()) throw std::runtime_error("Cannot open the game selection dialog");
  return {};
}
class ImportProgress {
 public:
  ImportProgress() {
    initialized_ = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    if (SUCCEEDED(CoCreateInstance(CLSID_ProgressDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&dialog_)))) {
      dialog_->SetTitle(L"N3 Recompiled - Importing disc");
      dialog_->SetLine(1, L"Copying game data from your disc image...", FALSE, nullptr);
      dialog_->SetLine(2, L"Your original image will remain unchanged.", FALSE, nullptr);
      dialog_->SetCancelMsg(L"Cancelling import...", nullptr);
      dialog_->StartProgressDialog(GetActiveWindow(), nullptr,
                                   PROGDLG_MODAL | PROGDLG_AUTOTIME | PROGDLG_NOMINIMIZE, nullptr);
    }
  }
  ~ImportProgress() {
    if (dialog_) { dialog_->StopProgressDialog(); dialog_->Release(); }
    if (initialized_) CoUninitialize();
  }
  bool Update(uint64_t done, uint64_t total) {
    if (!dialog_) return true;
    dialog_->SetProgress64(done, total);
    return !dialog_->HasUserCancelled();
  }
 private:
  bool initialized_ = false;
  IProgressDialog* dialog_ = nullptr;
};
fs::path NewImportFolder(const fs::path& root) {
  GUID guid{};
  if (FAILED(CoCreateGuid(&guid))) throw std::runtime_error("Cannot create an import identifier");
  wchar_t name[64]{};
  StringFromGUID2(guid, name, static_cast<int>(std::size(name)));
  return root / (std::wstring(L"game-data-") + name);
}
}  // namespace

void ConfigureGameDataPaths(rex::PathConfig& paths) {
  if (rex::cvar::GetFlagByName("user_data_root").empty()) {
    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &local)))
      throw std::runtime_error("Cannot locate the local application data folder");
    paths.user_data_root = fs::path(local) / "N3Recompiled";
    CoTaskMemFree(local);
    if (rex::cvar::GetFlagByName("cache_root").empty()) paths.cache_root = paths.user_data_root / "cache";
  }
  fs::create_directories(paths.user_data_root);
}

std::optional<rex::PathConfig> ResolveGameData(const rex::PathConfig& defaults) {
  auto paths = defaults;
  const auto link = paths.user_data_root / "game-path.txt";
  if (!paths.game_data_root.empty()) {
    n3::data::VerifyGameFolder(paths.game_data_root);
    return paths;
  }
  const auto remembered = ReadLink(link);
  if (!remembered.empty() && Matches(remembered)) {
    paths.game_data_root = remembered;
    return paths;
  }
  if (REXCVAR_GET(headless)) throw std::runtime_error("Matching game data is required; run once without --headless to choose your disc or extracted default.xex");
  for (;;) {
    try {
      const auto source = PickSource();
      if (source.empty()) return std::nullopt;
      fs::path folder;
      if (_wcsicmp(source.extension().c_str(), L".iso") == 0) {
        n3::data::DiscImage(source).VerifyExecutable();
        if (MessageBoxW(GetActiveWindow(),
            L"Import game data from this disc image? About 5.8 GB of free space is required.\n\nThe original image will remain unchanged. You only need to import once.",
            L"N3 Recompiled - Import your disc", MB_OKCANCEL | MB_ICONINFORMATION) != IDOK) continue;
        folder = NewImportFolder(paths.user_data_root);
        ImportProgress progress;
        n3::data::ImportDisc(source, folder, [&](uint64_t done, uint64_t total) { return progress.Update(done, total); });
      } else {
        folder = source.parent_path();
        n3::data::VerifyGameFolder(folder);
      }
      SaveLink(link, folder);
      paths.game_data_root = folder;
      return paths;
    } catch (const std::exception& error) {
      MessageBoxA(GetActiveWindow(), error.what(), "N3 Recompiled - Game setup", MB_OK | MB_ICONERROR);
    }
  }
}
