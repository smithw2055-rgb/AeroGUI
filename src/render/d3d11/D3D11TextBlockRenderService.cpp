#include <Aero/Render/D3D11TextBlockRenderService.hpp>

namespace Aero::Render {

Base::Result<void>
D3D11GlyphRunResourceRegistry::RegisterGlyphRun(
    Presentation::RenderGlyphRunId glyphRun,
    Rhi::ResourceHandle vertexBuffer,
    Rhi::ResourceHandle indexBuffer,
    std::uint32_t indexCount,
    Rhi::ResourceHandle atlasTexture,
    Rhi::ResourceHandle sampler,
    Rhi::IndexType indexType) noexcept {
    return backend_->RegisterGlyphRun(
        glyphRun, vertexBuffer, indexBuffer,
        indexCount, atlasTexture, sampler,
        indexType);
}

Base::Result<void>
D3D11GlyphRunResourceRegistry::UnregisterGlyphRun(
    Presentation::RenderGlyphRunId glyphRun) noexcept {
    return backend_->UnregisterGlyphRun(glyphRun);
}

} // namespace Aero::Render
