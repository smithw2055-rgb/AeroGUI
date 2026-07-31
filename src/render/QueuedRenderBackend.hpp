#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/FrameworkElement.hpp>

#include <cstdint>

namespace Aero::Render {

enum class FrameQueueFullPolicy : std::uint8_t {
    Reject = 0U,
    DropOldest,
};

struct FrameQueueStatistics final {
    std::uint64_t accepted = 0U;
    std::uint64_t consumed = 0U;
    std::uint64_t dropped = 0U;
    std::uint64_t rejected = 0U;
    std::uint32_t failed = 0U;
    std::uint32_t pending = 0U;
    std::uint32_t highWatermark = 0U;
};

class AERO_API QueuedRenderBackend final : public IRenderBackend {
public:
    explicit QueuedRenderBackend(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~QueuedRenderBackend() noexcept override;

    QueuedRenderBackend(const QueuedRenderBackend&) = delete;
    QueuedRenderBackend& operator=(const QueuedRenderBackend&) = delete;

    Base::Result<void> Initialize(
        IRenderBackend& downstream,
        std::uint32_t capacity = 3U,
        FrameQueueFullPolicy policy =
            FrameQueueFullPolicy::DropOldest) noexcept;
    void Shutdown() noexcept;

    Base::Result<void> Submit(
        const RenderPlan& plan) noexcept override;
    Base::Result<bool> ConsumeOne() noexcept;
    Base::Result<std::uint32_t> Drain() noexcept;

    bool IsInitialized() const noexcept;
    FrameQueueStatistics Statistics() const noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Render
