#include <Aero/Base/Vector.hpp>

#include <climits>

#include "D3D11BackendPrivate.hpp"

namespace Aero::Rhi {

#include "D3D11BackendDevice.inc"
#include "D3D11BackendResources.inc"
#include "D3D11BackendCommands1.inc"
#include "D3D11BackendCommands2.inc"
#include "D3D11BackendCommands3.inc"
#include "D3D11BackendReadback.inc"
#include "D3D11BackendSurface.inc"

} // namespace Aero::Rhi
