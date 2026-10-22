#pragma once
#include <cstdint>
#include <span>

namespace n3::native {
struct HandleRecord { uint32_t key, object, auxiliary; };
// A packed handle's first doubleword contains its key and object word. The
// original lookup sign-extends the key's low 16 bits before treating it as an
// unsigned index. A negative index must retain the caller's invalid path.
uint32_t PackedHandleKey(uint64_t packed);
uint32_t PackedHandleIndex(uint64_t packed);
bool ValidateHandleKey(HandleRecord record, uint64_t packed);
// Copy an existing record by index. Rejection leaves output unchanged. The
// caller supplies a bounded native table, independently of guest addresses.
bool TryLookupHandleRecord(std::span<const HandleRecord> records, uint32_t index, HandleRecord& output);
}
