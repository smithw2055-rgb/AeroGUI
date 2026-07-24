#include <Aero/Text/GlyphAtlas.hpp>

#include <cstdio>

namespace {

using namespace Aero::Base;
using namespace Aero::Text;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

class AtlasProvider final
    : public IFontProvider,
      public ITextShaper,
      public IGlyphRasterizer {
public:
    FontProviderIdentity Identity() const noexcept override {
        return {707U, 1U};
    }

    Result<void> LoadFace(
        const FontSource&,
        const Typeface&,
        FontFace& output) noexcept override {
        output.handle = {Identity(), 1U, 1U};
        output.metrics.unitsPerEm = 1000.0F;
        return {};
    }

    Result<void> ResolveFace(
        const FontQuery&,
        FontFace& output) noexcept override {
        output.handle = {Identity(), 1U, 1U};
        output.metrics.unitsPerEm = 1000.0F;
        return {};
    }

    void ReleaseFace(FontFaceHandle) noexcept override {}

    bool Supports(
        FontProviderIdentity provider) const noexcept override {
        return provider == Identity();
    }

    Result<void> Shape(
        const ShapingRequest&,
        ShapedTextRun&) noexcept override {
        return Status::Failure(
            ErrorCode::Unsupported, "not used");
    }

    Result<void> GetMetrics(
        const GlyphRequest&,
        GlyphMetrics& output) noexcept override {
        output.width = 4.0F;
        output.height = 4.0F;
        output.advanceX = 5.0F;
        return {};
    }

    Result<void> Rasterize(
        const GlyphRequest& request,
        GlyphBitmap& output) noexcept override {
        ++rasterCount;
        output.width = 4U;
        output.height = 4U;
        output.strideBytes = 4U;
        output.bearingX = 1;
        output.bearingY = 3;
        Result<void> resized = output.pixels.TryResize(16U);
        if (!resized) return resized;
        for (std::uint8_t& pixel : output.pixels) {
            pixel = static_cast<std::uint8_t>(request.glyph);
        }
        return {};
    }

    Result<void> ExtractOutline(
        const GlyphRequest&,
        GlyphOutline&) noexcept override {
        return Status::Failure(
            ErrorCode::Unsupported, "not used");
    }

    std::uint32_t rasterCount = 0U;
};

GlyphRequest MakeRequest(
    FontFaceHandle face,
    GlyphId glyph) noexcept {
    GlyphRequest request;
    request.face = face;
    request.glyph = glyph;
    request.pixelSize = 16.0F;
    request.dpiScale = 1.0F;
    return request;
}

bool TestShelfCacheFenceAndDeviceLoss() {
    AtlasProvider provider;
    FontManager fonts;
    CHECK(fonts.Initialize());
    CHECK(fonts.RegisterProvider(
        {&provider, &provider, &provider}));

    Typeface typeface;
    CHECK(typeface.TrySetFamily("Atlas Test"));
    FontSource source;
    source.identifier = "atlas-test";
    FontFace face;
    CHECK(fonts.LoadFace(
        provider.Identity().id, source, typeface, face));

    GlyphAtlas atlas;
    GlyphAtlasConfig config;
    config.pageWidth = 8U;
    config.pageHeight = 8U;
    config.maxPages = 1U;
    config.padding = 0U;
    CHECK(atlas.Initialize(config));

    GlyphAtlasPlacement first;
    CHECK(atlas.EnsureGlyph(
        fonts, MakeRequest(face.handle, 1U),
        1U, 0U, first));
    CHECK(atlas.PageCount() == 1U);
    CHECK(atlas.EntryCount() == 1U);
    CHECK(atlas.PendingUploads().Size() == 1U);
    CHECK(first.x == 0U && first.y == 0U);
    CHECK(first.width == 4U && first.height == 4U);
    CHECK(first.advanceX == 5.0F);

    GlyphAtlasPlacement cached;
    CHECK(atlas.EnsureGlyph(
        fonts, MakeRequest(face.handle, 1U),
        2U, 0U, cached));
    CHECK(cached.page == first.page);
    CHECK(cached.pageGeneration == first.pageGeneration);
    CHECK(provider.rasterCount == 1U);
    CHECK(atlas.PendingUploads().Size() == 1U);

    GlyphAtlasPlacement placement;
    CHECK(atlas.EnsureGlyph(
        fonts, MakeRequest(face.handle, 2U),
        3U, 0U, placement));
    CHECK(atlas.EnsureGlyph(
        fonts, MakeRequest(face.handle, 3U),
        4U, 0U, placement));
    CHECK(atlas.EnsureGlyph(
        fonts, MakeRequest(face.handle, 4U),
        5U, 0U, placement));
    CHECK(atlas.EntryCount() == 4U);
    CHECK(atlas.PendingUploads().Size() == 4U);

    Result<void> pendingBlocked = atlas.EnsureGlyph(
        fonts, MakeRequest(face.handle, 5U),
        6U, 0U, placement);
    CHECK(!pendingBlocked);
    CHECK(pendingBlocked.GetStatus().code == ErrorCode::OutOfMemory);

    atlas.ClearPendingUploads();
    CHECK(atlas.PendingUploads().Empty());
    CHECK(atlas.MarkSubmitted(first, 10U));
    Result<void> fenceBlocked = atlas.EnsureGlyph(
        fonts, MakeRequest(face.handle, 5U),
        7U, 9U, placement);
    CHECK(!fenceBlocked);
    CHECK(fenceBlocked.GetStatus().code == ErrorCode::OutOfMemory);

    CHECK(atlas.EnsureGlyph(
        fonts, MakeRequest(face.handle, 5U),
        8U, 10U, placement));
    CHECK(!atlas.IsValid(first));
    CHECK(atlas.IsValid(placement));
    CHECK(placement.pageGeneration != first.pageGeneration);
    CHECK(atlas.EntryCount() == 1U);

    const std::uint32_t beforeLoss = atlas.DeviceGeneration();
    atlas.NotifyDeviceLost();
    CHECK(atlas.DeviceGeneration() != beforeLoss);
    CHECK(atlas.EntryCount() == 0U);
    CHECK(atlas.PendingUploads().Empty());
    CHECK(!atlas.IsValid(placement));

    GlyphAtlasPlacement rebuilt;
    CHECK(atlas.EnsureGlyph(
        fonts, MakeRequest(face.handle, 5U),
        9U, 0U, rebuilt));
    CHECK(atlas.IsValid(rebuilt));
    CHECK(rebuilt.deviceGeneration == atlas.DeviceGeneration());

    atlas.Shutdown();
    fonts.Shutdown();
    return true;
}

} // namespace

int main() {
    if (!TestShelfCacheFenceAndDeviceLoss()) return 1;
    std::puts("Aero glyph atlas tests passed");
    return 0;
}
