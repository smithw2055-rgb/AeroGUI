#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Rhi/Rhi.hpp>

#include <cmath>
#include <cstdint>

namespace Aero::Render {

enum class RenderEffectKind : std::uint8_t {
    None = 0U,
    Opacity,
    Blur,
    DropShadow,
    ColorMatrix,
};

enum class RenderMaskKind : std::uint8_t {
    None = 0U,
    Rectangle,
    RoundedRectangle,
    Geometry,
    AlphaTexture,
};

struct ProductionPassDescriptor final {
    std::uint64_t id = 0U;
    std::uint64_t parent = 0U;
    Presentation::Rect bounds;
    RenderEffectKind effect = RenderEffectKind::None;
    RenderMaskKind mask = RenderMaskKind::None;
    std::uint32_t sampleCount = 1U;
    bool offscreen = false;
};

// Immutable cross-thread packet copied from the UI-thread RenderPlan. No UI
// object pointer is retained; packets can safely live in the M4 render queue.
class AERO_API FrozenRenderPacket final {
public:
    explicit FrozenRenderPacket(
        Base::IAllocator* allocator = nullptr) noexcept
        : nodes_(allocator), commands_(allocator) {}

    Base::Result<void> Freeze(
        const Presentation::RenderPlan& plan,
        std::uint64_t resourceGeneration = 1U) noexcept {
        if (resourceGeneration == 0U) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Render packet resource generation must be non-zero");
        }
        Base::Vector<Presentation::RenderNodeSnapshot> nodes(
            &nodes_.Allocator());
        Base::Vector<Presentation::RenderCommand> commands(
            &commands_.Allocator());
        Base::Result<void> copiedNodes =
            nodes.TryAssign(plan.Nodes());
        if (!copiedNodes) return copiedNodes.GetStatus();
        Base::Result<void> copiedCommands =
            commands.TryAssign(plan.Commands());
        if (!copiedCommands) return copiedCommands.GetStatus();
        nodes_ = std::move(nodes);
        commands_ = std::move(commands);
        planVersion_ = plan.Version();
        planHash_ = plan.StableHash();
        resourceGeneration_ = resourceGeneration;
        frozen_ = true;
        return {};
    }

    void Clear() noexcept {
        nodes_.Clear();
        commands_.Clear();
        planVersion_ = 0U;
        planHash_ = 0U;
        resourceGeneration_ = 0U;
        frozen_ = false;
    }

    bool IsFrozen() const noexcept { return frozen_; }
    std::uint64_t PlanVersion() const noexcept { return planVersion_; }
    std::uint64_t PlanHash() const noexcept { return planHash_; }
    std::uint64_t ResourceGeneration() const noexcept {
        return resourceGeneration_;
    }
    Base::Span<const Presentation::RenderNodeSnapshot> Nodes() const noexcept {
        return nodes_.AsSpan();
    }
    Base::Span<const Presentation::RenderCommand> Commands() const noexcept {
        return commands_.AsSpan();
    }

private:
    Base::Vector<Presentation::RenderNodeSnapshot> nodes_;
    Base::Vector<Presentation::RenderCommand> commands_;
    std::uint64_t planVersion_ = 0U;
    std::uint64_t planHash_ = 0U;
    std::uint64_t resourceGeneration_ = 0U;
    bool frozen_ = false;
};

class AERO_API ProductionRenderGraph final {
public:
    explicit ProductionRenderGraph(
        Base::IAllocator* allocator = nullptr) noexcept
        : passes_(allocator) {}

    Base::Result<void> TryAddPass(
        const ProductionPassDescriptor& descriptor) noexcept {
        if (descriptor.id == 0U ||
            !Presentation::IsValidLayoutRect(descriptor.bounds) ||
            descriptor.sampleCount == 0U ||
            (descriptor.sampleCount & (descriptor.sampleCount - 1U)) != 0U) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Production render pass descriptor is invalid");
        }
        for (const ProductionPassDescriptor& pass : passes_) {
            if (pass.id == descriptor.id) {
                return Base::Status::Failure(
                    Base::ErrorCode::AlreadyExists,
                    "Production render pass ID is duplicated");
            }
        }
        if (descriptor.parent != 0U) {
            bool parentFound = false;
            for (const ProductionPassDescriptor& pass : passes_) {
                if (pass.id == descriptor.parent) {
                    parentFound = true;
                    break;
                }
            }
            if (!parentFound) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Production render pass parent must precede its child");
            }
        }
        if ((descriptor.effect != RenderEffectKind::None ||
             descriptor.mask == RenderMaskKind::AlphaTexture) &&
            !descriptor.offscreen) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Effects and alpha masks require an offscreen pass");
        }
        return passes_.TryPushBack(descriptor);
    }

    Base::Span<const ProductionPassDescriptor> Passes() const noexcept {
        return passes_.AsSpan();
    }
    void Clear() noexcept { passes_.Clear(); }
    std::uint32_t OffscreenPassCount() const noexcept {
        std::uint32_t count = 0U;
        for (const ProductionPassDescriptor& pass : passes_) {
            if (pass.offscreen) ++count;
        }
        return count;
    }

private:
    Base::Vector<ProductionPassDescriptor> passes_;
};

using GeometryTessellateCallback = Base::Result<void> (*)(
    Base::Span<const std::uint8_t> geometry,
    Base::Vector<float>& vertices,
    Base::Vector<std::uint32_t>& indices,
    void* context) noexcept;

struct GeometryTessellationProvider final {
    GeometryTessellateCallback tessellate = nullptr;
    void* context = nullptr;
    std::uint32_t abiVersion = 1U;

    bool IsValid() const noexcept {
        return abiVersion == 1U && tessellate != nullptr;
    }
};

enum class RenderCacheKind : std::uint8_t {
    Geometry = 0U,
    Glyph,
    Image,
    Offscreen,
    Pipeline,
};

struct RenderCacheEntry final {
    std::uint64_t id = 0U;
    std::uint64_t bytes = 0U;
    std::uint64_t lastUse = 0U;
    Rhi::FenceValue inUseUntil = 0U;
    RenderCacheKind kind = RenderCacheKind::Image;
};

struct RenderCacheStatistics final {
    std::uint64_t budgetBytes = 0U;
    std::uint64_t usedBytes = 0U;
    std::uint64_t evictedBytes = 0U;
    std::uint64_t evictionCount = 0U;
    std::uint32_t entryCount = 0U;
};

class AERO_API RenderCacheBudgetManager final {
public:
    explicit RenderCacheBudgetManager(
        Base::IAllocator* allocator = nullptr) noexcept
        : entries_(allocator) {}

    Base::Result<void> SetBudget(std::uint64_t bytes) noexcept {
        if (bytes == 0U) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Render cache budget must be non-zero");
        }
        budgetBytes_ = bytes;
        return {};
    }

    Base::Result<void> InsertOrTouch(
        std::uint64_t id,
        std::uint64_t bytes,
        RenderCacheKind kind,
        std::uint64_t useSerial,
        Rhi::FenceValue inUseUntil = 0U) noexcept {
        if (id == 0U || bytes == 0U || useSerial == 0U) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Render cache entry is invalid");
        }
        for (RenderCacheEntry& entry : entries_) {
            if (entry.id != id) continue;
            if (usedBytes_ < entry.bytes) usedBytes_ = 0U;
            else usedBytes_ -= entry.bytes;
            entry.bytes = bytes;
            entry.kind = kind;
            entry.lastUse = useSerial;
            entry.inUseUntil = inUseUntil;
            usedBytes_ += bytes;
            return {};
        }
        Base::Result<void> appended = entries_.TryPushBack(
            {id, bytes, useSerial, inUseUntil, kind});
        if (!appended) return appended.GetStatus();
        usedBytes_ += bytes;
        return {};
    }

    Base::Result<std::uint32_t> Trim(
        Rhi::FenceValue completedFence,
        Base::Vector<std::uint64_t>* evicted = nullptr) noexcept {
        std::uint32_t count = 0U;
        while (usedBytes_ > budgetBytes_) {
            std::uint32_t candidate = UINT32_MAX;
            for (std::uint32_t index = 0U; index < entries_.Size(); ++index) {
                if (entries_[index].inUseUntil > completedFence) continue;
                if (candidate == UINT32_MAX ||
                    entries_[index].lastUse < entries_[candidate].lastUse) {
                    candidate = index;
                }
            }
            if (candidate == UINT32_MAX) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Render cache exceeds budget but all entries are fence-protected");
            }
            const RenderCacheEntry removed = entries_[candidate];
            if (evicted != nullptr) {
                Base::Result<void> tracked = evicted->TryPushBack(removed.id);
                if (!tracked) return tracked.GetStatus();
            }
            usedBytes_ -= removed.bytes;
            evictedBytes_ += removed.bytes;
            ++evictionCount_;
            ++count;
            for (std::uint32_t next = candidate + 1U; next < entries_.Size(); ++next) {
                entries_[next - 1U] = entries_[next];
            }
            entries_.PopBack();
        }
        return count;
    }

    RenderCacheStatistics Statistics() const noexcept {
        return {budgetBytes_, usedBytes_, evictedBytes_,
            evictionCount_, entries_.Size()};
    }
    void Clear() noexcept {
        entries_.Clear();
        usedBytes_ = 0U;
    }

private:
    Base::Vector<RenderCacheEntry> entries_;
    std::uint64_t budgetBytes_ = 64U * 1024U * 1024U;
    std::uint64_t usedBytes_ = 0U;
    std::uint64_t evictedBytes_ = 0U;
    std::uint64_t evictionCount_ = 0U;
};

enum class AnimationFillMode : std::uint8_t {
    Stop = 0U,
    HoldEnd,
};

struct ScalarAnimation final {
    double from = 0.0;
    double to = 0.0;
    std::uint64_t durationMicroseconds = 0U;
    std::uint32_t repeatCount = 1U;
    AnimationFillMode fillMode = AnimationFillMode::HoldEnd;
};

struct AnimationSample final {
    double value = 0.0;
    double progress = 0.0;
    bool active = false;
    bool completed = false;
};

class AERO_API AnimationClock final {
public:
    Base::Result<void> Start(
        const ScalarAnimation& animation,
        std::uint64_t startTimeMicroseconds) noexcept {
        if (!std::isfinite(animation.from) ||
            !std::isfinite(animation.to) ||
            animation.durationMicroseconds == 0U ||
            animation.repeatCount == 0U) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Scalar animation descriptor is invalid");
        }
        animation_ = animation;
        start_ = startTimeMicroseconds;
        running_ = true;
        return {};
    }

    AnimationSample Sample(std::uint64_t nowMicroseconds) const noexcept {
        AnimationSample result;
        result.value = animation_.from;
        if (!running_ || nowMicroseconds < start_) return result;
        const std::uint64_t elapsed = nowMicroseconds - start_;
        const std::uint64_t total = animation_.durationMicroseconds *
            static_cast<std::uint64_t>(animation_.repeatCount);
        if (elapsed >= total) {
            result.completed = true;
            result.progress = 1.0;
            result.value = animation_.fillMode == AnimationFillMode::HoldEnd
                ? animation_.to : animation_.from;
            return result;
        }
        const std::uint64_t within = elapsed % animation_.durationMicroseconds;
        result.progress = static_cast<double>(within) /
            static_cast<double>(animation_.durationMicroseconds);
        result.value = animation_.from +
            (animation_.to - animation_.from) * result.progress;
        result.active = true;
        return result;
    }

    void Stop() noexcept { running_ = false; }
    bool IsRunning() const noexcept { return running_; }

private:
    ScalarAnimation animation_;
    std::uint64_t start_ = 0U;
    bool running_ = false;
};

} // namespace Aero::Render
