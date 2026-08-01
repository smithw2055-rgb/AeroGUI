#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Integration/ViewOptions.hpp>

namespace Aero::Controls::Detail {
class TextLayoutService;
}

namespace Aero::Integration { class RenderEndpoint; }

namespace Aero::Detail {

class TextRuntime final {
public:
    explicit TextRuntime(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~TextRuntime() noexcept;

    TextRuntime(const TextRuntime&) = delete;
    TextRuntime& operator=(const TextRuntime&) = delete;

    Base::Result<void> Initialize(
        Integration::RenderEndpoint& endpoint,
        const Integration::TextOptions& options) noexcept;
    Base::Result<bool> SynchronizeBackend(
        Integration::RenderEndpoint& endpoint,
        bool force = false) noexcept;
    Base::Result<std::uint32_t> CollectGarbage() noexcept;
    void Shutdown() noexcept;

    Controls::Detail::TextLayoutService*
    Service() noexcept;

private:
    struct Impl;

    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Detail
