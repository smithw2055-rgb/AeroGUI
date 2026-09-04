#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DispatcherReentrancyGuard.hpp>
#include <Aero/PropertySlab.hpp>

#include <cstdint>
#include <mutex>

namespace Aero::Threading {

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

inline constexpr std::uint32_t
    DispatcherFramePhaseCount =
        static_cast<std::uint32_t>(
            DispatcherFramePhase::Count);

struct DispatcherFrameTimings  {
    std::uint64_t frameSequence = 0U;
    DispatcherTime totalMicroseconds = 0U;
    DispatcherTime phaseMicroseconds[
        DispatcherFramePhaseCount]{};
};

struct DispatcherTaskHandle  {
    std::uint64_t value = 0U;

    constexpr bool IsValid() const noexcept {
        return value != 0U;
    }
};

constexpr bool operator==(
    DispatcherTaskHandle left, DispatcherTaskHandle right) noexcept {
    return left.value == right.value;
}

constexpr bool operator!=(
    DispatcherTaskHandle left, DispatcherTaskHandle right) noexcept {
    return !(left == right);
}

using DispatcherCallback = void (*)(void* context) noexcept;
using DispatcherCleanupCallback = void (*)(void* context) noexcept;
using DispatcherNowCallback = DispatcherTime (*)(void* context) noexcept;
using DispatcherWakeCallback = void (*)(void* context) noexcept;

struct DispatcherOptions  {
    DispatcherNowCallback now = nullptr;
    void* clockContext = nullptr;
    DispatcherWakeCallback wake = nullptr;
    void* wakeContext = nullptr;
};

class Dispatcher;

AERO_GUI_API DispatcherThreadToken
CurrentDispatcherThreadToken() noexcept;

class AERO_GUI_API Dispatcher  {
public:
    explicit Dispatcher(
        const DispatcherOptions& options = DispatcherOptions{}) noexcept;
    ~Dispatcher() noexcept;

    Dispatcher(const Dispatcher&) = delete;
    Dispatcher& operator=(const Dispatcher&) = delete;
    Dispatcher(Dispatcher&&) = delete;
    Dispatcher& operator=(Dispatcher&&) = delete;

    bool CheckAccess() const noexcept;
    Result<void> VerifyAccess() const noexcept;
    DispatcherThreadToken OwnerThreadToken() const noexcept {
        return ownerThread_;
    }

    DispatcherTime NowMicroseconds() const noexcept;
    Result<DispatcherTaskHandle> Post(
        DispatcherPriority priority,
        DispatcherCallback callback,
        void* context = nullptr,
        DispatcherCleanupCallback cleanup = nullptr) noexcept;

    Result<DispatcherTaskHandle> PostDelayed(
        DispatcherTime delayMicroseconds,
        DispatcherPriority priority,
        DispatcherCallback callback,
        void* context = nullptr,
        DispatcherCleanupCallback cleanup = nullptr) noexcept;

    Result<DispatcherTaskHandle> PostAt(
        DispatcherTime dueTimeMicroseconds,
        DispatcherPriority priority,
        DispatcherCallback callback,
        void* context = nullptr,
        DispatcherCleanupCallback cleanup = nullptr) noexcept;

    // Cancel may be called from any thread. If cancellation wins the race with
    // execution, cleanup is invoked exactly once on the cancelling thread.
    bool Cancel(
        DispatcherTaskHandle handle) noexcept;

    // Pumps queued callbacks in FIFO order. throughPriority is an admission
    // filter (P3.1): callbacks posted at a priority above throughPriority
    // stay queued for a later pump. Ordering within the admitted set is
    // always FIFO; the former 10-level sorted insertion is gone.
    // Processes ready callbacks from Send through throughPriority. The host
    // controls when this is called; Dispatcher never creates a worker thread.
    Result<std::uint32_t> ProcessPending(
        DispatcherPriority throughPriority = DispatcherPriority::Idle,
        std::uint32_t maxCallbacks =
            UnlimitedDispatcherCallbacks) noexcept;

    // Snapshot of last recorded frame-phase durations. ViewFrame drives
    // engines directly, so this remains zero unless a host fills it.
    DispatcherFrameTimings
    FrameTimings() const noexcept;

    Result<DispatcherReentrancyGuard>
    EnterReentrancyGuard() noexcept;

    std::uint32_t PendingTaskCount() const noexcept;
    bool IsPumping() const noexcept;
    std::uint32_t ReentrancyDepth() const noexcept;

    // P2.4: Dispatcher-owned slab for dependency-property storage blocks
    // (PropertyStore / StoredValueRare). Lifetime is strictly bound to the
    // Dispatcher; pooled blocks never outlive it.
    PropertySlab& GetPropertySlab() noexcept {
        return propertySlab_;
    }

private:
    friend class DispatcherReentrancyGuard;

    enum class RecordState : std::uint8_t {
        Pending,
        Cancelled,
        Finished
    };

    struct TaskRecord  {
        DispatcherTaskHandle handle;
        DispatcherTime dueTimeMicroseconds = 0U;
        // Admission filter for ProcessPending(throughPriority). Never used
        // for ordering (P3.1: the ready queue is pure FIFO).
        DispatcherPriority priority = DispatcherPriority::Normal;
        DispatcherCallback callback = nullptr;
        DispatcherCleanupCallback cleanup = nullptr;
        void* context = nullptr;
        RecordState state = RecordState::Pending;
    };

    Base::Vector<TaskRecord> ready_;
    Base::Vector<TaskRecord> delayed_;
    mutable std::mutex mutex_;
    PropertySlab propertySlab_;

    std::uint32_t readyHead_ = 0U;
    std::uint32_t delayedHead_ = 0U;
    DispatcherThreadToken ownerThread_ = 0U;
    DispatcherNowCallback now_ = nullptr;
    void* clockContext_ = nullptr;
    DispatcherWakeCallback wake_ = nullptr;
    void* wakeContext_ = nullptr;
    std::uint64_t nextTaskHandle_ = 1U;
    std::uint32_t guardDepth_ = 0U;
    bool pumping_ = false;
    bool shuttingDown_ = false;

    Result<DispatcherTaskHandle> Enqueue(
        DispatcherTime dueTimeMicroseconds,
        bool delayed,
        DispatcherPriority priority,
        DispatcherCallback callback,
        void* context,
        DispatcherCleanupCallback cleanup) noexcept;

    Result<void> InsertReadyLocked(
        const TaskRecord& record) noexcept;
    Result<void> InsertDelayedLocked(
        const TaskRecord& record) noexcept;
    Result<void> PromoteDueLocked(
        DispatcherTime nowMicroseconds) noexcept;

    void CompactReadyLocked(bool force) noexcept;
    void CompactDelayedLocked(bool force) noexcept;
    void DiscardCompletedReadyPrefixLocked() noexcept;
    void DiscardCompletedDelayedPrefixLocked() noexcept;
    void LeaveReentrancyGuard() noexcept;
    void NotifyWake() const noexcept;

    static bool IsValidPriority(
        DispatcherPriority priority) noexcept;
};

} // namespace Aero::Threading

#include <Aero/DispatcherObject.hpp>
