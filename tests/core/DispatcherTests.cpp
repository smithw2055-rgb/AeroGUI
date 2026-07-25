#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include "TestAllocatorScope.hpp"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>
#include <utility>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

class TrackingAllocator final : public IAllocator {
public:
    void* Allocate(const AllocationRequest& request) noexcept override {
        if (failAfter_ == 0U) {
            return nullptr;
        }
        if (failAfter_ != UINT32_MAX) {
            --failAfter_;
        }

        void* memory = upstream_.Allocate(request);
        if (memory != nullptr) {
            ++active_;
        }
        return memory;
    }

    void Deallocate(
        void* memory,
        std::size_t size,
        std::size_t alignment,
        MemoryTag tag) noexcept override {
        if (memory != nullptr) {
            if (active_ == 0U) {
                std::abort();
            }
            --active_;
        }
        upstream_.Deallocate(memory, size, alignment, tag);
    }

    void FailAfter(std::uint32_t successfulAllocations) noexcept {
        failAfter_ = successfulAllocations;
    }

    void DisableFailures() noexcept {
        failAfter_ = UINT32_MAX;
    }

    std::uint32_t Active() const noexcept {
        return active_;
    }

private:
    MallocAllocator upstream_;
    std::uint32_t failAfter_ = UINT32_MAX;
    std::uint32_t active_ = 0U;
};

struct ManualClock final {
    std::atomic<DispatcherTime> now{0U};

    static DispatcherTime Read(void* context) noexcept {
        return static_cast<ManualClock*>(context)->now.load(
            std::memory_order_acquire);
    }
};

struct WakeCounter final {
    std::atomic<std::uint32_t> count{0U};

    static void Wake(void* context) noexcept {
        static_cast<WakeCounter*>(context)->count.fetch_add(
            1U, std::memory_order_relaxed);
    }
};

struct Recorder final {
    int values[64]{};
    std::uint32_t count = 0U;

    void Add(int value) noexcept {
        if (count >= 64U) {
            std::abort();
        }
        values[count] = value;
        ++count;
    }
};

struct RecordContext final {
    Recorder* recorder = nullptr;
    int value = 0;
    DispatcherThreadToken threadToken = 0U;
    std::uint32_t callbackCount = 0U;
    std::uint32_t cleanupCount = 0U;

    static void Run(void* context) noexcept {
        auto* self = static_cast<RecordContext*>(context);
        ++self->callbackCount;
        self->threadToken = CurrentDispatcherThreadToken();
        if (self->recorder != nullptr) {
            self->recorder->Add(self->value);
        }
    }

    static void Cleanup(void* context) noexcept {
        auto* self = static_cast<RecordContext*>(context);
        ++self->cleanupCount;
    }
};

class ProbeDispatcherObject final : public DispatcherObject {
public:
    explicit ProbeDispatcherObject(Dispatcher& dispatcher) noexcept
        : DispatcherObject(dispatcher) {}

    ~ProbeDispatcherObject() override = default;
};

bool TestThreadAffinityAndCrossThreadPost() {
    Dispatcher dispatcher;
    ProbeDispatcherObject object(dispatcher);
    RecordContext callback;

    CHECK(dispatcher.CheckAccess());
    CHECK(dispatcher.VerifyAccess());
    CHECK(object.CheckAccess());
    CHECK(object.VerifyAccess());
    CHECK(&object.GetDispatcher() == &dispatcher);

    std::atomic<std::uint32_t> dispatcherStatus{
        static_cast<std::uint32_t>(ErrorCode::Ok)};
    std::atomic<std::uint32_t> objectStatus{
        static_cast<std::uint32_t>(ErrorCode::Ok)};
    std::atomic<bool> workerAccess{true};
    std::atomic<bool> postSucceeded{false};

    std::thread worker([&]() noexcept {
        workerAccess.store(
            dispatcher.CheckAccess(),
            std::memory_order_release);

        const Result<void> verifyDispatcher =
            dispatcher.VerifyAccess();
        dispatcherStatus.store(
            static_cast<std::uint32_t>(
                verifyDispatcher.GetStatus().code),
            std::memory_order_release);

        const Result<void> verifyObject =
            object.VerifyAccess();
        objectStatus.store(
            static_cast<std::uint32_t>(
                verifyObject.GetStatus().code),
            std::memory_order_release);

        const auto posted = dispatcher.Post(
            DispatcherPriority::Input,
            &RecordContext::Run,
            &callback);
        postSucceeded.store(
            static_cast<bool>(posted),
            std::memory_order_release);
    });
    worker.join();

    CHECK(!workerAccess.load(std::memory_order_acquire));
    CHECK(dispatcherStatus.load(std::memory_order_acquire) ==
        static_cast<std::uint32_t>(ErrorCode::WrongThread));
    CHECK(objectStatus.load(std::memory_order_acquire) ==
        static_cast<std::uint32_t>(ErrorCode::WrongThread));
    CHECK(postSucceeded.load(std::memory_order_acquire));

    const auto processed = dispatcher.ProcessPending();
    CHECK(processed);
    CHECK(processed.Value() == 1U);
    CHECK(callback.callbackCount == 1U);
    CHECK(callback.threadToken == dispatcher.OwnerThreadToken());
    return true;
}

bool TestPriorityFifoAndBudget() {
    ManualClock clock;
    WakeCounter wake;
    Dispatcher dispatcher({
        &ManualClock::Read,
        &clock,
        &WakeCounter::Wake,
        &wake
    });

    Recorder recorder;
    RecordContext normal{&recorder, 1};
    RecordContext inputA{&recorder, 2};
    RecordContext inputB{&recorder, 3};
    RecordContext background{&recorder, 4};
    RecordContext send{&recorder, 5};

    CHECK(dispatcher.Post(
        DispatcherPriority::Normal,
        &RecordContext::Run,
        &normal));
    CHECK(dispatcher.Post(
        DispatcherPriority::Input,
        &RecordContext::Run,
        &inputA));
    CHECK(dispatcher.Post(
        DispatcherPriority::Input,
        &RecordContext::Run,
        &inputB));
    CHECK(dispatcher.Post(
        DispatcherPriority::Background,
        &RecordContext::Run,
        &background));
    CHECK(dispatcher.Post(
        DispatcherPriority::Send,
        &RecordContext::Run,
        &send));

    auto first = dispatcher.ProcessPending(
        DispatcherPriority::Input,
        2U);
    CHECK(first);
    CHECK(first.Value() == 2U);
    CHECK(recorder.count == 2U);
    CHECK(recorder.values[0] == 5);
    CHECK(recorder.values[1] == 2);

    auto second = dispatcher.ProcessPending(
        DispatcherPriority::Input);
    CHECK(second);
    CHECK(second.Value() == 1U);
    CHECK(recorder.values[2] == 3);

    auto rest = dispatcher.ProcessPending();
    CHECK(rest);
    CHECK(rest.Value() == 2U);
    CHECK(recorder.values[3] == 1);
    CHECK(recorder.values[4] == 4);
    CHECK(dispatcher.PendingTaskCount() == 0U);
    CHECK(wake.count.load(std::memory_order_acquire) == 5U);
    return true;
}

bool TestDelayedOrderingAndOverflow() {
    ManualClock clock;
    clock.now.store(100U, std::memory_order_release);
    Dispatcher dispatcher({
        &ManualClock::Read,
        &clock,
        nullptr,
        nullptr
    });

    Recorder recorder;
    RecordContext normal{&recorder, 1};
    RecordContext sameDueNormal{&recorder, 2};
    RecordContext sameDueInput{&recorder, 3};
    RecordContext earlier{&recorder, 4};

    CHECK(dispatcher.PostDelayed(
        100U,
        DispatcherPriority::Normal,
        &RecordContext::Run,
        &sameDueNormal));
    CHECK(dispatcher.PostAt(
        200U,
        DispatcherPriority::Input,
        &RecordContext::Run,
        &sameDueInput));
    CHECK(dispatcher.PostAt(
        150U,
        DispatcherPriority::Background,
        &RecordContext::Run,
        &earlier));
    CHECK(dispatcher.Post(
        DispatcherPriority::Normal,
        &RecordContext::Run,
        &normal));

    auto immediate = dispatcher.ProcessPending();
    CHECK(immediate && immediate.Value() == 1U);
    CHECK(recorder.values[0] == 1);

    clock.now.store(149U, std::memory_order_release);
    auto beforeDue = dispatcher.ProcessPending();
    CHECK(beforeDue && beforeDue.Value() == 0U);

    clock.now.store(150U, std::memory_order_release);
    auto firstDue = dispatcher.ProcessPending();
    CHECK(firstDue && firstDue.Value() == 1U);
    CHECK(recorder.values[1] == 4);

    clock.now.store(200U, std::memory_order_release);
    auto sameDue = dispatcher.ProcessPending();
    CHECK(sameDue && sameDue.Value() == 2U);
    CHECK(recorder.values[2] == 3);
    CHECK(recorder.values[3] == 2);

    clock.now.store(
        std::numeric_limits<DispatcherTime>::max() - 2U,
        std::memory_order_release);
    auto overflow = dispatcher.PostDelayed(
        3U,
        DispatcherPriority::Normal,
        &RecordContext::Run,
        &normal);
    CHECK(!overflow);
    CHECK(overflow.GetStatus().code == ErrorCode::OutOfRange);
    return true;
}

bool TestCancellationCleanupAndOom() {
    TrackingAllocator allocator;
    RecordContext cancelled;
    {
        Aero::Tests::ScopedDefaultAllocator allocatorScope(allocator);
        Dispatcher dispatcher;

        auto posted = dispatcher.Post(
            DispatcherPriority::Normal,
            &RecordContext::Run,
            &cancelled,
            &RecordContext::Cleanup);
        CHECK(posted);
        CHECK(dispatcher.PendingTaskCount() == 1U);
        CHECK(dispatcher.Cancel(posted.Value()));
        CHECK(!dispatcher.Cancel(posted.Value()));
        CHECK(cancelled.callbackCount == 0U);
        CHECK(cancelled.cleanupCount == 1U);
        CHECK(dispatcher.PendingTaskCount() == 0U);

        auto processed = dispatcher.ProcessPending();
        CHECK(processed && processed.Value() == 0U);
    }
    CHECK(allocator.Active() == 0U);

    TrackingAllocator failingAllocator;
    failingAllocator.FailAfter(0U);
    RecordContext rejected;
    {
        Aero::Tests::ScopedDefaultAllocator allocatorScope(failingAllocator);
        Dispatcher dispatcher;
        auto failed = dispatcher.Post(
            DispatcherPriority::Normal,
            &RecordContext::Run,
            &rejected,
            &RecordContext::Cleanup);
        CHECK(!failed);
        CHECK(failed.GetStatus().code == ErrorCode::OutOfMemory);
        CHECK(rejected.callbackCount == 0U);
        CHECK(rejected.cleanupCount == 0U);
    }
    CHECK(failingAllocator.Active() == 0U);
    return true;
}

struct ReentrantContext final {
    Dispatcher* dispatcher = nullptr;
    Recorder* recorder = nullptr;
    ErrorCode nestedStatus = ErrorCode::Ok;
    RecordContext followup;

    static void Run(void* context) noexcept {
        auto* self = static_cast<ReentrantContext*>(context);
        self->recorder->Add(1);

        const auto nested = self->dispatcher->ProcessPending();
        self->nestedStatus = nested
            ? ErrorCode::Ok
            : nested.GetStatus().code;

        const auto posted = self->dispatcher->Post(
            DispatcherPriority::Send,
            &RecordContext::Run,
            &self->followup);
        if (!posted) {
            std::abort();
        }
    }
};

bool TestReentrancyGuardAndNestedPump() {
    Dispatcher dispatcher;
    Recorder recorder;
    ReentrantContext context;
    context.dispatcher = &dispatcher;
    context.recorder = &recorder;
    context.followup.recorder = &recorder;
    context.followup.value = 2;

    CHECK(dispatcher.Post(
        DispatcherPriority::Normal,
        &ReentrantContext::Run,
        &context));

    auto processed = dispatcher.ProcessPending();
    CHECK(processed);
    CHECK(processed.Value() == 2U);
    CHECK(context.nestedStatus == ErrorCode::InvalidState);
    CHECK(recorder.count == 2U);
    CHECK(recorder.values[0] == 1);
    CHECK(recorder.values[1] == 2);

    RecordContext guarded;
    guarded.recorder = &recorder;
    guarded.value = 3;
    CHECK(dispatcher.Post(
        DispatcherPriority::Normal,
        &RecordContext::Run,
        &guarded));

    {
        auto entered = dispatcher.EnterReentrancyGuard();
        CHECK(entered);
        DispatcherReentrancyGuard guard =
            std::move(entered).Value();
        CHECK(guard.Active());
        CHECK(dispatcher.ReentrancyDepth() == 1U);

        auto blocked = dispatcher.ProcessPending();
        CHECK(!blocked);
        CHECK(blocked.GetStatus().code == ErrorCode::InvalidState);
    }

    CHECK(dispatcher.ReentrancyDepth() == 0U);
    auto afterGuard = dispatcher.ProcessPending();
    CHECK(afterGuard && afterGuard.Value() == 1U);
    CHECK(recorder.values[2] == 3);
    return true;
}

struct HookContext final {
    Dispatcher* dispatcher = nullptr;
    Recorder* recorder = nullptr;
    int value = 0;
    DispatcherFramePhase phase = DispatcherFramePhase::Layout;
    DispatcherFrameHookHandle self;
    HookContext* deferred = nullptr;
    DispatcherFrameHookHandle deferredHandle;
    bool addDeferred = false;
    bool removeSelf = false;
    bool tryNestedPhase = false;
    ErrorCode operationStatus = ErrorCode::Ok;
    bool operationValue = false;
    std::uint32_t cleanupCount = 0U;

    static void Run(void* context) noexcept {
        auto* self = static_cast<HookContext*>(context);
        self->recorder->Add(self->value);

        if (self->addDeferred) {
            self->addDeferred = false;
            auto added = self->dispatcher->RegisterFrameHook(
                self->phase,
                &HookContext::Run,
                self->deferred,
                &HookContext::Cleanup);
            if (!added) {
                self->operationStatus = added.GetStatus().code;
            } else {
                self->deferredHandle = added.Value();
                self->deferred->self = added.Value();
            }
        }

        if (self->tryNestedPhase) {
            auto nested = self->dispatcher->RunFramePhase(
                self->phase);
            self->operationStatus = nested
                ? ErrorCode::Ok
                : nested.GetStatus().code;
        }

        if (self->removeSelf) {
            auto removed = self->dispatcher->RemoveFrameHook(
                self->self);
            self->operationStatus = removed
                ? ErrorCode::Ok
                : removed.GetStatus().code;
            self->operationValue =
                removed && removed.Value();
        }
    }

    static void Cleanup(void* context) noexcept {
        auto* self = static_cast<HookContext*>(context);
        ++self->cleanupCount;
    }
};

bool TestFrameHooksAndSnapshotSemantics() {
    Recorder recorder;
    HookContext a;
    HookContext b;
    HookContext deferred;
    HookContext selfRemoving;
    HookContext otherPhase;

    {
        Dispatcher dispatcher;
        a.dispatcher = &dispatcher;
        a.recorder = &recorder;
        a.value = 1;
        a.deferred = &deferred;
        a.addDeferred = true;

        b.dispatcher = &dispatcher;
        b.recorder = &recorder;
        b.value = 2;
        b.tryNestedPhase = true;

        deferred.dispatcher = &dispatcher;
        deferred.recorder = &recorder;
        deferred.value = 3;

        selfRemoving.dispatcher = &dispatcher;
        selfRemoving.recorder = &recorder;
        selfRemoving.value = 4;
        selfRemoving.removeSelf = true;

        otherPhase.dispatcher = &dispatcher;
        otherPhase.recorder = &recorder;
        otherPhase.value = 9;
        otherPhase.phase = DispatcherFramePhase::Input;

        auto aHandle = dispatcher.RegisterFrameHook(
            DispatcherFramePhase::Layout,
            &HookContext::Run,
            &a,
            &HookContext::Cleanup);
        auto bHandle = dispatcher.RegisterFrameHook(
            DispatcherFramePhase::Layout,
            &HookContext::Run,
            &b,
            &HookContext::Cleanup);
        auto selfHandle = dispatcher.RegisterFrameHook(
            DispatcherFramePhase::Layout,
            &HookContext::Run,
            &selfRemoving,
            &HookContext::Cleanup);
        auto otherHandle = dispatcher.RegisterFrameHook(
            DispatcherFramePhase::Input,
            &HookContext::Run,
            &otherPhase,
            &HookContext::Cleanup);

        CHECK(aHandle && bHandle && selfHandle && otherHandle);
        a.self = aHandle.Value();
        b.self = bHandle.Value();
        selfRemoving.self = selfHandle.Value();
        otherPhase.self = otherHandle.Value();

        auto first = dispatcher.RunFramePhase(
            DispatcherFramePhase::Layout);
        CHECK(first && first.Value() == 3U);
        CHECK(recorder.count == 3U);
        CHECK(recorder.values[0] == 1);
        CHECK(recorder.values[1] == 2);
        CHECK(recorder.values[2] == 4);
        CHECK(a.deferredHandle.IsValid());
        CHECK(b.operationStatus == ErrorCode::InvalidState);
        CHECK(selfRemoving.operationStatus == ErrorCode::Ok);
        CHECK(selfRemoving.operationValue);
        CHECK(selfRemoving.cleanupCount == 1U);

        auto second = dispatcher.RunFramePhase(
            DispatcherFramePhase::Layout);
        CHECK(second && second.Value() == 3U);
        CHECK(recorder.values[3] == 1);
        CHECK(recorder.values[4] == 2);
        CHECK(recorder.values[5] == 3);

        auto removedB = dispatcher.RemoveFrameHook(
            bHandle.Value());
        CHECK(removedB && removedB.Value());
        CHECK(b.cleanupCount == 1U);

        auto third = dispatcher.RunFramePhase(
            DispatcherFramePhase::Layout);
        CHECK(third && third.Value() == 2U);
        CHECK(recorder.values[6] == 1);
        CHECK(recorder.values[7] == 3);

        auto input = dispatcher.RunFramePhase(
            DispatcherFramePhase::Input);
        CHECK(input && input.Value() == 1U);
        CHECK(recorder.values[8] == 9);
        CHECK(dispatcher.RegisteredFrameHookCount() == 3U);
    }

    CHECK(a.cleanupCount == 1U);
    CHECK(b.cleanupCount == 1U);
    CHECK(deferred.cleanupCount == 1U);
    CHECK(selfRemoving.cleanupCount == 1U);
    CHECK(otherPhase.cleanupCount == 1U);
    return true;
}

bool TestDestructorCleanup() {
    RecordContext task;
    HookContext hook;
    Recorder recorder;

    {
        Dispatcher dispatcher;
        hook.dispatcher = &dispatcher;
        hook.recorder = &recorder;
        hook.value = 1;

        CHECK(dispatcher.Post(
            DispatcherPriority::Idle,
            &RecordContext::Run,
            &task,
            &RecordContext::Cleanup));
        CHECK(dispatcher.RegisterFrameHook(
            DispatcherFramePhase::EndFrame,
            &HookContext::Run,
            &hook,
            &HookContext::Cleanup));
    }

    CHECK(task.callbackCount == 0U);
    CHECK(task.cleanupCount == 1U);
    CHECK(hook.cleanupCount == 1U);
    return true;
}

void AdvanceClock(void* context) noexcept {
    static_cast<ManualClock*>(context)->
        now.fetch_add(
            3U,
            std::memory_order_release);
}

bool TestFrameTimings() {
    ManualClock clock;
    DispatcherOptions options;
    options.now = &ManualClock::Read;
    options.clockContext = &clock;
    Dispatcher dispatcher(options);
    const DispatcherFramePhase measured[] = {
        DispatcherFramePhase::BeginFrame,
        DispatcherFramePhase::Layout,
        DispatcherFramePhase::EndFrame};
    for (DispatcherFramePhase phase :
         measured) {
        CHECK(dispatcher.RegisterFrameHook(
            phase,
            &AdvanceClock,
            &clock));
    }
    for (std::uint32_t index = 0U;
         index < DispatcherFramePhaseCount;
         ++index) {
        CHECK(dispatcher.RunFramePhase(
            static_cast<
                DispatcherFramePhase>(
                    index)));
    }
    DispatcherFrameTimings timings =
        dispatcher.FrameTimings();
    CHECK(timings.frameSequence == 1U);
    CHECK(timings.totalMicroseconds == 9U);
    CHECK(timings.phaseMicroseconds[
        static_cast<std::uint32_t>(
            DispatcherFramePhase::
                BeginFrame)] == 3U);
    CHECK(timings.phaseMicroseconds[
        static_cast<std::uint32_t>(
            DispatcherFramePhase::
                Layout)] == 3U);
    CHECK(timings.phaseMicroseconds[
        static_cast<std::uint32_t>(
            DispatcherFramePhase::
                EndFrame)] == 3U);

    CHECK(dispatcher.RunFramePhase(
        DispatcherFramePhase::BeginFrame));
    timings = dispatcher.FrameTimings();
    CHECK(timings.frameSequence == 1U);
    CHECK(timings.totalMicroseconds == 0U);
    CHECK(timings.phaseMicroseconds[
        static_cast<std::uint32_t>(
            DispatcherFramePhase::
                BeginFrame)] == 3U);
    CHECK(timings.phaseMicroseconds[
        static_cast<std::uint32_t>(
            DispatcherFramePhase::
                Layout)] == 0U);
    return true;
}

struct TestCase final {
    const char* name;
    bool (*run)();
};

} // namespace

int main() {
    const TestCase tests[] = {
        {"Thread affinity/cross-thread post",
         &TestThreadAffinityAndCrossThreadPost},
        {"Priority/FIFO/budget",
         &TestPriorityFifoAndBudget},
        {"Delayed ordering/overflow",
         &TestDelayedOrderingAndOverflow},
        {"Cancellation/cleanup/OOM",
         &TestCancellationCleanupAndOom},
        {"Reentrancy guard/nested pump",
         &TestReentrancyGuardAndNestedPump},
        {"Frame hooks/snapshot semantics",
         &TestFrameHooksAndSnapshotSemantics},
        {"Destructor cleanup",
         &TestDestructorCleanup},
        {"Frame timings",
         &TestFrameTimings},
    };

    for (const TestCase& test : tests) {
        if (!test.run()) {
            std::printf("[FAIL] %s\n", test.name);
            return 1;
        }
        std::printf("[PASS] %s\n", test.name);
    }
    return 0;
}
