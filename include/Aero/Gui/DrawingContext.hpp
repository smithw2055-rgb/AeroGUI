#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>

namespace Aero::Media {
class Brush;

// WPF-facing retained drawing surface used by FrameworkElement::OnRender().
// The context records semantic drawing operations; render plans, resource IDs
// and backend command streams remain private runtime implementation.
class AERO_API DrawingContext {
public:
    struct Impl;

    DrawingContext(const DrawingContext&) = delete;
    DrawingContext& operator=(const DrawingContext&) = delete;

    Base::Result<void> PushClip(Base::Rect clip) noexcept;
    Base::Result<void> PopClip() noexcept;
    Base::Result<void> PushOpacity(double opacity) noexcept;
    Base::Result<void> PopOpacity() noexcept;
    Base::Result<void> PushTransform(
        Base::Transform2D transform) noexcept;
    Base::Result<void> PopTransform() noexcept;

    Base::Result<void> DrawRectangle(
        Base::Rect bounds,
        Base::Color color) noexcept;
    Base::Result<void> DrawRectangle(
        Base::Rect bounds,
        const Base::Ref<Brush>& brush) noexcept;
    Base::Result<void> DrawRectangle(
        const Base::Ref<Brush>& fill,
        const Base::Ref<Brush>& stroke,
        Base::Rect bounds,
        double strokeThickness = 1.0) noexcept;
    Base::Result<void> DrawRoundedRectangle(
        Base::Rect bounds,
        Base::Color color,
        double radius) noexcept;
    Base::Result<void> DrawRoundedRectangle(
        Base::Rect bounds,
        const Base::Ref<Brush>& brush,
        double radius) noexcept;
    Base::Result<void> DrawRectangleOutline(
        Base::Rect bounds,
        Base::Color color,
        double thickness) noexcept;
    Base::Result<void> DrawRectangleOutline(
        Base::Rect bounds,
        const Base::Ref<Brush>& brush,
        double thickness) noexcept;

private:
    friend struct Impl;

    explicit DrawingContext(void* implementation) noexcept
        : implementation_(implementation) {}

    void* implementation_ = nullptr;
};

} // namespace Aero::Media
