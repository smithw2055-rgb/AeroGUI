#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>

namespace Aero::Media {
class Brush;
class Pen;
class Geometry;
struct DrawingContextRuntime;

// WPF-facing retained drawing surface used by FrameworkElement::OnRender().
// The context records semantic drawing operations; render plans, resource IDs
// and backend command streams remain private runtime implementation.
class AERO_GUI_API DrawingContext {
public:

    DrawingContext(const DrawingContext&) = delete;
    DrawingContext& operator=(const DrawingContext&) = delete;

    Result<void> PushClip(Base::Rect clip) noexcept;
    Result<void> PopClip() noexcept;
    Result<void> PushOpacity(double opacity) noexcept;
    Result<void> PopOpacity() noexcept;
    Result<void> PushTransform(
        Base::Transform2D transform) noexcept;
    Result<void> PopTransform() noexcept;

    Result<void> DrawRectangle(
        Base::Rect bounds,
        Base::Color color) noexcept;
    Result<void> DrawRectangle(
        Base::Rect bounds,
        const Ref<Brush>& brush) noexcept;
    Result<void> DrawRectangle(
        const Ref<Brush>& fill,
        const Ref<Brush>& stroke,
        Base::Rect bounds,
        double strokeThickness = 1.0) noexcept;
    Result<void> DrawRoundedRectangle(
        Base::Rect bounds,
        Base::Color color,
        double radius) noexcept;
    Result<void> DrawRoundedRectangle(
        Base::Rect bounds,
        const Ref<Brush>& brush,
        double radius) noexcept;
    Result<void> DrawRectangleOutline(
        Base::Rect bounds,
        Base::Color color,
        double thickness) noexcept;
    Result<void> DrawRectangleOutline(
        Base::Rect bounds,
        const Ref<Brush>& brush,
        double thickness) noexcept;
    Result<void> DrawLine(
        const Ref<Pen>& pen,
        Base::Point start,
        Base::Point end) noexcept;
    Result<void> DrawGeometry(
        const Ref<Brush>& brush,
        const Ref<Pen>& pen,
        const Geometry& geometry) noexcept;

private:
    friend struct DrawingContextRuntime;

    explicit DrawingContext(void* implementation) noexcept
        : implementation_(implementation) {}

    void* implementation_ = nullptr;
};

} // namespace Aero::Media
