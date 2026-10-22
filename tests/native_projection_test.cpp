#include "native/shape_projection.h"
#include <bit>
#include <cmath>
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
          std::bit_cast<uint32_t>(value.z) == std::bit_cast<uint32_t>(expected.z), "Projection vector bits differ");
}
int main() {
  try {
    OrientedCapsule capsule{{10, 20, 30}, {0, 1, 0}, 2, 3};
    Require(SupportsCapsuleProjection(capsule), "Ordinary capsule rejected");
    auto projection = ProjectCapsuleBounds(capsule);
    Equal(projection.negative_endpoint.minimum, {8, 15, 28});
    Equal(projection.positive_endpoint.maximum, {12, 25, 32});
    Equal(projection.bounds.minimum, {8, 15, 28}); Equal(projection.bounds.maximum, {12, 25, 32});
    capsule.axis = {1, -2, 0.5f};
    projection = ProjectCapsuleBounds(capsule);
    Equal(projection.bounds.minimum, {5, 12, 26.5f}); Equal(projection.bounds.maximum, {15, 28, 33.5f});
    capsule.half_height = -3;
    auto reversed = ProjectCapsuleBounds(capsule);
    Equal(reversed.bounds.minimum, projection.bounds.minimum); Equal(reversed.bounds.maximum, projection.bounds.maximum);
    capsule = {{10, 20, 30}, {}, -2, 0};
    projection = ProjectCapsuleBounds(capsule);
    Equal(projection.bounds.minimum, {12, 22, 32}); Equal(projection.bounds.maximum, {8, 18, 28});
    capsule = {{-0.0f, -0.0f, -0.0f}, {}, 0, 1};
    projection = ProjectCapsuleBounds(capsule);
    Equal(projection.bounds.minimum, {-0.0f, -0.0f, -0.0f});
    Equal(projection.positive_endpoint.minimum, {0, 0, 0});
    capsule.radius = -0.0f;
    projection = ProjectCapsuleBounds(capsule);
    Equal(projection.bounds.maximum, {-0.0f, -0.0f, -0.0f});
    Equal(projection.positive_endpoint.maximum, {0, 0, 0});
    capsule.axis.x = std::numeric_limits<float>::infinity();
    Require(!SupportsCapsuleProjection(capsule), "Nonfinite axis requires original path");
    capsule.axis.x = std::bit_cast<float>(1u);
    Require(!SupportsCapsuleProjection(capsule), "Subnormal axis requires original path");
    Require(BoxSphereRadius({3, 4, 0}) == 5 && BoxSphereRadius({-3, -4, 0}) == 5, "Box radius changed");
    const auto precise_radius = BoxSphereRadius({1, 1, 1});
    Require(precise_radius == std::sqrt(3.0) && precise_radius != double(float(precise_radius)),
            "Radius must retain the double square-root result before storage");
    Require(std::isinf(BoxSphereRadius({std::numeric_limits<float>::max(), 0, 0})),
            "Original float32 square overflow was incorrectly corrected");
    Require(BoxSphereRadius({std::numeric_limits<float>::min(), 0, 0}) == 0,
            "Original float32 square underflow was incorrectly corrected");
    std::cout << "Capsule endpoints/union, editable axis/dimensions, signed-zero ties and box radius precision passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
