#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/HashSet.hpp>
#include <Aero/Base/SmallVector.hpp>
#include <Aero/Base/Vector.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace {
using namespace Aero::Base;

#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    return false; } } while (false)

class TrackingAllocator final : public IAllocator {
public:
    void* Allocate(const AllocationRequest& request) noexcept override {
        if (failAfter_ == 0U) return nullptr;
        if (failAfter_ != UINT32_MAX) --failAfter_;
        void* result = upstream_.Allocate(request);
        if (result != nullptr) { ++active_; ++total_; }
        return result;
    }
    void Deallocate(void* memory, std::size_t size,
        std::size_t alignment, MemoryTag tag) noexcept override {
        if (memory != nullptr) { if (active_ == 0U) std::abort(); --active_; }
        upstream_.Deallocate(memory, size, alignment, tag);
    }
    void FailAfter(std::uint32_t count) noexcept { failAfter_ = count; }
    void DisableFailures() noexcept { failAfter_ = UINT32_MAX; }
    std::uint32_t Active() const noexcept { return active_; }
    std::uint32_t Total() const noexcept { return total_; }
private:
    MallocAllocator upstream_;
    std::uint32_t failAfter_ = UINT32_MAX;
    std::uint32_t active_ = 0U;
    std::uint32_t total_ = 0U;
};

struct Probe final {
    explicit Probe(int v = 0) noexcept : value(v) { ++alive; }
    Probe(const Probe& other) noexcept : value(other.value) { ++alive; ++copies; }
    Probe(Probe&& other) noexcept : value(other.value) { other.value = -1; ++alive; ++moves; }
    Probe& operator=(const Probe& other) noexcept { value = other.value; ++copies; return *this; }
    Probe& operator=(Probe&& other) noexcept { value = other.value; other.value = -1; ++moves; return *this; }
    ~Probe() { --alive; }
    bool operator==(const Probe& other) const noexcept { return value == other.value; }
    int value = 0;
    static int alive;
    static int copies;
    static int moves;
};
int Probe::alive = 0;
int Probe::copies = 0;
int Probe::moves = 0;

struct ConstantHash final {
    HashCode operator()(int) const noexcept { return 7U; }
};

bool TestVector() {
    TrackingAllocator allocator;
    Vector<int> values(&allocator);
    CHECK(values.Empty());
    for (int value = 0; value < 32; ++value) {
        CHECK(values.TryPushBack(value));
    }
    CHECK(values.Size() == 32U);
    CHECK(values[17] == 17);
    CHECK(allocator.Active() == 1U);

    const Span<const int> firstEight(values.Data(), 8U);
    CHECK(values.TryAppend(firstEight));
    CHECK(values.Size() == 40U);
    for (std::uint32_t index = 0U; index < 8U; ++index) {
        CHECK(values[32U + index] == static_cast<int>(index));
    }

    const Span<const int> middle(values.Data() + 10U, 5U);
    CHECK(values.TryAssign(middle));
    CHECK(values.Size() == 5U);
    CHECK(values[0] == 10 && values[4] == 14);

    Vector<int> copy(values);
    CHECK(copy.Size() == values.Size());
    CHECK(copy[2] == 12);

    Vector<int> moved(std::move(copy));
    CHECK(moved.Size() == 5U);
    CHECK(copy.Empty());
    return true;
}

bool TestVectorLifetimeAndOom() {
    TrackingAllocator allocator;
    {
        Vector<Probe> values(&allocator);
        CHECK(values.TryEmplaceBack(1));
        CHECK(values.TryEmplaceBack(2));
        CHECK(values.TryEmplaceBack(3));
        CHECK(Probe::alive == 3);
        CHECK(values.TryReserve(32U));
        CHECK(Probe::alive == 3);
        CHECK(values[1].value == 2);

        allocator.FailAfter(0U);
        const std::uint32_t oldCapacity = values.Capacity();
        CHECK(!values.TryReserve(oldCapacity + 100U));
        CHECK(values.Size() == 3U);
        CHECK(values[0].value == 1 && values[2].value == 3);
        allocator.DisableFailures();
    }
    CHECK(Probe::alive == 0);
    CHECK(allocator.Active() == 0U);
    return true;
}

bool TestSmallVector() {
    TrackingAllocator allocator;
    SmallVector<int, 4U> values(&allocator);
    for (int value = 0; value < 4; ++value) CHECK(values.TryPushBack(value));
    CHECK(allocator.Active() == 0U);
    CHECK(values.Capacity() == 4U);
    CHECK(values.TryPushBack(4));
    CHECK(allocator.Active() == 1U);

    SmallVector<int, 4U> moved(std::move(values));
    CHECK(moved.Size() == 5U);
    CHECK(values.Empty());
    CHECK(moved[4] == 4);
    return true;
}

bool TestHashing() {
    const HashCode first = HashBytes("AeroGUI", 7U);
    const HashCode second = HashBytes("AeroGUI", 7U);
    const HashCode changed = HashBytes("Aerogui", 7U);
    CHECK(first == second);
    CHECK(first != changed);
    CHECK(DefaultHash<int>{}(42) == DefaultHash<int>{}(42));
    CHECK(DefaultHash<bool>{}(true) != DefaultHash<bool>{}(false));
    CHECK(DefaultHash<StringView>{}(StringView("abc")) ==
        DefaultHash<StringView>{}(StringView("abc")));
    return true;
}

bool TestStringHashMap() {
    String alpha;
    String beta;
    String lookup;
    CHECK(alpha.TryAssign(StringView("alpha")));
    CHECK(beta.TryAssign(StringView("beta")));
    CHECK(lookup.TryAssign(StringView("alpha")));

    HashMap<String, int> map;
    CHECK(map.TryInsert(alpha, 1));
    CHECK(map.TryInsert(beta, 2));
    int* value = map.Find(lookup);
    CHECK(value != nullptr);
    CHECK(*value == 1);
    return true;
}

bool TestHashMap() {
    TrackingAllocator allocator;
    HashMap<int, int, ConstantHash> map(&allocator, 123U);
    for (int key = 0; key < 100; ++key) {
        auto inserted = map.TryInsert(key, key * 10);
        CHECK(inserted);
        CHECK(inserted.Value().inserted);
    }
    CHECK(map.Size() == 100U);
    for (int key = 0; key < 100; ++key) {
        int* value = map.Find(key);
        CHECK(value != nullptr && *value == key * 10);
    }

    auto duplicate = map.TryInsert(20, 999);
    CHECK(duplicate && !duplicate.Value().inserted);
    CHECK(*map.Find(20) == 200);

    auto set = map.TrySet(20, 999);
    CHECK(set && *set.Value() == 999);
    CHECK(map.Erase(20));
    CHECK(!map.Contains(20));
    CHECK(!map.Erase(20));

    std::uint32_t iterated = 0U;
    for (auto& entry : map) {
        CHECK(entry.Key() != 20);
        ++iterated;
    }
    CHECK(iterated == map.Size());

    HashMap<int, int, ConstantHash> copy(map);
    CHECK(copy.Size() == map.Size());
    CHECK(*copy.Find(30) == 300);

    HashMap<int, int, ConstantHash> moved(std::move(copy));
    CHECK(moved.Size() == map.Size());
    CHECK(copy.Empty());
    return true;
}

bool TestHashMapOom() {
    TrackingAllocator allocator;
    HashMap<int, int> map(&allocator);
    CHECK(map.TryInsert(1, 10));
    const std::uint32_t oldSize = map.Size();
    const std::uint32_t oldCapacity = map.Capacity();
    allocator.FailAfter(0U);
    CHECK(!map.TryReserve(oldCapacity * 4U));
    CHECK(map.Size() == oldSize);
    CHECK(*map.Find(1) == 10);
    allocator.DisableFailures();
    return true;
}

bool TestAllocatorAwareMove() {
    TrackingAllocator sourceAllocator;
    TrackingAllocator destinationAllocator;

    Vector<int> sourceVector(&sourceAllocator);
    for (int value = 0; value < 20; ++value) {
        CHECK(sourceVector.TryPushBack(value));
    }
    Vector<int> destinationVector(&destinationAllocator);
    destinationVector = std::move(sourceVector);
    CHECK(sourceVector.Empty());
    CHECK(sourceAllocator.Active() == 0U);
    CHECK(destinationVector.Size() == 20U);
    CHECK(&destinationVector.Allocator() == &destinationAllocator);

    HashMap<int, int> sourceMap(&sourceAllocator);
    for (int key = 0; key < 40; ++key) {
        CHECK(sourceMap.TryInsert(key, key + 1));
    }
    HashMap<int, int> destinationMap(&destinationAllocator);
    destinationMap = std::move(sourceMap);
    CHECK(sourceMap.Empty());
    CHECK(sourceMap.Capacity() == 0U);
    CHECK(sourceAllocator.Active() == 0U);
    CHECK(destinationMap.Size() == 40U);
    CHECK(*destinationMap.Find(12) == 13);
    CHECK(&destinationMap.Allocator() == &destinationAllocator);
    return true;
}

bool TestHashMapChurn() {
    HashMap<int, int> map;
    bool present[128]{};
    int expected[128]{};
    std::uint32_t state = 0x12345678U;
    for (std::uint32_t step = 0U; step < 20000U; ++step) {
        state = state * 1664525U + 1013904223U;
        const int key = static_cast<int>((state >> 8U) & 127U);
        switch (state % 3U) {
        case 0U: {
            const int value = static_cast<int>(state);
            Result<int*> result = map.TrySet(key, value);
            CHECK(result);
            present[key] = true;
            expected[key] = value;
            break;
        }
        case 1U:
            CHECK(map.Erase(key) == present[key]);
            present[key] = false;
            break;
        default: {
            int* value = map.Find(key);
            CHECK((value != nullptr) == present[key]);
            if (value != nullptr) {
                CHECK(*value == expected[key]);
            }
            break;
        }
        }
    }

    std::uint32_t expectedSize = 0U;
    for (int key = 0; key < 128; ++key) {
        int* value = map.Find(key);
        CHECK((value != nullptr) == present[key]);
        if (present[key]) {
            CHECK(*value == expected[key]);
            ++expectedSize;
        }
    }
    CHECK(map.Size() == expectedSize);
    return true;
}

bool TestHashSet() {
    HashSet<int, ConstantHash> set;
    for (int value = 0; value < 50; ++value) {
        auto result = set.TryInsert(value);
        CHECK(result && result.Value().inserted);
    }
    auto duplicate = set.TryInsert(10);
    CHECK(duplicate && !duplicate.Value().inserted);
    CHECK(set.Contains(10));
    CHECK(set.Erase(10));
    CHECK(!set.Contains(10));
    std::uint32_t count = 0U;
    for (int value : set) {
        CHECK(value != 10);
        ++count;
    }
    CHECK(count == set.Size());
    return true;
}

struct TestCase { const char* name; bool (*run)(); };
}

int main() {
    const TestCase tests[] = {
        {"Vector", &TestVector},
        {"Vector lifetime/OOM", &TestVectorLifetimeAndOom},
        {"SmallVector", &TestSmallVector},
        {"Hashing", &TestHashing},
        {"String HashMap", &TestStringHashMap},
        {"HashMap", &TestHashMap},
        {"HashMap OOM", &TestHashMapOom},
        {"Allocator-aware move", &TestAllocatorAwareMove},
        {"HashMap churn", &TestHashMapChurn},
        {"HashSet", &TestHashSet},
    };
    for (const TestCase& test : tests) {
        if (!test.run()) { std::printf("[FAIL] %s\n", test.name); return 1; }
        std::printf("[PASS] %s\n", test.name);
    }
    return 0;
}
