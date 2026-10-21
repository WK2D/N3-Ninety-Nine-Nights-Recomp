#include "profile_config.h"
#include <rex/cvar.h>
#include <toml++/toml.hpp>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace {
constexpr auto header = "# Auto-generated cvar configuration";
void Write(const std::filesystem::path& path, const toml::table& values) {
  auto temporary = path; temporary += ".tmp";
  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    out << header << '\n' << values << '\n';
    out.close();
    if (!out) throw std::runtime_error("Cannot save profile configuration");
  }
  // Replace an existing profile only after the complete new file is closed.
#ifdef _WIN32
  if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    throw std::runtime_error("Cannot replace profile configuration");
#else
  std::filesystem::rename(temporary, path);
#endif
}
}

void SaveProfileConfig(const std::filesystem::path& path) {
  toml::table values;
  for (const auto& name : rex::cvar::ListModifiedFlags()) {
    const auto* info = rex::cvar::GetFlagInfo(name);
    if (!info || info->is_debug_only || info->type == rex::cvar::FlagType::Command) continue;
    const auto value = rex::cvar::GetFlagByName(name);
    if (info->type == rex::cvar::FlagType::String) values.insert(name, value);
    else {
      const auto scalar = toml::parse("value = " + value);
      values.insert(name, *scalar.get("value"));
    }
  }
  Write(path, values);
}

bool RepairLegacyProfileConfig(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) return false;
  try { (void)toml::parse_file(path.string()); return false; }
  catch (const toml::parse_error&) {}
  // The pinned SDK serializer surrounded raw strings with quotes. Only repair
  // its identifiable flat format; leave other malformed configurations alone.
  std::ifstream in(path, std::ios::binary);
  std::string line;
  if (!std::getline(in, line)) return false;
  if (!line.empty() && line.back() == '\r') line.pop_back();
  if (line != header) return false;
  toml::table values;
  try {
    while (std::getline(in, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.empty()) continue;
      const auto equal = line.find(" = ");
      if (equal == std::string::npos) return false;
      const auto name = line.substr(0, equal), value = line.substr(equal + 3);
      if (name.empty() || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != std::string::npos || values.contains(name)) return false;
      if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        values.insert(name, value.substr(1, value.size() - 2));
      else {
        const auto scalar = toml::parse("value = " + value);
        const auto* node = scalar.get("value");
        if (!node || (!node->is_number() && !node->is_boolean())) return false;
        values.insert(name, *node);
      }
    }
    in.close();
    Write(path, values);
    return true;
  } catch (const toml::parse_error&) { return false; }
}
