#include "box_geometry.h"
#include <bit>
#include <cmath>

#pragma STDC FENV_ACCESS ON

namespace n3::native {
namespace {
float Product(float first, float second) { return static_cast<float>(double(first) * double(second)); }
float Sum(float first, float second) { return static_cast<float>(double(first) + double(second)); }
float Difference(float first, float second) { return static_cast<float>(double(first) - double(second)); }
float Magnitude(float value) { return std::bit_cast<float>(std::bit_cast<uint32_t>(value) & 0x7fffffffu); }
float Extent(Vec3 row, Vec3 half_extents) {
  const float yz = Sum(Magnitude(Product(row.z, half_extents.z)), Magnitude(Product(row.y, half_extents.y)));
  return Sum(yz, Magnitude(Product(row.x, half_extents.x)));
}
float TransformComponent(Vec3 row, Vec3 point, float translation) {
  const float z = Product(row.z, point.z);
  // These are double FMAs narrowed to float32, as in the retained translation;
  // replacing them with float fma or reassociating the dot product can differ.
  const float yz = static_cast<float>(std::fma(double(row.y), double(point.y), double(z)));
  const float xyz = static_cast<float>(std::fma(double(row.x), double(point.x), double(yz)));
  return Sum(translation, xyz);
}
}
bool SupportsBoxGeometry(const TransformedBox& box) {
  return SupportsShapeValue(box.half_extents) && SupportsShapeValue(box.transform.translation) &&
      SupportsShapeValue(box.transform.rows[0]) && SupportsShapeValue(box.transform.rows[1]) &&
      SupportsShapeValue(box.transform.rows[2]);
}
Bounds3 TransformedBoxBounds(const TransformedBox& box) {
  const Vec3 extent{Extent(box.transform.rows[0], box.half_extents),
                    Extent(box.transform.rows[1], box.half_extents),
                    Extent(box.transform.rows[2], box.half_extents)};
  const auto center = box.transform.translation;
  return {{Difference(center.x, extent.x), Difference(center.y, extent.y), Difference(center.z, extent.z)},
          {Sum(center.x, extent.x), Sum(center.y, extent.y), Sum(center.z, extent.z)}};
}
bool TryTransformBoxSample(const TransformedBox& box, std::array<int32_t, 3> coordinates, Vec3& output) {
  if (!SupportsBoxGeometry(box)) return false;
  const Vec3 point{Product(box.half_extents.x, static_cast<float>(double(coordinates[0]))),
                   Product(box.half_extents.y, static_cast<float>(double(coordinates[1]))),
                   Product(box.half_extents.z, static_cast<float>(double(coordinates[2])))};
  if (!SupportsShapeValue(point)) return false;
  output = {TransformComponent(box.transform.rows[0], point, box.transform.translation.x),
            TransformComponent(box.transform.rows[1], point, box.transform.translation.y),
            TransformComponent(box.transform.rows[2], point, box.transform.translation.z)};
  return true;
}
}
