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
    Presentation,
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
    AERO_NODISCARD virtual void* Allocate(const AllocationRequest& request) noexcept = 0;
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
    AERO_NODISCARD void* Allocate(const AllocationRequest& request) noexcept override;
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

AERO_NODISCARD AERO_API IAllocator& GetSystemAllocator() noexcept;
AERO_NODISCARD AERO_API IAllocator& GetDefaultAllocator() noexcept;
AERO_NODISCARD AERO_API IAllocator* SetDefaultAllocator(IAllocator* allocator) noexcept;

AERO_NODISCARD AERO_API OutOfMemoryHandler
SetOutOfMemoryHandler(OutOfMemoryHandler handler) noexcept;

[[noreturn]] AERO_API void ReportOutOfMemory(
    std::size_t size,
    std::size_t alignment,
    MemoryTag tag) noexcept;

AERO_NODISCARD constexpr bool IsValidAlignment(std::size_t alignment) noexcept {
    return alignment != 0U && (alignment & (alignment - 1U)) == 0U;
}

} // namespace Aero::Base
