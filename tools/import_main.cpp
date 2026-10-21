#include "game_data.h"
#include <cstdio>
#include <iostream>

int wmain(int argc, wchar_t** argv) {
  try {
    if (argc == 3 && std::wstring_view(argv[1]) == L"inspect") {
      const std::filesystem::path source(argv[2]);
      if (std::filesystem::is_directory(source)) n3::data::VerifyGameFolder(source);
      else if (source.extension() == L".xex") n3::data::VerifyGameFolder(source.parent_path());
      else {
        n3::data::DiscImage disc(source);
        disc.VerifyExecutable();
        std::cout << "XDVDFS files: " << disc.Files().size() << '\n';
      }
      std::puts("Matching N3 executable verified");
      return 0;
    }
    if (argc == 4 && std::wstring_view(argv[1]) == L"import") {
      n3::data::ImportDisc(argv[2], argv[3]);
      std::puts("Disc import and executable verification complete");
      return 0;
    }
    std::fputs("Usage: n3_import inspect <disc.iso|game-folder|default.xex> | import <disc.iso> <new-output-folder>\n", stderr);
    return 2;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
