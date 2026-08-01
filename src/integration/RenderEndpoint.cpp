#include "RenderEndpointInternal.hpp"
#include "render/BatchPlanner.hpp"

#include <mutex>
#include <new>
#include <utility>

namespace Aero::Integration {
namespace {

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotInitialized, message);
}

class HeadlessEndpointBackend final
    : public Detail::EndpointBackend {
public:
    Base::Result<void> Submit(
        const Render::RenderFrame&) noexcept override {
        return {};
    }

    Base::Result<void> Resize(
        std::uint32_t,
        std::uint32_t) noexcept override {
        return {};
    }

    void NotifySurfaceLost() noexcept override {}
    void NotifyDeviceLost() noexcept override {}
    Base::Result<void> Restore() noexcept override { return {}; }
    Base::Result<void> WaitIdle(
        std::uint32_t) noexcept override {
        return {};
    }
};

} // namespace

struct RenderEndpoint::Impl final {
    Impl(
        RenderEndpointMode endpointMode,
        Base::IAllocator& value) noexcept
        : allocator(&value), mode(endpointMode) {}

    Base::IAllocator* allocator = nullptr;
    RenderEndpointMode mode = RenderEndpointMode::Headless;
    RenderEndpointState state = RenderEndpointState::Ready;
    Detail::EndpointBackend* backend = nullptr;
    const void* boundOwner = nullptr;
    RenderEndpointStatistics statistics;
    RenderFrameStatistics lastFrameStatistics;
    std::mutex mutex;
    bool batchingEnabled = true;

    bool AcceptingFrames() const noexcept {
        return state == RenderEndpointState::Ready;
    }

    Base::Result<RenderFrameStatistics>
    AnalyzeFrameStatistics(
        const Render::RenderFrame& frame) noexcept {
        Base::Result<void> valid =
            Render::ValidateRenderFrame(frame);
        if (!valid) return valid.GetStatus();

        Render::Detail::BatchPlanner planner(allocator);
        Base::Result<Render::Detail::BatchPlan> planned =
            planner.Build(frame, batchingEnabled);
        if (!planned) return planned.GetStatus();

        const auto& source = planned.Value().Statistics();
        RenderFrameStatistics result;
        result.sourceCommandCount = source.sourceCommandCount;
        result.drawPacketCount = source.drawPacketCount;
        result.batchCount = source.batchCount;
        result.mergedPacketCount = source.mergedPacketCount;
        result.barrierCount = source.barrierCount;
        result.batchingEnabled = batchingEnabled;
        return result;
    }

    void MergeBackendStatistics(
        RenderFrameStatistics& result) const noexcept {
        const RenderFrameStatistics backendStatistics =
            backend != nullptr
            ? backend->LastFrameStatistics()
            : RenderFrameStatistics{};
        result.drawCallCount =
            backendStatistics.drawCallCount != 0U
            ? backendStatistics.drawCallCount
            : result.batchCount;
        result.instanceCount =
            backendStatistics.instanceCount != 0U
            ? backendStatistics.instanceCount
            : result.drawPacketCount;
        result.stateBindingCount =
            backendStatistics.stateBindingCount;
    }
};

RenderEndpoint::RenderEndpoint(
    ConstructionToken,
    RenderEndpointMode mode,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    void* memory = allocator_->Allocate({
        sizeof(Impl), alignof(Impl),
        Base::MemoryTag::Render});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl), alignof(Impl),
            Base::MemoryTag::Render);
    }
    impl_ = new (memory) Impl(mode, *allocator_);
}

RenderEndpoint::~RenderEndpoint() noexcept {
    if (impl_ == nullptr) return;
    if (impl_->backend != nullptr) {
        static_cast<void>(impl_->backend->WaitIdle(5000U));
        delete impl_->backend;
        impl_->backend = nullptr;
    }
    impl_->state = RenderEndpointState::Shutdown;
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl),
        Base::MemoryTag::Render);
    impl_ = nullptr;
}

RenderEndpointMode RenderEndpoint::Mode() const noexcept {
    return impl_ != nullptr
        ? impl_->mode
        : RenderEndpointMode::Headless;
}

RenderEndpointState RenderEndpoint::State() const noexcept {
    if (impl_ == nullptr) return RenderEndpointState::Shutdown;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->state;
}

std::uint64_t RenderEndpoint::Generation() const noexcept {
    if (impl_ == nullptr) return 0U;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->statistics.generation;
}

RenderEndpointStatistics
RenderEndpoint::Statistics() const noexcept {
    if (impl_ == nullptr) return {};
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->statistics;
}

RenderFrameStatistics
RenderEndpoint::LastFrameStatistics() const noexcept {
    if (impl_ == nullptr) return {};
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->lastFrameStatistics;
}

Base::Result<void> RenderEndpoint::Resize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    if (impl_ == nullptr || impl_->backend == nullptr) {
        return NotInitialized(
            "Render endpoint is not initialized");
    }
    if (width == 0U || height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render endpoint dimensions must be nonzero");
    }
    Base::Result<void> idle = WaitIdle();
    if (!idle) return idle.GetStatus();
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->AcceptingFrames()) {
            return InvalidState(
                "Render endpoint cannot resize in its current state");
        }
    }
    return impl_->backend->Resize(width, height);
}

Base::Result<void> RenderEndpoint::NotifySurfaceLost() noexcept {
    if (impl_ == nullptr || impl_->backend == nullptr) {
        return NotInitialized(
            "Render endpoint is not initialized");
    }
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->state != RenderEndpointState::Ready) {
            return InvalidState(
                "Render endpoint cannot lose its surface in its current state");
        }
        impl_->state = RenderEndpointState::SurfaceLost;
        ++impl_->statistics.generation;
    }
    impl_->backend->NotifySurfaceLost();
    return {};
}

Base::Result<void> RenderEndpoint::NotifyDeviceLost() noexcept {
    if (impl_ == nullptr || impl_->backend == nullptr) {
        return NotInitialized(
            "Render endpoint is not initialized");
    }
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->state != RenderEndpointState::Ready) {
            return InvalidState(
                "Render endpoint cannot lose its device in its current state");
        }
        impl_->state = RenderEndpointState::DeviceLost;
        ++impl_->statistics.generation;
    }
    impl_->backend->NotifyDeviceLost();
    return {};
}

Base::Result<void> RenderEndpoint::Restore() noexcept {
    if (impl_ == nullptr || impl_->backend == nullptr) {
        return NotInitialized(
            "Render endpoint is not initialized");
    }
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->state != RenderEndpointState::SurfaceLost &&
            impl_->state != RenderEndpointState::DeviceLost) {
            return InvalidState(
                "Only a lost render endpoint can be restored");
        }
    }

    Base::Result<void> restored = impl_->backend->Restore();
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!restored) {
        impl_->state = RenderEndpointState::Failed;
        ++impl_->statistics.failedFrameCount;
        return restored.GetStatus();
    }
    impl_->state = RenderEndpointState::Ready;
    return {};
}

Base::Result<void> RenderEndpoint::WaitIdle(
    std::uint32_t timeoutMilliseconds) noexcept {
    if (impl_ == nullptr || impl_->backend == nullptr) {
        return NotInitialized(
            "Render endpoint is not initialized");
    }
    return impl_->backend->WaitIdle(timeoutMilliseconds);
}

namespace Detail {

Base::Result<Base::Ref<RenderEndpoint>>
RenderEndpointAccess::Create(
    RenderEndpointMode mode,
    EndpointBackend* backend,
    Base::IAllocator* allocator) noexcept {
    if (backend == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render endpoint backend is required");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    Base::Result<Base::Ref<RenderEndpoint>> made =
        Base::MakeRefWithAllocator<RenderEndpoint>(
            selected,
            RenderEndpoint::ConstructionToken{},
            mode,
            &selected);
    if (!made) {
        delete backend;
        return made.GetStatus();
    }
    made.Value()->impl_->backend = backend;
    return std::move(made).Value();
}

Base::Result<Base::Ref<RenderEndpoint>>
RenderEndpointAccess::CreateHeadless(
    Base::IAllocator* allocator) noexcept {
    HeadlessEndpointBackend* backend =
        new (std::nothrow) HeadlessEndpointBackend();
    if (backend == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Unable to allocate the headless render endpoint");
    }
    return Create(
        RenderEndpointMode::Headless,
        backend,
        allocator);
}

Base::Result<void> RenderEndpointAccess::Bind(
    RenderEndpoint& endpoint,
    const void* owner) noexcept {
    if (endpoint.impl_ == nullptr || owner == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render endpoint binding requires an owner");
    }
    std::lock_guard<std::mutex> lock(endpoint.impl_->mutex);
    if (endpoint.impl_->state != RenderEndpointState::Ready) {
        return InvalidState(
            "Render endpoint is not ready for binding");
    }
    if (endpoint.impl_->boundOwner != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Render endpoint is already bound to a View");
    }
    endpoint.impl_->boundOwner = owner;
    return {};
}

void RenderEndpointAccess::Unbind(
    RenderEndpoint& endpoint,
    const void* owner) noexcept {
    if (endpoint.impl_ == nullptr || owner == nullptr) return;
    static_cast<void>(endpoint.WaitIdle());
    std::lock_guard<std::mutex> lock(endpoint.impl_->mutex);
    if (endpoint.impl_->boundOwner == owner) {
        endpoint.impl_->boundOwner = nullptr;
    }
}

Base::Result<void> RenderEndpointAccess::Submit(
    RenderEndpoint& endpoint,
    const Render::RenderFrame& frame) noexcept {
    if (endpoint.impl_ == nullptr ||
        endpoint.impl_->backend == nullptr) {
        return NotInitialized(
            "Render endpoint is not initialized");
    }
    RenderEndpoint::Impl& impl = *endpoint.impl_;
    {
        std::lock_guard<std::mutex> lock(impl.mutex);
        if (!impl.AcceptingFrames()) {
            return InvalidState(
                "Render endpoint is not ready");
        }
    }

    Base::Result<RenderFrameStatistics> frameStatistics =
        impl.AnalyzeFrameStatistics(frame);
    if (!frameStatistics) {
        return frameStatistics.GetStatus();
    }

    Base::Result<void> submitted = impl.backend->Submit(frame);
    if (submitted) {
        impl.MergeBackendStatistics(frameStatistics.Value());
    }

    std::lock_guard<std::mutex> lock(impl.mutex);
    if (!submitted) {
        ++impl.statistics.failedFrameCount;
        impl.state = RenderEndpointState::Failed;
        return submitted.GetStatus();
    }
    impl.lastFrameStatistics = frameStatistics.Value();
    ++impl.statistics.acceptedFrameCount;
    ++impl.statistics.completedFrameCount;
    impl.statistics.lastAcceptedVersion = frame.Version();
    impl.statistics.lastCompletedVersion = frame.Version();
    return {};
}

Base::Status RenderEndpointAccess::FrameStatus(
    RenderEndpoint& endpoint) noexcept {
    if (endpoint.impl_ == nullptr ||
        endpoint.impl_->backend == nullptr) {
        return NotInitialized(
            "Render endpoint is not initialized");
    }
    std::lock_guard<std::mutex> lock(endpoint.impl_->mutex);
    switch (endpoint.impl_->state) {
    case RenderEndpointState::Ready:
        return {};
    case RenderEndpointState::SurfaceLost:
        return InvalidState(
            "Render endpoint surface is lost");
    case RenderEndpointState::DeviceLost:
        return InvalidState(
            "Render endpoint device is lost");
    case RenderEndpointState::Failed:
        return InvalidState(
            "Render endpoint has failed");
    case RenderEndpointState::Shutdown:
        return InvalidState(
            "Render endpoint is shut down");
    }
    return InvalidState(
        "Render endpoint state is invalid");
}

Aero::Detail::TextBackendServices* RenderEndpointAccess::TextServices(
    RenderEndpoint& endpoint) noexcept {
    return endpoint.impl_ != nullptr && endpoint.impl_->backend != nullptr
        ? endpoint.impl_->backend->TextServices() : nullptr;
}

Aero::Detail::MeshBackendServices* RenderEndpointAccess::MeshServices(
    RenderEndpoint& endpoint) noexcept {
    return endpoint.impl_ != nullptr && endpoint.impl_->backend != nullptr
        ? endpoint.impl_->backend->MeshServices() : nullptr;
}

Aero::Detail::ImageBackendServices* RenderEndpointAccess::ImageServices(
    RenderEndpoint& endpoint) noexcept {
    return endpoint.impl_ != nullptr && endpoint.impl_->backend != nullptr
        ? endpoint.impl_->backend->ImageServices() : nullptr;
}

} // namespace Detail
} // namespace Aero::Integration
