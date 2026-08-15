#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <AeroApp/App.hpp>
#include <AeroRender/WindowInterop.hpp>
#include "render/RenderContext.hpp"

#include <cstdint>

namespace Aero::App {

// Creates the concrete desktop context selected by RunOptions. Ownership of a
// successful result transfers to the caller. The dispatch lives in App because
// App is the single composition unit that links the opt-in native backends.
Base::Result<Render::RenderContext*> CreateRenderContext(
    GraphicsBackend backend,
    Platform::NativeWindowHandle window,
    std::uint32_t width,
    std::uint32_t height,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::App