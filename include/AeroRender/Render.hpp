#pragma once

#include <Aero/IRenderer.hpp>
#include <AeroRender/RenderDevice.hpp>
#include <AeroRender/RenderTarget.hpp>

// Backend factories remain explicit opt-ins through D3D11.hpp or
// OpenGL33.hpp. This umbrella contains only backend-neutral render contracts.
