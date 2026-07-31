#include <Aero/DrawingContext.hpp>

#include "DrawingContextAccess.hpp"

namespace Aero {

Base::Result<void> DrawingContext::PushClip(
    Base::Rect clip) noexcept {
    return Detail::DrawingContextAccess::Builder(*this)
        .PushClip(clip);
}

Base::Result<void> DrawingContext::PopClip() noexcept {
    return Detail::DrawingContextAccess::Builder(*this)
        .PopClip();
}

Base::Result<void> DrawingContext::PushOpacity(
    double opacity) noexcept {
    return Detail::DrawingContextAccess::Builder(*this)
        .PushOpacity(opacity);
}

Base::Result<void> DrawingContext::PopOpacity() noexcept {
    return Detail::DrawingContextAccess::Builder(*this)
        .PopOpacity();
}

Base::Result<void> DrawingContext::PushTransform(
    Base::Transform2D transform) noexcept {
    return Detail::DrawingContextAccess::Builder(*this)
        .PushTransform(transform);
}

Base::Result<void> DrawingContext::PopTransform() noexcept {
    return Detail::DrawingContextAccess::Builder(*this)
        .PopTransform();
}

Base::Result<void> DrawingContext::DrawRectangle(
    Base::Rect bounds,
    Base::Color color) noexcept {
    return Detail::DrawingContextAccess::Builder(*this)
        .FillRect(bounds, color);
}

Base::Result<void> DrawingContext::DrawRoundedRectangle(
    Base::Rect bounds,
    Base::Color color,
    double radius) noexcept {
    return Detail::DrawingContextAccess::Builder(*this)
        .FillRoundedRect(bounds, color, radius);
}

Base::Result<void> DrawingContext::DrawRectangleOutline(
    Base::Rect bounds,
    Base::Color color,
    double thickness) noexcept {
    return Detail::DrawingContextAccess::Builder(*this)
        .StrokeRect(bounds, color, thickness);
}

} // namespace Aero
