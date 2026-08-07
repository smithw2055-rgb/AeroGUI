#pragma once

// Source-only migration shim. Public code must include <Aero/RenderTarget.hpp>.
// Backend translation units are moved off these spellings incrementally without
// preserving them in the installed SDK.
#include "render/private/RenderTarget.hpp"

namespace Aero {
using RenderSurface = RenderTarget;
using RenderSurfaceKind = RenderTargetKind;
using RenderSurfaceState = RenderTargetState;
}

namespace Aero::Render::Detail {
Base::Result<Base::Ref<Aero::RenderTarget>> AdoptRenderSurface(
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderTarget>> AdoptOwnedRenderSurface(
    Base::Ref<Aero::RenderDevice> device,
    NativeRenderTarget* target,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator = nullptr) noexcept;
}
