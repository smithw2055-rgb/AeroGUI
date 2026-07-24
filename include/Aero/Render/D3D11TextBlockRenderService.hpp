#pragma once

#include <Aero/Render/TextBlockRenderService.hpp>
#include <Aero/Render/D3D11RendererBackend.hpp>

namespace Aero::Render {

class AERO_API D3D11GlyphRunResourceRegistry final
    : public IGlyphRunResourceRegistry {
public:
    explicit D3D11GlyphRunResourceRegistry(
        D3D11RenderPlanBackend& backend) noexcept
        : backend_(&backend) {}

    Base::Result<void> RegisterGlyphRun(
        Presentation::RenderGlyphRunId glyphRun,
        Rhi::ResourceHandle vertexBuffer,
        Rhi::ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        Rhi::ResourceHandle atlasTexture,
        Rhi::ResourceHandle sampler,
        Rhi::IndexType indexType) noexcept override;
    Base::Result<void> UnregisterGlyphRun(
        Presentation::RenderGlyphRunId glyphRun) noexcept override;

private:
    D3D11RenderPlanBackend* backend_ = nullptr;
};

} // namespace Aero::Render
