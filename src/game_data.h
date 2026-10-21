#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace n3::data {
inline constexpr char kExecutableHash[] = "0fe8973b6ff45d3f8bd03a51a8c0d8cdf278b3d43e793497777e075f4e94a804";
inline constexpr uint64_t kExecutableSize = 6270976;

struct DiscFile {
  std::filesystem::path relative_path;
  uint64_t offset;
  uint32_t size;
};

class DiscImage {
 public:
  explicit DiscImage(std::filesystem::path source);
  std::vector<DiscFile> Files() const;
  std::vector<uint8_t> Read(uint64_t offset, size_t size) const;
  uint64_t partition_offset() const { return partition_; }
  void VerifyExecutable() const;
 private:
  std::filesystem::path source_;
  uint64_t size_ = 0;
  uint64_t partition_ = 0;
  uint32_t root_sector_ = 0;
  uint32_t root_size_ = 0;
};

std::string Sha256(std::span<const uint8_t> bytes);
void VerifyGameFolder(const std::filesystem::path& folder);
// Output must not exist. Progress returns false to cancel. Failed imports remove
// only the newly created output tree, after checking that it contains no links.
void ImportDisc(const std::filesystem::path& source, const std::filesystem::path& output,
                const std::function<bool(uint64_t, uint64_t)>& progress = {});
}  // namespace n3::data
