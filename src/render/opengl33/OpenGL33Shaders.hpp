#pragma once

#include "../FrameEncoder.hpp"

namespace Aero::Render {

// OpenGL 3.3 GLSL catalog selected from UiFrameEncoder pipeline keys.
BackendShaderCatalog MakeOpenGL33BackendShaderCatalog() noexcept;
Graphics::NativePipelineState MakeOpenGL33UiPipeline(
    ::Aero::Render::UiPipelineKey key) noexcept;

} // namespace Aero::Render
