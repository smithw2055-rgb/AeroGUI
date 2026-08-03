#include <Aero/DrawingContext.hpp>

#include "render/RenderPrivate.hpp"

namespace Aero {

Base::Result<void> DrawingContext::PushClip(
    Base::Rect clip) noexcept {
    return ::Aero::Render::Detail::DrawingPrivate::Builder(*this)
        .PushClip(clip);
}

Base::Result<void> DrawingContext::PopClip() noexcept {
    return ::Aero::Render::Detail::DrawingPrivate::Builder(*this)
        .PopClip();
}

Base::Result<void> DrawingContext::PushOpacity(
    double opacity) noexcept {
    return ::Aero::Render::Detail::DrawingPrivate::Builder(*this)
        .PushOpacity(opacity);
}

Base::Result<void> DrawingContext::PopOpacity() noexcept {
    return ::Aero::Render::Detail::DrawingPrivate::Builder(*this)
        .PopOpacity();
}

Base::Result<void> DrawingContext::PushTransform(
    Base::Transform2D transform) noexcept {
    return ::Aero::Render::Detail::DrawingPrivate::Builder(*this)
        .PushTransform(transform);
}

Base::Result<void> DrawingContext::PopTransform() noexcept {
    return ::Aero::Render::Detail::DrawingPrivate::Builder(*this)
        .PopTransform();
}

Base::Result<void> DrawingContext::DrawRectangle(
    Base::Rect bounds,
    Base::Color color) noexcept {
    return ::Aero::Render::Detail::DrawingPrivate::Builder(*this)
        .FillRect(bounds, color);
}

Base::Result<void> DrawingContext::DrawRoundedRectangle(
    Base::Rect bounds,
    Base::Color color,
    double radius) noexcept {
    return ::Aero::Render::Detail::DrawingPrivate::Builder(*this)
        .FillRoundedRect(bounds, color, radius);
}

Base::Result<void> DrawingContext::DrawRectangleOutline(
    Base::Rect bounds,
    Base::Color color,
    double thickness) noexcept {
    return ::Aero::Render::Detail::DrawingPrivate::Builder(*this)
        .StrokeRect(bounds, color, thickness);
}

} // namespace Aero
