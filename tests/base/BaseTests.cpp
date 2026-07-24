#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Utf8.hpp>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>

namespace {

using namespace Aero::Base;

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
        if (failAfter_ != Unlimited) {
            --failAfter_;
        }

        void* memory = upstream_.Allocate(request);
        if (memory != nullptr) {
            ++allocationCount_;
            ++activeCount_;
        }
        return memory;
    }

    void Deallocate(
        void* memory,
        std::size_t size,
        std::size_t alignment,
        MemoryTag tag) noexcept override {
        if (memory != nullptr) {
            CHECK_INTERNAL(activeCount_ > 0U);
            --activeCount_;
        }
        upstream_.Deallocate(memory, size, alignment, tag);
    }

    void FailAfter(std::uint32_t successfulAllocations) noexcept {
        failAfter_ = successfulAllocations;
    }

    void DisableFailures() noexcept {
        failAfter_ = Unlimited;
    }

    std::uint32_t ActiveCount() const noexcept { return activeCount_; }
    std::uint32_t AllocationCount() const noexcept { return allocationCount_; }

private:
    static constexpr std::uint32_t Unlimited = UINT32_MAX;

    static void CHECK_INTERNAL(bool condition) noexcept {
        if (!condition) {
            std::abort();
        }
    }

    MallocAllocator upstream_;
    std::uint32_t activeCount_ = 0U;
    std::uint32_t allocationCount_ = 0U;
    std::uint32_t failAfter_ = Unlimited;
};

class ProbeObject final : public Object {
public:
    explicit ProbeObject(int value) noexcept
        : value_(value) {
        ++aliveCount;
    }

    ~ProbeObject() override {
        --aliveCount;
    }

    int Value() const noexcept { return value_; }

    static std::atomic<int> aliveCount;

private:
    int value_ = 0;
};

std::atomic<int> ProbeObject::aliveCount{0};

int DoubleValue(int value) noexcept {
    return value * 2;
}

class DelegateTarget final {
public:
    explicit DelegateTarget(int bias) noexcept : bias_(bias) {}

    int Add(int value) noexcept { return value + bias_; }
    int AddConst(int value) const noexcept { return value + bias_ + 1; }

private:
    int bias_ = 0;
};

struct SequenceHandler final {
    std::uint32_t* sequence = nullptr;
    std::uint32_t* count = nullptr;
    std::uint32_t value = 0U;

    void operator()(int) const noexcept {
        sequence[(*count)++] = value;
    }
};

struct NestedDelegateHandler final {
    Delegate<void(int)>* multicast = nullptr;
    const Delegate<void(int)>* appended = nullptr;
    bool* added = nullptr;

    void operator()(int) const noexcept {
        if (!*added) {
            *added = true;
            CHECK_INTERNAL(multicast->TryAdd(*appended));
        }
    }

private:
    static void CHECK_INTERNAL(const Result<void>& result) noexcept {
        if (!result) std::abort();
    }
};

bool TestResult() {
    Result<int> value(42);
    CHECK(value);
    CHECK(value.Value() == 42);
    CHECK(value.GetStatus().IsOk());

    Result<int> error(Status::Failure(
        ErrorCode::InvalidArgument, "bad argument"));
    CHECK(!error);
    CHECK(error.GetStatus().code == ErrorCode::InvalidArgument);
    CHECK(std::strcmp(error.GetStatus().message, "bad argument") == 0);

    Result<void> ok;
    CHECK(ok);
    Result<void> failed(Status::Failure(ErrorCode::Unsupported, "unsupported"));
    CHECK(!failed);
    Result<int> missingValue(Status::Ok());
    CHECK(!missingValue);
    CHECK(missingValue.GetStatus().code == ErrorCode::InternalError);
    Result<void> explicitSuccess(Status::Ok());
    CHECK(explicitSuccess);
    return true;
}

bool TestSpan() {
    int values[] = {1, 2, 3, 4};
    Span<int> span(values);
    CHECK(span.Size() == 4U);
    CHECK(span[2] == 3);

    Span<int> middle = span.Subspan(1U, 2U);
    CHECK(middle.Size() == 2U);
    CHECK(middle[0] == 2);
    CHECK(middle[1] == 3);
    return true;
}

bool TestUtf8() {
    CHECK(ValidateUtf8(StringView("ASCII")).valid);
    CHECK(ValidateUtf8(StringView(u8"你好，AeroGUI")).valid);

    const char overlong[] = {static_cast<char>(0xC0), static_cast<char>(0xAF)};
    CHECK(!ValidateUtf8(StringView(overlong, 2U)).valid);

    const char surrogate[] = {
        static_cast<char>(0xED), static_cast<char>(0xA0), static_cast<char>(0x80)};
    CHECK(!ValidateUtf8(StringView(surrogate, 3U)).valid);
    return true;
}

bool TestAllocator() {
    MallocAllocator allocator;
    for (std::size_t alignment : {std::size_t{1U}, std::size_t{8U},
                                  std::size_t{16U}, std::size_t{64U}}) {
        void* memory = allocator.Allocate({128U, alignment, MemoryTag::Test});
        CHECK(memory != nullptr);
        const std::size_t effective = alignment < alignof(void*)
            ? alignof(void*)
            : alignment;
        CHECK(reinterpret_cast<std::uintptr_t>(memory) % effective == 0U);
        allocator.Deallocate(memory, 128U, alignment, MemoryTag::Test);
    }

    CHECK(allocator.Allocate({16U, 3U, MemoryTag::Test}) == nullptr);
    return true;
}

bool TestString() {
    TrackingAllocator allocator;
    String text(&allocator);

    CHECK(text.Empty());
    CHECK(text.TryAssign(StringView("AeroGUI")));
    CHECK(text == StringView("AeroGUI"));
    CHECK(allocator.ActiveCount() == 0U);

    CHECK(text.TryAppend(StringView(u8" 跨平台")));
    CHECK(text == StringView(u8"AeroGUI 跨平台"));

    const char invalid[] = {static_cast<char>(0xFF)};
    Result<void> invalidResult = text.TryAssign(StringView(invalid, 1U));
    CHECK(!invalidResult);
    CHECK(invalidResult.GetStatus().code == ErrorCode::InvalidUtf8);
    CHECK(text == StringView(u8"AeroGUI 跨平台"));

    const StringView longText(
        "This string is deliberately longer than the inline storage capacity.");
    CHECK(text.TryAssign(longText));
    CHECK(text.View() == longText);
    CHECK(text.CapacityBytes() >= longText.SizeBytes());
    CHECK(allocator.ActiveCount() == 1U);

    CHECK(text.TryAppend(text.View()));
    CHECK(text.SizeBytes() == longText.SizeBytes() * 2U);

    String copy(text);
    CHECK(copy.View() == text.View());
    CHECK(allocator.ActiveCount() == 2U);

    String moved(std::move(copy));
    CHECK(moved.View() == text.View());
    CHECK(copy.Empty());

    text.Clear();
    CHECK(text.Empty());
    return true;
}

bool TestStringAllocatorSemantics() {
    TrackingAllocator destinationAllocator;
    TrackingAllocator sourceAllocator;
    String destination(&destinationAllocator);
    String source(&sourceAllocator);

    const StringView longText(
        "Copy assignment preserves the allocator owned by the destination string.");
    CHECK(source.TryAssign(longText));
    destination = source;

    CHECK(destination.View() == source.View());
    CHECK(&destination.Allocator() == &destinationAllocator);
    CHECK(&source.Allocator() == &sourceAllocator);
    CHECK(destinationAllocator.ActiveCount() == 1U);
    CHECK(sourceAllocator.ActiveCount() == 1U);
    return true;
}

bool TestStringAllocationFailure() {
    TrackingAllocator allocator;
    String text(&allocator);
    CHECK(text.TryAssign(StringView("stable")));

    allocator.FailAfter(0U);
    const StringView longText(
        "This allocation must fail without changing the existing string value.");
    Result<void> result = text.TryAssign(longText);
    CHECK(!result);
    CHECK(result.GetStatus().code == ErrorCode::OutOfMemory);
    CHECK(text == StringView("stable"));
    CHECK(allocator.ActiveCount() == 0U);
    return true;
}

bool TestRefAndWeakRef() {
    TrackingAllocator allocator;
    CHECK(ProbeObject::aliveCount.load() == 0);

    auto made = MakeRefWithAllocator<ProbeObject>(allocator, 99);
    CHECK(made);
    Ref<ProbeObject> strong = std::move(made).Value();
    CHECK(strong);
    CHECK(strong->Value() == 99);
    CHECK(strong->UseCount() == 1U);
    CHECK(ProbeObject::aliveCount.load() == 1);
    CHECK(allocator.ActiveCount() == 2U);

    WeakRef<ProbeObject> weak(strong);
    CHECK(!weak.Expired());

    {
        Ref<ProbeObject> copy = strong;
        CHECK(strong->UseCount() == 2U);

        Ref<ProbeObject> locked = weak.Lock();
        CHECK(locked);
        CHECK(locked->UseCount() == 3U);
    }

    CHECK(strong->UseCount() == 1U);
    strong.Reset();
    CHECK(ProbeObject::aliveCount.load() == 0);
    CHECK(weak.Expired());
    CHECK(!weak.Lock());
    CHECK(allocator.ActiveCount() == 1U);

    weak.Reset();
    CHECK(allocator.ActiveCount() == 0U);
    return true;
}

bool TestRefAllocationFailure() {
    TrackingAllocator allocator;
    allocator.FailAfter(0U);
    auto failedObject = MakeRefWithAllocator<ProbeObject>(allocator, 1);
    CHECK(!failedObject);
    CHECK(failedObject.GetStatus().code == ErrorCode::OutOfMemory);
    CHECK(ProbeObject::aliveCount.load() == 0);
    CHECK(allocator.ActiveCount() == 0U);

    allocator.FailAfter(1U);
    auto failedControl = MakeRefWithAllocator<ProbeObject>(allocator, 2);
    CHECK(!failedControl);
    CHECK(failedControl.GetStatus().code == ErrorCode::OutOfMemory);
    CHECK(ProbeObject::aliveCount.load() == 0);
    CHECK(allocator.ActiveCount() == 0U);
    return true;
}

bool TestDelegate() {
    using IntDelegate = Delegate<int(int)>;
    static_assert(sizeof(IntDelegate) == sizeof(void*) * 4U,
        "Delegate must remain four pointers wide");

    IntDelegate freeFunction(&DoubleValue);
    CHECK(freeFunction.Size() == 1U);
    CHECK(freeFunction(4) == 8);

    DelegateTarget target(5);
    IntDelegate member(&target, &DelegateTarget::Add);
    const DelegateTarget constTarget(7);
    IntDelegate constMember(&constTarget, &DelegateTarget::AddConst);
    CHECK(member(4) == 9);
    CHECK(constMember(4) == 12);

    int bias = 3;
    IntDelegate lambda([bias](int value) noexcept { return value + bias; });
    CHECK(lambda(9) == 12);
    IntDelegate lambdaCopy = lambda;
    CHECK(lambda == lambdaCopy);

    std::uint32_t sequence[8]{};
    std::uint32_t count = 0U;
    SequenceHandler first{sequence, &count, 1U};
    SequenceHandler second{sequence, &count, 2U};
    SequenceHandler third{sequence, &count, 3U};
    Delegate<void(int)> firstDelegate(&first);
    Delegate<void(int)> secondDelegate(&second);
    Delegate<void(int)> thirdDelegate(&third);

    Delegate<void(int)> multicast = firstDelegate;
    CHECK(multicast.TryAdd(secondDelegate));
    CHECK(multicast.TryAdd(thirdDelegate));
    CHECK(multicast.Size() == 3U);
    multicast(0);
    CHECK(count == 3U);
    CHECK(sequence[0] == 1U && sequence[1] == 2U && sequence[2] == 3U);

    Delegate<void(int)> copy = multicast;
    CHECK(copy.Remove(secondDelegate));
    CHECK(copy.Size() == 2U);
    count = 0U;
    copy(0);
    CHECK(count == 2U && sequence[0] == 1U && sequence[1] == 3U);
    CHECK(multicast.Size() == 3U);

    count = 0U;
    bool added = false;
    Delegate<void(int)> nested;
    NestedDelegateHandler nestedTarget{&nested, &thirdDelegate, &added};
    Delegate<void(int)> nestedDelegate(&nestedTarget);
    nested = nestedDelegate;
    CHECK(nested.TryAdd(firstDelegate));
    nested(0);
    CHECK(count == 1U && sequence[0] == 1U);
    CHECK(nested.Size() == 3U);
    count = 0U;
    nested(0);
    CHECK(count == 2U && sequence[0] == 1U && sequence[1] == 3U);

    TrackingAllocator allocator;
    Delegate<void(int)> allocationFailure = firstDelegate;
    allocator.FailAfter(0U);
    Result<void> failed = allocationFailure.TryAdd(secondDelegate, &allocator);
    CHECK(!failed);
    CHECK(failed.GetStatus().code == ErrorCode::OutOfMemory);
    CHECK(allocationFailure == firstDelegate);
    CHECK(allocator.ActiveCount() == 0U);
    return true;
}

struct TestCase final {
    const char* name;
    bool (*run)();
};

} // namespace

int main() {
    const TestCase tests[] = {
        {"Result", &TestResult},
        {"Span", &TestSpan},
        {"UTF-8", &TestUtf8},
        {"Allocator", &TestAllocator},
        {"String", &TestString},
        {"String allocator semantics", &TestStringAllocatorSemantics},
        {"String allocation failure", &TestStringAllocationFailure},
        {"Ref/WeakRef", &TestRefAndWeakRef},
        {"Ref allocation failure", &TestRefAllocationFailure},
        {"Delegate", &TestDelegate},
    };

    std::uint32_t passed = 0U;
    for (const TestCase& test : tests) {
        const bool ok = test.run();
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", test.name);
        if (!ok) {
            return 1;
        }
        ++passed;
    }

    std::printf("%u tests passed\n", passed);
    return 0;
}
