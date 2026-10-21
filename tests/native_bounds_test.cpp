#include "native/shape_bounds.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace n3::native;
void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
bool Same(float a, float b) { return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b); }
void Vector(Vec3 value, Vec3 expected) {
  Require(Same(value.x, expected.x) && Same(value.y, expected.y) && Same(value.z, expected.z),
          "Bounds component mismatch");
}
int main() {
  try {
    const auto sphere = SphereBounds({2, -3, 5}, 4);
    Vector(sphere.minimum, {-2, -7, 1}); Vector(sphere.maximum, {6, 1, 9});
    const auto local = SphereLocalBounds(2);
    Vector(local.minimum, {-2, -2, -2}); Vector(local.maximum, {2, 2, 2});
    const auto capsule = CapsuleLocalBounds(2, 7);
    Vector(capsule.minimum, {-2, -9, -2}); Vector(capsule.maximum, {2, 9, 2});
    const auto box = BoxLocalBounds({2, 3, 4});
    Vector(box.minimum, {-2, -3, -4}); Vector(box.maximum, {2, 3, 4});
    const auto envelope = CapsuleSphereEnvelope({1, 2, 3}, 2, 7);
    Vector(envelope.center, {1, 2, 3}); Require(envelope.radius == 9, "Envelope radius mismatch");
    const auto negative = SphereLocalBounds(-2);
    Vector(negative.minimum, {2, 2, 2}); Vector(negative.maximum, {-2, -2, -2});
    const auto positive_zero = SphereLocalBounds(0.0f);
    Require(Same(positive_zero.minimum.x, -0.0f) && Same(positive_zero.maximum.x, 0.0f),
            "Positive-zero radius lost its sign");
    const auto negative_zero = BoxLocalBounds({-0.0f, 0.0f, -0.0f});
    Vector(negative_zero.minimum, {0.0f, -0.0f, 0.0f});
    for (uint32_t bits : {0u, 0x80000000u, 0x00800000u, 0x80800000u, 0x7f7fffffu, 0xff7fffffu})
      Require(SupportsShapeValue(std::bit_cast<float>(bits)), "Finite normal/zero source rejected");
    for (uint32_t bits : {1u, 0x80000001u, 0x007fffffu, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f812345u})
      Require(!SupportsShapeValue(std::bit_cast<float>(bits)), "Exceptional source must retain translated path");
    Require(!SupportsShapeValue(Vec3{1, std::numeric_limits<float>::infinity(), 2}),
            "Exceptional vector component accepted");
    std::cout << "Sphere, capsule, box and envelope bounds; signed zeros and fallback classification passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
