#include "native/state_payload.h"
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
  StatePayload result{};
  result.word0 = 0xfedcba98; result.word44 = 0x7f812345; result.word96 = 0xdeadbeef;
  for (uint32_t i = 0; i < 7; ++i) {
    result.words4[i] = 0x11111111u * (i + 1); result.words56[i] = ~(0x11111111u * i);
  }
  result.floats32 = {1.25f, -0.0f, -42}; result.floats48 = {0, std::bit_cast<float>(0x00800000u)};
  result.floats84 = {std::bit_cast<float>(0x7f7fffffu), -3.5f, std::bit_cast<float>(0x80800000u)};
  return result;
}
int main() {
  const auto saved = std::fegetround();
  try {
    for (auto rounding : {FE_TONEAREST, FE_TOWARDZERO, FE_DOWNWARD, FE_UPWARD}) {
      Require(std::fesetround(rounding) == 0, "Rounding mode unavailable");
      auto source = Fixture(); StatePayload destination{};
      Require(SupportsStatePayload(source) && TryCopyStatePayload(source, destination) && Same(source, destination),
              "Payload copy changed fields, integer bits or signed zeros");
      source.word0 ^= 0xffffffffu; source.word44 ^= 7; source.word96 = 0;
      for (auto& word : source.words4) word += 7;
      for (auto& word : source.words56) word -= 11;
      source.floats32 = {-12, 0, -0.0f}; source.floats48 = {4, -6}; source.floats84 = {7, 8, -9};
      Require(TryCopyStatePayload(source, destination) && Same(source, destination), "Edits to payload groups were ignored");
      const auto before = destination;
      Require(!TryCopyStatePayload(destination, destination) && Same(destination, before), "Self copy must reject unchanged");
      for (uint32_t lane = 0; lane < 8; ++lane) {
        for (uint32_t bits : {1u, 0x80000001u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u}) {
          source = Fixture();
          float& value = lane < 3 ? source.floats32[lane] : lane < 5 ? source.floats48[lane - 3] : source.floats84[lane - 5];
          value = std::bit_cast<float>(bits);
          Require(!SupportsStatePayload(source) && !TryCopyStatePayload(source, destination) && Same(destination, before),
                  "Rejected float transfer partially published the payload");
        }
      }
    }
    std::fesetround(saved);
    std::cout << "Editable 100-byte payload fields, integer bits, signed zero and rejection passed\n";
  } catch (const std::exception& error) {
    std::fesetround(saved); std::cerr << error.what() << '\n'; return 1;
  }
}
