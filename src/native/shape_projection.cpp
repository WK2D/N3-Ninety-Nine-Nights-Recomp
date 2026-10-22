#include "shape_projection.h"
#include <bit>
#include <cmath>

#pragma STDC FENV_ACCESS ON

namespace n3::native {
namespace {
float Product(float a, float b) { return static_cast<float>(double(a) * double(b)); }
float Sum(float a, float b) { return static_cast<float>(double(a) + double(b)); }
float Difference(float a, float b) { return static_cast<float>(double(a) - double(b)); }
float Negate(float value) { return std::bit_cast<float>(std::bit_cast<uint32_t>(value) ^ 0x80000000u); }
float Lower(float first, float second) { return first > second ? second : first; }
float Upper(float first, float second) { return first < second ? second : first; }
}
bool SupportsCapsuleProjection(const OrientedCapsule& capsule) {
  return SupportsShapeValue(capsule.center) && SupportsShapeValue(capsule.axis) &&
      SupportsShapeValue(capsule.radius) && SupportsShapeValue(capsule.half_height);
}
CapsuleProjection ProjectCapsuleBounds(const OrientedCapsule& capsule) {
  const auto center = capsule.center;
  const Vec3 offset{Product(capsule.axis.x, capsule.half_height),
                    Product(capsule.axis.y, capsule.half_height),
                    Product(capsule.axis.z, capsule.half_height)};
  const Vec3 negative{Sum(Negate(offset.x), center.x), Sum(center.y, Negate(offset.y)),
                      Sum(center.z, Negate(offset.z))};
  const Vec3 positive{Sum(center.x, offset.x), Sum(center.y, offset.y), Sum(center.z, offset.z)};
  const auto radius = capsule.radius;
  const Bounds3 first{{Difference(negative.x, radius), Difference(negative.y, radius), Difference(negative.z, radius)},
                      {Sum(radius, negative.x), Sum(radius, negative.y), Sum(radius, negative.z)}};
  const Bounds3 second = SphereBounds(positive, radius);
  // The original updates only for strict comparisons. Equal values, including
  // opposite signed zeros, keep the first endpoint's bits.
  return {first, second,
          {{Lower(first.minimum.x, second.minimum.x), Lower(first.minimum.y, second.minimum.y), Lower(first.minimum.z, second.minimum.z)},
           {Upper(first.maximum.x, second.maximum.x), Upper(first.maximum.y, second.maximum.y), Upper(first.maximum.z, second.maximum.z)}}};
}
double BoxSphereRadius(Vec3 half_extents) {
  const float y_squared = Product(half_extents.y, half_extents.y);
  const float xy_squared = static_cast<float>(std::fma(double(half_extents.x), double(half_extents.x), double(y_squared)));
  const float xyz_squared = static_cast<float>(std::fma(double(half_extents.z), double(half_extents.z), double(xy_squared)));
  return std::sqrt(double(xyz_squared));
}
}
