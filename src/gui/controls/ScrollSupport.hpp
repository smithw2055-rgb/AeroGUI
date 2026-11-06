#pragma once

#include <Aero/Controls.hpp>
#include <Aero/Value.hpp>
#include <algorithm>
#include <cmath>

namespace Aero::Controls::ScrollSupport {

inline constexpr float LayoutInfinity = 1.0e12f;

inline bool Same(float left, float right) noexcept {
    return std::fabs(left - right) <= 0.000001f;
}

inline bool Same(double left, double right) noexcept {
    return std::fabs(left - right) <= 0.000001;
}

inline bool SameData(
    const ScrollData& left,
    const ScrollData& right) noexcept {
    return Same(left.horizontalOffset, right.horizontalOffset) &&
        Same(left.verticalOffset, right.verticalOffset) &&
        Same(left.extentWidth, right.extentWidth) &&
        Same(left.extentHeight, right.extentHeight) &&
        Same(left.viewportWidth, right.viewportWidth) &&
        Same(left.viewportHeight, right.viewportHeight);
}

inline bool ValidNonnegative(double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

inline bool ValidData(const ScrollData& value) noexcept {
    return ValidNonnegative(value.horizontalOffset) &&
        ValidNonnegative(value.verticalOffset) &&
        ValidNonnegative(value.extentWidth) &&
        ValidNonnegative(value.extentHeight) &&
        ValidNonnegative(value.viewportWidth) &&
        ValidNonnegative(value.viewportHeight);
}

inline Visibility ComputeScrollBarVisibility(
    ScrollBarVisibility mode,
    double extent,
    double viewport) noexcept {
    switch (mode) {
    case ScrollBarVisibility::Visible:
        return Visibility::Visible;
    case ScrollBarVisibility::Auto:
        return extent > viewport + 0.000001
            ? Visibility::Visible
            : Visibility::Collapsed;
    case ScrollBarVisibility::Disabled:
    case ScrollBarVisibility::Hidden:
    default:
        return Visibility::Collapsed;
    }
}

inline double ClampOffset(double offset, double extent, double viewport) noexcept {
    if (!std::isfinite(offset) || offset <= 0.0) {
        return 0.0;
    }
    const double maxOffset = extent > viewport ? extent - viewport : 0.0;
    return offset > maxOffset ? maxOffset : offset;
}

template <typename TProperty>
inline double ReadDouble(
    const DependencyObject& object,
    const TProperty& property,
    double fallback = 0.0) noexcept {
    (void)fallback;
    return object.GetValue(property);
}

template <typename TProperty>
inline bool ReadBool(
    const DependencyObject& object,
    const TProperty& property,
    bool fallback = false) noexcept {
    (void)fallback;
    return object.GetValue(property);
}

template <typename TProperty>
inline Orientation ReadOrientation(
    const DependencyObject& object,
    const TProperty& property) noexcept {
    return object.GetValue(property);
}

template <typename TProperty>
inline void StoreDouble(
    DependencyObject& object,
    const TProperty& property,
    double value) noexcept {
    object.SetValue(property, value);
}

template <typename TProperty>
inline void StoreOrientation(
    DependencyObject& object,
    const TProperty& property,
    Orientation value) noexcept {
    object.SetValue(property, value);
}

} // namespace Aero::Controls::ScrollSupport
