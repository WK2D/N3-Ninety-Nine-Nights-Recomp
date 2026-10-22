#include "native/payload_install.h"
#include <array>
#include <bit>
#include <cfenv>
#include <cstring>
#include <iostream>
#include <stdexcept>

using namespace n3::native;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
bool Same(const StatePayload& a, const StatePayload& b) { return std::memcmp(&a, &b, sizeof(a)) == 0; }
StatePayload Fixture() {
  StatePayload value{};
  value.word0 = 0x81234567; value.word44 = 0x7f812345; value.word96 = 0xfedcba98;
  for (uint32_t i = 0; i < 7; ++i) {
    value.words4[i] = 0x11111111u * (i + 1); value.words56[i] = ~i;
  }
  value.floats32 = {1.25f, -0.0f, -42}; value.floats48 = {0, std::bit_cast<float>(0x00800000u)};
  value.floats84 = {std::bit_cast<float>(0x7f7fffffu), -3.5f, std::bit_cast<float>(0x80800000u)};
  return value;
}
int main() {
  const auto saved_rounding = std::fegetround();
  try {
    for (auto rounding : {FE_TONEAREST, FE_TOWARDZERO, FE_DOWNWARD, FE_UPWARD}) {
      Require(std::fesetround(rounding) == 0, "Rounding mode unavailable");
      auto source = Fixture(); StatePayload state{}; PayloadInstallUpdate update{true, -1};
      Require(TryInstallStatePayload(state, source, nullptr, 5, {true, 1000, 0}, update) &&
              Same(state, source) && !update.multiplied && update.signed_product == 0,
              "A low own kind must copy all fields without applying the multiplier");
      const Vec3 optional{-0.0f, std::bit_cast<float>(0x00800000u), -7.5f};
      auto expected = source; expected.words56[5] = 1; expected.floats84 = {optional.x, optional.y, optional.z};
      Require(TryInstallStatePayload(state, source, &optional, 0xffffffffu, {true, 1000, 0}, update) &&
              Same(state, expected) && !update.multiplied,
              "Optional vector and enable word changed unrelated payload fields or signed zero");
      constexpr std::array<uint32_t, 8> kinds{0, 5, 6, 7, 1000, 0x7fffffffu, 0x80000000u, 0xffffffffu};
      for (auto own_kind : kinds) for (bool matches : {false, true})
        for (auto related_kind : {0u, 6u, 1000u, 1001u}) for (auto variant : {0u, 0xfffffffeu, 0xffffffffu}) {
          source = Fixture(); source.words4[4] = 12; source.words4[5] = 0xfffffffdu;
          const bool should_multiply = std::bit_cast<int32_t>(own_kind) >= 6 && matches &&
              related_kind == 1000 && variant != 0xffffffffu;
          expected = source; if (should_multiply) expected.words4[4] = 0xffffffdcu;
          update = {true, -1};
          Require(TryInstallStatePayload(state, source, nullptr, own_kind, {matches, related_kind, variant}, update) &&
                  Same(state, expected) && update.multiplied == should_multiply &&
                  update.signed_product == (should_multiply ? int64_t{-36} : 0),
                  "Kind, key or variant gate changed the full signed product contract");
        }
      constexpr std::array<std::array<uint32_t, 3>, 7> products{{
          {0x80000000u, 0x80000000u, 0}, {0x7fffffffu, 2, 0xfffffffeu},
          {0xffffffffu, 3, 0xfffffffdu}, {0xfffffffdu, 0xfffffffbu, 15},
          {0, 0x80000000u, 0}, {0x12345678u, 0x10, 0x23456780u},
          {0x80000000u, 0xffffffffu, 0x80000000u}}};
      for (const auto& words : products) {
        source = Fixture(); source.words4[4] = words[0]; source.words4[5] = words[1];
        expected = source; expected.words4[4] = words[2];
        const auto product = int64_t(std::bit_cast<int32_t>(words[0])) * int64_t(std::bit_cast<int32_t>(words[1]));
        Require(TryInstallStatePayload(state, source, nullptr, 6, {true, 1000, 0xfffffffeu}, update) &&
                Same(state, expected) && update.multiplied && update.signed_product == product,
                "Multiplier did not retain low payload word and full signed product");
      }
      source = Fixture(); source.words4[4] = 7; source.words4[5] = 8;
      expected = source; expected.words4[4] = 56; expected.words56[5] = 1;
      expected.floats84 = {optional.x, optional.y, optional.z};
      Require(TryInstallStatePayload(state, source, &optional, 1000, {true, 1000, 0}, update) &&
              Same(state, expected) && update.multiplied && update.signed_product == 56,
              "Optional vector replacement interfered with the related multiplier");
      const auto before = state; const PayloadInstallUpdate before_update{true, -123456789}; update = before_update;
      for (uint32_t lane = 0; lane < 8; ++lane)
        for (uint32_t bits : {1u, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u}) {
          source = Fixture();
          float& value = lane < 3 ? source.floats32[lane] : lane < 5 ? source.floats48[lane - 3] : source.floats84[lane - 5];
          value = std::bit_cast<float>(bits);
          Require(!TryInstallStatePayload(state, source, &optional, 1000, {true, 1000, 0}, update) && Same(state, before) &&
                  update.multiplied == before_update.multiplied && update.signed_product == before_update.signed_product,
                  "A rejected source float partially published state or details");
        }
      for (uint32_t lane = 0; lane < 3; ++lane)
        for (uint32_t bits : {1u, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u}) {
          source = Fixture(); Vec3 invalid{1, 2, 3};
          (lane == 0 ? invalid.x : lane == 1 ? invalid.y : invalid.z) = std::bit_cast<float>(bits);
          Require(!TryInstallStatePayload(state, source, &invalid, 1000, {true, 1000, 0}, update) && Same(state, before) &&
                  update.multiplied == before_update.multiplied && update.signed_product == before_update.signed_product,
                  "A rejected optional float partially published state or details");
        }
      for (uint32_t category : {0u, 1u, 2u, 127u, 255u}) for (bool matches : {false, true})
        for (uint32_t related_kind : {0u, 5u, 6u, 7u, 999u, 1000u, 1001u, 0xffffffffu}) {
          const auto selected = matches && (related_kind == 6 || related_kind == 1000) ? 2u : category;
          Require(SelectRelatedTableCategory(uint8_t(category), matches, related_kind) == selected,
                  "Related category selection ignored its key or accepted another kind");
        }
    }
    std::fesetround(saved_rounding);
    std::cout << "Payload installation, optional vector, related-object gates, signed product and rollback passed\n";
  } catch (const std::exception& error) {
    std::fesetround(saved_rounding); std::cerr << error.what() << '\n'; return 1;
  }
}
