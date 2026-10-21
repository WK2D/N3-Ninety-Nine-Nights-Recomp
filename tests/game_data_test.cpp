#include "game_data.h"
#include "import_paths.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <random>

namespace fs = std::filesystem;
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class Function> void Reject(Function fn) {
  bool rejected = false;
  try { fn(); } catch (const std::exception&) { rejected = true; }
  Require(rejected, "Malformed image was accepted");
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
  const auto base = fs::canonical(fs::temp_directory_path());
  const auto root = base / ("n3-import-test-" + std::to_string(std::random_device{}()) + "-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  if (!fs::create_directory(root)) return 1;
  int result = 0;
  try {
    const std::array<uint8_t, 3> abc{'a', 'b', 'c'};
    Require(n3::data::Sha256(abc) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 vector mismatch");
    Require(n3::data::Sha256({}) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "Empty SHA-256 mismatch");
    const std::vector<uint8_t> million(1000000, 'a');
    Require(n3::data::Sha256(million) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "Multi-block SHA-256 mismatch");
    n3::data::CheckImportPath(root / "missing" / "output");
    fs::create_directory(root / "real");
    std::error_code link_error;
    fs::create_directory_symlink(root / "real", root / "linked", link_error);
    if (!link_error) {
      Reject([&] { n3::data::CheckImportPath(root / "linked"); });
      Reject([&] { n3::data::CheckImportPath(root / "linked" / "new-output"); });
      fs::create_directory_symlink(root / "absent", root / "dangling");
      Reject([&] { n3::data::CheckImportPath(root / "dangling" / "new-output"); });
      std::cout << "Link and ancestor rejection passed\n";
    } else {
#ifndef _WIN32
      throw fs::filesystem_error("Cannot create link fixture", root, link_error);
#else
      std::cout << "Link fixtures skipped: Windows symlink privilege unavailable\n";
#endif
    }
    std::vector<uint8_t> image(40 * 2048);
    const std::string magic = "MICROSOFT*XBOX*MEDIA";
    std::copy(magic.begin(), magic.end(), image.begin() + 32 * 2048);
    std::copy(magic.begin(), magic.end(), image.begin() + 33 * 2048 - 20);
    auto put16 = [&](size_t at, uint16_t v) { image[at] = v & 255; image[at + 1] = v >> 8; };
    auto put32 = [&](size_t at, uint32_t v) { put16(at, v & 65535); put16(at + 2, v >> 16); };
    put32(32 * 2048 + 20, 33);
    put32(32 * 2048 + 24, 2048);
    put32(33 * 2048 + 4, 34);
    put32(33 * 2048 + 8, 5);
    image[33 * 2048 + 13] = 9;
    std::copy_n("hello.txt", 9, image.begin() + 33 * 2048 + 14);
    auto save = [&] {
      std::ofstream out(root / "test.iso", std::ios::binary | std::ios::trunc);
      out.write(reinterpret_cast<const char*>(image.data()), image.size());
    };
    save();
    auto files = n3::data::DiscImage(root / "test.iso").Files();
    Require(files.size() == 1 && files[0].size == 5 && files[0].offset == 34 * 2048, "Disc entry mismatch");
    Reject([&] { n3::data::DiscImage(root / "test.iso").VerifyExecutable(); });
    Reject([&] { n3::data::ImportDisc(root / "test.iso", root / "output"); });
    Require(!fs::exists(root / "output"), "Rejected image created output");
    image[33 * 2048 + 13] = 2;
    image[33 * 2048 + 14] = '.'; image[33 * 2048 + 15] = '.';
    save();
    Reject([&] { n3::data::DiscImage(root / "test.iso").Files(); });
    image[33 * 2048 + 14] = 'o'; image[33 * 2048 + 15] = 'k';
    put32(33 * 2048 + 4, 100);
    save();
    Reject([&] { n3::data::DiscImage(root / "test.iso").Files(); });
    put32(33 * 2048 + 4, 34);
    put16(33 * 2048, 8);
    put16(33 * 2048 + 32, 8);
    put32(33 * 2048 + 36, 34);
    image[33 * 2048 + 45] = 1; image[33 * 2048 + 46] = 'x';
    save();
    Reject([&] { n3::data::DiscImage(root / "test.iso").Files(); });
    image[33 * 2048 - 1] = 0;
    save();
    Reject([&] { n3::data::DiscImage(root / "test.iso"); });
    Reject([&] { n3::data::VerifyGameFolder(root); });
    if (argc == 2) {
      n3::data::DiscImage(argv[1]).VerifyExecutable();
      bool cancelled = false;
      try {
        n3::data::ImportDisc(argv[1], root / "cancelled", [](uint64_t, uint64_t) { return false; });
      } catch (const std::exception& error) {
        cancelled = std::string_view(error.what()) == "Disc import cancelled";
      }
      Require(cancelled && !fs::exists(root / "cancelled"), "Cancelled import was not cleaned up");
      fs::create_directory(root / "existing");
      std::ofstream(root / "existing" / "keep.txt") << "keep";
      Reject([&] { n3::data::ImportDisc(argv[1], root / "existing"); });
      Require(fs::file_size(root / "existing" / "keep.txt") == 4, "Existing destination changed");
      std::cout << "Matching-disc cancellation and destination protection passed\n";
    }
    std::cout << "SHA-256, disc traversal, wrong-game rejection, unsafe names, extents, cycles, and volume checks passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; result = 1; }
  // Remove only the unique temporary fixture directory created above.
  if (fs::weakly_canonical(root).parent_path() == fs::weakly_canonical(base) &&
      root.filename().string().starts_with("n3-import-test-")) fs::remove_all(root);
  return result;
}
