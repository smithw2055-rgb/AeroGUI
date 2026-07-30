#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>

#include <cstdint>
#include <memory>

namespace Aero::Samples::ControlGallery {

enum class GalleryLoadMode : std::uint8_t {
    Runtime = 0U,
    Compiled,
};

enum class GalleryTheme : std::uint8_t {
    Light = 0U,
    Dark,
};

enum class GalleryScenario : std::uint8_t {
    Smoke = 0U,
    Interaction,
    Scroll,
    Batch,
};

struct GallerySnapshot final {
    std::uint64_t planHash = 0U;
    std::uint32_t nodeCount = 0U;
    std::uint32_t commandCount = 0U;
    std::uint32_t textCommandCount = 0U;
    std::uint32_t drawPacketCount = 0U;
    std::uint32_t batchCount = 0U;
    std::uint32_t drawCallCount = 0U;
    std::uint32_t mergedPacketCount = 0U;
    std::uint32_t barrierCount = 0U;
    std::uint32_t instanceCount = 0U;
    std::uint32_t stateBindingCount = 0U;
    std::uint64_t layoutPassVersion = 0U;
    std::uint32_t measuredCount = 0U;
    std::uint32_t arrangedCount = 0U;
    std::uint32_t pendingMeasureCount = 0U;
    std::uint32_t pendingArrangeCount = 0U;
    std::uint32_t namedObjectCount = 0U;
    std::uint32_t itemCount = 0U;
    std::uint32_t realizedItemCount = 0U;
    std::uint32_t createdContainerCount = 0U;
    std::uint32_t recycledContainerUseCount = 0U;
    std::uint32_t executedFrameCount = 0U;
    GalleryLoadMode loadMode = GalleryLoadMode::Runtime;
    GalleryTheme theme = GalleryTheme::Light;
    GalleryScenario scenario = GalleryScenario::Smoke;
    bool batchingEnabled = true;
};

class GalleryRuntime final {
public:
    GalleryRuntime() noexcept;
    ~GalleryRuntime();

    GalleryRuntime(const GalleryRuntime&) = delete;
    GalleryRuntime& operator=(const GalleryRuntime&) = delete;

    Base::Result<void> Initialize(
        Base::StringView assetDirectory,
        GalleryLoadMode loadMode,
        GalleryTheme theme)
        noexcept;
    Base::Result<void> RunScenario(
        GalleryScenario scenario,
        std::uint32_t frameCount) noexcept;
    Base::Result<void> SelectPage(
        Base::StringView page) noexcept;
    Base::Result<void>
        SetBatchingEnabledForTesting(
            bool enabled) noexcept;
    void Shutdown() noexcept;

    const GallerySnapshot& Snapshot() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Aero::Samples::ControlGallery
