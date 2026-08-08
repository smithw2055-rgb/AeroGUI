#pragma once

#include "../FrameEncoder.hpp"

namespace Aero::Render {

// OpenGL 3.3 GLSL catalog consumed by the backend-neutral Renderer.
BackendShaderCatalog MakeOpenGL33BackendShaderCatalog() noexcept;
Graphics::NativePipelineState MakeOpenGL33UiPipeline(
    Detail::UiPipelineKey key) noexcept;

} // namespace Aero::Render
