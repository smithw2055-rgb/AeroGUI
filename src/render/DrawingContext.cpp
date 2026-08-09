#include <Aero/Media/DrawingContext.hpp>

#include "DisplayList.hpp"
#include "gui/media/BrushRendering.hpp"
#include "gui/MetadataRuntime.hpp"
#include "gui/PropertyRuntime.hpp"
#include "gui/FreezableRuntime.hpp"
#include "gui/ElementRuntime.hpp"
#include "gui/RoutedEventRuntime.hpp"
#include "gui/InputRuntime.hpp"
#include "gui/LayoutRuntime.hpp"
#include "gui/BindingRuntime.hpp"
#include "gui/AnimationRuntime.hpp"
#include "gui/StyleRuntime.hpp"
#include "gui/media/AnimationRuntime.hpp"
#include "gui/media/BrushRuntime.hpp"
#include "gui/media/EffectRuntime.hpp"
#include "gui/media/TransformRuntime.hpp"

namespace Aero::Media {

Base::Result<void> DrawingContext::PushClip(
    Base::Rect clip) noexcept {
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .PushClip(clip);
}

Base::Result<void> DrawingContext::PopClip() noexcept {
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .PopClip();
}

Base::Result<void> DrawingContext::PushOpacity(
    double opacity) noexcept {
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .PushOpacity(opacity);
}

Base::Result<void> DrawingContext::PopOpacity() noexcept {
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .PopOpacity();
}

Base::Result<void> DrawingContext::PushTransform(
    Base::Transform2D transform) noexcept {
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .PushTransform(transform);
}

Base::Result<void> DrawingContext::PopTransform() noexcept {
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .PopTransform();
}

Base::Result<void> DrawingContext::DrawRectangle(
    Base::Rect bounds,
    Base::Color color) noexcept {
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .FillRect(bounds, color);
}

Base::Result<void> DrawingContext::DrawRectangle(
    Base::Rect bounds,
    const Base::Ref<Media::Brush>& brush) noexcept {
    return Media::PaintBrushRect(
        ::Aero::Render::DrawingPrivate::Builder(*this),
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
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .FillRoundedRect(bounds, color, radius);
}

Base::Result<void> DrawingContext::DrawRoundedRectangle(
    Base::Rect bounds,
    const Base::Ref<Media::Brush>& brush,
    double radius) noexcept {
    return Media::PaintBrushRect(
        ::Aero::Render::DrawingPrivate::Builder(*this),
        brush,
        bounds,
        radius);
}

Base::Result<void> DrawingContext::DrawRectangleOutline(
    Base::Rect bounds,
    Base::Color color,
    double thickness) noexcept {
    return ::Aero::Render::DrawingPrivate::Builder(*this)
        .StrokeRect(bounds, color, thickness);
}

Base::Result<void> DrawingContext::DrawRectangleOutline(
    Base::Rect bounds,
    const Base::Ref<Media::Brush>& brush,
    double thickness) noexcept {
    const Base::Color color =
        Media::SampleBrush(brush);
    return color.alpha > 0.0F
        ? ::Aero::Render::DrawingPrivate::Builder(*this)
              .StrokeRect(bounds, color, thickness)
        : Base::Result<void>();
}

} // namespace Aero::Media
