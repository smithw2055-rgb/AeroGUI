#include <Aero/Render/TextBlockRenderService.hpp>

#include <cstdio>

namespace {

using namespace Aero::Base;
using namespace Aero::Controls;
using namespace Aero::Presentation;
using namespace Aero::Render;
using namespace Aero::Rhi;
using namespace Aero::Text;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

constexpr FontProviderIdentity ProviderIdentity{601U, 1U};

class RenderFontProvider final
    : public IFontProvider,
      public ITextShaper,
      public IGlyphRasterizer {
public:
    FontProviderIdentity Identity() const noexcept override {
        return ProviderIdentity;
    }

    Result<void> LoadFace(
        const FontSource&,
        const Typeface&,
        FontFace& output) noexcept override {
        output = MakeFace();
        return {};
    }

    Result<void> ResolveFace(
        const FontQuery&,
        FontFace& output) noexcept override {
        output = MakeFace();
        return {};
    }

    Result<bool> HasCodePoint(
        FontFaceHandle,
        std::uint32_t codePoint) noexcept override {
        return codePoint <= 0x10FFFFU;
    }

    void ReleaseFace(FontFaceHandle) noexcept override {}

    bool Supports(FontProviderIdentity provider) const noexcept override {
        return provider == ProviderIdentity;
    }

    Result<void> Shape(
        const ShapingRequest& request,
        ShapedTextRun& output) noexcept override {
        output.face = request.face;
        output.direction = TextDirection::LeftToRight;
        output.script = Script::Latin;
        for (std::uint32_t index = 0U;
             index < request.text.SizeBytes(); ++index) {
            ShapedGlyph glyph;
            glyph.glyph =
                static_cast<unsigned char>(request.text[index]);
            glyph.cluster = index;
            glyph.advanceX = 4.0F;
            Result<void> appended =
                output.glyphs.TryPushBack(glyph);
            if (!appended) return appended.GetStatus();
        }
        return {};
    }

    Result<void> GetMetrics(
        const GlyphRequest&,
        GlyphMetrics& output) noexcept override {
        output.width = 4.0F;
        output.height = 4.0F;
        output.bearingY = 4.0F;
        output.advanceX = 4.0F;
        return {};
    }

    Result<void> Rasterize(
        const GlyphRequest& request,
        GlyphBitmap& output) noexcept override {
        output.width = 4U;
        output.height = 4U;
        output.strideBytes = 4U;
        output.bearingY = 4;
        Result<void> resized = output.pixels.TryResize(16U);
        if (!resized) return resized.GetStatus();
        const std::uint8_t value =
            static_cast<std::uint8_t>(request.glyph);
        for (std::uint8_t& pixel : output.pixels) {
            pixel = value;
        }
        return {};
    }

    Result<void> ExtractOutline(
        const GlyphRequest&,
        GlyphOutline&) noexcept override {
        return Status::Failure(
            ErrorCode::Unsupported,
            "Outline is not used by text render tests");
    }

    static FontFace MakeFace() noexcept {
        FontFace face;
        face.handle.provider = ProviderIdentity;
        face.handle.face = 1U;
        face.handle.generation = 1U;
        face.metrics.unitsPerEm = 1000.0F;
        face.metrics.ascent = 800.0F;
        face.metrics.descent = -200.0F;
        return face;
    }
};

class MockGlyphRegistry final
    : public IGlyphRunResourceRegistry {
public:
    explicit MockGlyphRegistry(
        RhiDevice& device) noexcept
        : device_(&device) {}

    Result<void> RegisterGlyphRun(
        RenderGlyphRunId glyphRun,
        ResourceHandle vertexBuffer,
        ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        ResourceHandle atlasTexture,
        ResourceHandle sampler,
        IndexType indexType) noexcept override {
        if (glyphRun == InvalidRenderGlyphRunId ||
            !device_->IsAlive(vertexBuffer) ||
            !device_->IsAlive(indexBuffer) ||
            !device_->IsAlive(atlasTexture) ||
            !device_->IsAlive(sampler) ||
            indexCount == 0U ||
            indexType != IndexType::UInt32) {
            return Status::Failure(
                ErrorCode::InvalidArgument,
                "Mock glyph registration is invalid");
        }
        for (RenderGlyphRunId existing : ids_) {
            if (existing == glyphRun) {
                return Status::Failure(
                    ErrorCode::AlreadyExists,
                    "Mock glyph run is already registered");
            }
        }
        return ids_.TryPushBack(glyphRun);
    }

    Result<void> UnregisterGlyphRun(
        RenderGlyphRunId glyphRun) noexcept override {
        for (std::uint32_t index = 0U;
             index < ids_.Size(); ++index) {
            if (ids_[index] != glyphRun) continue;
            for (std::uint32_t next = index + 1U;
                 next < ids_.Size(); ++next) {
                ids_[next - 1U] = ids_[next];
            }
            ids_.PopBack();
            return {};
        }
        return Status::Failure(
            ErrorCode::NotFound,
            "Mock glyph run is not registered");
    }

    std::uint32_t Count() const noexcept {
        return ids_.Size();
    }

private:
    RhiDevice* device_ = nullptr;
    Vector<RenderGlyphRunId> ids_;
};

bool TestAtlasUploadBatchesAndFenceRetirement() {
    RenderFontProvider provider;
    FontManager fonts;
    CHECK(fonts.Initialize());
    CHECK(fonts.RegisterProvider(
        {&provider, &provider, &provider}));

    NullGraphicsBackend graphics;
    RhiDevice device(graphics);
    CHECK(device.Initialize());
    MockGlyphRegistry registry(device);
    TextBlockRenderService service(
        fonts, device, graphics, registry);

    TextBlockRenderServiceConfig config;
    config.face = RenderFontProvider::MakeFace();
    config.pixelSize = 10.0F;
    config.atlas.pageWidth = 8U;
    config.atlas.pageHeight = 8U;
    config.atlas.maxPages = 2U;
    config.atlas.padding = 0U;
    CHECK(service.Initialize(config));

    TextBlockLayoutRequest request;
    request.text = "ABCDE";
    request.availableSize = {100.0, 40.0};
    request.dpiScale = 1.0;
    TextBlockLayoutResult output;
    CHECK(service.ShapeAndPrepare(request, output));
    CHECK(output.glyphRuns.Size() == 2U);
    CHECK(output.desiredSize.width == 20.0);
    CHECK(output.desiredSize.height == 10.0);
    CHECK(registry.Count() == 2U);
    CHECK(graphics.SubmissionCount() == 1U);

    TextBlockRenderServiceStatistics statistics =
        service.Statistics();
    CHECK(statistics.atlasPages == 2U);
    CHECK(statistics.atlasEntries == 5U);
    CHECK(statistics.activeGlyphRuns == 2U);
    CHECK(statistics.retiredGlyphRuns == 0U);
    CHECK(statistics.lastUploadFence == 1U);

    for (RenderGlyphRunId glyphRun : output.glyphRuns) {
        service.ReleaseGlyphRun(glyphRun);
    }
    statistics = service.Statistics();
    CHECK(statistics.activeGlyphRuns == 0U);
    CHECK(statistics.retiredGlyphRuns == 2U);
    CHECK(registry.Count() == 2U);
    CHECK(service.CollectGarbage());
    CHECK(registry.Count() == 2U);

    graphics.CompleteThrough(
        statistics.lastUploadFence);
    Result<std::uint32_t> collected =
        service.CollectGarbage();
    CHECK(collected);
    CHECK(collected.Value() == 2U);
    CHECK(registry.Count() == 0U);
    statistics = service.Statistics();
    CHECK(statistics.retiredGlyphRuns == 0U);

    service.Shutdown();
    CHECK(!service.IsInitialized());
    CHECK(device.CollectGarbage());
    return true;
}

bool TestDeviceLossDiagnostic() {
    RenderFontProvider provider;
    FontManager fonts;
    CHECK(fonts.Initialize());
    CHECK(fonts.RegisterProvider(
        {&provider, &provider, &provider}));
    NullGraphicsBackend graphics;
    RhiDevice device(graphics);
    CHECK(device.Initialize());
    MockGlyphRegistry registry(device);
    TextBlockRenderService service(
        fonts, device, graphics, registry);
    TextBlockRenderServiceConfig config;
    config.face = RenderFontProvider::MakeFace();
    config.atlas.pageWidth = 16U;
    config.atlas.pageHeight = 16U;
    CHECK(service.Initialize(config));

    graphics.SimulateDeviceLoss();
    TextBlockLayoutRequest request;
    request.text = "A";
    request.availableSize = {20.0, 20.0};
    TextBlockLayoutResult output;
    Result<void> lost =
        service.ShapeAndPrepare(request, output);
    CHECK(!lost);
    CHECK(lost.GetStatus().code == ErrorCode::InvalidState);
    return true;
}

} // namespace

int main() {
    if (!TestAtlasUploadBatchesAndFenceRetirement()) return 1;
    if (!TestDeviceLossDiagnostic()) return 1;
    std::puts("Aero TextBlock render service tests passed");
    return 0;
}
