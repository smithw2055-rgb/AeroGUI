#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Dispatcher.hpp>

#include <cstdio>
#include <cstring>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf( \
                stderr, \
                "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

class TrackingAllocator final
    : public IAllocator {
public:
    void* Allocate(
        const AllocationRequest& request)
        noexcept override {
        void* memory =
            GetSystemAllocator().Allocate(
                request);
        if (memory != nullptr) {
            ++liveAllocations;
            liveBytes += request.size;
            if (liveBytes > peakBytes) {
                peakBytes = liveBytes;
            }
        }
        return memory;
    }

    void Deallocate(
        void* memory,
        std::size_t size,
        std::size_t alignment,
        MemoryTag tag) noexcept override {
        if (memory != nullptr) {
            --liveAllocations;
            liveBytes -= size;
        }
        GetSystemAllocator().Deallocate(
            memory, size, alignment, tag);
    }

    std::size_t liveAllocations = 0U;
    std::size_t liveBytes = 0U;
    std::size_t peakBytes = 0U;
};

bool MemoryStabilityGate() {
    TrackingAllocator allocator;
    for (std::uint32_t cycle = 0U;
         cycle < 200U;
         ++cycle) {
        {
            Vector<std::uint64_t> values(
                &allocator);
            HashMap<
                std::uint64_t,
                std::uint64_t> map(
                    &allocator);
            for (std::uint32_t index = 0U;
                 index < 1024U;
                 ++index) {
                CHECK(values.TryPushBack(
                    (static_cast<std::uint64_t>(
                        cycle) << 32U) |
                    index));
                CHECK(map.TryInsert(
                    index,
                    values[index]));
            }
            CHECK(values.Size() == 1024U);
            CHECK(map.Size() == 1024U);
        }
        CHECK(allocator.liveAllocations ==
            0U);
        CHECK(allocator.liveBytes == 0U);
    }
    CHECK(allocator.peakBytes != 0U);
    std::printf(
        "MemoryStabilityGate cycles=200 "
        "peak-bytes=%zu outstanding=0\n",
        allocator.peakBytes);
    return true;
}

void Increment(void* context) noexcept {
    auto* value =
        static_cast<std::uint64_t*>(
            context);
    ++*value;
}

bool LongRunStabilityGate() {
    Dispatcher dispatcher;
    std::uint64_t callbacks = 0U;
    constexpr std::uint32_t Frames = 25000U;
    const DispatcherFramePhase phases[] = {
        DispatcherFramePhase::BeginFrame,
        DispatcherFramePhase::Input,
        DispatcherFramePhase::PropertyChanges,
        DispatcherFramePhase::DataBind,
        DispatcherFramePhase::Animation,
        DispatcherFramePhase::Layout,
        DispatcherFramePhase::Lifecycle,
        DispatcherFramePhase::RenderCommit,
        DispatcherFramePhase::EndFrame};
    for (std::uint32_t frame = 0U;
         frame < Frames;
         ++frame) {
        CHECK(dispatcher.Post(
            DispatcherPriority::Normal,
            &Increment,
            &callbacks));
        CHECK(dispatcher.ProcessPending());
        for (DispatcherFramePhase phase :
             phases) {
            CHECK(dispatcher.RunFramePhase(
                phase));
        }
        CHECK(!dispatcher.IsPumping());
        CHECK(dispatcher.
            PendingTaskCount() == 0U);
    }
    CHECK(callbacks == Frames);
    CHECK(dispatcher.FrameTimings().
        frameSequence == Frames);
    std::printf(
        "LongRunStabilityGate frames=%u "
        "callbacks=%llu\n",
        Frames,
        static_cast<unsigned long long>(
            callbacks));
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    if (std::strcmp(
            argv[1], "memory") == 0) {
        return MemoryStabilityGate()
            ? 0
            : 1;
    }
    if (std::strcmp(
            argv[1], "longrun") == 0) {
        return LongRunStabilityGate()
            ? 0
            : 1;
    }
    return 2;
}
