#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Visual.hpp>

#include <cstdint>
#include <utility>

// ---- Merged from Invariants.hpp (single-TU helper; no other includers) ----
// Per SOURCE_ARCHITECTURE: helpers needed by one TU stay in that .cpp.
namespace Aero {

using MutationRollbackCallback = void (*)(void* context) noexcept;

struct MutationRollbackAction  {
    MutationRollbackCallback rollback = nullptr;
    void* context = nullptr;
};

// A small failure-atomic journal. Callers register compensation before
// publishing each externally visible mutation. Unless Commit() is called, the
// journal unwinds in reverse order, including on early Result returns.
class MutationJournal  {
public:
    explicit MutationJournal(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~MutationJournal() noexcept;

    MutationJournal(const MutationJournal&) = delete;
    MutationJournal& operator=(const MutationJournal&) = delete;

    Base::Result<void> AddRollback(
        MutationRollbackCallback rollback,
        void* context = nullptr) noexcept;
    void Commit() noexcept;
    void Rollback() noexcept;

    bool IsCommitted() const noexcept { return committed_; }
    std::uint32_t ActionCount() const noexcept {
        return actions_.Size();
    }

private:
    Base::Vector<MutationRollbackAction> actions_;
    bool committed_ = false;
    bool rollingBack_ = false;
};

struct DeferredWorkHandle  {
    std::uint64_t value = 0U;
    constexpr bool IsValid() const noexcept { return value != 0U; }
};

using DeferredObjectWorkCallback = Base::Result<void> (*)(
    Base::Object& object,
    void* context) noexcept;

struct DeferredWorkStatistics  {
    std::uint64_t queued = 0U;
    std::uint64_t executed = 0U;
    std::uint64_t expired = 0U;
    std::uint64_t cancelled = 0U;
    std::uint64_t failed = 0U;
    std::uint32_t pending = 0U;
};

// Deferred runtime work stores WeakRef rather than raw pointers. Destroyed
// objects are skipped deterministically, while a callback receives a strong
// reference for the entire invocation.
class SafeDeferredWorkQueue  {
public:
    explicit SafeDeferredWorkQueue(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~SafeDeferredWorkQueue() noexcept;

    SafeDeferredWorkQueue(const SafeDeferredWorkQueue&) = delete;
    SafeDeferredWorkQueue& operator=(
        const SafeDeferredWorkQueue&) = delete;

    Base::Result<DeferredWorkHandle> Enqueue(
        Base::Object& object,
        DeferredObjectWorkCallback callback,
        void* context = nullptr) noexcept;
    Base::Result<bool> Cancel(
        DeferredWorkHandle handle) noexcept;
    Base::Result<std::uint32_t> Flush() noexcept;
    void Clear() noexcept;

    DeferredWorkStatistics Statistics() const noexcept;

private:
    struct Record  {
        DeferredWorkHandle handle;
        Base::WeakRef<Base::Object> object;
        DeferredObjectWorkCallback callback = nullptr;
        void* context = nullptr;
        bool cancelled = false;
    };

    Base::Vector<Record> records_;
    std::uint64_t nextHandle_ = 1U;
    std::uint64_t queued_ = 0U;
    std::uint64_t executed_ = 0U;
    std::uint64_t expired_ = 0U;
    std::uint64_t cancelled_ = 0U;
    std::uint64_t failed_ = 0U;
    bool flushing_ = false;
};

// Strong route snapshot used when dispatch must tolerate handlers detaching or
// releasing nodes. Each node remains alive until the snapshot is destroyed.
class EventRouteLifetimeSnapshot  {
public:
    explicit EventRouteLifetimeSnapshot(
        Base::IAllocator* allocator = nullptr) noexcept;

    Base::Result<void> Add(
        Aero::Media::Visual& visual) noexcept;
    void Clear() noexcept { nodes_.Clear(); }

    std::uint32_t Size() const noexcept { return nodes_.Size(); }
    Aero::Media::Visual* operator[](
        std::uint32_t index) const noexcept {
        return index < nodes_.Size() ? nodes_[index].Get() : nullptr;
    }

private:
    Base::Vector<Base::Ref<Aero::Media::Visual>> nodes_;
};

} // namespace Aero

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
    actions_.PushBack({rollback, context});
    return {};
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
    records_.PushBack(std::move(record));
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
    nodes_.PushBack(
        Base::Ref<Aero::Media::Visual>::FromBorrowed(
            visual));
    return {};
}

} // namespace Aero
