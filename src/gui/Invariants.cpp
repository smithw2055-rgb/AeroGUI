#include "gui/Invariants.hpp"

#include <utility>

namespace Aero {

namespace {

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

} // namespace

MutationJournal::MutationJournal(
    Base::IAllocator* allocator) noexcept
    : actions_(allocator) {}

MutationJournal::~MutationJournal() noexcept {
    Rollback();
}

Base::Result<void> MutationJournal::AddRollback(
    MutationRollbackCallback rollback,
    void* context) noexcept {
    if (committed_ || rollingBack_) {
        return InvalidState(
            "Mutation journal is no longer mutable");
    }
    if (rollback == nullptr) {
        return InvalidArgument(
            "Mutation rollback callback is null");
    }
    return actions_.PushBack({rollback, context});
}

void MutationJournal::Commit() noexcept {
    if (rollingBack_) {
        return;
    }
    committed_ = true;
    actions_.Clear();
}

void MutationJournal::Rollback() noexcept {
    if (committed_ || rollingBack_) {
        return;
    }
    rollingBack_ = true;
    for (std::uint32_t index = actions_.Size();
         index > 0U;
         --index) {
        MutationRollbackAction& action = actions_[index - 1U];
        if (action.rollback != nullptr) {
            action.rollback(action.context);
        }
    }
    actions_.Clear();
    rollingBack_ = false;
}

SafeDeferredWorkQueue::SafeDeferredWorkQueue(
    Base::IAllocator* allocator) noexcept
    : records_(allocator) {}

SafeDeferredWorkQueue::~SafeDeferredWorkQueue() noexcept {
    Clear();
}

Base::Result<DeferredWorkHandle>
SafeDeferredWorkQueue::Enqueue(
    Base::Object& object,
    DeferredObjectWorkCallback callback,
    void* context) noexcept {
    if (callback == nullptr) {
        return InvalidArgument(
            "Deferred work callback is null");
    }
    if (flushing_) {
        return InvalidState(
            "Deferred work cannot be enqueued during Flush");
    }
    if (nextHandle_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Deferred work handle space is exhausted");
    }

    Base::Ref<Base::Object> strong =
        Base::Ref<Base::Object>::FromBorrowed(object);
    Record record;
    record.handle.value = nextHandle_++;
    record.object = Base::WeakRef<Base::Object>(strong);
    record.callback = callback;
    record.context = context;
    Base::Result<void> appended =
        records_.PushBack(std::move(record));
    if (!appended) {
        return appended.GetStatus();
    }
    ++queued_;
    return records_.Back().handle;
}

Base::Result<bool> SafeDeferredWorkQueue::Cancel(
    DeferredWorkHandle handle) noexcept {
    if (!handle.IsValid()) {
        return InvalidArgument(
            "Deferred work handle is invalid");
    }
    if (flushing_) {
        return InvalidState(
            "Deferred work cannot be cancelled during Flush");
    }
    for (Record& record : records_) {
        if (record.handle.value != handle.value) {
            continue;
        }
        if (!record.cancelled) {
            record.cancelled = true;
            ++cancelled_;
        }
        return true;
    }
    return false;
}

Base::Result<std::uint32_t>
SafeDeferredWorkQueue::Flush() noexcept {
    if (flushing_) {
        return InvalidState(
            "Nested deferred-work Flush is not allowed");
    }
    flushing_ = true;
    std::uint32_t invoked = 0U;
    Base::Status firstFailure;

    for (Record& record : records_) {
        if (record.cancelled) {
            continue;
        }
        Base::Ref<Base::Object> strong =
            record.object.Lock();
        if (!strong) {
            ++expired_;
            continue;
        }
        Base::Result<void> result =
            record.callback(*strong, record.context);
        if (!result) {
            ++failed_;
            if (firstFailure.IsOk()) {
                firstFailure = result.GetStatus();
            }
            continue;
        }
        ++executed_;
        ++invoked;
    }

    records_.Clear();
    flushing_ = false;
    if (!firstFailure.IsOk()) {
        return firstFailure;
    }
    return invoked;
}

void SafeDeferredWorkQueue::Clear() noexcept {
    if (!flushing_) {
        records_.Clear();
    }
}

DeferredWorkStatistics
SafeDeferredWorkQueue::Statistics() const noexcept {
    DeferredWorkStatistics result;
    result.queued = queued_;
    result.executed = executed_;
    result.expired = expired_;
    result.cancelled = cancelled_;
    result.failed = failed_;
    result.pending = records_.Size();
    return result;
}

EventRouteLifetimeSnapshot::EventRouteLifetimeSnapshot(
    Base::IAllocator* allocator) noexcept
    : nodes_(allocator) {}

Base::Result<void> EventRouteLifetimeSnapshot::Add(
    Aero::Media::Visual& visual) noexcept {
    return nodes_.PushBack(
        Base::Ref<Aero::Media::Visual>::FromBorrowed(
            visual));
}

} // namespace Aero
