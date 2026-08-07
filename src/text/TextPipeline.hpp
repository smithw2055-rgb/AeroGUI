#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/ViewOptions.hpp>

namespace Aero::Controls::Detail { class TextBlockLayout; }

namespace Aero { class RenderDevice; }

namespace Aero::Text::Detail {

class TextPipeline {
public:
    explicit TextPipeline(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~TextPipeline() noexcept;

    TextPipeline(const TextPipeline&) = delete;
    TextPipeline& operator=(const TextPipeline&) = delete;

    Base::Result<void> Initialize(
        RenderDevice& device,
        const TextOptions& options) noexcept;
    Base::Result<bool> SynchronizeBackend(
        RenderDevice& device,
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

} // namespace Aero::Text::Detail
