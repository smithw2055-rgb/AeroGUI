#include "RenderingInternal.hpp"
#include "QueuedRenderBackend.hpp"

#include <mutex>
#include <new>
#include <utility>

namespace Aero::Render {
namespace {

Base::Status RuntimeInvalidState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

Base::Status RuntimeNotInitialized(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotInitialized, message);
}

} // namespace

struct QueuedRenderBackend::Impl final {
    explicit Impl(Base::IAllocator& value) noexcept
        : allocator(&value), queue(&value) {}

    Base::IAllocator* allocator = nullptr;
    IRenderBackend* downstream = nullptr;
    Base::Vector<RenderPlan> queue;
    std::uint32_t capacity = 0U;
    FrameQueueFullPolicy policy =
        FrameQueueFullPolicy::DropOldest;
    FrameQueueStatistics statistics;
    mutable std::mutex mutex;
    bool initialized = false;
};

QueuedRenderBackend::QueuedRenderBackend(
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
    impl_ = new (memory) Impl(*allocator_);
}

QueuedRenderBackend::~QueuedRenderBackend() noexcept {
    Shutdown();
    if (impl_ != nullptr) {
        impl_->~Impl();
        allocator_->Deallocate(
            impl_, sizeof(Impl), alignof(Impl),
            Base::MemoryTag::Render);
        impl_ = nullptr;
    }
}

Base::Result<void> QueuedRenderBackend::Initialize(
    IRenderBackend& downstream,
    std::uint32_t capacity,
    FrameQueueFullPolicy policy) noexcept {
    if (impl_ == nullptr) {
        return RuntimeNotInitialized(
            "Render queue storage is unavailable");
    }
    if (&downstream == this || capacity == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render queue requires a downstream backend and nonzero capacity");
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Render queue is already initialized");
    }
    Base::Result<void> reserved =
        impl_->queue.TryReserve(capacity);
    if (!reserved) return reserved.GetStatus();
    impl_->downstream = &downstream;
    impl_->capacity = capacity;
    impl_->policy = policy;
    impl_->statistics = {};
    impl_->initialized = true;
    return {};
}

void QueuedRenderBackend::Shutdown() noexcept {
    if (impl_ == nullptr) return;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->queue.Clear();
    impl_->downstream = nullptr;
    impl_->capacity = 0U;
    impl_->statistics.pending = 0U;
    impl_->initialized = false;
}

Base::Result<void> QueuedRenderBackend::Submit(
    const RenderPlan& plan) noexcept {
    if (impl_ == nullptr) {
        return RuntimeNotInitialized(
            "Render queue storage is unavailable");
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->initialized || impl_->downstream == nullptr) {
        return RuntimeNotInitialized(
            "Render queue is not initialized");
    }
    if (impl_->queue.Size() >= impl_->capacity) {
        if (impl_->policy == FrameQueueFullPolicy::Reject) {
            ++impl_->statistics.rejected;
            return RuntimeInvalidState(
                "Render queue capacity is exhausted");
        }
        for (std::uint32_t index = 1U;
             index < impl_->queue.Size(); ++index) {
            impl_->queue[index - 1U] =
                std::move(impl_->queue[index]);
        }
        impl_->queue.PopBack();
        ++impl_->statistics.dropped;
    }
    Base::Result<void> appended =
        impl_->queue.TryPushBack(plan);
    if (!appended) return appended.GetStatus();
    ++impl_->statistics.accepted;
    impl_->statistics.pending = impl_->queue.Size();
    if (impl_->statistics.pending >
        impl_->statistics.highWatermark) {
        impl_->statistics.highWatermark =
            impl_->statistics.pending;
    }
    return {};
}

Base::Result<bool> QueuedRenderBackend::ConsumeOne() noexcept {
    if (impl_ == nullptr) {
        return RuntimeNotInitialized(
            "Render queue storage is unavailable");
    }
    RenderPlan plan;
    IRenderBackend* downstream = nullptr;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->initialized || impl_->downstream == nullptr) {
            return RuntimeNotInitialized(
                "Render queue is not initialized");
        }
        if (impl_->queue.Empty()) return false;
        plan = std::move(impl_->queue[0]);
        for (std::uint32_t index = 1U;
             index < impl_->queue.Size(); ++index) {
            impl_->queue[index - 1U] =
                std::move(impl_->queue[index]);
        }
        impl_->queue.PopBack();
        impl_->statistics.pending = impl_->queue.Size();
        downstream = impl_->downstream;
    }

    Base::Result<void> submitted =
        downstream->Submit(plan);
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (submitted) {
            ++impl_->statistics.consumed;
        } else {
            ++impl_->statistics.failed;
        }
    }
    if (!submitted) return submitted.GetStatus();
    return true;
}

Base::Result<std::uint32_t>
QueuedRenderBackend::Drain() noexcept {
    std::uint32_t count = 0U;
    while (true) {
        Base::Result<bool> consumed = ConsumeOne();
        if (!consumed) return consumed.GetStatus();
        if (!consumed.Value()) return count;
        ++count;
    }
}

bool QueuedRenderBackend::IsInitialized() const noexcept {
    if (impl_ == nullptr) return false;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->initialized;
}

FrameQueueStatistics
QueuedRenderBackend::Statistics() const noexcept {
    if (impl_ == nullptr) return {};
    std::lock_guard<std::mutex> lock(impl_->mutex);
    FrameQueueStatistics result = impl_->statistics;
    result.pending = impl_->queue.Size();
    return result;
}

} // namespace Aero::Render
