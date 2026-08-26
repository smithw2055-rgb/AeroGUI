#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>

namespace Aero {

class View;
struct ViewState;

// Global frame notification matching WPF CompositionTarget.Rendering. Hosts
// still own the frame clock through View::Update; subscribers use this event to
// invalidate custom visuals immediately before retained render commit.
using RenderingEventHandler = Base::Delegate<void()>;

namespace Media {

class AERO_GUI_API CompositionTarget final {
public:
    // Explicit View overloads are preferred for multi-view hosts. The legacy
    // overloads remain dispatcher-thread scoped for WPF-shaped source
    // compatibility.
    static void AddRendering(
        ::Aero::View& view,
        const ::Aero::RenderingEventHandler& handler) noexcept;
    static bool RemoveRendering(
        ::Aero::View& view,
        const ::Aero::RenderingEventHandler& handler) noexcept;
    static void AddRendering(
        const ::Aero::RenderingEventHandler& handler) noexcept;
    static bool RemoveRendering(
        const ::Aero::RenderingEventHandler& handler) noexcept;

private:
    friend class ::Aero::View;
    friend struct ::Aero::ViewState;
    static void RaiseRendering(::Aero::View& view) noexcept;
};

} // namespace Media

} // namespace Aero
