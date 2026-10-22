#include "native/vector_ops.h"
#include <bit>
#include <cstdint>
#include <iostream>
#include <stdexcept>

using namespace n3::native;
void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
void Equal(Vec3 value, Vec3 expected) {
  Require(std::bit_cast<uint32_t>(value.x) == std::bit_cast<uint32_t>(expected.x) &&
          std::bit_cast<uint32_t>(value.y) == std::bit_cast<uint32_t>(expected.y) &&
          std::bit_cast<uint32_t>(value.z) == std::bit_cast<uint32_t>(expected.z), "Vector bits differ");
}
int main() {
  try {
    Equal(AccumulateScaledVector({1, 2, 3}, {2, -4, 8}, 0.5), {2, 0, 7});
    Equal(AccumulateScaledVector({1, 2, 3}, {2, -4, 8}, -0.5), {0, 4, -1});
    Equal(AccumulateScaledVector({1, 2, 3}, {2, -4, 8}, 0), {1, 2, 3});
    const auto precise = AccumulateScaledVector({0, 0, 0}, {3, 0, 0}, 1.0 + 0x1p-24);
    Require(std::bit_cast<uint32_t>(precise.x) == 0x40400001u, "Scalar was narrowed to float before multiplication");
    const auto zero = AccumulateScaledVector({-0.0f, 0, 0}, {-0.0f, 0, 0}, 1);
    Require(std::bit_cast<uint32_t>(zero.x) == 0x80000000u, "Signed-zero accumulation changed");
    MirroredVector3 three{{1, 2, 3}, {4, 5, 6}};
    SetMirroredVector(three, Vec3{7, -0.0f, -9});
    Equal(three.current, {7, -0.0f, -9}); Equal(three.cached, three.current);
    MirroredVector4 four{};
    SetMirroredVector(four, Vec4{1, 2, 3, -0.0f});
    Equal({four.current.x, four.current.y, four.current.z}, {1, 2, 3});
    Equal({four.cached.x, four.cached.y, four.cached.z}, {1, 2, 3});
    Require(std::bit_cast<uint32_t>(four.current.w) == 0x80000000u &&
            std::bit_cast<uint32_t>(four.cached.w) == 0x80000000u, "Fourth cached component changed");
    for (uint64_t bits : {0ull, 0x8000000000000000ull, 0x0010000000000000ull, 0x7fefffffffffffffull})
      Require(SupportsVectorScalar(std::bit_cast<double>(bits)), "Normal scalar rejected");
    for (uint64_t bits : {1ull, 0x8000000000000001ull, 0x7ff0000000000000ull, 0x7ff8123456789abcull})
      Require(!SupportsVectorScalar(std::bit_cast<double>(bits)), "Exceptional scalar needs original path");
    std::cout << "Scaled accumulation, double scalar precision and synchronized editable channels passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
