#include "profile_config.h"
#include <rex/cvar.h>
#include <toml++/toml.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>

REXCVAR_DEFINE_STRING(config_probe_path, "", "Test", "Persistence probe");
REXCVAR_DEFINE_UINT32(config_probe_fps, 30, "Test", "Persistence probe");
REXCVAR_DEFINE_BOOL(config_probe_enabled, false, "Test", "Persistence probe");
void Require(bool ok, const char* error) { if (!ok) throw std::runtime_error(error); }
std::string Read(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), {}};
}
int main() {
  const auto folder = std::filesystem::temp_directory_path() /
      ("n3-config-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    std::filesystem::create_directory(folder);
    const auto file = folder / "settings.toml";
    const std::string path = "C:\\Games\\N3\\a \"quoted\" folder\\next\nline\tend";
    rex::cvar::SetFlagByName("config_probe_path", path);
    rex::cvar::SetFlagByName("config_probe_fps", "60");
    rex::cvar::SetFlagByName("config_probe_enabled", "true");
    SaveProfileConfig(file);
    auto saved = toml::parse_file(file.string());
    Require(saved["config_probe_path"].value_or(std::string{}) == path, "String escaping changed a saved value");
    for (const auto* name : {"config_probe_path", "config_probe_fps", "config_probe_enabled"}) rex::cvar::ResetToDefault(name);
    rex::cvar::LoadConfig(file);
    Require(rex::cvar::GetFlagByName("config_probe_path") == path && rex::cvar::GetFlagByName("config_probe_fps") == "60" && rex::cvar::GetFlagByName("config_probe_enabled") == "true", "Saved settings did not reload");
    Require(!RepairLegacyProfileConfig(file), "Valid configuration was rewritten");
    {
      std::ofstream out(file);
      out << "# Auto-generated cvar configuration\nconfig_probe_path = \"C:\\Games\\N3\\folder\"\nconfig_probe_fps = 40\n";
    }
    Require(RepairLegacyProfileConfig(file), "Legacy Windows-path configuration was not repaired");
    saved = toml::parse_file(file.string());
    Require(saved["config_probe_path"].value_or(std::string{}) == "C:\\Games\\N3\\folder" && saved["config_probe_fps"].value_or(0) == 40, "Legacy settings changed during repair");
    const std::string malformed = "# Auto-generated cvar configuration\ninvalid = [\n";
    { std::ofstream out(file, std::ios::binary); out << malformed; }
    Require(!RepairLegacyProfileConfig(file) && Read(file) == malformed, "Unknown malformed config was altered");
    // Replacing a nonempty config with defaults must remove the old settings.
    for (const auto* name : {"config_probe_path", "config_probe_fps", "config_probe_enabled"}) rex::cvar::ResetToDefault(name);
    SaveProfileConfig(file);
    Require(!toml::parse_file(file.string()).contains("config_probe_fps"), "Reset-to-default left an old value on disk");
    std::filesystem::remove_all(folder);
    std::cout << "Configuration escaping, reload, legacy repair and default reset passed\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    std::filesystem::remove_all(folder);
    return 1;
  }
}
