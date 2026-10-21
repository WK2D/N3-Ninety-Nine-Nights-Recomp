#include "game_data.h"
#include "import_paths.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <set>
#include <stdexcept>

namespace fs = std::filesystem;
namespace n3::data {
namespace {
constexpr uint64_t kSector = 2048;
constexpr char kMagic[] = "MICROSOFT*XBOX*MEDIA";
uint16_t U16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
uint32_t U32(const uint8_t* p) { return uint32_t(U16(p)) | uint32_t(U16(p + 2)) << 16; }
std::string Lower(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
void CheckName(const std::string& name) {
  if (name.empty() || name == "." || name == ".." || name.back() == '.' || name.back() == ' ') {
    throw std::runtime_error("Invalid disc entry name");
  }
  for (const unsigned char c : name) {
    if (c < 32 || c > 126 || std::string_view("<>:\"/\\|?*").find(c) != std::string_view::npos) {
      throw std::runtime_error("Unsafe disc entry name");
    }
  }
  const auto base = Lower(name.substr(0, name.find('.')));
  if (base == "con" || base == "prn" || base == "aux" || base == "nul" ||
      (base.size() == 4 && (base.starts_with("com") || base.starts_with("lpt")) &&
       base[3] >= '1' && base[3] <= '9')) throw std::runtime_error("Reserved disc entry name");
}
std::vector<uint8_t> ReadFile(const fs::path& path) {
  if (!fs::is_regular_file(path) || fs::file_size(path) != kExecutableSize) {
    throw std::runtime_error("A matching default.xex is required (USA/Europe disc, version 0.0.0.3)");
  }
  std::ifstream in(path, std::ios::binary);
  std::vector<uint8_t> bytes(kExecutableSize);
  if (!in.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) throw std::runtime_error("Cannot read default.xex");
  return bytes;
}
}  // namespace

DiscImage::DiscImage(fs::path source) : source_(std::move(source)), size_(fs::file_size(source_)) {
  bool found = false;
  for (const uint64_t offset : {0ull, 0xfb20ull, 0x20600ull, 0x2080000ull, 0xfd90000ull}) {
    if (offset + 33 * kSector > size_) continue;
    const auto volume = Read(offset + 32 * kSector, kSector);
    if (!std::equal(volume.begin(), volume.begin() + 20, kMagic)) continue;
    if (!std::equal(volume.begin() + 2028, volume.end(), kMagic)) throw std::runtime_error("Damaged XDVDFS descriptor");
    partition_ = offset;
    root_sector_ = U32(volume.data() + 20);
    root_size_ = U32(volume.data() + 24);
    found = true;
    break;
  }
  if (!found) throw std::runtime_error("No Xbox game partition found in this image");
}

std::vector<uint8_t> DiscImage::Read(uint64_t offset, size_t length) const {
  if (offset > size_ || length > size_ - offset) throw std::runtime_error("Disc extent outside image");
  std::ifstream in(source_, std::ios::binary);
  in.seekg(static_cast<std::streamoff>(offset));
  std::vector<uint8_t> bytes(length);
  if (length && !in.read(reinterpret_cast<char*>(bytes.data()), length)) throw std::runtime_error("Short disc read");
  return bytes;
}

std::vector<DiscFile> DiscImage::Files() const {
  struct Directory { uint32_t sector, size; fs::path path; };
  std::vector<Directory> pending{{root_sector_, root_size_, {}}};
  std::set<uint32_t> directories;
  std::set<std::string> names;
  std::vector<DiscFile> files;
  while (!pending.empty()) {
    const auto directory = pending.back();
    pending.pop_back();
    if (!directory.size) continue;
    if (directory.size < 14 || directory.size > 32 * 1024 * 1024) throw std::runtime_error("Invalid directory size");
    if (!directories.insert(directory.sector).second) throw std::runtime_error("Directory cycle or alias");
    const auto table = Read(partition_ + directory.sector * kSector, directory.size);
    std::vector<uint32_t> nodes{0};
    std::set<uint32_t> visited;
    while (!nodes.empty()) {
      const auto pos = nodes.back(); nodes.pop_back();
      if (!visited.insert(pos).second) throw std::runtime_error("Directory node cycle");
      if (pos > table.size() || table.size() - pos < 14) throw std::runtime_error("Invalid directory node");
      const auto* p = table.data() + pos;
      if (!p[13] || size_t(pos) + 14 + p[13] > table.size()) throw std::runtime_error("Invalid entry length");
      const std::string name(reinterpret_cast<const char*>(p + 14), p[13]);
      CheckName(name);
      const auto relative = directory.path / name;
      if (!names.insert(Lower(relative.generic_string())).second) throw std::runtime_error("Duplicate disc entry");
      if (names.size() > 100000) throw std::runtime_error("Disc contains too many entries");
      const uint64_t offset = partition_ + U32(p + 4) * kSector;
      const uint32_t length = U32(p + 8);
      if (offset > size_ || length > size_ - offset) throw std::runtime_error("Disc entry outside image");
      if (p[12] & 0x10) pending.push_back({U32(p + 4), length, relative});
      else files.push_back({relative, offset, length});
      if (U16(p)) nodes.push_back(uint32_t(U16(p)) * 4);
      if (U16(p + 2)) nodes.push_back(uint32_t(U16(p + 2)) * 4);
    }
  }
  std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.relative_path < b.relative_path; });
  return files;
}

void DiscImage::VerifyExecutable() const {
  for (const auto& file : Files()) {
    if (Lower(file.relative_path.generic_string()) != "default.xex") continue;
    if (file.size != kExecutableSize || Sha256(Read(file.offset, file.size)) != kExecutableHash) {
      throw std::runtime_error("Unsupported disc revision. This release requires the USA/Europe executable version 0.0.0.3.");
    }
    return;
  }
  throw std::runtime_error("The selected image has no default.xex");
}

void VerifyGameFolder(const fs::path& folder) {
  if (Sha256(ReadFile(folder / "default.xex")) != kExecutableHash) {
    throw std::runtime_error("Unsupported executable revision. This release requires the USA/Europe version 0.0.0.3.");
  }
  if (!fs::is_directory(folder / "data")) throw std::runtime_error("The game data folder is missing beside default.xex");
}

void ImportDisc(const fs::path& source, const fs::path& output,
                const std::function<bool(uint64_t, uint64_t)>& progress) {
  DiscImage disc(source);
  disc.VerifyExecutable();
  const auto files = disc.Files();
  uint64_t total = 0, done = 0;
  for (const auto& file : files) total += file.size;
  const auto root = fs::absolute(output).lexically_normal();
  CheckImportPath(root);
  if (fs::exists(root)) throw std::runtime_error("Import destination already exists");
  fs::create_directories(root.parent_path());
  if (!fs::create_directory(root)) throw std::runtime_error("Cannot create import destination");
  try {
    if (fs::space(root).available < total) throw std::runtime_error("Not enough free space to import the disc (about 5.8 GB required)");
    std::ifstream in(source, std::ios::binary);
    std::vector<char> buffer(1024 * 1024);
    for (const auto& file : files) {
      if (progress && !progress(done, total)) throw std::runtime_error("Disc import cancelled");
      const auto target = root / file.relative_path;
      fs::create_directories(target.parent_path());
      CheckImportPath(target);
      if (fs::exists(target)) throw std::runtime_error("Import file already exists");
      std::ofstream out(target, std::ios::binary);
      in.seekg(static_cast<std::streamoff>(file.offset));
      uint64_t remaining = file.size;
      while (remaining) {
        const auto count = static_cast<std::streamsize>(std::min<uint64_t>(remaining, buffer.size()));
        if (!in.read(buffer.data(), count) || !out.write(buffer.data(), count)) throw std::runtime_error("Disc copy failed");
        remaining -= count; done += count;
        if (progress && !progress(done, total)) throw std::runtime_error("Disc import cancelled");
      }
      out.close();
      if (!out) throw std::runtime_error("Cannot finish writing imported file");
    }
    VerifyGameFolder(root);
  } catch (...) {
    // This tree was created above after refusing every pre-existing destination.
    // Recheck its resolved identity and reparse status before recursive cleanup.
    if (fs::exists(root) && fs::weakly_canonical(root) == root) {
      CheckImportPath(root);
      fs::remove_all(root);
    }
    throw;
  }
}
}  // namespace n3::data
