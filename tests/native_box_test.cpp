#include "native/box_geometry.h"
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace n3::native;
void Require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
void Equal(Vec3 value, Vec3 expected) {
  Require(std::bit_cast<uint32_t>(value.x) == std::bit_cast<uint32_t>(expected.x) &&
          std::bit_cast<uint32_t>(value.y) == std::bit_cast<uint32_t>(expected.y) &&
          std::bit_cast<uint32_t>(value.z) == std::bit_cast<uint32_t>(expected.z), "Box vector bits differ");
}
int main() {
  try {
    TransformedBox box{{{Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, 1}}, {10, 20, 30}}, {2, 3, 4}};
    Require(SupportsBoxGeometry(box), "Identity box rejected");
    auto bounds = TransformedBoxBounds(box);
    Equal(bounds.minimum, {8, 17, 26}); Equal(bounds.maximum, {12, 23, 34});
    for (int32_t x : {-1, 1}) for (int32_t y : {-1, 1}) for (int32_t z : {-1, 1}) {
      Vec3 point{};
      Require(TryTransformBoxSample(box, {x, y, z}, point), "Ordinary corner rejected");
      Equal(point, {10.0f + 2 * x, 20.0f + 3 * y, 30.0f + 4 * z});
    }
    box.transform.rows = {{{0, -2, 0}, {3, 0, 1}, {1, 1, 0}}};
    bounds = TransformedBoxBounds(box);
    Equal(bounds.minimum, {4, 10, 25}); Equal(bounds.maximum, {16, 30, 35});
    Vec3 point{};
    Require(TryTransformBoxSample(box, {-1, 1, -1}, point), "Sheared corner rejected");
    Equal(point, {4, 10, 31});
    box.half_extents = {-2, -3, -4};
    auto reflected = TransformedBoxBounds(box);
    Equal(reflected.minimum, bounds.minimum); Equal(reflected.maximum, bounds.maximum);
    box = {{{Vec3{1, 1, 1}, Vec3{0, 1, 0}, Vec3{0, 0, 1}}, {}}, {16777216.0f, 1, 1}};
    Require(TransformedBoxBounds(box).maximum.x == 16777218.0f, "Extent sum order changed");
    box.half_extents = {1, -1, 0x1p-27f};
    Require(TryTransformBoxSample(box, {1, 1, 1}, point) && point.x == 0,
            "Corner dot product must round Z + Y before adding X");
    box.half_extents = {1, 0, 0};
    Require(TryTransformBoxSample(box, {16777217, 0, 0}, point) && point.x == 16777216.0f,
            "Integer coordinates must round before multiplication");
    point = {99, 98, 97};
    box.half_extents.x = std::numeric_limits<float>::max();
    Require(!TryTransformBoxSample(box, {2, 0, 0}, point), "Overflowing scaled extent needs fallback");
    Equal(point, {99, 98, 97});
    box.half_extents.x = std::bit_cast<float>(1u);
    Require(!SupportsBoxGeometry(box) && !TryTransformBoxSample(box, {1, 0, 0}, point),
            "Subnormal source needs fallback");
    Equal(point, {99, 98, 97});
    std::cout << "World box bounds/corners, shear, negative extents, rounding order and rejection passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
