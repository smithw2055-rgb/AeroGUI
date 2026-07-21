#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>

#include <cstdint>
#include <mutex>

namespace Aero::Core {

using DispatcherTime = std::uint64_t;
using DispatcherThreadToken = std::uint64_t;

inline constexpr std::uint32_t UnlimitedDispatcherCallbacks = UINT32_MAX;

enum class DispatcherPriority : std::uint8_t {
    Send = 0U,
    Input,
    DataBind,
    Animation,
    Layout,
    RenderCommit,
    Loaded,
    Normal,
    Background,
    Idle,
    Count
};

enum class DispatcherFramePhase : std::uint8_t {
    BeginFrame = 0U,
    Input,
    PropertyChanges,
    DataBind,
    Animation,
    Layout,
    Lifecycle,
    RenderCommit,
    EndFrame,
    Count
};

struct DispatcherTaskHandle final {
    std::uint64_t value = 0U;

    AERO_NODISCARD constexpr bool IsValid() const noexcept {
        return value != 0U;
    }
};

struct DispatcherFrameHookHandle final {
    std::uint64_t value = 0U;

    AERO_NODISCARD constexpr bool IsValid() const noexcept {
        return value != 0U;
    }
};

AERO_NODISCARD constexpr bool operator==(
    DispatcherTaskHandle left, DispatcherTaskHandle right) noexcept {
    return left.value == right.value;
}

AERO_NODISCARD constexpr bool operator!=(
    DispatcherTaskHandle left, DispatcherTaskHandle right) noexcept {
    return !(left == right);
}

AERO_NODISCARD constexpr bool operator==(
    DispatcherFrameHookHandle left,
    DispatcherFrameHookHandle right) noexcept {
    return left.value == right.value;
}

AERO_NODISCARD constexpr bool operator!=(
    DispatcherFrameHookHandle left,
    DispatcherFrameHookHandle right) noexcept {
    return !(left == right);
}

using DispatcherCallback = void (*)(void* context) noexcept;
using DispatcherCleanupCallback = void (*)(void* context) noexcept;
using DispatcherNowCallback = DispatcherTime (*)(void* context) noexcept;
using DispatcherWakeCallback = void (*)(void* context) noexcept;

struct DispatcherOptions final {
    Base::IAllocator* allocator = nullptr;
    DispatcherNowCallback now = nullptr;
    void* clockContext = nullptr;
    DispatcherWakeCallback wake = nullptr;
    void* wakeContext = nullptr;
};

class Dispatcher;

class AERO_API DispatcherReentrancyGuard final {
public:
    DispatcherReentrancyGuard() noexcept = default;
    DispatcherReentrancyGuard(
        DispatcherReentrancyGuard&& other) noexcept;
    DispatcherReentrancyGuard& operator=(
        DispatcherReentrancyGuard&& other) noexcept;
    ~DispatcherReentrancyGuard();

    DispatcherReentrancyGuard(
        const DispatcherReentrancyGuard&) = delete;
    DispatcherReentrancyGuard& operator=(
        const DispatcherReentrancyGuard&) = delete;

    AERO_NODISCARD bool Active() const noexcept {
        return dispatcher_ != nullptr;
    }

    void Release() noexcept;

private:
    friend class Dispatcher;

    explicit DispatcherReentrancyGuard(
        Dispatcher* dispatcher) noexcept
        : dispatcher_(dispatcher) {}

    Dispatcher* dispatcher_ = nullptr;
};

AERO_NODISCARD AERO_API DispatcherThreadToken
CurrentDispatcherThreadToken() noexcept;

class AERO_API Dispatcher final {
public:
    explicit Dispatcher(
        const DispatcherOptions& options = DispatcherOptions{}) noexcept;
    ~Dispatcher() noexcept;

    Dispatcher(const Dispatcher&) = delete;
    Dispatcher& operator=(const Dispatcher&) = delete;
    Dispatcher(Dispatcher&&) = delete;
    Dispatcher& operator=(Dispatcher&&) = delete;

    AERO_NODISCARD bool CheckAccess() const noexcept;
    AERO_NODISCARD Base::Result<void> VerifyAccess() const noexcept;
    AERO_NODISCARD DispatcherThreadToken OwnerThreadToken() const noexcept {
        return ownerThread_;
    }

    AERO_NODISCARD DispatcherTime NowMicroseconds() const noexcept;
    AERO_NODISCARD Base::IAllocator& Allocator() const noexcept {
        return *allocator_;
    }

    AERO_NODISCARD Base::Result<DispatcherTaskHandle> Post(
        DispatcherPriority priority,
        DispatcherCallback callback,
        void* context = nullptr,
        DispatcherCleanupCallback cleanup = nullptr) noexcept;

    AERO_NODISCARD Base::Result<DispatcherTaskHandle> PostDelayed(
        DispatcherTime delayMicroseconds,
        DispatcherPriority priority,
        DispatcherCallback callback,
        void* context = nullptr,
        DispatcherCleanupCallback cleanup = nullptr) noexcept;

    AERO_NODISCARD Base::Result<DispatcherTaskHandle> PostAt(
        DispatcherTime dueTimeMicroseconds,
        DispatcherPriority priority,
        DispatcherCallback callback,
        void* context = nullptr,
        DispatcherCleanupCallback cleanup = nullptr) noexcept;

    // Cancel may be called from any thread. If cancellation wins the race with
    // execution, cleanup is invoked exactly once on the cancelling thread.
    AERO_NODISCARD bool Cancel(
        DispatcherTaskHandle handle) noexcept;

    // Processes ready callbacks from Send through throughPriority. The host
    // controls when this is called; Dispatcher never creates a worker thread.
    AERO_NODISCARD Base::Result<std::uint32_t> ProcessPending(
        DispatcherPriority throughPriority = DispatcherPriority::Idle,
        std::uint32_t maxCallbacks =
            UnlimitedDispatcherCallbacks) noexcept;

    AERO_NODISCARD Base::Result<DispatcherFrameHookHandle>
    RegisterFrameHook(
        DispatcherFramePhase phase,
        DispatcherCallback callback,
        void* context = nullptr,
        DispatcherCleanupCallback cleanup = nullptr) noexcept;

    AERO_NODISCARD Base::Result<bool> RemoveFrameHook(
        DispatcherFrameHookHandle handle) noexcept;

    // Hooks run in registration order. Hooks added while a phase is running
    // are deferred until the next invocation of that phase.
    AERO_NODISCARD Base::Result<std::uint32_t> RunFramePhase(
        DispatcherFramePhase phase) noexcept;

    AERO_NODISCARD Base::Result<DispatcherReentrancyGuard>
    EnterReentrancyGuard() noexcept;

    AERO_NODISCARD std::uint32_t PendingTaskCount() const noexcept;
    AERO_NODISCARD std::uint32_t RegisteredFrameHookCount() const noexcept;
    AERO_NODISCARD bool IsPumping() const noexcept;
    AERO_NODISCARD std::uint32_t ReentrancyDepth() const noexcept;

private:
    friend class DispatcherReentrancyGuard;

    enum class RecordState : std::uint8_t {
        Pending,
        Cancelled,
        Finished
    };

    struct TaskRecord final {
        DispatcherTaskHandle handle;
        std::uint64_t sequence = 0U;
        DispatcherTime dueTimeMicroseconds = 0U;
        DispatcherPriority priority = DispatcherPriority::Normal;
        DispatcherCallback callback = nullptr;
        DispatcherCleanupCallback cleanup = nullptr;
        void* context = nullptr;
        RecordState state = RecordState::Pending;
    };

    struct FrameHookRecord final {
        DispatcherFrameHookHandle handle;
        std::uint64_t sequence = 0U;
        DispatcherFramePhase phase = DispatcherFramePhase::BeginFrame;
        DispatcherCallback callback = nullptr;
        DispatcherCleanupCallback cleanup = nullptr;
        void* context = nullptr;
        RecordState state = RecordState::Pending;
    };

    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<TaskRecord> ready_;
    Base::Vector<TaskRecord> delayed_;
    Base::Vector<FrameHookRecord> hooks_;
    mutable std::mutex mutex_;

    std::uint32_t readyHead_ = 0U;
    std::uint32_t delayedHead_ = 0U;
    DispatcherThreadToken ownerThread_ = 0U;
    DispatcherNowCallback now_ = nullptr;
    void* clockContext_ = nullptr;
    DispatcherWakeCallback wake_ = nullptr;
    void* wakeContext_ = nullptr;
    std::uint64_t nextTaskHandle_ = 1U;
    std::uint64_t nextTaskSequence_ = 1U;
    std::uint64_t nextHookHandle_ = 1U;
    std::uint64_t nextHookSequence_ = 1U;
    DispatcherFrameHookHandle activeHook_;
    std::uint32_t guardDepth_ = 0U;
    bool pumping_ = false;
    bool phaseActive_ = false;
    bool shuttingDown_ = false;

    AERO_NODISCARD Base::Result<DispatcherTaskHandle> Enqueue(
        DispatcherTime dueTimeMicroseconds,
        bool delayed,
        DispatcherPriority priority,
        DispatcherCallback callback,
        void* context,
        DispatcherCleanupCallback cleanup) noexcept;

    AERO_NODISCARD Base::Result<void> InsertReadyLocked(
        const TaskRecord& record) noexcept;
    AERO_NODISCARD Base::Result<void> InsertDelayedLocked(
        const TaskRecord& record) noexcept;
    AERO_NODISCARD Base::Result<void> PromoteDueLocked(
        DispatcherTime nowMicroseconds) noexcept;

    void CompactReadyLocked(bool force) noexcept;
    void CompactDelayedLocked(bool force) noexcept;
    void CompactHooksLocked() noexcept;
    void DiscardCompletedReadyPrefixLocked() noexcept;
    void DiscardCompletedDelayedPrefixLocked() noexcept;
    void LeaveReentrancyGuard() noexcept;
    void NotifyWake() const noexcept;

    AERO_NODISCARD static bool IsValidPriority(
        DispatcherPriority priority) noexcept;
    AERO_NODISCARD static bool IsValidFramePhase(
        DispatcherFramePhase phase) noexcept;
    AERO_NODISCARD static bool ReadyLess(
        const TaskRecord& left,
        const TaskRecord& right) noexcept;
    AERO_NODISCARD static bool DelayedLess(
        const TaskRecord& left,
        const TaskRecord& right) noexcept;
};

class AERO_API DispatcherObject : public Base::Object {
public:
    AERO_NODISCARD bool CheckAccess() const noexcept;
    AERO_NODISCARD Base::Result<void> VerifyAccess() const noexcept;
    AERO_NODISCARD Dispatcher& GetDispatcher() const noexcept;

protected:
    explicit DispatcherObject(Dispatcher& dispatcher) noexcept;
    ~DispatcherObject() override = default;

private:
    Dispatcher* dispatcher_ = nullptr;
};

} // namespace Aero::Core
