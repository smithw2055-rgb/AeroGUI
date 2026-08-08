#include <Aero/Base/Vector.hpp>

#include <climits>

#include "D3D11RenderDeviceState.hpp"
#include "render/RenderDeviceState.hpp"
#include "render/d3d11/D3D11Shaders.hpp"

namespace Aero::Graphics {

#include "D3D11RenderDeviceCore.inc"
#include "D3D11RenderDeviceResources.inc"
#include "D3D11RenderDeviceDraw1.inc"
#include "D3D11RenderDeviceDraw2.inc"
#include "D3D11RenderDeviceDraw3.inc"
#include "D3D11RenderDeviceReadback.inc"

// D3D11 implementation fragments deliberately share one private translation
// unit so cross-stage SRV hazard tracking, state caching/preservation, capability
// queries, DXBC reflection validation, and device-loss propagation stay
// consistent across resource creation and direct draw submission.

} // namespace Aero::Graphics
