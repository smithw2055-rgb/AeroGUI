#pragma once

#include <Aero/Base/Allocator.hpp>

namespace Aero::Tests {

class ScopedDefaultAllocator final {
public:
    explicit ScopedDefaultAllocator(Base::IAllocator& allocator) noexcept
        : previous_(Base::SetDefaultAllocator(&allocator)) {}

    ~ScopedDefaultAllocator() {
        Base::SetDefaultAllocator(previous_);
    }

    ScopedDefaultAllocator(const ScopedDefaultAllocator&) = delete;
    ScopedDefaultAllocator& operator=(const ScopedDefaultAllocator&) = delete;
    ScopedDefaultAllocator(ScopedDefaultAllocator&&) = delete;
    ScopedDefaultAllocator& operator=(ScopedDefaultAllocator&&) = delete;

private:
    Base::IAllocator* previous_ = nullptr;
};

} // namespace Aero::Tests
