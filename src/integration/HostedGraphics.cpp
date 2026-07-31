#include "../render/DisplayList.hpp"
#include <Aero/Integration/HostedGraphics.hpp>

#include "RenderEndpointInternal.hpp"

#include <Aero/Base/Vector.hpp>

#include <new>

namespace Aero::Integration {
namespace {

Base::Status HostedStatus(
    HostedGraphicsResult result,
    const char* operation) noexcept {
    switch (result) {
    case HostedGraphicsResult::Success:
        return {};
    case HostedGraphicsResult::InvalidArgument:
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument, operation);
    case HostedGraphicsResult::Unsupported:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported, operation);
    case HostedGraphicsResult::OutOfMemory:
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory, operation);
    case HostedGraphicsResult::SurfaceLost:
    case HostedGraphicsResult::DeviceLost:
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState, operation);
    case HostedGraphicsResult::Failed:
        break;
    }
    return Base::Status::Failure(
        Base::ErrorCode::InternalError, operation);
}

HostedGraphicsCommand LowerCommand(
    const Render::RenderCommand& source,
    std::uint32_t resourceGeneration) noexcept {
    HostedGraphicsCommand target;
    target.kind = static_cast<HostedGraphicsCommandKind>(
        source.kind);
    target.rect[0] = static_cast<float>(source.rect.x);
    target.rect[1] = static_cast<float>(source.rect.y);
    target.rect[2] = static_cast<float>(source.rect.width);
    target.rect[3] = static_cast<float>(source.rect.height);
    target.transform[0] =
        static_cast<float>(source.transform.m11);
    target.transform[1] =
        static_cast<float>(source.transform.m12);
    target.transform[2] =
        static_cast<float>(source.transform.m21);
    target.transform[3] =
        static_cast<float>(source.transform.m22);
    target.transform[4] =
        static_cast<float>(source.transform.dx);
    target.transform[5] =
        static_cast<float>(source.transform.dy);
    target.color[0] = source.color.red;
    target.color[1] = source.color.green;
    target.color[2] = source.color.blue;
    target.color[3] = source.color.alpha;
    target.sourceUv[0] =
        static_cast<float>(source.sourceUv.x);
    target.sourceUv[1] =
        static_cast<float>(source.sourceUv.y);
    target.sourceUv[2] =
        static_cast<float>(source.sourceUv.width);
    target.sourceUv[3] =
        static_cast<float>(source.sourceUv.height);
    switch (source.kind) {
    case Render::RenderCommandKind::DrawImage:
        target.resourceId = source.image;
        break;
    case Render::RenderCommandKind::DrawMesh:
        target.resourceId = source.mesh;
        break;
    case Render::RenderCommandKind::DrawGlyphRun:
        target.resourceId = source.glyphRun;
        break;
    default:
        break;
    }
    target.resourceGeneration = resourceGeneration;
    target.scalar = static_cast<float>(source.scalar);
    return target;
}

class HostedEndpointDriver final
    : public Detail::EndpointDriver {
public:
    HostedEndpointDriver(
        const HostedGraphicsCallbacks& callbacks,
        bool ownsPresentation,
        Base::IAllocator& allocator) noexcept
        : callbacks_(callbacks),
          ownsPresentation_(ownsPresentation),
          commands_(&allocator) {}

    bool SupportsDedicatedThread() const noexcept override {
        return (callbacks_.capabilities &
            HostedGraphicsCapabilityThreadSafe) != 0U;
    }

    Base::Result<void> Submit(
        const Render::RenderPlan& plan) noexcept override {
        if (callbacks_.submit == nullptr ||
            callbacks_.acquireTarget == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Hosted graphics callbacks are incomplete");
        }
        if (callbacks_.isDeviceLost != nullptr &&
            callbacks_.isDeviceLost(callbacks_.context)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Hosted graphics device is lost");
        }

        commands_.Clear();
        Base::Result<void> reserved =
            commands_.TryReserve(plan.Commands().Size());
        if (!reserved) return reserved.GetStatus();
        for (const Render::RenderCommand& command :
             plan.Commands()) {
            Base::Result<void> appended =
                commands_.TryPushBack(LowerCommand(
                    command, resourceGeneration_));
            if (!appended) return appended.GetStatus();
        }

        HostedGraphicsTarget target;
        HostedGraphicsResult acquired =
            callbacks_.acquireTarget(
                callbacks_.context, &target);
        Base::Status acquireStatus = HostedStatus(
            acquired,
            "Hosted graphics target acquisition failed");
        if (!acquireStatus.IsOk()) return acquireStatus;
        if (target.width == 0U || target.height == 0U) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Hosted graphics target dimensions are empty");
        }

        HostedGraphicsCommandListView list;
        list.commands = commands_.Data();
        list.commandCount = commands_.Size();
        list.frameVersion = plan.Version();
        const std::uint64_t fence = nextFence_++;
        Base::Status submitStatus = HostedStatus(
            callbacks_.submit(
                callbacks_.context, &target, &list, fence),
            "Hosted graphics submission failed");
        if (!submitStatus.IsOk()) return submitStatus;

        if (ownsPresentation_) {
            if (callbacks_.present == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotInitialized,
                    "Hosted window endpoint has no present callback");
            }
            Base::Status presentStatus = HostedStatus(
                callbacks_.present(
                    callbacks_.context, fence),
                "Hosted graphics present failed");
            if (!presentStatus.IsOk()) return presentStatus;
        }
        lastFence_ = fence;
        return {};
    }

    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept override {
        if (callbacks_.resizeSurface == nullptr) {
            return ownsPresentation_
                ? Base::Result<void>(
                      Base::Status::Failure(
                          Base::ErrorCode::Unsupported,
                          "Hosted window endpoint cannot resize"))
                : Base::Result<void>();
        }
        Base::Status status = HostedStatus(
            callbacks_.resizeSurface(
                callbacks_.context, width, height),
            "Hosted graphics resize failed");
        return status.IsOk()
            ? Base::Result<void>()
            : Base::Result<void>(status);
    }

    void NotifySurfaceLost() noexcept override {
        surfaceLost_ = true;
        AdvanceResourceGeneration();
        if (callbacks_.notifySurfaceLost != nullptr) {
            callbacks_.notifySurfaceLost(callbacks_.context);
        }
    }

    void NotifyDeviceLost() noexcept override {
        deviceLost_ = true;
        surfaceLost_ = true;
        AdvanceResourceGeneration();
        if (callbacks_.notifyDeviceLost != nullptr) {
            callbacks_.notifyDeviceLost(callbacks_.context);
        }
    }

    Base::Result<void> Restore() noexcept override {
        if (deviceLost_ &&
            callbacks_.restoreDevice != nullptr) {
            Base::Status status = HostedStatus(
                callbacks_.restoreDevice(callbacks_.context),
                "Hosted graphics device restore failed");
            if (!status.IsOk()) return status;
        }
        if (surfaceLost_ &&
            callbacks_.restoreSurface != nullptr) {
            Base::Status status = HostedStatus(
                callbacks_.restoreSurface(callbacks_.context),
                "Hosted graphics surface restore failed");
            if (!status.IsOk()) return status;
        }
        deviceLost_ = false;
        surfaceLost_ = false;
        return {};
    }

    Base::Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds) noexcept override {
        if (lastFence_ == 0U ||
            callbacks_.waitFence == nullptr) {
            return {};
        }
        Base::Status status = HostedStatus(
            callbacks_.waitFence(
                callbacks_.context,
                lastFence_,
                timeoutMilliseconds),
            "Hosted graphics fence wait failed");
        return status.IsOk()
            ? Base::Result<void>()
            : Base::Result<void>(status);
    }

private:
    void AdvanceResourceGeneration() noexcept {
        ++resourceGeneration_;
        if (resourceGeneration_ == 0U) {
            resourceGeneration_ = 1U;
        }
    }

    HostedGraphicsCallbacks callbacks_;
    bool ownsPresentation_ = false;
    bool surfaceLost_ = false;
    bool deviceLost_ = false;
    Base::Vector<HostedGraphicsCommand> commands_;
    std::uint32_t resourceGeneration_ = 1U;
    std::uint64_t nextFence_ = 1U;
    std::uint64_t lastFence_ = 0U;
};

Base::Result<Base::Ref<RenderEndpoint>>
CreateHostedEndpoint(
    const HostedGraphicsCallbacks& callbacks,
    RenderEndpointMode mode,
    RenderSubmissionMode submissionMode,
    Base::IAllocator* allocator) noexcept {
    if (callbacks.structSize < sizeof(HostedGraphicsCallbacks) ||
        callbacks.abiVersion != HostedGraphicsAbiVersion ||
        callbacks.submit == nullptr ||
        callbacks.acquireTarget == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Hosted graphics callback table is invalid");
    }
    const HostedGraphicsCapability required =
        mode == RenderEndpointMode::Window
        ? HostedGraphicsCapabilityWindowSurface
        : HostedGraphicsCapabilityEmbeddedTarget;
    if ((callbacks.capabilities & required) == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Hosted graphics endpoint mode is unsupported");
    }
    if (mode == RenderEndpointMode::Window &&
        callbacks.present == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Hosted window endpoint requires a present callback");
    }
    if (submissionMode ==
            RenderSubmissionMode::DedicatedThread &&
        (callbacks.capabilities &
            HostedGraphicsCapabilityThreadSafe) == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Hosted callbacks are not declared thread safe");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    auto* driver = new (std::nothrow) HostedEndpointDriver(
        callbacks,
        mode == RenderEndpointMode::Window,
        selected);
    if (driver == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Unable to allocate the hosted graphics endpoint");
    }
    return Detail::RenderEndpointAccess::Create(
        mode, submissionMode, driver, &selected);
}

} // namespace

Base::Result<Base::Ref<RenderEndpoint>>
CreateHostedEmbeddedEndpoint(
    const HostedGraphicsCallbacks& callbacks,
    RenderSubmissionMode submissionMode,
    Base::IAllocator* allocator) noexcept {
    return CreateHostedEndpoint(
        callbacks,
        RenderEndpointMode::Embedded,
        submissionMode,
        allocator);
}

Base::Result<Base::Ref<RenderEndpoint>>
CreateHostedWindowEndpoint(
    const HostedGraphicsCallbacks& callbacks,
    RenderSubmissionMode submissionMode,
    Base::IAllocator* allocator) noexcept {
    return CreateHostedEndpoint(
        callbacks,
        RenderEndpointMode::Window,
        submissionMode,
        allocator);
}

} // namespace Aero::Integration
