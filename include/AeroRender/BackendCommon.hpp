#pragma once

// Shared backend factory vocabulary. D3D11 and OpenGL33 keep their own
// DeviceOptions/EmbeddedTarget/TargetOptions spellings for host code, but the
// state-preservation policy is identical and owned here so the two opt-in
// headers cannot drift.
#include <Aero/Base/Config.hpp>

#include <cstdint>

namespace Aero::Render {

enum class StatePreservationPolicy : std::uint8_t {
    HostResetsState = 0U,
    PreserveRequiredState
};

} // namespace Aero::Render
