#pragma once

#include "../FrameEncoder.hpp"

namespace Aero::Render {

// OpenGL 3.3 GLSL catalog consumed by the backend-neutral Renderer.
FrameShaderSet MakeOpenGL33FrameShaderSet() noexcept;

} // namespace Aero::Render
