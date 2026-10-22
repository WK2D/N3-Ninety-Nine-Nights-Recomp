#pragma once
#include "native/state_payload.h"
#include "guest_memory_address.h"
#include <rex/memory/utils.h>
#include <bit>

namespace n3::guest {
extern thread_local uint32_t state_payload_retained_depth;
inline bool PayloadChainRetained() { return state_payload_retained_depth != 0; }
// Outer adapters use this only while delegating an unsupported operation. Every
// nested payload transfer then retains the original sequential load/store path.
class RetainPayloadChain {
 public:
  RetainPayloadChain() { ++state_payload_retained_depth; }
  ~RetainPayloadChain() { --state_payload_retained_depth; }
  RetainPayloadChain(const RetainPayloadChain&) = delete;
  RetainPayloadChain& operator=(const RetainPayloadChain&) = delete;
};
// Caller validates FitsLinearGuestRange(address, kStatePayloadSize) and mapped
// read/write ordering before use.
// These adapters transfer bits only; they perform no floating-point arithmetic.
inline native::StatePayload ReadStatePayload(uint8_t* base, uint32_t address) {
  auto word = [&](uint32_t offset) { return rex::memory::load_and_swap<uint32_t>(GuestHostAddress(base, address + offset)); };
  auto scalar = [&](uint32_t offset) { return std::bit_cast<float>(word(offset)); };
  native::StatePayload payload{};
  payload.word0 = word(0);
  for (uint32_t i = 0; i < 7; ++i) payload.words4[i] = word(4 + i * 4);
  for (uint32_t i = 0; i < 3; ++i) payload.floats32[i] = scalar(32 + i * 4);
  payload.word44 = word(44);
  for (uint32_t i = 0; i < 2; ++i) payload.floats48[i] = scalar(48 + i * 4);
  for (uint32_t i = 0; i < 7; ++i) payload.words56[i] = word(56 + i * 4);
  for (uint32_t i = 0; i < 3; ++i) payload.floats84[i] = scalar(84 + i * 4);
  payload.word96 = word(96);
  return payload;
}
inline void WriteStatePayload(uint8_t* base, uint32_t address, const native::StatePayload& payload) {
  auto word = [&](uint32_t offset, uint32_t value) {
    rex::memory::store_and_swap<uint32_t>(GuestHostAddress(base, address + offset), value);
  };
  auto scalar = [&](uint32_t offset, float value) { word(offset, std::bit_cast<uint32_t>(value)); };
  word(0, payload.word0);
  for (uint32_t i = 0; i < 7; ++i) word(4 + i * 4, payload.words4[i]);
  for (uint32_t i = 0; i < 3; ++i) scalar(32 + i * 4, payload.floats32[i]);
  word(44, payload.word44);
  for (uint32_t i = 0; i < 2; ++i) scalar(48 + i * 4, payload.floats48[i]);
  for (uint32_t i = 0; i < 7; ++i) word(56 + i * 4, payload.words56[i]);
  for (uint32_t i = 0; i < 3; ++i) scalar(84 + i * 4, payload.floats84[i]);
  word(96, payload.word96);
}
}
