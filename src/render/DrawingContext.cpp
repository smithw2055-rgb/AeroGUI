#include <Aero/Gui/DrawingContext.hpp>

#include "DisplayList.hpp"
#include "../media/BrushRendering.hpp"
#include "gui/MetadataInternal.hpp"
#include "gui/PropertyInternal.hpp"
#include "gui/FreezableInternal.hpp"
#include "gui/ElementInternal.hpp"
#include "gui/RoutedEventInternal.hpp"
#include "gui/InputInternal.hpp"
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/AnimationInternal.hpp"
#include "gui/StyleInternal.hpp"
#include "media/AnimationInternal.hpp"
#include "media/BrushInternal.hpp"
#include "media/EffectInternal.hpp"
#include "media/TransformInternal.hpp"

namespace Aero::Media {

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

Base::Result<void> DrawingContext::DrawRectangle(
    Base::Rect bounds,
    const Base::Ref<Media::Brush>& brush) noexcept {
    return Media::PaintBrushRect(
        ::Aero::Render::Detail::DrawingPrivate::Builder(*this),
        brush,
        bounds);
}

Base::Result<void> DrawingContext::DrawRectangle(
    const Base::Ref<Media::Brush>& fill,
    const Base::Ref<Media::Brush>& stroke,
    Base::Rect bounds,
    double strokeThickness) noexcept {
    Base::Result<void> result = DrawRectangle(bounds, fill);
    if (!result || !stroke || strokeThickness <= 0.0) {
        return result;
    }
    return DrawRectangleOutline(
        bounds, stroke, strokeThickness);
}

Base::Result<void> DrawingContext::DrawRoundedRectangle(
    Base::Rect bounds,
    Base::Color color,
    double radius) noexcept {
    return ::Aero::Render::Detail::DrawingPrivate::Builder(*this)
        .FillRoundedRect(bounds, color, radius);
}

Base::Result<void> DrawingContext::DrawRoundedRectangle(
    Base::Rect bounds,
    const Base::Ref<Media::Brush>& brush,
    double radius) noexcept {
    return Media::PaintBrushRect(
        ::Aero::Render::Detail::DrawingPrivate::Builder(*this),
        brush,
        bounds,
        radius);
}

Base::Result<void> DrawingContext::DrawRectangleOutline(
    Base::Rect bounds,
    Base::Color color,
    double thickness) noexcept {
    return ::Aero::Render::Detail::DrawingPrivate::Builder(*this)
        .StrokeRect(bounds, color, thickness);
}

Base::Result<void> DrawingContext::DrawRectangleOutline(
    Base::Rect bounds,
    const Base::Ref<Media::Brush>& brush,
    double thickness) noexcept {
    const Base::Color color =
        Media::Detail::SampleBrush(brush);
    return color.alpha > 0.0F
        ? ::Aero::Render::Detail::DrawingPrivate::Builder(*this)
              .StrokeRect(bounds, color, thickness)
        : Base::Result<void>();
}

} // namespace Aero::Media
