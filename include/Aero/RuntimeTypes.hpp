#pragma once

#include <Aero/Media/Geometry.hpp>
#include <Aero/Input/Values.hpp>

#include <cstdint>

namespace Aero {

enum class BuiltInTheme : std::uint8_t {
    Light = 0U,
    Dark
};

enum class RuntimeResourceLayer : std::uint8_t {
    Application = 0U,
    Theme,
    System
};

enum class RuntimeResourceLoadMode : std::uint8_t {
    Replace = 0U,
    Merge
};

struct ViewLayoutDiagnostics final {
    std::uint64_t passVersion = 0U;
    std::uint32_t measuredCount = 0U;
    std::uint32_t arrangedCount = 0U;
    std::uint32_t pendingMeasureCount = 0U;
    std::uint32_t pendingArrangeCount = 0U;
};

struct ViewRenderDiagnostics final {
    std::uint64_t snapshotVersion = 0U;
    std::uint32_t nodeCount = 0U;
    std::uint32_t commandCount = 0U;
    std::uint32_t glyphCommandCount = 0U;
    std::uint32_t dirtyCount = 0U;
    std::uint64_t snapshotHash = 0U;
    std::uint32_t drawPacketCount = 0U;
    std::uint32_t batchCount = 0U;
    std::uint32_t drawCallCount = 0U;
    std::uint32_t mergedPacketCount = 0U;
    std::uint32_t barrierCount = 0U;
    std::uint32_t instanceCount = 0U;
    std::uint32_t stateBindingCount = 0U;
    bool batchingEnabled = true;
};

struct ViewFrameResult final {
    std::uint64_t frameNumber = 0U;
    std::uint32_t callbackCount = 0U;
    ViewLayoutDiagnostics layout;
    ViewRenderDiagnostics render;
};

} // namespace Aero
