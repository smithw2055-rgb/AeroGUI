#include <Aero/Text/Text.hpp>

#include <cstdio>
#include <utility>

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

FontFace MakeFace(
    FontProviderIdentity provider,
    FontFaceId face = 11U) noexcept {
    FontFace result;
    result.handle.provider = provider;
    result.handle.face = face;
    result.handle.generation = 1U;
    result.metrics.unitsPerEm = 2048.0F;
    result.metrics.ascent = 1900.0F;
    result.metrics.descent = -500.0F;
    result.metrics.lineGap = 100.0F;
    result.metrics.underlinePosition = -200.0F;
    result.metrics.underlineThickness = 100.0F;
    return result;
}

class MockFontProvider final : public IFontProvider {
public:
    explicit MockFontProvider(
        FontProviderIdentity identity,
        bool resolves = true) noexcept
        : identity_(identity), resolves_(resolves) {}

    FontProviderIdentity Identity() const noexcept override {
        return identity_;
    }

    Result<void> LoadFace(
        const FontSource&,
        const Typeface&,
        FontFace& output) noexcept override {
        ++loadCount;
        output = MakeFace(identity_, 21U);
        return {};
    }

    Result<void> ResolveFace(
        const FontQuery&,
        FontFace& output) noexcept override {
        ++resolveCount;
        if (!resolves_) {
            return Status::Failure(
                ErrorCode::NotFound,
                "Mock provider did not resolve the face");
        }
        output = MakeFace(identity_);
        return {};
    }

    void ReleaseFace(FontFaceHandle face) noexcept override {
        released = face;
        ++releaseCount;
    }

    std::uint32_t loadCount = 0U;
    std::uint32_t resolveCount = 0U;
    std::uint32_t releaseCount = 0U;
    FontFaceHandle released;

private:
    FontProviderIdentity identity_;
    bool resolves_ = true;
};

class MockShaper final : public ITextShaper {
public:
    explicit MockShaper(
        FontProviderIdentity supported) noexcept
        : supported_(supported) {}

    bool Supports(
        FontProviderIdentity provider) const noexcept override {
        return provider == supported_;
    }

    Result<void> Shape(
        const ShapingRequest& request,
        ShapedTextRun& output) noexcept override {
        ++shapeCount;
        output.face = request.face;
        output.direction =
            request.direction == TextDirection::Auto
                ? TextDirection::LeftToRight
                : request.direction;
        output.script =
            request.script == Script::Unknown
                ? Script::Latin
                : request.script;
        ShapedGlyph glyph;
        glyph.glyph = 7U;
        glyph.cluster = 0U;
        glyph.advanceX = request.pixelSize * 0.5F;
        return output.glyphs.TryPushBack(glyph);
    }

    std::uint32_t shapeCount = 0U;

private:
    FontProviderIdentity supported_;
};

class MockRasterizer final : public IGlyphRasterizer {
public:
    explicit MockRasterizer(
        FontProviderIdentity supported) noexcept
        : supported_(supported) {}

    bool Supports(
        FontProviderIdentity provider) const noexcept override {
        return provider == supported_;
    }

    Result<void> GetMetrics(
        const GlyphRequest& request,
        GlyphMetrics& output) noexcept override {
        ++metricsCount;
        output.width = request.pixelSize * 0.5F;
        output.height = request.pixelSize;
        output.bearingY = request.pixelSize;
        output.advanceX = request.pixelSize * 0.6F;
        return {};
    }

    Result<void> Rasterize(
        const GlyphRequest&,
        GlyphBitmap& output) noexcept override {
        ++rasterCount;
        output.width = 2U;
        output.height = 2U;
        output.strideBytes = 2U;
        const std::uint8_t pixels[] = {0U, 64U, 128U, 255U};
        return output.pixels.TryAppend(pixels);
    }

    Result<void> ExtractOutline(
        const GlyphRequest&,
        GlyphOutline& output) noexcept override {
        ++outlineCount;
        OutlineCommand move;
        move.kind = OutlineCommandKind::MoveTo;
        move.pointCount = 1U;
        move.points[0] = {0.0F, 0.0F};
        Result<void> appended = output.commands.TryPushBack(move);
        if (!appended) return appended;
        OutlineCommand line;
        line.kind = OutlineCommandKind::LineTo;
        line.pointCount = 1U;
        line.points[0] = {1.0F, 1.0F};
        appended = output.commands.TryPushBack(line);
        if (!appended) return appended;
        OutlineCommand close;
        close.kind = OutlineCommandKind::Close;
        return output.commands.TryPushBack(close);
    }

    std::uint32_t metricsCount = 0U;
    std::uint32_t rasterCount = 0U;
    std::uint32_t outlineCount = 0U;

private:
    FontProviderIdentity supported_;
};

bool TestTypefaceContract() {
    Typeface typeface;
    CHECK(!typeface.TrySetFamily({}));
    CHECK(typeface.TrySetFamily("Aero Sans"));
    CHECK(typeface.Family() == StringView("Aero Sans"));
    CHECK(typeface.TrySetLanguage("zh-CN"));
    CHECK(typeface.Language() == StringView("zh-CN"));
    CHECK(!typeface.TrySetWeight(0U));
    CHECK(!typeface.TrySetWeight(1001U));
    CHECK(typeface.TrySetWeight(650U));
    CHECK(typeface.Weight() == 650U);
    typeface.SetStyle(FontStyle::Italic);
    typeface.SetStretch(FontStretch::Condensed);
    CHECK(typeface.Style() == FontStyle::Italic);
    CHECK(typeface.Stretch() == FontStretch::Condensed);
    return true;
}

bool TestRegistrationAndFallback() {
    const FontProviderIdentity firstIdentity{101U, 1U};
    const FontProviderIdentity secondIdentity{202U, 3U};
    MockFontProvider firstFonts(firstIdentity, false);
    MockShaper firstShaper(firstIdentity);
    MockRasterizer firstRasterizer(firstIdentity);
    MockFontProvider secondFonts(secondIdentity);
    MockShaper secondShaper(secondIdentity);
    MockRasterizer secondRasterizer(secondIdentity);

    FontManager manager;
    Typeface typeface;
    CHECK(typeface.TrySetFamily("Aero Sans"));
    FontQuery query;
    query.typeface = &typeface;
    FontFace face;
    Result<void> beforeInitialize = manager.ResolveFace(query, face);
    CHECK(!beforeInitialize);
    CHECK(beforeInitialize.GetStatus().code == ErrorCode::NotInitialized);

    CHECK(manager.Initialize());
    CHECK(!manager.Initialize());
    CHECK(!manager.RegisterProvider({&firstFonts, nullptr, &firstRasterizer}));
    CHECK(manager.RegisterProvider(
        {&firstFonts, &firstShaper, &firstRasterizer}));
    CHECK(!manager.RegisterProvider(
        {&firstFonts, &firstShaper, &firstRasterizer}));
    CHECK(manager.RegisterProvider(
        {&secondFonts, &secondShaper, &secondRasterizer}));
    CHECK(manager.ProviderCount() == 2U);

    CHECK(manager.ResolveFace(query, face));
    CHECK(firstFonts.resolveCount == 1U);
    CHECK(secondFonts.resolveCount == 1U);
    CHECK(face.handle.provider == secondIdentity);

    query.preferredProvider = firstIdentity.id;
    Result<void> preferred = manager.ResolveFace(query, face);
    CHECK(!preferred);
    CHECK(preferred.GetStatus().code == ErrorCode::NotFound);

    query.preferredProvider = 9999U;
    Result<void> missing = manager.ResolveFace(query, face);
    CHECK(!missing);
    CHECK(missing.GetStatus().code == ErrorCode::NotFound);

    const std::uint8_t memoryFont[] = {1U, 2U, 3U, 4U};
    FontSource source;
    source.kind = FontSourceKind::Memory;
    source.identifier = "mock-memory-font";
    source.bytes = memoryFont;
    CHECK(manager.LoadFace(
        firstIdentity.id, source, typeface, face));
    CHECK(face.handle.face == 21U);
    CHECK(firstFonts.loadCount == 1U);
    CHECK(manager.ReleaseFace(face.handle));
    CHECK(firstFonts.releaseCount == 1U);
    CHECK(firstFonts.released == face.handle);

    CHECK(manager.UnregisterProvider(firstIdentity.id));
    CHECK(!manager.UnregisterProvider(firstIdentity.id));
    manager.Shutdown();
    CHECK(!manager.IsInitialized());
    CHECK(manager.ProviderCount() == 0U);
    return true;
}

bool TestShapeRasterAndVersioning() {
    const FontProviderIdentity identity{303U, 5U};
    MockFontProvider fonts(identity);
    MockShaper shaper(identity);
    MockRasterizer rasterizer(identity);
    FontManager manager;
    CHECK(manager.Initialize());
    CHECK(manager.RegisterProvider({&fonts, &shaper, &rasterizer}));

    const FontFace face = MakeFace(identity);
    ShapingRequest shaping;
    shaping.face = face.handle;
    shaping.text = "A";
    shaping.pixelSize = 20.0F;
    ShapedTextRun shaped;
    CHECK(manager.Shape(shaping, shaped));
    CHECK(shaper.shapeCount == 1U);
    CHECK(shaped.face == face.handle);
    CHECK(shaped.direction == TextDirection::LeftToRight);
    CHECK(shaped.script == Script::Latin);
    CHECK(shaped.glyphs.Size() == 1U);
    CHECK(shaped.glyphs[0].glyph == 7U);

    const char invalidUtf8[] = {static_cast<char>(0xFF)};
    shaping.text = StringView(invalidUtf8, 1U);
    Result<void> invalid = manager.Shape(shaping, shaped);
    CHECK(!invalid);
    CHECK(invalid.GetStatus().code == ErrorCode::InvalidUtf8);

    GlyphRequest glyph;
    glyph.face = face.handle;
    glyph.glyph = 7U;
    glyph.pixelSize = 20.0F;
    glyph.dpiScale = 1.25F;
    GlyphMetrics metrics;
    CHECK(manager.GetGlyphMetrics(glyph, metrics));
    CHECK(metrics.width == 10.0F);
    GlyphBitmap bitmap;
    CHECK(manager.RasterizeGlyph(glyph, bitmap));
    CHECK(bitmap.width == 2U);
    CHECK(bitmap.height == 2U);
    CHECK(bitmap.pixels.Size() == 4U);
    GlyphOutline outline;
    CHECK(manager.ExtractGlyphOutline(glyph, outline));
    CHECK(outline.commands.Size() == 3U);

    CHECK(manager.UnregisterProvider(identity.id));
    const FontProviderIdentity replacementIdentity{identity.id, 6U};
    MockFontProvider replacementFonts(replacementIdentity);
    MockShaper replacementShaper(replacementIdentity);
    MockRasterizer replacementRasterizer(replacementIdentity);
    CHECK(manager.RegisterProvider(
        {&replacementFonts, &replacementShaper, &replacementRasterizer}));
    shaping.text = "A";
    Result<void> stale = manager.Shape(shaping, shaped);
    CHECK(!stale);
    CHECK(stale.GetStatus().code == ErrorCode::ValidationFailed);
    return true;
}

bool TestTextLayoutDataContract() {
    const FontFace face = MakeFace({404U, 1U});
    GlyphRun run;
    run.face = face.handle;
    run.pixelSize = 16.0F;
    PositionedGlyph glyph;
    glyph.glyph = 9U;
    glyph.x = 2.0F;
    glyph.y = 12.0F;
    CHECK(run.glyphs.TryPushBack(glyph));

    TextLayout layout;
    CHECK(layout.TryAddRun(std::move(run)));
    TextLine line;
    line.firstRun = 0U;
    line.runCount = 1U;
    line.textLength = 1U;
    line.width = 8.0F;
    line.ascent = 12.0F;
    line.descent = 4.0F;
    line.baseline = 12.0F;
    CHECK(layout.TryAddLine(line));
    CHECK(layout.SetSize({8.0F, 16.0F}));
    CHECK(layout.Runs().Size() == 1U);
    CHECK(layout.Lines().Size() == 1U);
    CHECK(layout.Size().height == 16.0F);

    TextLine invalidLine;
    invalidLine.firstRun = 2U;
    invalidLine.runCount = 1U;
    CHECK(!layout.TryAddLine(invalidLine));
    layout.Clear();
    CHECK(layout.Runs().Empty());
    CHECK(layout.Lines().Empty());
    CHECK(layout.Size().width == 0.0F);
    return true;
}

} // namespace

int main() {
    if (!TestTypefaceContract()) return 1;
    if (!TestRegistrationAndFallback()) return 1;
    if (!TestShapeRasterAndVersioning()) return 1;
    if (!TestTextLayoutDataContract()) return 1;
    std::puts("Aero text provider contract tests passed");
    return 0;
}
