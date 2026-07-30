#include "RenderEndpointInternal.hpp"
#include "presentation/BatchPlanner.hpp"

#include <Aero/Base/Vector.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>
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

class HeadlessEndpointDriver final
    : public Detail::EndpointDriver {
public:
    Base::Result<void> Submit(
        const Presentation::RenderPlan&) noexcept override {
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
        RenderSubmissionMode endpointSubmissionMode,
        Base::IAllocator& value) noexcept
        : allocator(&value),
          mode(endpointMode),
          submissionMode(endpointSubmissionMode),
          pending(&value) {}

    Base::IAllocator* allocator = nullptr;
    RenderEndpointMode mode = RenderEndpointMode::Headless;
    RenderSubmissionMode submissionMode =
        RenderSubmissionMode::Immediate;
    RenderEndpointState state = RenderEndpointState::Ready;
    Detail::EndpointDriver* driver = nullptr;
    const void* boundOwner = nullptr;
    Base::Vector<Presentation::RenderPlan> pending;
    RenderEndpointStatistics statistics;
    RenderFrameStatistics lastFrameStatistics;
    Base::Status asynchronousFailure;
    std::mutex mutex;
    std::condition_variable wake;
    std::condition_variable idle;
    std::thread worker;
    bool workerRunning = false;
    bool workerStop = false;
    bool executing = false;
    bool batchingEnabled = true;

    bool AcceptingFrames() const noexcept {
        return state == RenderEndpointState::Ready;
    }

    Base::Result<RenderFrameStatistics>
    PlanFrameStatistics(
        const Presentation::RenderPlan& plan) noexcept {
        Presentation::Detail::BatchPlanner planner(
            allocator);
        Base::Result<Presentation::Detail::BatchPlan>
            planned = planner.Build(
                plan, batchingEnabled);
        if (!planned) return planned.GetStatus();
        const auto& source =
            planned.Value().Statistics();
        RenderFrameStatistics result;
        result.sourceCommandCount =
            source.sourceCommandCount;
        result.drawPacketCount =
            source.drawPacketCount;
        result.batchCount =
            source.batchCount;
        result.mergedPacketCount =
            source.mergedPacketCount;
        result.barrierCount =
            source.barrierCount;
        result.batchingEnabled =
            batchingEnabled;
        return result;
    }

    void MergeBackendStatistics(
        RenderFrameStatistics& result) const noexcept {
        const RenderFrameStatistics backend =
            driver != nullptr
            ? driver->LastFrameStatistics()
            : RenderFrameStatistics{};
        result.drawCallCount =
            backend.drawCallCount != 0U
            ? backend.drawCallCount
            : result.batchCount;
        result.instanceCount =
            backend.instanceCount != 0U
            ? backend.instanceCount
            : result.drawPacketCount;
        result.stateBindingCount =
            backend.stateBindingCount;
    }

    void WorkerMain() noexcept {
        for (;;) {
            Presentation::RenderPlan plan;
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock, [this] {
                    return workerStop || !pending.Empty();
                });
                if (workerStop && pending.Empty()) break;
                plan = std::move(pending[0]);
                pending.Clear();
                executing = true;
                statistics.pendingFrameCount = 0U;
            }

            Base::Result<RenderFrameStatistics>
                frameStatistics =
                    PlanFrameStatistics(plan);
            Base::Result<void> submitted =
                frameStatistics && driver != nullptr
                ? driver->Submit(plan)
                : Base::Result<void>(
                      frameStatistics
                      ? NotInitialized(
                            "Render endpoint has no backend driver")
                      : frameStatistics.GetStatus());
            if (submitted) {
                MergeBackendStatistics(
                    frameStatistics.Value());
            }

            {
                std::lock_guard<std::mutex> lock(mutex);
                executing = false;
                if (submitted) {
                    lastFrameStatistics =
                        frameStatistics.Value();
                    ++statistics.completedFrameCount;
                    statistics.lastCompletedVersion =
                        plan.Version();
                } else {
                    ++statistics.failedFrameCount;
                    if (state == RenderEndpointState::Ready) {
                        asynchronousFailure =
                            submitted.GetStatus();
                        state = RenderEndpointState::Failed;
                    }
                    pending.Clear();
                    statistics.pendingFrameCount = 0U;
                }
                idle.notify_all();
            }
        }
        std::lock_guard<std::mutex> lock(mutex);
        executing = false;
        idle.notify_all();
    }

    Base::Result<void> StartWorker() noexcept {
        if (submissionMode !=
            RenderSubmissionMode::DedicatedThread) {
            return {};
        }
        if (driver == nullptr ||
            !driver->SupportsDedicatedThread()) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Render backend does not support dedicated-thread submission");
        }
        worker = std::thread([this] { WorkerMain(); });
        workerRunning = true;
        return {};
    }

    void StopWorker() noexcept {
        {
            std::lock_guard<std::mutex> lock(mutex);
            workerStop = true;
        }
        wake.notify_all();
        if (workerRunning && worker.joinable()) {
            worker.join();
        }
        workerRunning = false;
    }
};

RenderEndpoint::RenderEndpoint(
    ConstructionToken,
    RenderEndpointMode mode,
    RenderSubmissionMode submissionMode,
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
    impl_ = new (memory) Impl(
        mode, submissionMode, *allocator_);
}

RenderEndpoint::~RenderEndpoint() noexcept {
    if (impl_ == nullptr) return;
    impl_->StopWorker();
    if (impl_->driver != nullptr) {
        static_cast<void>(impl_->driver->WaitIdle(5000U));
        delete impl_->driver;
        impl_->driver = nullptr;
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

RenderSubmissionMode
RenderEndpoint::SubmissionMode() const noexcept {
    return impl_ != nullptr
        ? impl_->submissionMode
        : RenderSubmissionMode::Immediate;
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
    RenderEndpointStatistics result = impl_->statistics;
    result.pendingFrameCount = impl_->pending.Size();
    return result;
}

RenderFrameStatistics
RenderEndpoint::LastFrameStatistics() const noexcept {
    if (impl_ == nullptr) return {};
    std::lock_guard<std::mutex> lock(
        impl_->mutex);
    return impl_->lastFrameStatistics;
}

Base::Result<void> RenderEndpoint::Resize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    if (impl_ == nullptr || impl_->driver == nullptr) {
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
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->AcceptingFrames()) {
        return InvalidState(
            "Render endpoint cannot resize in its current state");
    }
    return impl_->driver->Resize(width, height);
}

Base::Result<void> RenderEndpoint::NotifySurfaceLost() noexcept {
    if (impl_ == nullptr || impl_->driver == nullptr) {
        return NotInitialized(
            "Render endpoint is not initialized");
    }
    {
        std::unique_lock<std::mutex> lock(impl_->mutex);
        if (impl_->state != RenderEndpointState::Ready) {
            return InvalidState(
                "Render endpoint cannot lose its surface in its current state");
        }
        impl_->state = RenderEndpointState::SurfaceLost;
        impl_->pending.Clear();
        impl_->statistics.pendingFrameCount = 0U;
        ++impl_->statistics.generation;
        impl_->wake.notify_all();
        const bool idle = impl_->idle.wait_for(
            lock,
            std::chrono::milliseconds(5000U),
            [this] { return !impl_->executing; });
        if (!idle) {
            impl_->state = RenderEndpointState::Failed;
            return InvalidState(
                "Timed out stopping surface submissions");
        }
    }
    impl_->driver->NotifySurfaceLost();
    return {};
}

Base::Result<void> RenderEndpoint::NotifyDeviceLost() noexcept {
    if (impl_ == nullptr || impl_->driver == nullptr) {
        return NotInitialized(
            "Render endpoint is not initialized");
    }
    {
        std::unique_lock<std::mutex> lock(impl_->mutex);
        if (impl_->state != RenderEndpointState::Ready) {
            return InvalidState(
                "Render endpoint cannot lose its device in its current state");
        }
        impl_->state = RenderEndpointState::DeviceLost;
        impl_->pending.Clear();
        impl_->statistics.pendingFrameCount = 0U;
        ++impl_->statistics.generation;
        impl_->wake.notify_all();
        const bool idle = impl_->idle.wait_for(
            lock,
            std::chrono::milliseconds(5000U),
            [this] { return !impl_->executing; });
        if (!idle) {
            impl_->state = RenderEndpointState::Failed;
            return InvalidState(
                "Timed out stopping device submissions");
        }
    }
    impl_->driver->NotifyDeviceLost();
    return {};
}

Base::Result<void> RenderEndpoint::Restore() noexcept {
    if (impl_ == nullptr || impl_->driver == nullptr) {
        return NotInitialized(
            "Render endpoint is not initialized");
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->state != RenderEndpointState::SurfaceLost &&
        impl_->state != RenderEndpointState::DeviceLost) {
        return InvalidState(
            "Only a lost render endpoint can be restored");
    }
    Base::Result<void> restored = impl_->driver->Restore();
    if (!restored) {
        impl_->state = RenderEndpointState::Failed;
        ++impl_->statistics.failedFrameCount;
        return restored.GetStatus();
    }
    impl_->asynchronousFailure = {};
    impl_->state = RenderEndpointState::Ready;
    return {};
}

Base::Result<void> RenderEndpoint::WaitIdle(
    std::uint32_t timeoutMilliseconds) noexcept {
    if (impl_ == nullptr || impl_->driver == nullptr) {
        return NotInitialized(
            "Render endpoint is not initialized");
    }
    if (impl_->submissionMode ==
        RenderSubmissionMode::DedicatedThread) {
        std::unique_lock<std::mutex> lock(impl_->mutex);
        const bool idle = impl_->idle.wait_for(
            lock,
            std::chrono::milliseconds(timeoutMilliseconds),
            [this] {
                return !impl_->executing &&
                    impl_->pending.Empty();
            });
        if (!idle) {
            return InvalidState(
                "Timed out waiting for the render endpoint");
        }
    }
    return impl_->driver->WaitIdle(timeoutMilliseconds);
}

Base::Result<void>
RenderEndpoint::SetBatchingEnabledForTesting(
    bool enabled) noexcept {
    if (impl_ == nullptr || impl_->driver == nullptr) {
        return NotInitialized(
            "Render endpoint is not initialized");
    }
    Base::Result<void> idle = WaitIdle();
    if (!idle) return idle.GetStatus();
    std::lock_guard<std::mutex> lock(
        impl_->mutex);
    if (!impl_->AcceptingFrames()) {
        return InvalidState(
            "Render endpoint cannot change batching in its current state");
    }
    impl_->batchingEnabled = enabled;
    impl_->driver->SetBatchingEnabled(
        enabled);
    return {};
}

namespace Detail {

Base::Result<Base::Ref<RenderEndpoint>>
RenderEndpointAccess::Create(
    RenderEndpointMode mode,
    RenderSubmissionMode submissionMode,
    EndpointDriver* driver,
    Base::IAllocator* allocator) noexcept {
    if (driver == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render endpoint driver is required");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    Base::Result<Base::Ref<RenderEndpoint>> made =
        Base::MakeRefWithAllocator<RenderEndpoint>(
            selected,
            RenderEndpoint::ConstructionToken{},
            mode,
            submissionMode,
            &selected);
    if (!made) {
        delete driver;
        return made.GetStatus();
    }
    RenderEndpoint::Impl& impl = *made.Value()->impl_;
    impl.driver = driver;
    Base::Result<void> started = impl.StartWorker();
    if (!started) return started.GetStatus();
    return std::move(made).Value();
}

Base::Result<Base::Ref<RenderEndpoint>>
RenderEndpointAccess::CreateHeadless(
    RenderSubmissionMode submissionMode,
    Base::IAllocator* allocator) noexcept {
    HeadlessEndpointDriver* driver =
        new (std::nothrow) HeadlessEndpointDriver();
    if (driver == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Unable to allocate the headless render endpoint");
    }
    return Create(
        RenderEndpointMode::Headless,
        submissionMode,
        driver,
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
    const Presentation::RenderPlan& plan) noexcept {
    if (endpoint.impl_ == nullptr ||
        endpoint.impl_->driver == nullptr) {
        return NotInitialized(
            "Render endpoint is not initialized");
    }
    RenderEndpoint::Impl& impl = *endpoint.impl_;
    if (impl.submissionMode ==
        RenderSubmissionMode::Immediate) {
        {
            std::lock_guard<std::mutex> lock(impl.mutex);
            if (!impl.AcceptingFrames()) {
                return impl.asynchronousFailure.IsOk()
                    ? InvalidState(
                          "Render endpoint is not ready")
                    : impl.asynchronousFailure;
            }
        }
        Base::Result<RenderFrameStatistics>
            frameStatistics =
                impl.PlanFrameStatistics(plan);
        if (!frameStatistics) {
            return frameStatistics.GetStatus();
        }
        Base::Result<void> submitted =
            impl.driver->Submit(plan);
        if (submitted) {
            impl.MergeBackendStatistics(
                frameStatistics.Value());
        }
        std::lock_guard<std::mutex> lock(impl.mutex);
        if (!submitted) {
            ++impl.statistics.failedFrameCount;
            impl.state = RenderEndpointState::Failed;
            return submitted.GetStatus();
        }
        impl.lastFrameStatistics =
            frameStatistics.Value();
        ++impl.statistics.acceptedFrameCount;
        ++impl.statistics.completedFrameCount;
        impl.statistics.lastAcceptedVersion = plan.Version();
        impl.statistics.lastCompletedVersion = plan.Version();
        return {};
    }

    std::lock_guard<std::mutex> lock(impl.mutex);
    if (!impl.AcceptingFrames()) {
        return impl.asynchronousFailure.IsOk()
            ? InvalidState(
                  "Render endpoint is not ready")
            : impl.asynchronousFailure;
    }
    if (impl.pending.Empty()) {
        Base::Result<void> appended =
            impl.pending.TryPushBack(plan);
        if (!appended) return appended.GetStatus();
    } else {
        impl.pending[0] = plan;
        ++impl.statistics.coalescedFrameCount;
    }
    ++impl.statistics.acceptedFrameCount;
    impl.statistics.lastAcceptedVersion = plan.Version();
    impl.statistics.pendingFrameCount = impl.pending.Size();
    const std::uint32_t activeSlots =
        impl.executing ? 1U : 0U;
    const std::uint32_t usedSlots =
        activeSlots + impl.pending.Size();
    if (usedSlots > impl.statistics.highWatermark) {
        impl.statistics.highWatermark = usedSlots;
    }
    impl.wake.notify_one();
    return {};
}

Base::Status RenderEndpointAccess::FrameStatus(
    RenderEndpoint& endpoint) noexcept {
    if (endpoint.impl_ == nullptr ||
        endpoint.impl_->driver == nullptr) {
        return NotInitialized(
            "Render endpoint is not initialized");
    }
    std::lock_guard<std::mutex> lock(endpoint.impl_->mutex);
    if (!endpoint.impl_->asynchronousFailure.IsOk()) {
        return endpoint.impl_->asynchronousFailure;
    }
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

void* RenderEndpointAccess::QueryInternalService(
    RenderEndpoint& endpoint,
    std::uint64_t service) noexcept {
    if (endpoint.impl_ == nullptr ||
        endpoint.impl_->driver == nullptr) {
        return nullptr;
    }
    return endpoint.impl_->driver->QueryInternalService(service);
}

} // namespace Detail
} // namespace Aero::Integration
