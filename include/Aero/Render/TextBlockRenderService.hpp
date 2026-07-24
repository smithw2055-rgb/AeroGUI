#pragma once

#include <Aero/Controls/TextBlockLayoutService.hpp>
#include <Aero/Rhi/Graphics.hpp>
#include <Aero/Text/GlyphAtlas.hpp>
#include <Aero/Text/TextLayout.hpp>

#include <cstdint>

namespace Aero::Render {

class AERO_API IGlyphRunResourceRegistry {
public:
    virtual ~IGlyphRunResourceRegistry() = default;

    virtual Base::Result<void> RegisterGlyphRun(
        Presentation::RenderGlyphRunId glyphRun,
        Rhi::ResourceHandle vertexBuffer,
        Rhi::ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        Rhi::ResourceHandle atlasTexture,
        Rhi::ResourceHandle sampler,
        Rhi::IndexType indexType) noexcept = 0;
    virtual Base::Result<void> UnregisterGlyphRun(
        Presentation::RenderGlyphRunId glyphRun) noexcept = 0;
};

struct TextBlockRenderServiceConfig final {
    Text::FontFace face;
    float pixelSize = 16.0F;
    float lineHeight = 0.0F;
    Text::TextWrapping wrapping = Text::TextWrapping::NoWrap;
    Text::TextTrimming trimming = Text::TextTrimming::None;
    Text::TextAlignment alignment = Text::TextAlignment::Start;
    Text::GlyphAtlasConfig atlas;
    Presentation::RenderGlyphRunId firstGlyphRunId =
        UINT64_C(1) << 32U;
};

struct TextBlockRenderServiceStatistics final {
    std::uint32_t atlasPages = 0U;
    std::uint32_t atlasEntries = 0U;
    std::uint32_t activeGlyphRuns = 0U;
    std::uint32_t retiredGlyphRuns = 0U;
    Rhi::FenceValue lastUploadFence = 0U;
};

class AERO_API TextBlockRenderService final
    : public Controls::ITextBlockLayoutService {
public:
    TextBlockRenderService(
        Text::FontManager& fonts,
        Rhi::RhiDevice& device,
        Rhi::IGraphicsBackend& graphics,
        IGlyphRunResourceRegistry& registry,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~TextBlockRenderService() override;

    TextBlockRenderService(
        const TextBlockRenderService&) = delete;
    TextBlockRenderService& operator=(
        const TextBlockRenderService&) = delete;

    Base::Result<void> Initialize(
        const TextBlockRenderServiceConfig& config) noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept;

    Base::Result<void> ShapeAndPrepare(
        const Controls::TextBlockLayoutRequest& request,
        Controls::TextBlockLayoutResult& output) noexcept override;
    void ReleaseGlyphRun(
        Presentation::RenderGlyphRunId glyphRun) noexcept override;

    Base::Result<std::uint32_t> CollectGarbage() noexcept;
    TextBlockRenderServiceStatistics Statistics() const noexcept;

private:
    struct Impl;

    Text::FontManager* fonts_ = nullptr;
    Rhi::RhiDevice* device_ = nullptr;
    Rhi::IGraphicsBackend* graphics_ = nullptr;
    IGlyphRunResourceRegistry* registry_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Render
