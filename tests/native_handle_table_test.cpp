#include "native/handle_table.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>

using namespace n3::native;
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool Same(HandleRecord a, HandleRecord b) {
  return a.key == b.key && a.object == b.object && a.auxiliary == b.auxiliary;
}
int main() {
  try {
    constexpr std::array<HandleRecord, 3> records{{{0x12340000, 0x10203040, 7},
        {0x92340001, 0, 0xffffffffu}, {0xffff0002, 0x87654321, 0x80000000}}};
    HandleRecord result{};
    Require(TryLookupHandleRecord(records, 0, result) && Same(result, records[0]), "First handle record differs");
    Require(TryLookupHandleRecord(records, 2, result) && Same(result, records[2]), "Last handle record differs");
    const auto before = result;
    Require(!TryLookupHandleRecord(records, 3, result) && Same(result, before), "Past-end lookup changed output");
    Require(!TryLookupHandleRecord(records, 0xffffffffu, result) && Same(result, before), "Negative packed index was accepted");
    Require(!TryLookupHandleRecord({}, 0, result) && Same(result, before), "Empty lookup changed output");
    constexpr std::array<uint32_t, 6> keys{0, 0xffff0000u, 0x81237fffu, 0x12348000u, 0x1234ffffu, 0xffffffffu};
    constexpr std::array<uint32_t, 6> indices{0, 0, 32767, 0xffff8000u, 0xffffffffu, 0xffffffffu};
    for (size_t i = 0; i < keys.size(); ++i) {
      const auto packed = uint64_t(keys[i]) << 32 | 0xfedcba98u;
      Require(PackedHandleKey(packed) == keys[i] && PackedHandleIndex(packed) == indices[i],
              "Packed key extraction or signed halfword index changed");
    }
    Require(ValidateHandleKey(records[1], uint64_t(records[1].key) << 32 | 0xdeadbeefu),
            "Object word must not participate in key validation");
    Require(!ValidateHandleKey(records[1], uint64_t(records[1].key ^ 0x10000u) << 32),
            "A stale generation with the same record index was accepted");
    Require(ValidateHandleKey({records[1].key, 0xffffffffu, 123}, uint64_t(records[1].key) << 32),
            "Record object and auxiliary words must not affect key equality");
    std::cout << "Typed handle records, signed packed indices, exact keys and transactional rejection passed\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 1;
  }
}
