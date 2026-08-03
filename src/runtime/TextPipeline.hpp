#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Integration/ViewOptions.hpp>

namespace Aero::Controls::Detail { class TextBlockLayout; }

namespace Aero::Integration { class RenderDevice; }

namespace Aero::Runtime::Detail {

class TextPipeline {
public:
    explicit TextPipeline(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~TextPipeline() noexcept;

    TextPipeline(const TextPipeline&) = delete;
    TextPipeline& operator=(const TextPipeline&) = delete;

    Base::Result<void> Initialize(
        Integration::RenderDevice& device,
        const Integration::TextOptions& options) noexcept;
    Base::Result<bool> SynchronizeBackend(
        Integration::RenderDevice& device,
        bool force = false) noexcept;
    Base::Result<std::uint32_t> CollectGarbage() noexcept;
    void Shutdown() noexcept;

    ::Aero::Controls::Detail::TextBlockLayout*
    Layout() noexcept;

private:
    struct Impl;

    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Runtime::Detail
