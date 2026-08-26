#pragma once

#include <Aero/Base/Config.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Aero::Base {

struct Point  { double x = 0.0; double y = 0.0; };
struct Size  { double width = 0.0; double height = 0.0; };
struct Rect  {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};
struct Thickness {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;
};
struct CornerRadius {
    double topLeft = 0.0;
    double topRight = 0.0;
    double bottomRight = 0.0;
    double bottomLeft = 0.0;
};
struct Color  {
    float red = 0.0F;
    float green = 0.0F;
    float blue = 0.0F;
    float alpha = 1.0F;
};
struct Transform2D  {
    double m11 = 1.0;
    double m12 = 0.0;
    double m21 = 0.0;
    double m22 = 1.0;
    double dx = 0.0;
    double dy = 0.0;
};

struct Point3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// 4×3 3D affine. Row-vector convention matches Transform2D:
// p' = p * M, x' = x*m11 + y*m21 + z*m31 + dx (and likewise for y/z).
struct Transform3 {
    double m11 = 1.0;
    double m12 = 0.0;
    double m13 = 0.0;
    double m21 = 0.0;
    double m22 = 1.0;
    double m23 = 0.0;
    double m31 = 0.0;
    double m32 = 0.0;
    double m33 = 1.0;
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;
};

// 3×3 projective 2D. Row-vector: [x y 1] * M, then homogeneous divide by w.
struct ProjectiveTransform2D {
    double m11 = 1.0;
    double m12 = 0.0;
    double m13 = 0.0;
    double m21 = 0.0;
    double m22 = 1.0;
    double m23 = 0.0;
    double m31 = 0.0;
    double m32 = 0.0;
    double m33 = 1.0;
};

using RenderNodeId = std::uint64_t;
constexpr RenderNodeId InvalidRenderNodeId = 0U;

inline bool IsFiniteRect(Rect value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.width) && std::isfinite(value.height);
}

inline bool IsFiniteColor(Color value) noexcept {
    return std::isfinite(value.red) && std::isfinite(value.green) &&
        std::isfinite(value.blue) && std::isfinite(value.alpha);
}

inline bool IsFiniteTransform(Transform2D value) noexcept {
    return std::isfinite(value.m11) && std::isfinite(value.m12) &&
        std::isfinite(value.m21) && std::isfinite(value.m22) &&
        std::isfinite(value.dx) && std::isfinite(value.dy);
}

inline bool IsFinitePoint3(Point3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

inline bool IsFiniteTransform(Transform3 value) noexcept {
    return std::isfinite(value.m11) && std::isfinite(value.m12) &&
        std::isfinite(value.m13) && std::isfinite(value.m21) &&
        std::isfinite(value.m22) && std::isfinite(value.m23) &&
        std::isfinite(value.m31) && std::isfinite(value.m32) &&
        std::isfinite(value.m33) && std::isfinite(value.dx) &&
        std::isfinite(value.dy) && std::isfinite(value.dz);
}

inline bool IsFiniteTransform(ProjectiveTransform2D value) noexcept {
    return std::isfinite(value.m11) && std::isfinite(value.m12) &&
        std::isfinite(value.m13) && std::isfinite(value.m21) &&
        std::isfinite(value.m22) && std::isfinite(value.m23) &&
        std::isfinite(value.m31) && std::isfinite(value.m32) &&
        std::isfinite(value.m33);
}

inline bool IsValidRect(Rect value) noexcept {
    return IsFiniteRect(value) && value.width >= 0.0 && value.height >= 0.0;
}

inline bool IsNormalizedOpacity(double value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

inline constexpr double DefaultPerspectiveDepth = 1000.0;
inline constexpr double PerspectiveZClampFraction = 0.95;
inline constexpr double ProjectiveWEpsilon = 1.0e-8;

inline Transform3 ToTransform3(Transform2D value) noexcept {
    Transform3 output;
    output.m11 = value.m11;
    output.m12 = value.m12;
    output.m21 = value.m21;
    output.m22 = value.m22;
    output.dx = value.dx;
    output.dy = value.dy;
    return output;
}

inline ProjectiveTransform2D ToProjective(Transform2D value) noexcept {
    ProjectiveTransform2D output;
    output.m11 = value.m11;
    output.m12 = value.m12;
    output.m21 = value.m21;
    output.m22 = value.m22;
    output.m31 = value.dx;
    output.m32 = value.dy;
    output.m33 = 1.0;
    return output;
}

inline bool IsAffine(const ProjectiveTransform2D& value) noexcept {
    return std::abs(value.m13) <= 1.0e-12 &&
        std::abs(value.m23) <= 1.0e-12 &&
        std::abs(value.m33 - 1.0) <= 1.0e-12;
}

inline bool TryToTransform2D(
    const ProjectiveTransform2D& value,
    Transform2D& affine) noexcept {
    if (!IsAffine(value) || !IsFiniteTransform(value)) {
        return false;
    }
    affine.m11 = value.m11;
    affine.m12 = value.m12;
    affine.m21 = value.m21;
    affine.m22 = value.m22;
    affine.dx = value.m31;
    affine.dy = value.m32;
    return IsFiniteTransform(affine);
}

inline Transform3 IdentityTransform3() noexcept {
    return {};
}

inline ProjectiveTransform2D IdentityProjective() noexcept {
    return {};
}

inline bool IsIdentityTransform3(const Transform3& value) noexcept {
    return std::abs(value.m11 - 1.0) <= 1.0e-12 &&
        std::abs(value.m22 - 1.0) <= 1.0e-12 &&
        std::abs(value.m33 - 1.0) <= 1.0e-12 &&
        std::abs(value.m12) <= 1.0e-12 && std::abs(value.m13) <= 1.0e-12 &&
        std::abs(value.m21) <= 1.0e-12 && std::abs(value.m23) <= 1.0e-12 &&
        std::abs(value.m31) <= 1.0e-12 && std::abs(value.m32) <= 1.0e-12 &&
        std::abs(value.dx) <= 1.0e-12 && std::abs(value.dy) <= 1.0e-12 &&
        std::abs(value.dz) <= 1.0e-12;
}

inline bool LeavesZ0PlaneUnchanged(const Transform3& value) noexcept {
    return std::abs(value.m13) <= 1.0e-12 &&
        std::abs(value.m23) <= 1.0e-12 &&
        std::abs(value.dz) <= 1.0e-12;
}

inline Transform3 Compose(const Transform3& first, const Transform3& second) noexcept {
    Transform3 output;
    output.m11 = first.m11 * second.m11 + first.m12 * second.m21 +
        first.m13 * second.m31;
    output.m12 = first.m11 * second.m12 + first.m12 * second.m22 +
        first.m13 * second.m32;
    output.m13 = first.m11 * second.m13 + first.m12 * second.m23 +
        first.m13 * second.m33;
    output.m21 = first.m21 * second.m11 + first.m22 * second.m21 +
        first.m23 * second.m31;
    output.m22 = first.m21 * second.m12 + first.m22 * second.m22 +
        first.m23 * second.m32;
    output.m23 = first.m21 * second.m13 + first.m22 * second.m23 +
        first.m23 * second.m33;
    output.m31 = first.m31 * second.m11 + first.m32 * second.m21 +
        first.m33 * second.m31;
    output.m32 = first.m31 * second.m12 + first.m32 * second.m22 +
        first.m33 * second.m32;
    output.m33 = first.m31 * second.m13 + first.m32 * second.m23 +
        first.m33 * second.m33;
    output.dx = first.dx * second.m11 + first.dy * second.m21 +
        first.dz * second.m31 + second.dx;
    output.dy = first.dx * second.m12 + first.dy * second.m22 +
        first.dz * second.m32 + second.dy;
    output.dz = first.dx * second.m13 + first.dy * second.m23 +
        first.dz * second.m33 + second.dz;
    return output;
}

inline ProjectiveTransform2D Compose(
    const ProjectiveTransform2D& first,
    const ProjectiveTransform2D& second) noexcept {
    ProjectiveTransform2D output;
    output.m11 = first.m11 * second.m11 + first.m12 * second.m21 +
        first.m13 * second.m31;
    output.m12 = first.m11 * second.m12 + first.m12 * second.m22 +
        first.m13 * second.m32;
    output.m13 = first.m11 * second.m13 + first.m12 * second.m23 +
        first.m13 * second.m33;
    output.m21 = first.m21 * second.m11 + first.m22 * second.m21 +
        first.m23 * second.m31;
    output.m22 = first.m21 * second.m12 + first.m22 * second.m22 +
        first.m23 * second.m32;
    output.m23 = first.m21 * second.m13 + first.m22 * second.m23 +
        first.m23 * second.m33;
    output.m31 = first.m31 * second.m11 + first.m32 * second.m21 +
        first.m33 * second.m31;
    output.m32 = first.m31 * second.m12 + first.m32 * second.m22 +
        first.m33 * second.m32;
    output.m33 = first.m31 * second.m13 + first.m32 * second.m23 +
        first.m33 * second.m33;
    return output;
}

inline Point3 TransformPoint(const Transform3& transform, Point3 point) noexcept {
    return {
        point.x * transform.m11 + point.y * transform.m21 +
            point.z * transform.m31 + transform.dx,
        point.x * transform.m12 + point.y * transform.m22 +
            point.z * transform.m32 + transform.dy,
        point.x * transform.m13 + point.y * transform.m23 +
            point.z * transform.m33 + transform.dz};
}

inline Point3 TransformPoint(const Transform3& transform, Point point) noexcept {
    return TransformPoint(transform, Point3{point.x, point.y, 0.0});
}

inline bool Invert(const Transform3& transform, Transform3& inverse) noexcept {
    if (!IsFiniteTransform(transform)) {
        return false;
    }
    const double a = transform.m11;
    const double b = transform.m12;
    const double c = transform.m13;
    const double d = transform.m21;
    const double e = transform.m22;
    const double f = transform.m23;
    const double g = transform.m31;
    const double h = transform.m32;
    const double i = transform.m33;
    const double det =
        a * (e * i - f * h) -
        b * (d * i - f * g) +
        c * (d * h - e * g);
    if (!std::isfinite(det) || std::abs(det) <= 1.0e-12) {
        return false;
    }
    const double invDet = 1.0 / det;
    inverse.m11 = (e * i - f * h) * invDet;
    inverse.m12 = (c * h - b * i) * invDet;
    inverse.m13 = (b * f - c * e) * invDet;
    inverse.m21 = (f * g - d * i) * invDet;
    inverse.m22 = (a * i - c * g) * invDet;
    inverse.m23 = (c * d - a * f) * invDet;
    inverse.m31 = (d * h - e * g) * invDet;
    inverse.m32 = (b * g - a * h) * invDet;
    inverse.m33 = (a * e - b * d) * invDet;
    inverse.dx = -(
        transform.dx * inverse.m11 +
        transform.dy * inverse.m21 +
        transform.dz * inverse.m31);
    inverse.dy = -(
        transform.dx * inverse.m12 +
        transform.dy * inverse.m22 +
        transform.dz * inverse.m32);
    inverse.dz = -(
        transform.dx * inverse.m13 +
        transform.dy * inverse.m23 +
        transform.dz * inverse.m33);
    return IsFiniteTransform(inverse);
}

inline bool Invert(
    const ProjectiveTransform2D& transform,
    ProjectiveTransform2D& inverse) noexcept {
    if (!IsFiniteTransform(transform)) {
        return false;
    }
    const double a = transform.m11;
    const double b = transform.m12;
    const double c = transform.m13;
    const double d = transform.m21;
    const double e = transform.m22;
    const double f = transform.m23;
    const double g = transform.m31;
    const double h = transform.m32;
    const double i = transform.m33;
    const double det =
        a * (e * i - f * h) -
        b * (d * i - f * g) +
        c * (d * h - e * g);
    if (!std::isfinite(det) || std::abs(det) <= 1.0e-12) {
        return false;
    }
    const double invDet = 1.0 / det;
    inverse.m11 = (e * i - f * h) * invDet;
    inverse.m12 = (c * h - b * i) * invDet;
    inverse.m13 = (b * f - c * e) * invDet;
    inverse.m21 = (f * g - d * i) * invDet;
    inverse.m22 = (a * i - c * g) * invDet;
    inverse.m23 = (c * d - a * f) * invDet;
    inverse.m31 = (d * h - e * g) * invDet;
    inverse.m32 = (b * g - a * h) * invDet;
    inverse.m33 = (a * e - b * d) * invDet;
    return IsFiniteTransform(inverse);
}

inline double ClampPerspectiveZ(double z, double depth) noexcept {
    const double limit = std::abs(depth) * PerspectiveZClampFraction;
    if (!(limit > 0.0)) {
        return 0.0;
    }
    if (z > limit) return limit;
    if (z < -limit) return -limit;
    return z;
}

inline Point TransformPoint(
    const ProjectiveTransform2D& transform,
    Point point) noexcept {
    const double x =
        point.x * transform.m11 + point.y * transform.m21 + transform.m31;
    const double y =
        point.x * transform.m12 + point.y * transform.m22 + transform.m32;
    double w =
        point.x * transform.m13 + point.y * transform.m23 + transform.m33;
    if (!std::isfinite(w) || std::abs(w) < ProjectiveWEpsilon) {
        w = w < 0.0 ? -ProjectiveWEpsilon : ProjectiveWEpsilon;
    }
    return {x / w, y / w};
}

inline bool TryTransformPoint(
    const ProjectiveTransform2D& transform,
    Point point,
    Point& output) noexcept {
    const double x =
        point.x * transform.m11 + point.y * transform.m21 + transform.m31;
    const double y =
        point.x * transform.m12 + point.y * transform.m22 + transform.m32;
    const double w =
        point.x * transform.m13 + point.y * transform.m23 + transform.m33;
    if (!std::isfinite(w) || std::abs(w) < ProjectiveWEpsilon ||
        !std::isfinite(x) || !std::isfinite(y)) {
        return false;
    }
    output = {x / w, y / w};
    return std::isfinite(output.x) && std::isfinite(output.y);
}

inline Rect TransformBounds(
    const ProjectiveTransform2D& transform,
    Rect rect) noexcept {
    const Point corners[4] = {
        {rect.x, rect.y},
        {rect.x + rect.width, rect.y},
        {rect.x, rect.y + rect.height},
        {rect.x + rect.width, rect.y + rect.height}};
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;
    bool any = false;
    for (const Point& corner : corners) {
        Point projected{};
        if (!TryTransformPoint(transform, corner, projected)) {
            continue;
        }
        if (!any) {
            left = right = projected.x;
            top = bottom = projected.y;
            any = true;
            continue;
        }
        left = std::min(left, projected.x);
        right = std::max(right, projected.x);
        top = std::min(top, projected.y);
        bottom = std::max(bottom, projected.y);
    }
    if (!any) {
        return {0.0, 0.0, 0.0, 0.0};
    }
    return {
        left,
        top,
        std::max(0.0, right - left),
        std::max(0.0, bottom - top)};
}

// Project a 3D affine mapping of the Z=0 plane through a camera at z=Depth
// looking at the XY plane. Perspective center is `center` (typically
// renderSize/2 plus PerspectiveTransform3D offsets). Z is clamped to
// ±0.95×Depth so w cannot reach 0.
inline ProjectiveTransform2D CollapsePerspective(
    const Transform3& transform,
    double depth,
    Point center) noexcept {
    if (!(std::abs(depth) > 0.0) || !std::isfinite(depth)) {
        depth = DefaultPerspectiveDepth;
    }
    const double d = depth;
    const double cx = center.x;
    const double cy = center.y;
    ProjectiveTransform2D output;
    output.m11 = d * transform.m11 - cx * transform.m13;
    output.m12 = d * transform.m12 - cy * transform.m13;
    output.m13 = -transform.m13;
    output.m21 = d * transform.m21 - cx * transform.m23;
    output.m22 = d * transform.m22 - cy * transform.m23;
    output.m23 = -transform.m23;
    output.m31 = d * transform.dx - cx * transform.dz;
    output.m32 = d * transform.dy - cy * transform.dz;
    output.m33 = d - transform.dz;
    return output;
}

inline Point TransformPointClamped(
    const Transform3& transform,
    Point point,
    double depth,
    Point center) noexcept {
    Point3 world = TransformPoint(transform, point);
    world.z = ClampPerspectiveZ(world.z, depth);
    const double d = std::abs(depth) > 0.0 ? depth : DefaultPerspectiveDepth;
    double w = d - world.z;
    if (!std::isfinite(w) || std::abs(w) < std::abs(d) * (1.0 - PerspectiveZClampFraction)) {
        w = (w < 0.0 ? -1.0 : 1.0) *
            std::abs(d) * (1.0 - PerspectiveZClampFraction);
    }
    const double scale = d / w;
    return {
        (world.x - center.x) * scale + center.x,
        (world.y - center.y) * scale + center.y};
}

// Unproject a parent-space 2D point onto the local Z=0 plane through the
// inverse of a collapsed projective mapping.
inline bool TryUnprojectPointToLocalPlane(
    const ProjectiveTransform2D& localToParent,
    Point parentPoint,
    Point& localPoint) noexcept {
    ProjectiveTransform2D inverse;
    if (!Invert(localToParent, inverse)) {
        return false;
    }
    return TryTransformPoint(inverse, parentPoint, localPoint);
}

inline Transform3 MakeTranslate3(double x, double y, double z = 0.0) noexcept {
    Transform3 output;
    output.dx = x;
    output.dy = y;
    output.dz = z;
    return output;
}

inline Transform3 MakeScale3(double x, double y, double z) noexcept {
    Transform3 output;
    output.m11 = x;
    output.m22 = y;
    output.m33 = z;
    return output;
}

inline Transform3 MakeRotationX(double radians) noexcept {
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    Transform3 output;
    output.m22 = cosine;
    output.m23 = sine;
    output.m32 = -sine;
    output.m33 = cosine;
    return output;
}

inline Transform3 MakeRotationY(double radians) noexcept {
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    Transform3 output;
    output.m11 = cosine;
    output.m13 = -sine;
    output.m31 = sine;
    output.m33 = cosine;
    return output;
}

inline Transform3 MakeRotationZ(double radians) noexcept {
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    Transform3 output;
    output.m11 = cosine;
    output.m12 = sine;
    output.m21 = -sine;
    output.m22 = cosine;
    return output;
}

} // namespace Aero::Base
