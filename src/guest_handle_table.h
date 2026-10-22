#pragma once
#include "guest_native_support.h"
#include "native/handle_table.h"
#include <cstdint>

namespace n3::guest {
// Exclude writes made by an original caller before it reads the table: the
// below-SP stack frame and an optional separate argument-home range above SP.
struct HandleScratch {
  uint32_t stack, below, above_offset = 0, above_size = 0;
};
struct HandleQuery {
  native::HandleRecord record;
  uint32_t index, count, address;
  bool key_matches;
};
// Read-only lookup of the verified 12-byte global handle-table format. Rejects
// reversed/wrapping/partial tables, unknown bounds and scratch aliases before
// following affected pointers. Rejection leaves query unchanged. No FP work.
bool QueryHandleRecord(uint8_t* base, uint32_t index, HandleScratch scratch, HandleQuery& query);
bool QueryPackedHandle(uint8_t* base, uint64_t packed, HandleScratch scratch, HandleQuery& query);

inline thread_local uint32_t retained_handle_depth = 0;
// Retain a complete nested handle chain when an outer native adapter rejects.
// Also lets differential oracles call the original wrapper and original lookup.
class RetainHandleChain {
 public:
  RetainHandleChain() { ++retained_handle_depth; }
  ~RetainHandleChain() { --retained_handle_depth; }
  RetainHandleChain(const RetainHandleChain&) = delete;
  RetainHandleChain& operator=(const RetainHandleChain&) = delete;
};
}
