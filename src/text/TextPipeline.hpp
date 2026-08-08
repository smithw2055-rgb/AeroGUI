#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/ViewOptions.hpp>

#include <cstddef>
#include <cstdint>

namespace Aero::Controls { class TextBlockLayout; }

namespace Aero { class RenderDevice; }
namespace Aero::Render { struct TextResources; }

namespace Aero::Text {

struct TextPipelineState;

class TextPipeline {
public:
    explicit TextPipeline(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~TextPipeline() noexcept;

    TextPipeline(const TextPipeline&) = delete;
    TextPipeline& operator=(const TextPipeline&) = delete;

    Base::Result<void> Initialize(
        RenderDevice& device,
        ::Aero::Render::TextResources* resources,
        const TextOptions& options) noexcept;
    Base::Result<bool> SynchronizeBackend(
        RenderDevice& device,
        ::Aero::Render::TextResources* resources,
        bool force = false) noexcept;
    Base::Result<std::uint32_t> CollectGarbage() noexcept;
    void Shutdown() noexcept;

    ::Aero::Controls::TextBlockLayout*
    Layout() noexcept;

private:
    Base::IAllocator* allocator_ = nullptr;
    // The source-private state is constructed directly in this storage. This
    // preserves local helper types without a heap-allocated Pimpl lifetime.
    alignas(std::max_align_t) std::uint8_t stateStorage_[16384]{};
    TextPipelineState* state_ = nullptr;
};

} // namespace Aero::Text
