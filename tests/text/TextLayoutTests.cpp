#include <Aero/Text/Text.hpp>

#include <cmath>
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

constexpr FontProviderIdentity ProviderIdentity{501U, 1U};

std::uint32_t Decode(
    StringView text,
    std::uint32_t& offset) noexcept {
    const auto* bytes =
        reinterpret_cast<const unsigned char*>(text.Data());
    const unsigned char lead = bytes[offset++];
    if (lead <= 0x7FU) return lead;
    std::uint32_t value = 0U;
    std::uint32_t remaining = 0U;
    if ((lead & 0xE0U) == 0xC0U) {
        value = lead & 0x1FU;
        remaining = 1U;
    } else if ((lead & 0xF0U) == 0xE0U) {
        value = lead & 0x0FU;
        remaining = 2U;
    } else {
        value = lead & 0x07U;
        remaining = 3U;
    }
    while (remaining-- > 0U) {
        value = (value << 6U) |
            static_cast<std::uint32_t>(bytes[offset++] & 0x3FU);
    }
    return value;
}

float Advance(std::uint32_t codePoint) noexcept {
    if (codePoint == 0x20U) return 4.0F;
    if (codePoint == 0x2026U) return 8.0F;
    if (codePoint >= 0x2E80U) return 16.0F;
    return 8.0F;
}

class LayoutProvider final
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

    void ReleaseFace(FontFaceHandle) noexcept override {}

    bool Supports(FontProviderIdentity provider) const noexcept override {
        return provider == ProviderIdentity;
    }

    Result<void> Shape(
        const ShapingRequest& request,
        ShapedTextRun& output) noexcept override {
        output.face = request.face;
        output.direction =
            request.direction == TextDirection::Auto
                ? TextDirection::LeftToRight
                : request.direction;
        output.script =
            request.script == Script::Unknown
                ? Script::Common
                : request.script;
        std::uint32_t offset = 0U;
        while (offset < request.text.SizeBytes()) {
            const std::uint32_t cluster = offset;
            const std::uint32_t codePoint =
                Decode(request.text, offset);
            ShapedGlyph glyph;
            glyph.glyph = codePoint;
            glyph.cluster = cluster;
            glyph.advanceX = Advance(codePoint);
            Result<void> appended =
                output.glyphs.TryPushBack(glyph);
            if (!appended) return appended.GetStatus();
        }
        return {};
    }

    Result<void> GetMetrics(
        const GlyphRequest&,
        GlyphMetrics&) noexcept override {
        return Status::Failure(
            ErrorCode::Unsupported, "Not used by layout tests");
    }

    Result<void> Rasterize(
        const GlyphRequest&,
        GlyphBitmap&) noexcept override {
        return Status::Failure(
            ErrorCode::Unsupported, "Not used by layout tests");
    }

    Result<void> ExtractOutline(
        const GlyphRequest&,
        GlyphOutline&) noexcept override {
        return Status::Failure(
            ErrorCode::Unsupported, "Not used by layout tests");
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

bool Near(float left, float right) noexcept {
    return std::fabs(left - right) < 0.001F;
}

bool TestMixedTextAndStableMeasure(
    FontManager& fonts,
    const FontFace& face) {
    TextLayoutRequest request;
    request.face = face;
    request.text =
        "ABC"
        "\xE4\xB8\xAD"
        "\xE6\x96\x87"
        "12";
    request.pixelSize = 10.0F;

    TextLayout layout;
    CHECK(layout.ShapeAndMeasure(fonts, request));
    CHECK(layout.Lines().Size() == 1U);
    CHECK(layout.Runs().Size() == 4U);
    CHECK(Near(layout.NaturalSize().width, 72.0F));
    CHECK(Near(layout.NaturalSize().height, 10.0F));
    CHECK(Near(layout.Size().width, 72.0F));
    CHECK(layout.Runs()[0].glyphs.Size() == 3U);
    CHECK(layout.Runs()[1].glyphs.Size() == 1U);
    CHECK(layout.Runs()[2].glyphs.Size() == 1U);
    CHECK(layout.Runs()[3].glyphs.Size() == 2U);

    const TextLayoutSize first = layout.NaturalSize();
    CHECK(layout.ShapeAndMeasure(fonts, request));
    CHECK(Near(layout.NaturalSize().width, first.width));
    CHECK(Near(layout.NaturalSize().height, first.height));
    return true;
}

bool TestWordAndCharacterWrapping(
    FontManager& fonts,
    const FontFace& face) {
    TextLayoutRequest request;
    request.face = face;
    request.text = "AB CD";
    request.pixelSize = 10.0F;
    request.maxWidth = 24.0F;
    request.wrapping = TextWrapping::Wrap;

    TextLayout words;
    CHECK(words.ShapeAndMeasure(fonts, request));
    CHECK(words.Lines().Size() == 2U);
    CHECK(Near(words.Lines()[0].width, 16.0F));
    CHECK(words.Lines()[0].textLength == 2U);
    CHECK(Near(words.Lines()[1].width, 16.0F));
    CHECK(words.Lines()[1].textStart == 3U);

    request.text = "ABCDE";
    request.maxWidth = 16.0F;
    TextLayout characters;
    CHECK(characters.ShapeAndMeasure(fonts, request));
    CHECK(characters.Lines().Size() == 3U);
    CHECK(Near(characters.Lines()[0].width, 16.0F));
    CHECK(Near(characters.Lines()[1].width, 16.0F));
    CHECK(Near(characters.Lines()[2].width, 8.0F));
    return true;
}

bool TestTrimming(
    FontManager& fonts,
    const FontFace& face) {
    TextLayoutRequest request;
    request.face = face;
    request.text = "ABCDE";
    request.pixelSize = 10.0F;
    request.maxWidth = 28.0F;
    request.trimming = TextTrimming::CharacterEllipsis;

    TextLayout characters;
    CHECK(characters.ShapeAndMeasure(fonts, request));
    CHECK(characters.IsTrimmed());
    CHECK(Near(characters.Lines()[0].width, 24.0F));
    CHECK(characters.Runs().Size() == 2U);
    CHECK(characters.Runs()[0].glyphs.Size() == 2U);
    CHECK(characters.Runs()[1].glyphs[0].glyph == 0x2026U);

    request.text = "one two three";
    request.maxWidth = 44.0F;
    request.trimming = TextTrimming::WordEllipsis;
    TextLayout words;
    CHECK(words.ShapeAndMeasure(fonts, request));
    CHECK(words.IsTrimmed());
    CHECK(Near(words.Lines()[0].width, 32.0F));
    CHECK(words.Runs().Size() == 2U);
    CHECK(words.Runs()[0].glyphs.Size() == 3U);
    return true;
}

bool TestLineHeightAndArrange(
    FontManager& fonts,
    const FontFace& face) {
    TextLayoutRequest request;
    request.face = face;
    request.text = "AB\nC";
    request.pixelSize = 10.0F;
    request.lineHeight = 18.0F;
    request.alignment = TextAlignment::Center;

    TextLayout layout;
    CHECK(layout.ShapeAndMeasure(fonts, request));
    CHECK(layout.Lines().Size() == 2U);
    CHECK(Near(layout.NaturalSize().height, 36.0F));
    CHECK(Near(layout.Lines()[0].baseline, 12.0F));
    CHECK(Near(layout.Lines()[1].y, 18.0F));
    CHECK(layout.Arrange(100.0F));
    CHECK(Near(layout.Lines()[0].x, 42.0F));
    CHECK(Near(layout.Lines()[1].x, 46.0F));
    CHECK(Near(layout.Runs()[0].glyphs[0].x, 42.0F));

    CHECK(layout.Arrange(80.0F));
    CHECK(Near(layout.Lines()[0].x, 32.0F));
    CHECK(Near(layout.Runs()[0].glyphs[0].x, 32.0F));
    CHECK(Near(layout.Size().width, 80.0F));
    return true;
}

bool TestValidationIsTransactional(
    FontManager& fonts,
    const FontFace& face) {
    TextLayoutRequest request;
    request.face = face;
    request.text = "OK";
    request.pixelSize = 10.0F;
    TextLayout layout;
    CHECK(layout.ShapeAndMeasure(fonts, request));
    const TextLayoutSize before = layout.Size();

    const char invalidBytes[] = {static_cast<char>(0xFF)};
    request.text = StringView(invalidBytes, 1U);
    Result<void> invalid =
        layout.ShapeAndMeasure(fonts, request);
    CHECK(!invalid);
    CHECK(invalid.GetStatus().code == ErrorCode::InvalidUtf8);
    CHECK(Near(layout.Size().width, before.width));
    CHECK(layout.Lines().Size() == 1U);
    return true;
}

} // namespace

int main() {
    LayoutProvider provider;
    FontManager fonts;
    if (!fonts.Initialize()) return 1;
    if (!fonts.RegisterProvider(
            {&provider, &provider, &provider})) {
        return 1;
    }
    const FontFace face = LayoutProvider::MakeFace();

    if (!TestMixedTextAndStableMeasure(fonts, face)) return 1;
    if (!TestWordAndCharacterWrapping(fonts, face)) return 1;
    if (!TestTrimming(fonts, face)) return 1;
    if (!TestLineHeightAndArrange(fonts, face)) return 1;
    if (!TestValidationIsTransactional(fonts, face)) return 1;

    std::puts("Aero text layout tests passed");
    return 0;
}
