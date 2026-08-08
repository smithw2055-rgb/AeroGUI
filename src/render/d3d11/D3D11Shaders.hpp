#pragma once

#include "../FrameEncoder.hpp"

namespace Aero::Render {

// D3D11 shader bytecode catalog selected from UiFrameEncoder pipeline keys.
BackendShaderCatalog MakeD3D11BackendShaderCatalog() noexcept;
Graphics::NativePipelineState MakeD3D11UiPipeline(
    ::Aero::Render::UiPipelineKey key) noexcept;

} // namespace Aero::Render
