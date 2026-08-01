#include <Aero/Base/Vector.hpp>

#include <climits>

#include "D3D11BackendPrivate.hpp"

namespace Aero::Graphics {

#include "D3D11BackendDevice.inc"
#include "D3D11BackendResources.inc"
#include "D3D11BackendCommands1.inc"
#include "D3D11BackendCommands2.inc"
#include "D3D11BackendCommands3.inc"
#include "D3D11BackendReadback.inc"
#include "D3D11BackendSurface.inc"

// D3D11 implementation fragments deliberately share one private translation
// unit so cross-stage SRV hazard tracking, state caching/preservation, capability
// queries, DXBC reflection validation, and device-loss propagation stay
// consistent across commands and swap-chain presentation.

} // namespace Aero::Graphics
