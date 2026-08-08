#pragma once

#include "../FrameEncoder.hpp"

namespace Aero::Render {

// D3D11 shader bytecode catalog consumed by the backend-neutral Renderer.
BackendShaderCatalog MakeD3D11BackendShaderCatalog() noexcept;
Graphics::NativePipelineState MakeD3D11UiPipeline(
    Detail::UiPipelineKey key) noexcept;

} // namespace Aero::Render
