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
    std::uint64_t nextRunId = 1U;
    std::uint64_t useStamp = 1U;

    explicit TextRendererState(
        const TextConfig& cfg,
        Base::IAllocator* allocator) noexcept
        : config(cfg),
          atlas(allocator),
          pageTextures(allocator) {}
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
    layoutRequest.fallbackFaces = state_->config.fallbackFaces;
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
        }
    }

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

    return {};
}

void TextRenderer::ReleaseGlyphRun(Render::RenderGlyphRunId glyphRun) noexcept {
    static_cast<void>(glyphRun);
}

Base::Result<std::uint32_t> TextRenderer::CollectGarbage() noexcept {
    return 0U;
}

} // namespace Aero::Render
