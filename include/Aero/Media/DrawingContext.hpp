#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>

namespace Aero::Media {
class Brush;
class Pen;
class Geometry;

// WPF-facing retained drawing surface used by FrameworkElement::OnRender().
// The context records semantic drawing operations; render plans, resource IDs
// and backend command streams remain private runtime implementation.
class AERO_GUI_API DrawingContext {
public:

    DrawingContext(const DrawingContext&) = delete;
    DrawingContext& operator=(const DrawingContext&) = delete;

    // Nested bridge for DisplayListBuilder (TU-local definition). Not a
    // separate public companion type.
    struct Bridge;

    void PushClip(Base::Rect clip) noexcept;
    void PopClip() noexcept;
    void PushOpacity(double opacity) noexcept;
    void PopOpacity() noexcept;
    void PushTransform(Base::Transform2D transform) noexcept;
    void PopTransform() noexcept;

    void DrawRectangle(Base::Rect bounds, Base::Color color) noexcept;
    void DrawRectangle(Base::Rect bounds, const Ref<Brush>& brush) noexcept;
    void DrawRectangle(const Ref<Brush>& fill, const Ref<Brush>& stroke, Base::Rect bounds,
        double strokeThickness = 1.0) noexcept;
    void DrawRoundedRectangle(Base::Rect bounds, Base::Color color, double radius) noexcept;
    void DrawRoundedRectangle(Base::Rect bounds, const Ref<Brush>& brush, double radius) noexcept;
    void DrawRectangleOutline(Base::Rect bounds, Base::Color color, double thickness) noexcept;
    void DrawRectangleOutline(Base::Rect bounds, const Ref<Brush>& brush, double thickness) noexcept;
    void DrawLine(const Ref<Pen>& pen, Base::Point start, Base::Point end) noexcept;
    void DrawGeometry(const Ref<Brush>& brush, const Ref<Pen>& pen, const Geometry& geometry) noexcept;

private:
    friend struct Bridge;

    explicit DrawingContext(void* implementation) noexcept : implementation_(implementation) {}

    void* implementation_ = nullptr;
};

} // namespace Aero::Media
