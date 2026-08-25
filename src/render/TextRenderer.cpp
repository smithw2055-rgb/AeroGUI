#include "TextRenderer.hpp"
#include "FrameEncoder.hpp"
#include "gui/text/FontManager.hpp"
#include "gui/text/GlyphAtlas.hpp"
#include "gui/text/TextLayout.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>

namespace Aero::Render {

namespace {

constexpr float GlyphRasterScale = 4.0F;

bool IsValidConfig(const TextConfig& config) noexcept {
    auto validFace = [](const Text::FontFace& face) noexcept {
        return face.handle.IsValid() &&
            std::isfinite(face.metrics.unitsPerEm) &&
            face.metrics.unitsPerEm > 0.0F &&
            std::isfinite(face.metrics.ascent) &&
            std::isfinite(face.metrics.descent) &&
            std::isfinite(face.metrics.lineGap);
    };
    if (!validFace(config.face) ||
        !std::isfinite(config.pixelSize) ||
        config.pixelSize <= 0.0F ||
        !std::isfinite(config.lineHeight) ||
        config.lineHeight < 0.0F) {
        return false;
    }
    for (const Text::FontFace& fallback : config.fallbackFaces) {
        if (!validFace(fallback)) return false;
    }
    return true;
}

} // namespace

struct TextRendererState {
    TextConfig config;
    Text::GlyphAtlas atlas;
    Base::Vector<Ref<Texture>> pageTextures;
    Base::Vector<RenderGlyphRunId> glyphRuns;
    std::uint64_t nextRunId = 1U;
    std::uint64_t useStamp = 1U;

    explicit TextRendererState(
        const TextConfig& cfg,
        Base::IAllocator* allocator) noexcept
        : config(cfg),
          atlas(allocator),
          pageTextures(allocator),
          glyphRuns(allocator) {}
};

TextRenderer::TextRenderer(
    Text::FontManager& fonts,
    RenderDevice& device,
    UiFrameEncoder* encoder,
    Base::IAllocator* allocator) noexcept
    : fonts_(&fonts),
      device_(&device),
      encoder_(encoder),
      allocator_(allocator) {}

TextRenderer::~TextRenderer() {
    Shutdown();
}

Base::Result<void> TextRenderer::Initialize(const TextConfig& config) noexcept {
    if (state_ != nullptr) return {};
    if (!IsValidConfig(config)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Text configuration is invalid");
    }

    state_ = new (stateStorage_) TextRendererState(config, allocator_);
    state_->nextRunId = config.firstGlyphRunId;
    Base::Result<void> atlasInit = state_->atlas.Initialize(config.atlas);
    if (!atlasInit) {
        state_->~TextRendererState();
        state_ = nullptr;
        return atlasInit.GetStatus();
    }
    return {};
}

void TextRenderer::Shutdown() noexcept {
    if (state_ != nullptr) {
        if (encoder_ != nullptr) {
            for (RenderGlyphRunId glyphRun : state_->glyphRuns) {
                encoder_->UnregisterGlyphRun(glyphRun);
            }
        }
        state_->~TextRendererState();
        state_ = nullptr;
    }
}

bool TextRenderer::IsInitialized() const noexcept {
    return state_ != nullptr;
}

Base::Result<void> TextRenderer::ShapeAndPrepare(
    const ::Aero::Controls::TextLayoutRequest& request,
    ::Aero::Controls::TextLayoutResult& output) noexcept {
    if (state_ == nullptr || fonts_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "TextRenderer is not initialized");
    }

    Text::TextLayoutRequest layoutRequest;
    layoutRequest.face = request.face.handle.IsValid() ? request.face : state_->config.face;
    layoutRequest.fallbackFaces = !request.fallbackFaces.Empty() ? request.fallbackFaces : state_->config.fallbackFaces;
    layoutRequest.text = request.text;
    layoutRequest.pixelSize = request.pixelSize > 0.0F ? request.pixelSize : state_->config.pixelSize;
    layoutRequest.maxWidth = request.availableSize.width > 0.0
        ? static_cast<float>(request.availableSize.width)
        : std::numeric_limits<float>::infinity();
    layoutRequest.lineHeight = request.lineHeight;
    layoutRequest.direction = request.direction;
    layoutRequest.wrapping = request.wrapping;
    layoutRequest.trimming = request.trimming;
    layoutRequest.alignment = request.alignment;

    Text::TextLayout layout(allocator_);
    Base::Result<void> shaped = layout.ShapeAndMeasure(*fonts_, layoutRequest);
    if (!shaped) return shaped.GetStatus();

    if (request.arrangeToAvailableWidth && std::isfinite(layoutRequest.maxWidth)) {
        static_cast<void>(layout.Arrange(layoutRequest.maxWidth));
    }

    output.desiredSize = Aero::Size{
        static_cast<double>(layout.Size().width),
        static_cast<double>(layout.Size().height)
    };

    const float dpi = static_cast<float>(request.dpiScale);
    const float glyphRasterDpi = std::max(1.0F, dpi * GlyphRasterScale);
    const auto& atlasConfig = state_->config.atlas;
    const float pageWidth = static_cast<float>(atlasConfig.pageWidth);
    const float pageHeight = static_cast<float>(atlasConfig.pageHeight);

    Base::Vector<RenderGlyphQuad> quads(allocator_);
    for (const Text::GlyphRun& run : layout.Runs()) {
        for (const Text::PositionedGlyph& glyph : run.glyphs) {
            Text::GlyphRequest glyphRequest;
            glyphRequest.face = run.face;
            glyphRequest.glyph = glyph.glyph;
            glyphRequest.pixelSize = run.pixelSize;
            glyphRequest.dpiScale = glyphRasterDpi;

            Text::GlyphMetrics metrics;
            Base::Result<void> measured = fonts_->GetGlyphMetrics(glyphRequest, metrics);
            if (!measured || metrics.width <= 0.0F || metrics.height <= 0.0F) continue;

            Text::GlyphAtlasPlacement placement;
            Base::Result<void> ensured = state_->atlas.EnsureGlyph(
                *fonts_, glyphRequest, state_->useStamp++, 0U, placement);
            if (!ensured) return ensured.GetStatus();

            RenderGlyphQuad quad;
            quad.x0 = static_cast<float>(glyph.x) +
                static_cast<float>(placement.bearingX) / glyphRasterDpi;
            quad.y0 = static_cast<float>(glyph.y) -
                static_cast<float>(placement.bearingY) / glyphRasterDpi;
            quad.x1 = quad.x0 +
                static_cast<float>(placement.width) / glyphRasterDpi;
            quad.y1 = quad.y0 +
                static_cast<float>(placement.height) / glyphRasterDpi;
            quad.u0 = static_cast<float>(placement.x) / pageWidth;
            quad.v0 = static_cast<float>(placement.y) / pageHeight;
            quad.u1 = static_cast<float>(placement.x + placement.width) / pageWidth;
            quad.v1 = static_cast<float>(placement.y + placement.height) / pageHeight;
            quad.page = placement.page;
            Base::Result<void> appended = quads.PushBack(quad);
            if (!appended) return appended.GetStatus();
        }
    }

    if (quads.Empty()) return {};

    const std::uint32_t pageCount = state_->atlas.PageCount();
    while (state_->pageTextures.Size() < pageCount) {
        Ref<Texture> tex = device_->CreateTexture(
            "GlyphAtlas", atlasConfig.pageWidth, atlasConfig.pageHeight,
            1, TextureFormat::R8, nullptr);
        if (!tex) break;
        if (encoder_ != nullptr) {
            static_cast<void>(encoder_->RegisterGlyphAtlas(
                static_cast<std::uint32_t>(state_->pageTextures.Size()), tex));
        }
        static_cast<void>(state_->pageTextures.PushBack(std::move(tex)));
    }

    for (const Text::GlyphAtlasUpload& upload : state_->atlas.PendingUploads()) {
        if (upload.page < state_->pageTextures.Size() && state_->pageTextures[upload.page]) {
            device_->UpdateTexture(
                state_->pageTextures[upload.page].Get(),
                0, upload.x, upload.y, upload.width, upload.height, upload.pixels.Data());
        }
    }
    state_->atlas.ClearPendingUploads();

    if (state_->nextRunId == Render::InvalidRenderGlyphRunId) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Text renderer glyph-run ID space is exhausted");
    }
    const RenderGlyphRunId glyphRun = state_->nextRunId++;
    if (encoder_ != nullptr) {
        Base::Result<void> registered =
            encoder_->RegisterGlyphRun(glyphRun, quads.AsSpan());
        if (!registered) return registered.GetStatus();
    }
    Base::Result<void> tracked = state_->glyphRuns.PushBack(glyphRun);
    if (!tracked) return tracked.GetStatus();
    Base::Result<void> emitted = output.glyphRuns.PushBack(glyphRun);
    if (!emitted) return emitted.GetStatus();

    output.hitRegions.Clear();
    for (const Text::TextLine& line : layout.Lines()) {
        const float lineHeight = (line.ascent + line.descent > 0.0F)
            ? (line.ascent + line.descent)
            : (request.pixelSize > 0.0F ? request.pixelSize : 16.0F);
        for (std::uint32_t r = 0U; r < line.runCount; ++r) {
            const Text::GlyphRun& run = layout.Runs()[line.firstRun + r];
            for (std::size_t g = 0; g < run.glyphs.Size(); ++g) {
                const Text::PositionedGlyph& glyph = run.glyphs[g];
                TextHitRegion region;
                region.textOffset = glyph.cluster;
                region.textLength = (g + 1 < run.glyphs.Size() && run.glyphs[g + 1].cluster > glyph.cluster)
                    ? run.glyphs[g + 1].cluster - glyph.cluster
                    : 1U;
                region.x = glyph.x;
                region.y = line.y;
                region.width = glyph.advanceX;
                region.height = lineHeight;
                (void)output.hitRegions.PushBack(region);
            }
        }
    }

    return {};
}

void TextRenderer::ReleaseGlyphRun(Render::RenderGlyphRunId glyphRun) noexcept {
    if (encoder_ != nullptr) {
        encoder_->UnregisterGlyphRun(glyphRun);
    }
    if (state_ != nullptr) {
        for (std::uint32_t index = 0U; index < state_->glyphRuns.Size(); ++index) {
            if (state_->glyphRuns[index] == glyphRun) {
                if (index != state_->glyphRuns.Size() - 1U) {
                    state_->glyphRuns[index] =
                        state_->glyphRuns[state_->glyphRuns.Size() - 1U];
                }
                static_cast<void>(
                    state_->glyphRuns.Resize(state_->glyphRuns.Size() - 1U));
                break;
            }
        }
    }
}

Base::Result<std::uint32_t> TextRenderer::CollectGarbage() noexcept {
    return 0U;
}

} // namespace Aero::Render
