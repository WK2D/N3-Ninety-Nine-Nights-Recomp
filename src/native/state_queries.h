#pragma once
#include <cstdint>

namespace n3::native {
struct KindFlags { uint32_t value; };
struct KindFlagDecision { uint32_t selected_bits; bool enabled; };
struct StateIdentity { uint32_t tag, value; };

// Masks retain their verified numeric meaning until callers establish names.
// Combined masks select any matching bit; a zero mask never enables a query.
KindFlagDecision QueryKindFlags(KindFlags flags, uint32_t mask);
bool SameStateIdentity(StateIdentity first, StateIdentity second);
// Category 1024 bypasses the kind-table lookup in the corresponding guest query.
bool CategoryAllowsKindFlags(int32_t category);
// The stored byte is a value, not a bool: preserve all eight bits. Only a
// nonzero request with a zero guard replaces it with 1.
uint8_t SelectStateByte(uint8_t request, uint8_t guard, uint8_t stored);
}
