#include <Aero/DrawingContext.hpp>

#include "DrawingInternals.hpp"

namespace Aero {

Base::Result<void> DrawingContext::PushClip(
    Base::Rect clip) noexcept {
    return Internal::DrawingPrivate::Builder(*this)
        .PushClip(clip);
}

Base::Result<void> DrawingContext::PopClip() noexcept {
    return Internal::DrawingPrivate::Builder(*this)
        .PopClip();
}

Base::Result<void> DrawingContext::PushOpacity(
    double opacity) noexcept {
    return Internal::DrawingPrivate::Builder(*this)
        .PushOpacity(opacity);
}

Base::Result<void> DrawingContext::PopOpacity() noexcept {
    return Internal::DrawingPrivate::Builder(*this)
        .PopOpacity();
}

Base::Result<void> DrawingContext::PushTransform(
    Base::Transform2D transform) noexcept {
    return Internal::DrawingPrivate::Builder(*this)
        .PushTransform(transform);
}

Base::Result<void> DrawingContext::PopTransform() noexcept {
    return Internal::DrawingPrivate::Builder(*this)
        .PopTransform();
}

Base::Result<void> DrawingContext::DrawRectangle(
    Base::Rect bounds,
    Base::Color color) noexcept {
    return Internal::DrawingPrivate::Builder(*this)
        .FillRect(bounds, color);
}

Base::Result<void> DrawingContext::DrawRoundedRectangle(
    Base::Rect bounds,
    Base::Color color,
    double radius) noexcept {
    return Internal::DrawingPrivate::Builder(*this)
        .FillRoundedRect(bounds, color, radius);
}

Base::Result<void> DrawingContext::DrawRectangleOutline(
    Base::Rect bounds,
    Base::Color color,
    double thickness) noexcept {
    return Internal::DrawingPrivate::Builder(*this)
        .StrokeRect(bounds, color, thickness);
}

} // namespace Aero
