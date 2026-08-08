#pragma once

#include <Aero/Base/Config.hpp>

#include <cstdint>

namespace Aero {
class RenderDevice;

namespace Diagnostics {

struct RenderDeviceStatistics {
    std::uint64_t acceptedFrameCount = 0U;
    std::uint64_t completedFrameCount = 0U;
    std::uint64_t failedFrameCount = 0U;
    std::uint64_t lastAcceptedVersion = 0U;
    std::uint64_t lastCompletedVersion = 0U;
    std::uint64_t generation = 1U;
};

struct RenderFrameStatistics {
    std::uint32_t sourceCommandCount = 0U;
    std::uint32_t drawPacketCount = 0U;
    std::uint32_t batchCount = 0U;
    std::uint32_t drawCallCount = 0U;
    std::uint32_t mergedPacketCount = 0U;
    std::uint32_t barrierCount = 0U;
    std::uint32_t instanceCount = 0U;
    std::uint32_t stateBindingCount = 0U;
    bool batchingEnabled = true;
};

AERO_GUI_API RenderDeviceStatistics GetRenderDeviceStatistics(
    const Aero::RenderDevice& device) noexcept;
AERO_GUI_API RenderFrameStatistics GetLastRenderFrameStatistics(
    const Aero::RenderDevice& device) noexcept;

} // namespace Diagnostics
} // namespace Aero
