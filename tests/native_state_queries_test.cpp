#include "native/state_queries.h"
#include <array>
#include <iostream>
#include <stdexcept>

using namespace n3::native;
void Require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
int main() {
  try {
    for (uint32_t bit = 0; bit < 32; ++bit) {
      const auto flag = uint32_t{1} << bit;
      Require(QueryKindFlags({flag}, flag).enabled && QueryKindFlags({flag}, flag).selected_bits == flag,
              "A selected flag was lost");
      Require(!QueryKindFlags({~flag}, flag).enabled && !QueryKindFlags({flag}, 0).enabled,
              "An unrelated or zero mask enabled a query");
      Require(QueryKindFlags({flag}, flag | 0x55555555u).enabled, "Combined masks must select any matching bit");
    }
    Require(QueryKindFlags({0xdeadbeef}, 0xf0f00f0f).selected_bits == 0xd0a00e0f,
            "Editable mask or unknown flag bits changed");
    constexpr std::array<uint32_t, 6> values{0, 1, 0xffffffff, 0x80000000, 0x7fffffff, 17};
    for (auto tag : values) for (auto value : values) {
      Require(SameStateIdentity({tag, value}, {tag, value}), "Equal identities were rejected");
      Require(!SameStateIdentity({tag, value}, {tag ^ 1, value}) &&
              !SameStateIdentity({tag, value}, {tag, value ^ 1}), "Both identity components must match");
    }
    for (int32_t category : {(-2147483647 - 1), -1, 0, 1023, 1024, 1025, 2147483647})
      Require(CategoryAllowsKindFlags(category) == (category != 1024), "Category gate was broadened");
    uint32_t byte_cases = 0;
    for (uint32_t stored = 0; stored < 256; ++stored)
      for (uint8_t request : {uint8_t{0}, uint8_t{1}, uint8_t{255}})
        for (uint8_t guard : {uint8_t{0}, uint8_t{1}, uint8_t{255}}) {
          Require(SelectStateByte(request, guard, uint8_t(stored)) == (request && !guard ? 1 : stored),
                  "Stored byte was booleanized or override ignored");
          ++byte_cases;
        }
    std::cout << "All flag bits/combined masks, identity/category gates and " << byte_cases
              << " byte-selection cases passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
