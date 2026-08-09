#pragma once

#include <AeroRender/RenderDevice.hpp>

namespace Aero::Render {

// Source-private frame-host maintenance. This keeps resource retirement out
// of the public RenderDevice contract while allowing RenderContext to release
// external swap-chain references before ResizeBuffers().
AERO_GUI_INTERNAL_API Base::Result<std::uint32_t> CollectDeviceGarbage(
    Aero::RenderDevice& device) noexcept;

} // namespace Aero::Render
