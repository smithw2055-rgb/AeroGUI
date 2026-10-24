#pragma once

#include "DisplayList.hpp"
#include "RenderResources.hpp"
#include "render/FrameEncoder.hpp"
#include "gui/controls/TextBlockLayout.hpp"
#include <AeroRender/RenderDevice.hpp>
#include <AeroRender/Texture.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace Aero::Text { class FontManager; }

#include "gui/text/TextLayout.hpp"

namespace Aero::Render {

class UiFrameEncoder;
struct TextRendererState {
    TextConfig config;
    Text::GlyphAtlas atlas;
    Base::Vector<Ref<Texture>> pageTextures;
    Base::Vector<std::uint32_t> pageIds;
    Base::Vector<RenderGlyphRunId> glyphRuns;
    Text::TextLayout layout;
    Base::Vector<RenderGlyphQuad> quads;
    std::uint64_t nextRunId = 1U;
    std::uint64_t useStamp = 1U;

    explicit TextRendererState(
        const TextConfig& cfg,
        Base::IAllocator* allocator) noexcept
        : config(cfg),
          atlas(allocator),
          pageTextures(allocator),
          pageIds(allocator),
          glyphRuns(allocator),
          layout(allocator),
          quads(allocator) {}
};

using TextConfig = Aero::Render::TextConfig;

class TextRenderer : public ::Aero::Controls::TextBlockLayout {
public:
    TextRenderer(
        Text::FontManager& fonts,
        RenderDevice& device,
        UiFrameEncoder* encoder = nullptr,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~TextRenderer() override;

    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    Base::Result<void> Initialize(const TextConfig& config) noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept;

    Base::Result<void> ShapeAndPrepare(
        const ::Aero::Controls::TextLayoutRequest& request,
        ::Aero::Controls::TextLayoutResult& output) noexcept override;
    void ReleaseGlyphRun(Render::RenderGlyphRunId glyphRun) noexcept override;

    Base::Result<std::uint32_t> CollectGarbage() noexcept;

private:
    Text::FontManager* fonts_ = nullptr;
    RenderDevice* device_ = nullptr;
    UiFrameEncoder* encoder_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    std::optional<TextRendererState> state_;
};

} // namespace Aero::Render
