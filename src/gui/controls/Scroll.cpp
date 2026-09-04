#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp" 
#include "gui/input/InputState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include "gui/media/MediaState.hpp"
#include <Aero/Input/Mouse.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Value.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include "ControlBehavior.hpp"
#include "gui/templates/TemplateState.hpp"

namespace Aero::Controls {
using Aero::Controls::ScrollBehavior;
using Aero::Controls::SliderBehavior;

using namespace Primitives;
using namespace ::Aero::Render;
namespace {

constexpr double LayoutInfinity = 1.0e12;

bool Same(double left, double right) noexcept {
    return std::fabs(left - right) <= 0.000001;
}

bool SameData(
    const ScrollData& left,
    const ScrollData& right) noexcept {
    return Same(left.horizontalOffset, right.horizontalOffset) &&
        Same(left.verticalOffset, right.verticalOffset) &&
        Same(left.extentWidth, right.extentWidth) &&
        Same(left.extentHeight, right.extentHeight) &&
        Same(left.viewportWidth, right.viewportWidth) &&
        Same(left.viewportHeight, right.viewportHeight);
}

bool ValidNonnegative(double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

bool ValidData(const ScrollData& value) noexcept {
    return ValidNonnegative(value.horizontalOffset) &&
        ValidNonnegative(value.verticalOffset) &&
        ValidNonnegative(value.extentWidth) &&
        ValidNonnegative(value.extentHeight) &&
        ValidNonnegative(value.viewportWidth) &&
        ValidNonnegative(value.viewportHeight);
}

Visibility ComputeScrollBarVisibility(
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

double ClampOffset(
    double value,
    double extent,
    double viewport,
    bool enabled) noexcept {
    if (!enabled) return 0.0;
    return std::clamp(
        value, 0.0, std::max(0.0, extent - viewport));
}

template <typename TProperty>
double ReadDouble(
    const DependencyObject& object,
    const TProperty& property,
    double fallback = 0.0) noexcept {
    return object.GetValueOr(property, fallback);
}

template <typename TProperty>
bool ReadBool(
    const DependencyObject& object,
    const TProperty& property,
    bool fallback) noexcept {
    return object.GetValueOr(property, fallback);
}

template <typename TProperty>
Orientation ReadOrientation(
    const DependencyObject& object,
    const TProperty& property) noexcept {
    return object.GetValueOr(
        property, Orientation::Vertical);
}

template <typename TProperty>
void StoreDouble(
    DependencyObject& object,
    const TProperty& property,
    double value) noexcept {
    object.SetValue(property, value);
}

template <typename TProperty>
void StoreOrientation(
    DependencyObject& object,
    const TProperty& property,
    Orientation value) noexcept {
    object.SetValue(property, value);
}

} // namespace


#include "ScrollContentPresenter.inl"
#include "ScrollViewer.inl"
#include "ScrollBar.inl"
#include "ScrollBehavior.inl"
