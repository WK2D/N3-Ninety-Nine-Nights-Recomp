#pragma once

namespace n3::native {
struct Vec3 { float x, y, z; };
struct Bounds3 { Vec3 minimum, maximum; };
struct SphereEnvelope { Vec3 center; float radius; };

// Preserve the game's arithmetic and signed zeros. Inputs are not clamped;
// negative radii/extents retain the original ordering, even if min > max.
Bounds3 SphereBounds(Vec3 center, float radius);
Bounds3 SphereLocalBounds(float radius);
Bounds3 CapsuleLocalBounds(float radius, float half_height);
Bounds3 BoxLocalBounds(Vec3 half_extents);
SphereEnvelope CapsuleSphereEnvelope(Vec3 center, float radius, float half_height);

// Nonfinite and subnormal source values retain the translated FP path, where
// NaN payloads, exception flags and platform conversions remain unchanged.
bool SupportsShapeValue(float value);
bool SupportsShapeValue(Vec3 value);
}
