#include <Aero/Core/Dispatcher.hpp>

#include <Aero/Base/Assert.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <utility>

namespace Aero::Core {
namespace {

std::atomic<DispatcherThreadToken> gNextThreadToken{1U};

DispatcherThreadToken AllocateThreadToken() noexcept {
    const DispatcherThreadToken token =
        gNextThreadToken.fetch_add(1U, std::memory_order_relaxed);
    if (token == 0U ||
        token == std::numeric_limits<DispatcherThreadToken>::max()) {
        std::abort();
    }
    return token;
}

thread_local const DispatcherThreadToken gCurrentThreadToken =
    AllocateThreadToken();

DispatcherTime DefaultNowMicroseconds(
    void*) noexcept {
    using Clock = std::chrono::steady_clock;
    using Microseconds = std::chrono::microseconds;
    const auto count = std::chrono::duration_cast<Microseconds>(
        Clock::now().time_since_epoch()).count();
    return count > 0
        ? static_cast<DispatcherTime>(count)
        : DispatcherTime{0U};
}

constexpr Base::Status WrongThreadStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::WrongThread,
        "Dispatcher access is restricted to its owner thread");
}

constexpr Base::Status InvalidDispatcherStateStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Dispatcher cannot perform the operation in its current state");
}

constexpr Base::Status DispatcherShuttingDownStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Dispatcher is shutting down");
}

constexpr Base::Status InvalidPriorityStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        "Dispatcher priority is invalid");
}

constexpr Base::Status InvalidPhaseStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        "Dispatcher frame phase is invalid");
}

constexpr Base::Status InvalidCallbackStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        "Dispatcher callback must not be null");
}

constexpr Base::Status TokenExhaustedStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfRange,
        "Dispatcher handle or sequence space is exhausted");
}

constexpr Base::Status DelayOverflowStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfRange,
        "Dispatcher delayed due time overflows");
}

struct CleanupInvocation final {
    DispatcherCleanupCallback callback = nullptr;
    void* context = nullptr;
};

} // namespace

DispatcherThreadToken CurrentDispatcherThreadToken() noexcept {
    return gCurrentThreadToken;
}

DispatcherReentrancyGuard::DispatcherReentrancyGuard(
    DispatcherReentrancyGuard&& other) noexcept
    : dispatcher_(other.dispatcher_) {
    other.dispatcher_ = nullptr;
}

DispatcherReentrancyGuard& DispatcherReentrancyGuard::operator=(
    DispatcherReentrancyGuard&& other) noexcept {
    if (this != &other) {
        Release();
        dispatcher_ = other.dispatcher_;
        other.dispatcher_ = nullptr;
    }
    return *this;
}

DispatcherReentrancyGuard::~DispatcherReentrancyGuard() {
    Release();
}

void DispatcherReentrancyGuard::Release() noexcept {
    Dispatcher* dispatcher = dispatcher_;
    dispatcher_ = nullptr;
    if (dispatcher != nullptr) {
        dispatcher->LeaveReentrancyGuard();
    }
}

Dispatcher::Dispatcher(const DispatcherOptions& options) noexcept
    : ready_(),
      delayed_(),
      hooks_(),
      ownerThread_(CurrentDispatcherThreadToken()),
      now_(options.now != nullptr
          ? options.now
          : &DefaultNowMicroseconds),
      clockContext_(options.clockContext),
      wake_(options.wake),
      wakeContext_(options.wakeContext) {}

Dispatcher::~Dispatcher() noexcept {
    AERO_ASSERT(CheckAccess());

    {
        std::lock_guard<std::mutex> lock(mutex_);
        AERO_ASSERT(!pumping_);
        AERO_ASSERT(!phaseActive_);
        AERO_ASSERT(guardDepth_ == 0U);
        shuttingDown_ = true;

        for (std::uint32_t index = readyHead_;
             index < ready_.Size();
             ++index) {
            TaskRecord& record = ready_[index];
            if (record.state == RecordState::Pending) {
                record.state = RecordState::Cancelled;
                record.callback = nullptr;
            }
        }
        for (std::uint32_t index = delayedHead_;
             index < delayed_.Size();
             ++index) {
            TaskRecord& record = delayed_[index];
            if (record.state == RecordState::Pending) {
                record.state = RecordState::Cancelled;
                record.callback = nullptr;
            }
        }
        for (std::uint32_t index = 0U;
             index < hooks_.Size();
             ++index) {
            FrameHookRecord& record = hooks_[index];
            if (record.state == RecordState::Pending) {
                record.state = RecordState::Cancelled;
                record.callback = nullptr;
            }
        }
    }

    for (;;) {
        CleanupInvocation invocation;
        {
            std::lock_guard<std::mutex> lock(mutex_);

            for (std::uint32_t index = readyHead_;
                 index < ready_.Size() && invocation.callback == nullptr;
                 ++index) {
                TaskRecord& record = ready_[index];
                if (record.cleanup != nullptr) {
                    invocation.callback = record.cleanup;
                    invocation.context = record.context;
                    record.cleanup = nullptr;
                    record.context = nullptr;
                }
            }
            for (std::uint32_t index = delayedHead_;
                 index < delayed_.Size() && invocation.callback == nullptr;
                 ++index) {
                TaskRecord& record = delayed_[index];
                if (record.cleanup != nullptr) {
                    invocation.callback = record.cleanup;
                    invocation.context = record.context;
                    record.cleanup = nullptr;
                    record.context = nullptr;
                }
            }
            for (std::uint32_t index = 0U;
                 index < hooks_.Size() && invocation.callback == nullptr;
                 ++index) {
                FrameHookRecord& record = hooks_[index];
                if (record.cleanup != nullptr) {
                    invocation.callback = record.cleanup;
                    invocation.context = record.context;
                    record.cleanup = nullptr;
                    record.context = nullptr;
                }
            }
        }

        if (invocation.callback == nullptr) {
            break;
        }
        invocation.callback(invocation.context);
    }
}

bool Dispatcher::CheckAccess() const noexcept {
    return CurrentDispatcherThreadToken() == ownerThread_;
}

Base::Result<void> Dispatcher::VerifyAccess() const noexcept {
    return CheckAccess()
        ? Base::Result<void>()
        : Base::Result<void>(WrongThreadStatus());
}

DispatcherTime Dispatcher::NowMicroseconds() const noexcept {
    return now_(clockContext_);
}

Base::Result<DispatcherTaskHandle> Dispatcher::Post(
    DispatcherPriority priority,
    DispatcherCallback callback,
    void* context,
    DispatcherCleanupCallback cleanup) noexcept {
    return Enqueue(
        NowMicroseconds(),
        false,
        priority,
        callback,
        context,
        cleanup);
}

Base::Result<DispatcherTaskHandle> Dispatcher::PostDelayed(
    DispatcherTime delayMicroseconds,
    DispatcherPriority priority,
    DispatcherCallback callback,
    void* context,
    DispatcherCleanupCallback cleanup) noexcept {
    const DispatcherTime now = NowMicroseconds();
    if (delayMicroseconds >
        std::numeric_limits<DispatcherTime>::max() - now) {
        return DelayOverflowStatus();
    }

    const DispatcherTime due = now + delayMicroseconds;
    return Enqueue(
        due,
        delayMicroseconds != 0U,
        priority,
        callback,
        context,
        cleanup);
}

Base::Result<DispatcherTaskHandle> Dispatcher::PostAt(
    DispatcherTime dueTimeMicroseconds,
    DispatcherPriority priority,
    DispatcherCallback callback,
    void* context,
    DispatcherCleanupCallback cleanup) noexcept {
    const DispatcherTime now = NowMicroseconds();
    return Enqueue(
        dueTimeMicroseconds,
        dueTimeMicroseconds > now,
        priority,
        callback,
        context,
        cleanup);
}

Base::Result<DispatcherTaskHandle> Dispatcher::Enqueue(
    DispatcherTime dueTimeMicroseconds,
    bool delayed,
    DispatcherPriority priority,
    DispatcherCallback callback,
    void* context,
    DispatcherCleanupCallback cleanup) noexcept {
    if (!IsValidPriority(priority)) {
        return InvalidPriorityStatus();
    }
    if (callback == nullptr) {
        return InvalidCallbackStatus();
    }

    DispatcherTaskHandle handle;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shuttingDown_) {
            return DispatcherShuttingDownStatus();
        }
        if (nextTaskHandle_ == 0U ||
            nextTaskHandle_ ==
                std::numeric_limits<std::uint64_t>::max() ||
            nextTaskSequence_ == 0U ||
            nextTaskSequence_ ==
                std::numeric_limits<std::uint64_t>::max()) {
            return TokenExhaustedStatus();
        }

        TaskRecord record;
        record.handle.value = nextTaskHandle_;
        record.sequence = nextTaskSequence_;
        record.dueTimeMicroseconds = dueTimeMicroseconds;
        record.priority = priority;
        record.callback = callback;
        record.cleanup = cleanup;
        record.context = context;
        record.state = RecordState::Pending;

        const Base::Result<void> insertResult = delayed
            ? InsertDelayedLocked(record)
            : InsertReadyLocked(record);
        if (!insertResult) {
            return insertResult.GetStatus();
        }

        handle = record.handle;
        ++nextTaskHandle_;
        ++nextTaskSequence_;
    }

    NotifyWake();
    return handle;
}

bool Dispatcher::Cancel(
    DispatcherTaskHandle handle) noexcept {
    if (!handle.IsValid()) {
        return false;
    }

    CleanupInvocation cleanup;
    bool cancelled = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shuttingDown_) {
            return false;
        }

        for (std::uint32_t index = readyHead_;
             index < ready_.Size();
             ++index) {
            TaskRecord& record = ready_[index];
            if (record.state == RecordState::Pending &&
                record.handle == handle) {
                record.state = RecordState::Cancelled;
                record.callback = nullptr;
                cleanup.callback = record.cleanup;
                cleanup.context = record.context;
                record.cleanup = nullptr;
                record.context = nullptr;
                cancelled = true;
                break;
            }
        }

        if (!cancelled) {
            for (std::uint32_t index = delayedHead_;
                 index < delayed_.Size();
                 ++index) {
                TaskRecord& record = delayed_[index];
                if (record.state == RecordState::Pending &&
                    record.handle == handle) {
                    record.state = RecordState::Cancelled;
                    record.callback = nullptr;
                    cleanup.callback = record.cleanup;
                    cleanup.context = record.context;
                    record.cleanup = nullptr;
                    record.context = nullptr;
                    cancelled = true;
                    break;
                }
            }
        }

        if (cancelled) {
            CompactReadyLocked(true);
            CompactDelayedLocked(true);
        }
    }

    if (cleanup.callback != nullptr) {
        cleanup.callback(cleanup.context);
    }
    if (cancelled) {
        NotifyWake();
    }
    return cancelled;
}

Base::Result<std::uint32_t> Dispatcher::ProcessPending(
    DispatcherPriority throughPriority,
    std::uint32_t maxCallbacks) noexcept {
    if (!IsValidPriority(throughPriority)) {
        return InvalidPriorityStatus();
    }

    const Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access.GetStatus();
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shuttingDown_) {
            return DispatcherShuttingDownStatus();
        }
        if (pumping_ || phaseActive_ || guardDepth_ != 0U) {
            return InvalidDispatcherStateStatus();
        }
        pumping_ = true;
    }

    std::uint32_t callbackCount = 0U;
    Base::Status terminalStatus = Base::Status::Ok();

    while (callbackCount < maxCallbacks) {
        TaskRecord invocation;
        bool hasInvocation = false;
        {
            const DispatcherTime now = NowMicroseconds();
            std::lock_guard<std::mutex> lock(mutex_);

            const Base::Result<void> promoteResult =
                PromoteDueLocked(now);
            if (!promoteResult) {
                terminalStatus = promoteResult.GetStatus();
            } else {
                DiscardCompletedReadyPrefixLocked();
                if (readyHead_ < ready_.Size()) {
                    TaskRecord& record = ready_[readyHead_];
                    const std::uint8_t recordPriority =
                        static_cast<std::uint8_t>(record.priority);
                    const std::uint8_t allowedPriority =
                        static_cast<std::uint8_t>(throughPriority);
                    if (recordPriority <= allowedPriority) {
                        invocation = record;
                        record.state = RecordState::Finished;
                        record.callback = nullptr;
                        record.cleanup = nullptr;
                        record.context = nullptr;
                        ++readyHead_;
                        CompactReadyLocked(false);
                        hasInvocation = true;
                    }
                }
            }
        }

        if (!terminalStatus.IsOk() || !hasInvocation) {
            break;
        }

        invocation.callback(invocation.context);
        if (invocation.cleanup != nullptr) {
            invocation.cleanup(invocation.context);
        }
        ++callbackCount;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (guardDepth_ != 0U) {
                break;
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        pumping_ = false;
    }

    return terminalStatus.IsOk()
        ? Base::Result<std::uint32_t>(callbackCount)
        : Base::Result<std::uint32_t>(terminalStatus);
}

Base::Result<DispatcherFrameHookHandle>
Dispatcher::RegisterFrameHook(
    DispatcherFramePhase phase,
    DispatcherCallback callback,
    void* context,
    DispatcherCleanupCallback cleanup) noexcept {
    if (!IsValidFramePhase(phase)) {
        return InvalidPhaseStatus();
    }
    if (callback == nullptr) {
        return InvalidCallbackStatus();
    }

    const Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access.GetStatus();
    }

    DispatcherFrameHookHandle handle;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shuttingDown_) {
            return DispatcherShuttingDownStatus();
        }
        if (nextHookHandle_ == 0U ||
            nextHookHandle_ ==
                std::numeric_limits<std::uint64_t>::max() ||
            nextHookSequence_ == 0U ||
            nextHookSequence_ ==
                std::numeric_limits<std::uint64_t>::max()) {
            return TokenExhaustedStatus();
        }

        FrameHookRecord record;
        record.handle.value = nextHookHandle_;
        record.sequence = nextHookSequence_;
        record.phase = phase;
        record.callback = callback;
        record.cleanup = cleanup;
        record.context = context;
        record.state = RecordState::Pending;

        const Base::Result<void> appendResult =
            hooks_.TryPushBack(record);
        if (!appendResult) {
            return appendResult.GetStatus();
        }

        handle = record.handle;
        ++nextHookHandle_;
        ++nextHookSequence_;
    }

    NotifyWake();
    return handle;
}

Base::Result<bool> Dispatcher::RemoveFrameHook(
    DispatcherFrameHookHandle handle) noexcept {
    const Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access.GetStatus();
    }
    if (!handle.IsValid()) {
        return false;
    }

    CleanupInvocation cleanup;
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shuttingDown_) {
            return DispatcherShuttingDownStatus();
        }

        for (std::uint32_t index = 0U;
             index < hooks_.Size();
             ++index) {
            FrameHookRecord& record = hooks_[index];
            if (record.state == RecordState::Pending &&
                record.handle == handle) {
                record.state = RecordState::Cancelled;
                record.callback = nullptr;
                if (activeHook_ != handle) {
                    cleanup.callback = record.cleanup;
                    cleanup.context = record.context;
                    record.cleanup = nullptr;
                    record.context = nullptr;
                }
                removed = true;
                break;
            }
        }

        if (removed && !phaseActive_) {
            CompactHooksLocked();
        }
    }

    if (cleanup.callback != nullptr) {
        cleanup.callback(cleanup.context);
    }
    return removed;
}

Base::Result<std::uint32_t> Dispatcher::RunFramePhase(
    DispatcherFramePhase phase) noexcept {
    if (!IsValidFramePhase(phase)) {
        return InvalidPhaseStatus();
    }

    const Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access.GetStatus();
    }

    std::uint32_t hookLimit = 0U;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shuttingDown_) {
            return DispatcherShuttingDownStatus();
        }
        if (phaseActive_ || pumping_ || guardDepth_ != 0U) {
            return InvalidDispatcherStateStatus();
        }
        phaseActive_ = true;
        activeHook_ = {};
        hookLimit = hooks_.Size();
    }

    std::uint32_t invoked = 0U;
    for (std::uint32_t index = 0U;
         index < hookLimit;
         ++index) {
        DispatcherCallback callback = nullptr;
        void* context = nullptr;
        DispatcherFrameHookHandle handle;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (index < hooks_.Size()) {
                FrameHookRecord& record = hooks_[index];
                if (record.state == RecordState::Pending &&
                    record.phase == phase) {
                    callback = record.callback;
                    context = record.context;
                    handle = record.handle;
                    activeHook_ = handle;
                }
            }
        }

        if (callback == nullptr) {
            continue;
        }

        callback(context);
        ++invoked;

        CleanupInvocation cleanup;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (index < hooks_.Size()) {
                FrameHookRecord& record = hooks_[index];
                if (record.handle == handle &&
                    record.state == RecordState::Cancelled &&
                    record.cleanup != nullptr) {
                    cleanup.callback = record.cleanup;
                    cleanup.context = record.context;
                    record.cleanup = nullptr;
                    record.context = nullptr;
                }
            }
            activeHook_ = {};
        }
        if (cleanup.callback != nullptr) {
            cleanup.callback(cleanup.context);
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        activeHook_ = {};
        phaseActive_ = false;
        CompactHooksLocked();
    }
    return invoked;
}

Base::Result<DispatcherReentrancyGuard>
Dispatcher::EnterReentrancyGuard() noexcept {
    const Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access.GetStatus();
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shuttingDown_) {
            return DispatcherShuttingDownStatus();
        }
        if (guardDepth_ == UINT32_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Dispatcher reentrancy depth limit reached");
        }
        ++guardDepth_;
    }
    return DispatcherReentrancyGuard(this);
}

std::uint32_t Dispatcher::PendingTaskCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::uint32_t count = 0U;

    for (std::uint32_t index = readyHead_;
         index < ready_.Size();
         ++index) {
        if (ready_[index].state == RecordState::Pending) {
            ++count;
        }
    }
    for (std::uint32_t index = delayedHead_;
         index < delayed_.Size();
         ++index) {
        if (delayed_[index].state == RecordState::Pending) {
            ++count;
        }
    }
    return count;
}

std::uint32_t Dispatcher::RegisteredFrameHookCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::uint32_t count = 0U;
    for (std::uint32_t index = 0U;
         index < hooks_.Size();
         ++index) {
        if (hooks_[index].state == RecordState::Pending) {
            ++count;
        }
    }
    return count;
}

bool Dispatcher::IsPumping() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return pumping_;
}

std::uint32_t Dispatcher::ReentrancyDepth() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return guardDepth_;
}

Base::Result<void> Dispatcher::InsertReadyLocked(
    const TaskRecord& record) noexcept {
    CompactReadyLocked(false);
    const Base::Result<void> appendResult =
        ready_.TryPushBack(record);
    if (!appendResult) {
        return appendResult.GetStatus();
    }

    std::uint32_t index = ready_.Size() - 1U;
    while (index > readyHead_ &&
           ReadyLess(ready_[index], ready_[index - 1U])) {
        std::swap(ready_[index], ready_[index - 1U]);
        --index;
    }
    return {};
}

Base::Result<void> Dispatcher::InsertDelayedLocked(
    const TaskRecord& record) noexcept {
    CompactDelayedLocked(false);
    const Base::Result<void> appendResult =
        delayed_.TryPushBack(record);
    if (!appendResult) {
        return appendResult.GetStatus();
    }

    std::uint32_t index = delayed_.Size() - 1U;
    while (index > delayedHead_ &&
           DelayedLess(delayed_[index], delayed_[index - 1U])) {
        std::swap(delayed_[index], delayed_[index - 1U]);
        --index;
    }
    return {};
}

Base::Result<void> Dispatcher::PromoteDueLocked(
    DispatcherTime nowMicroseconds) noexcept {
    DiscardCompletedDelayedPrefixLocked();

    while (delayedHead_ < delayed_.Size()) {
        TaskRecord& source = delayed_[delayedHead_];
        if (source.state != RecordState::Pending) {
            ++delayedHead_;
            continue;
        }
        if (source.dueTimeMicroseconds > nowMicroseconds) {
            break;
        }

        const Base::Result<void> insertResult =
            InsertReadyLocked(source);
        if (!insertResult) {
            return insertResult.GetStatus();
        }

        source.state = RecordState::Finished;
        source.callback = nullptr;
        source.cleanup = nullptr;
        source.context = nullptr;
        ++delayedHead_;
    }

    CompactDelayedLocked(false);
    return {};
}

void Dispatcher::CompactReadyLocked(bool force) noexcept {
    if (readyHead_ == 0U && !force) {
        return;
    }
    if (!force &&
        readyHead_ < 64U &&
        static_cast<std::uint64_t>(readyHead_) * 2U < ready_.Size()) {
        return;
    }

    std::uint32_t write = 0U;
    for (std::uint32_t read = readyHead_;
         read < ready_.Size();
         ++read) {
        TaskRecord& record = ready_[read];
        if (record.state == RecordState::Pending) {
            if (write != read) {
                ready_[write] = std::move(record);
            }
            ++write;
        }
    }

    while (ready_.Size() > write) {
        ready_.PopBack();
    }
    readyHead_ = 0U;
}

void Dispatcher::CompactDelayedLocked(bool force) noexcept {
    if (delayedHead_ == 0U && !force) {
        return;
    }
    if (!force &&
        delayedHead_ < 64U &&
        static_cast<std::uint64_t>(delayedHead_) * 2U < delayed_.Size()) {
        return;
    }

    std::uint32_t write = 0U;
    for (std::uint32_t read = delayedHead_;
         read < delayed_.Size();
         ++read) {
        TaskRecord& record = delayed_[read];
        if (record.state == RecordState::Pending) {
            if (write != read) {
                delayed_[write] = std::move(record);
            }
            ++write;
        }
    }

    while (delayed_.Size() > write) {
        delayed_.PopBack();
    }
    delayedHead_ = 0U;
}

void Dispatcher::CompactHooksLocked() noexcept {
    AERO_ASSERT(!phaseActive_);

    std::uint32_t write = 0U;
    for (std::uint32_t read = 0U;
         read < hooks_.Size();
         ++read) {
        FrameHookRecord& record = hooks_[read];
        if (record.state == RecordState::Pending) {
            if (write != read) {
                hooks_[write] = std::move(record);
            }
            ++write;
        }
    }

    while (hooks_.Size() > write) {
        hooks_.PopBack();
    }
}

void Dispatcher::DiscardCompletedReadyPrefixLocked() noexcept {
    while (readyHead_ < ready_.Size() &&
           ready_[readyHead_].state != RecordState::Pending) {
        ++readyHead_;
    }
    if (readyHead_ == ready_.Size()) {
        ready_.Clear();
        readyHead_ = 0U;
    }
}

void Dispatcher::DiscardCompletedDelayedPrefixLocked() noexcept {
    while (delayedHead_ < delayed_.Size() &&
           delayed_[delayedHead_].state != RecordState::Pending) {
        ++delayedHead_;
    }
    if (delayedHead_ == delayed_.Size()) {
        delayed_.Clear();
        delayedHead_ = 0U;
    }
}

void Dispatcher::LeaveReentrancyGuard() noexcept {
    AERO_ASSERT(CheckAccess());
    std::lock_guard<std::mutex> lock(mutex_);
    AERO_ASSERT(guardDepth_ > 0U);
    --guardDepth_;
}

void Dispatcher::NotifyWake() const noexcept {
    if (wake_ != nullptr) {
        wake_(wakeContext_);
    }
}

bool Dispatcher::IsValidPriority(
    DispatcherPriority priority) noexcept {
    return static_cast<std::uint8_t>(priority) <
        static_cast<std::uint8_t>(DispatcherPriority::Count);
}

bool Dispatcher::IsValidFramePhase(
    DispatcherFramePhase phase) noexcept {
    return static_cast<std::uint8_t>(phase) <
        static_cast<std::uint8_t>(DispatcherFramePhase::Count);
}

bool Dispatcher::ReadyLess(
    const TaskRecord& left,
    const TaskRecord& right) noexcept {
    const std::uint8_t leftPriority =
        static_cast<std::uint8_t>(left.priority);
    const std::uint8_t rightPriority =
        static_cast<std::uint8_t>(right.priority);
    return leftPriority < rightPriority ||
        (leftPriority == rightPriority &&
         left.sequence < right.sequence);
}

bool Dispatcher::DelayedLess(
    const TaskRecord& left,
    const TaskRecord& right) noexcept {
    if (left.dueTimeMicroseconds !=
        right.dueTimeMicroseconds) {
        return left.dueTimeMicroseconds <
            right.dueTimeMicroseconds;
    }

    const std::uint8_t leftPriority =
        static_cast<std::uint8_t>(left.priority);
    const std::uint8_t rightPriority =
        static_cast<std::uint8_t>(right.priority);
    return leftPriority < rightPriority ||
        (leftPriority == rightPriority &&
         left.sequence < right.sequence);
}

DispatcherObject::DispatcherObject(
    Dispatcher& dispatcher) noexcept
    : dispatcher_(&dispatcher) {}

bool DispatcherObject::CheckAccess() const noexcept {
    return dispatcher_ != nullptr &&
        dispatcher_->CheckAccess();
}

Base::Result<void> DispatcherObject::VerifyAccess() const noexcept {
    return dispatcher_ != nullptr
        ? dispatcher_->VerifyAccess()
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::NotInitialized,
              "DispatcherObject has no Dispatcher"));
}

Dispatcher& DispatcherObject::GetDispatcher() const noexcept {
    AERO_ASSERT(dispatcher_ != nullptr);
    return *dispatcher_;
}

} // namespace Aero::Core
