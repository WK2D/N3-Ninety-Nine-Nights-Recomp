#include "handle_table.h"
#include <bit>

namespace n3::native {
uint32_t PackedHandleKey(uint64_t packed) { return uint32_t(packed >> 32); }
uint32_t PackedHandleIndex(uint64_t packed) {
  return uint32_t(int32_t(std::bit_cast<int16_t>(uint16_t(PackedHandleKey(packed)))));
}
bool ValidateHandleKey(HandleRecord record, uint64_t packed) { return record.key == PackedHandleKey(packed); }
bool TryLookupHandleRecord(std::span<const HandleRecord> records, uint32_t index, HandleRecord& output) {
  if (index >= records.size()) return false;
  output = records[index];
  return true;
}
}
