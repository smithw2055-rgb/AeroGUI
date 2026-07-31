#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>

namespace Aero::Render {
class RenderManager;
}
namespace Aero::Detail {
class DrawingContextAccess;
}

namespace Aero {

// WPF-facing retained drawing surface used by FrameworkElement::OnRender().
// The context records semantic drawing operations; render plans, resource IDs
// and backend command streams remain private runtime implementation.
class AERO_API DrawingContext final {
public:
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
    Base::Result<void> DrawRoundedRectangle(
        Base::Rect bounds,
        Base::Color color,
        double radius) noexcept;
    Base::Result<void> DrawRectangleOutline(
        Base::Rect bounds,
        Base::Color color,
        double thickness) noexcept;

private:
    friend class ::Aero::Render::RenderManager;
    friend class ::Aero::Detail::DrawingContextAccess;

    explicit DrawingContext(void* implementation) noexcept
        : implementation_(implementation) {}

    void* implementation_ = nullptr;
};

} // namespace Aero
