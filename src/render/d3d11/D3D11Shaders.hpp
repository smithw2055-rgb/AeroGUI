#pragma once

#include "../FrameEncoder.hpp"

namespace Aero::Render {

// D3D11 shader bytecode catalog consumed by the backend-neutral DeviceRenderer.
FrameShaderSet MakeD3D11FrameShaderSet() noexcept;

} // namespace Aero::Render
