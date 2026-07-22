#pragma once

#include <Aero/Core/Rendering.hpp>
#include <Aero/Rhi/Rhi.hpp>

namespace Aero::Render {

// Integration boundary from the retained Core snapshot model to backend-neutral
// RHI commands. Keeping it outside AeroRhi prevents the low-level RHI from
// depending on the UI object/render-plan layer.
class AERO_API RenderPlanTranslator final {
public:
    explicit RenderPlanTranslator(
        Base::IAllocator* allocator = nullptr) noexcept
        : allocator_(allocator) {}

    AERO_NODISCARD Base::Result<Rhi::CommandBuffer> Translate(
        const Core::RenderPlan& plan) const noexcept;

private:
    Base::IAllocator* allocator_ = nullptr;
};

} // namespace Aero::Render
