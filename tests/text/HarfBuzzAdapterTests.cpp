#include <Aero/Text/FreeTypeAdapter.hpp>
#include <Aero/Text/HarfBuzzAdapter.hpp>
#include <Aero/Text/TextLayout.hpp>

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

bool TestRealFontPipeline() {
    FreeTypeAdapter fonts;
    CHECK(fonts.Initialize());
    CHECK(fonts.IsInitialized());
    CHECK(!fonts.Initialize());
    HarfBuzzAdapter shaper(fonts);

    FontManager manager;
    CHECK(manager.Initialize());
    CHECK(manager.RegisterProvider({&fonts, &shaper, &fonts}));

    Typeface typeface;
    CHECK(typeface.TrySetFamily("Roboto"));
    CHECK(typeface.TrySetLanguage("en"));

    FontSource source;
    source.kind = FontSourceKind::File;
    source.identifier = AERO_TEXT_TEST_FONT;

    FontFace loaded;
    CHECK(manager.LoadFace(
        fonts.Identity().id, source, typeface, loaded));
    CHECK(loaded.handle.IsValid());
    CHECK(loaded.metrics.unitsPerEm > 0.0F);
    CHECK(loaded.metrics.ascent > 0.0F);
    CHECK(loaded.metrics.descent < 0.0F);

    FontQuery query;
    query.typeface = &typeface;
    query.codePoint = static_cast<std::uint32_t>('A');
    query.requireCodePoint = true;
    FontFace resolved;
    CHECK(manager.ResolveFace(query, resolved));
    CHECK(resolved.handle == loaded.handle);

    ShapingRequest shaping;
    shaping.face = resolved.handle;
    shaping.text = "Aero 123";
    shaping.pixelSize = 24.0F;
    shaping.direction = TextDirection::LeftToRight;
    shaping.script = Script::Latin;
    shaping.language = "en";
    ShapedTextRun shaped;
    CHECK(manager.Shape(shaping, shaped));
    CHECK(shaped.face == resolved.handle);
    CHECK(shaped.direction == TextDirection::LeftToRight);
    CHECK(shaped.script == Script::Latin);
    CHECK(!shaped.glyphs.Empty());
    CHECK(shaped.glyphs[0].glyph != InvalidGlyphId);
    CHECK(shaped.glyphs[0].advanceX > 0.0F);

    ShapingRequest ligatureShaping = shaping;
    ligatureShaping.text = "ffi";
    ShapedTextRun ligature;
    CHECK(manager.Shape(ligatureShaping, ligature));
    CHECK(!ligature.glyphs.Empty());
    CHECK(ligature.glyphs.Size() < 3U);

    Typeface cjkTypeface;
    CHECK(cjkTypeface.TrySetFamily("Mplus"));
    CHECK(cjkTypeface.TrySetLanguage("zh-CN"));
    FontSource cjkSource;
    cjkSource.kind = FontSourceKind::File;
    cjkSource.identifier = AERO_TEXT_TEST_CJK_FONT;
    FontFace cjkFace;
    CHECK(manager.LoadFace(
        fonts.Identity().id, cjkSource, cjkTypeface, cjkFace));
    FontQuery cjkQuery;
    cjkQuery.typeface = &cjkTypeface;
    cjkQuery.codePoint = 0x4E2DU;
    cjkQuery.requireCodePoint = true;
    FontFace resolvedCjk;
    CHECK(manager.ResolveFace(cjkQuery, resolvedCjk));
    Result<bool> robotoHasLatin =
        manager.HasCodePoint(
            resolved.handle,
            static_cast<std::uint32_t>('A'));
    CHECK(robotoHasLatin && robotoHasLatin.Value());
    Result<bool> robotoHasCjk =
        manager.HasCodePoint(
            resolved.handle, 0x4E2DU);
    CHECK(robotoHasCjk && !robotoHasCjk.Value());
    Result<bool> mplusHasCjk =
        manager.HasCodePoint(
            resolvedCjk.handle, 0x4E2DU);
    CHECK(mplusHasCjk && mplusHasCjk.Value());
    ShapingRequest cjkShaping;
    cjkShaping.face = resolvedCjk.handle;
    cjkShaping.text = "\xE4\xB8\xAD\xE6\x96\x87";
    cjkShaping.pixelSize = 24.0F;
    cjkShaping.direction = TextDirection::LeftToRight;
    cjkShaping.script = Script::Han;
    cjkShaping.language = "zh-CN";
    ShapedTextRun shapedCjk;
    CHECK(manager.Shape(cjkShaping, shapedCjk));
    CHECK(shapedCjk.script == Script::Han);
    CHECK(shapedCjk.glyphs.Size() == 2U);
    CHECK(shapedCjk.glyphs[0].glyph != 0U);
    CHECK(shapedCjk.glyphs[1].glyph != 0U);

    TextLayoutRequest mixedRequest;
    mixedRequest.face = resolved;
    mixedRequest.fallbackFaces = {&resolvedCjk, 1U};
    mixedRequest.text =
        "A1"
        "\xE4\xB8\xAD"
        "\xE6\x96\x87";
    mixedRequest.pixelSize = 24.0F;
    mixedRequest.language = "zh-CN";
    TextLayout mixedLayout;
    CHECK(mixedLayout.ShapeAndMeasure(manager, mixedRequest));
    CHECK(mixedLayout.Lines().Size() == 1U);
    CHECK(mixedLayout.Runs().Size() == 3U);
    CHECK(mixedLayout.Runs()[0].face == resolved.handle);
    CHECK(mixedLayout.Runs()[1].face == resolvedCjk.handle);
    CHECK(mixedLayout.Runs()[2].face == resolvedCjk.handle);
    CHECK(mixedLayout.NaturalSize().width > 0.0F);
    const TextLayoutSize firstMixedSize =
        mixedLayout.NaturalSize();
    CHECK(mixedLayout.ShapeAndMeasure(manager, mixedRequest));
    CHECK(mixedLayout.NaturalSize().width ==
        firstMixedSize.width);
    CHECK(mixedLayout.NaturalSize().height ==
        firstMixedSize.height);

    Typeface arabicTypeface;
    CHECK(arabicTypeface.TrySetFamily("Amiri"));
    CHECK(arabicTypeface.TrySetLanguage("ar"));
    FontSource arabicSource;
    arabicSource.kind = FontSourceKind::File;
    arabicSource.identifier = AERO_TEXT_TEST_ARABIC_FONT;
    FontFace arabicFace;
    CHECK(manager.LoadFace(
        fonts.Identity().id,
        arabicSource, arabicTypeface, arabicFace));
    ShapingRequest arabicShaping;
    arabicShaping.face = arabicFace.handle;
    arabicShaping.text =
        "\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85";
    arabicShaping.pixelSize = 24.0F;
    arabicShaping.direction = TextDirection::RightToLeft;
    arabicShaping.script = Script::Arabic;
    arabicShaping.language = "ar";
    ShapedTextRun shapedArabic;
    CHECK(manager.Shape(arabicShaping, shapedArabic));
    CHECK(shapedArabic.direction == TextDirection::RightToLeft);
    CHECK(shapedArabic.script == Script::Arabic);
    CHECK(!shapedArabic.glyphs.Empty());
    CHECK(shapedArabic.glyphs[0].cluster >
        shapedArabic.glyphs.Back().cluster);

    GlyphRequest glyph;
    glyph.face = resolved.handle;
    glyph.glyph = shaped.glyphs[0].glyph;
    glyph.pixelSize = 32.0F;
    glyph.dpiScale = 1.0F;

    GlyphMetrics metrics;
    CHECK(manager.GetGlyphMetrics(glyph, metrics));
    CHECK(metrics.width > 0.0F);
    CHECK(metrics.height > 0.0F);
    CHECK(metrics.advanceX > 0.0F);

    GlyphBitmap bitmap;
    CHECK(manager.RasterizeGlyph(glyph, bitmap));
    CHECK(bitmap.format == GlyphPixelFormat::Gray8);
    CHECK(bitmap.width > 0U);
    CHECK(bitmap.height > 0U);
    CHECK(bitmap.strideBytes == bitmap.width);
    CHECK(bitmap.pixels.Size() ==
        bitmap.strideBytes * bitmap.height);

    GlyphOutline outline;
    CHECK(manager.ExtractGlyphOutline(glyph, outline));
    CHECK(!outline.commands.Empty());
    CHECK(outline.commands[0].kind == OutlineCommandKind::MoveTo);
    CHECK(outline.commands.Back().kind == OutlineCommandKind::Close);

    CHECK(manager.ReleaseFace(loaded.handle));
    CHECK(manager.ReleaseFace(resolved.handle));
    CHECK(manager.ReleaseFace(cjkFace.handle));
    CHECK(manager.ReleaseFace(resolvedCjk.handle));
    CHECK(manager.ReleaseFace(arabicFace.handle));
    Result<void> stale = manager.Shape(shaping, shaped);
    CHECK(!stale);
    CHECK(stale.GetStatus().code == ErrorCode::NotFound);

    fonts.Shutdown();
    CHECK(!fonts.IsInitialized());
    Result<bool> coverageAfterShutdown =
        manager.HasCodePoint(
            resolved.handle,
            static_cast<std::uint32_t>('A'));
    CHECK(!coverageAfterShutdown);
    CHECK(coverageAfterShutdown.GetStatus().code ==
        ErrorCode::NotInitialized);
    Result<void> managedAfterShutdown =
        manager.Shape(shaping, shaped);
    CHECK(!managedAfterShutdown);
    CHECK(managedAfterShutdown.GetStatus().code ==
        ErrorCode::NotInitialized);
    manager.Shutdown();
    ShapedTextRun afterShutdown;
    Result<void> unavailable =
        shaper.Shape(shaping, afterShutdown);
    CHECK(!unavailable);
    CHECK(unavailable.GetStatus().code == ErrorCode::NotInitialized);
    return true;
}

} // namespace

int main() {
    if (!TestRealFontPipeline()) return 1;
    std::puts("Aero HarfBuzz adapter tests passed");
    return 0;
}
