#include <Aero/Base/Allocator.hpp>

#include <atomic>
#include <cstdlib>

#if defined(_MSC_VER)
#  include <malloc.h>
#endif

namespace Aero::Base {
namespace {

MallocAllocator gSystemAllocator;
std::atomic<IAllocator*> gDefaultAllocator{&gSystemAllocator};

void DefaultOutOfMemoryHandler(
    std::size_t,
    std::size_t,
    MemoryTag) noexcept {
    std::abort();
}

std::atomic<OutOfMemoryHandler> gOutOfMemoryHandler{
    &DefaultOutOfMemoryHandler};

} // namespace

void* MallocAllocator::Allocate(const AllocationRequest& request) noexcept {
    if (!IsValidAlignment(request.alignment)) {
        return nullptr;
    }

    const std::size_t size = request.size == 0U ? 1U : request.size;
    const std::size_t alignment = request.alignment < alignof(void*)
        ? alignof(void*)
        : request.alignment;

#if defined(_MSC_VER)
    return _aligned_malloc(size, alignment);
#else
    void* memory = nullptr;
    if (posix_memalign(&memory, alignment, size) != 0) {
        return nullptr;
    }
    return memory;
#endif
}

void MallocAllocator::Deallocate(
    void* memory,
    std::size_t,
    std::size_t,
    MemoryTag) noexcept {
    if (memory == nullptr) {
        return;
    }

#if defined(_MSC_VER)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}

IAllocator& GetSystemAllocator() noexcept {
    return gSystemAllocator;
}

IAllocator& GetDefaultAllocator() noexcept {
    IAllocator* allocator = gDefaultAllocator.load(std::memory_order_acquire);
    return allocator != nullptr ? *allocator : gSystemAllocator;
}

IAllocator* SetDefaultAllocator(IAllocator* allocator) noexcept {
    IAllocator* replacement = allocator != nullptr ? allocator : &gSystemAllocator;
    return gDefaultAllocator.exchange(replacement, std::memory_order_acq_rel);
}

OutOfMemoryHandler SetOutOfMemoryHandler(OutOfMemoryHandler handler) noexcept {
    OutOfMemoryHandler replacement =
        handler != nullptr ? handler : &DefaultOutOfMemoryHandler;
    return gOutOfMemoryHandler.exchange(replacement, std::memory_order_acq_rel);
}

[[noreturn]] void ReportOutOfMemory(
    std::size_t size,
    std::size_t alignment,
    MemoryTag tag) noexcept {
    OutOfMemoryHandler handler =
        gOutOfMemoryHandler.load(std::memory_order_acquire);
    handler(size, alignment, tag);
    std::abort();
}

} // namespace Aero::Base
