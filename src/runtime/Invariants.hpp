#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Visual.hpp>

#include <cstdint>

namespace Aero {

using MutationRollbackCallback = void (*)(void* context) noexcept;

struct MutationRollbackAction  {
    MutationRollbackCallback rollback = nullptr;
    void* context = nullptr;
};

// A small failure-atomic journal. Callers register compensation before
// publishing each externally visible mutation. Unless Commit() is called, the
// journal unwinds in reverse order, including on early Result returns.
class AERO_API MutationJournal  {
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
class AERO_API SafeDeferredWorkQueue  {
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
class AERO_API EventRouteLifetimeSnapshot  {
public:
    explicit EventRouteLifetimeSnapshot(
        Base::IAllocator* allocator = nullptr) noexcept;

    Base::Result<void> Add(
        Aero::Visual& visual) noexcept;
    void Clear() noexcept { nodes_.Clear(); }

    std::uint32_t Size() const noexcept { return nodes_.Size(); }
    Aero::Visual* operator[](
        std::uint32_t index) const noexcept {
        return index < nodes_.Size() ? nodes_[index].Get() : nullptr;
    }

private:
    Base::Vector<Base::Ref<Aero::Visual>> nodes_;
};

} // namespace Aero
