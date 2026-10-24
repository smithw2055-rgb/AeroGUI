#include "TextRenderer.hpp"
#include "FrameEncoder.hpp"
#include "gui/text/FontManager.hpp"
#include "gui/text/GlyphAtlas.hpp"
#include "gui/text/TextLayout.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>

namespace Aero::Render {

namespace {

constexpr float GlyphRasterScale = 2.0F;

std::uint32_t AllocateAtlasPage() noexcept {
    static std::atomic<std::uint32_t> next{1U};
    return next.fetch_add(1U, std::memory_order_relaxed);
}


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
    if (state_.has_value()) return {};
    if (!IsValidConfig(config)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Text configuration is invalid");
    }

    state_.emplace(config, allocator_);
    state_->nextRunId = config.firstGlyphRunId;
    Base::Result<void> atlasInit = state_->atlas.Initialize(config.atlas);
    if (!atlasInit) {
        state_.reset();
        return atlasInit.GetStatus();
    }
    return {};
}

void TextRenderer::Shutdown() noexcept {
    if (state_.has_value()) {
        if (encoder_ != nullptr) {
            for (RenderGlyphRunId glyphRun : state_->glyphRuns) {
                encoder_->UnregisterGlyphRun(glyphRun);
            }
        }
        state_.reset();
    }
}

bool TextRenderer::IsInitialized() const noexcept {
    return state_.has_value();
}

Base::Result<void> TextRenderer::ShapeAndPrepare(
    const ::Aero::Controls::TextLayoutRequest& request,
    ::Aero::Controls::TextLayoutResult& output) noexcept {
    if (!state_.has_value() || fonts_ == nullptr) {
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

    Text::TextLayout& layout = state_->layout;
    layout.Clear();
    Base::Result<void> shaped = layout.ShapeAndMeasure(*fonts_, layoutRequest);
    if (!shaped) return shaped.GetStatus();

    if (request.arrangeToAvailableWidth && std::isfinite(layoutRequest.maxWidth)) {
        static_cast<void>(layout.Arrange(layoutRequest.maxWidth));
    }

    output.desiredSize = ::Aero::Size{
        static_cast<double>(layout.Size().width),
        static_cast<double>(layout.Size().height)
    };

    const float dpi = static_cast<float>(request.dpiScale);
    const float glyphRasterDpi = std::max(1.0F, dpi * GlyphRasterScale);
    const auto& atlasConfig = state_->config.atlas;
    const float pageWidth = static_cast<float>(atlasConfig.pageWidth);
    const float pageHeight = static_cast<float>(atlasConfig.pageHeight);
    const std::uint64_t runStamp = ++state_->useStamp;

    state_->quads.Clear();
    Base::Vector<RenderGlyphQuad>& quads = state_->quads;
    for (const Text::GlyphRun& run : layout.Runs()) {
        for (const Text::PositionedGlyph& glyph : run.glyphs) {
            Text::GlyphRequest glyphRequest;
            glyphRequest.face = run.face;
            glyphRequest.glyph = glyph.glyph;
            glyphRequest.pixelSize = run.pixelSize;
            glyphRequest.dpiScale = glyphRasterDpi;

            Text::GlyphAtlasPlacement placement;
            Base::Result<void> ensured = state_->atlas.EnsureGlyph(
                *fonts_, glyphRequest, ++state_->useStamp, runStamp, placement);
            if (!ensured || placement.width == 0U || placement.height == 0U) continue;

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
            quads.PushBack(quad);
        }
    }

    if (quads.Empty()) return {};

    const std::uint32_t pageCount = state_->atlas.PageCount();
    while (state_->pageTextures.Size() < pageCount) {
        Ref<Texture> tex = device_->CreateTexture(
            "GlyphAtlas", atlasConfig.pageWidth, atlasConfig.pageHeight,
            1, TextureFormat::R8, nullptr);
        if (!tex) break;
        const std::uint32_t globalPage = AllocateAtlasPage();
        if (encoder_ != nullptr) {
            static_cast<void>(encoder_->RegisterGlyphAtlas(globalPage, tex));
        }
        static_cast<void>(state_->pageIds.PushBack(globalPage));
        static_cast<void>(state_->pageTextures.PushBack(std::move(tex)));
    }
    for (std::uint32_t i = 0U; i < quads.Size(); ++i) {
        const std::uint32_t localPage = quads[i].page;
        if (localPage < state_->pageIds.Size()) {
            quads[i].page = state_->pageIds[localPage];
        }
    }

    for (const Text::GlyphAtlasUpload& upload : state_->atlas.PendingUploads()) {
        if (upload.page < state_->pageTextures.Size() && state_->pageTextures[upload.page]) {
            device_->UpdateTexture(
                state_->pageTextures[upload.page].Get(),
                0, upload.x, upload.y, upload.width, upload.height, upload.pixels.Data());
        }
    }
    state_->atlas.ClearPendingUploads();

    const RenderGlyphRunId glyphRun = state_->nextRunId++;
    if (glyphRun == Render::InvalidRenderGlyphRunId) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Text renderer glyph-run ID space is exhausted");
    }
    if (encoder_ != nullptr) {
        Base::Result<void> registered =
            encoder_->RegisterGlyphRun(glyphRun, quads.AsSpan());
        if (!registered) return registered.GetStatus();
    }
    state_->glyphRuns.PushBack(glyphRun);
    output.glyphRuns.PushBack(glyphRun);

    output.hitRegions.Clear();
    for (const Text::TextLine& line : layout.Lines()) {
        const float lineHeight = (line.ascent + line.descent > 0.0F)
            ? (line.ascent + line.descent)
            : (request.pixelSize > 0.0F ? request.pixelSize : 16.0F);
        for (std::uint32_t r = 0U; r < line.runCount; ++r) {
            const Text::GlyphRun& run = layout.Runs()[line.firstRun + r];
            for (std::uint32_t g = 0U; g < run.glyphs.Size(); ++g) {
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
    if (state_.has_value()) {
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
