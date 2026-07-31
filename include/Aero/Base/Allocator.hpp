#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>

#include <cstddef>

namespace Aero::Base {

enum class MemoryTag : std::uint16_t {
    General = 0,
    String,
    Container,
    Object,
    WeakControlBlock,
    Markup,
    Ui,
    Render,
    Platform,
    Test
};

struct AllocationRequest final {
    std::size_t size = 0;
    std::size_t alignment = alignof(std::max_align_t);
    MemoryTag tag = MemoryTag::General;
};

class IAllocator {
public:
    virtual void* Allocate(const AllocationRequest& request) noexcept = 0;
    virtual void Deallocate(
        void* memory,
        std::size_t size,
        std::size_t alignment,
        MemoryTag tag) noexcept = 0;

protected:
    ~IAllocator() = default;
};

class MallocAllocator final : public IAllocator {
public:
    void* Allocate(const AllocationRequest& request) noexcept override;
    void Deallocate(
        void* memory,
        std::size_t size,
        std::size_t alignment,
        MemoryTag tag) noexcept override;
};

using OutOfMemoryHandler = void(*)(
    std::size_t size,
    std::size_t alignment,
    MemoryTag tag) noexcept;

AERO_API IAllocator& GetSystemAllocator() noexcept;
AERO_API IAllocator& GetDefaultAllocator() noexcept;
AERO_API IAllocator* SetDefaultAllocator(IAllocator* allocator) noexcept;

AERO_API OutOfMemoryHandler
SetOutOfMemoryHandler(OutOfMemoryHandler handler) noexcept;

[[noreturn]] AERO_API void ReportOutOfMemory(
    std::size_t size,
    std::size_t alignment,
    MemoryTag tag) noexcept;

constexpr bool IsValidAlignment(std::size_t alignment) noexcept {
    return alignment != 0U && (alignment & (alignment - 1U)) == 0U;
}

} // namespace Aero::Base
