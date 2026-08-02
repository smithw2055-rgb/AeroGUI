#pragma once

#include "TextTypes.hpp"

namespace Aero::Text {

struct ShapingRequest final {
    FontFaceHandle face;
    Base::StringView text;
    float pixelSize = 0.0F;
    TextDirection direction = TextDirection::Auto;
    Script script = Script::Unknown;
    Base::StringView language;
};

struct GlyphRequest final {
    FontFaceHandle face;
    GlyphId glyph = InvalidGlyphId;
    float pixelSize = 0.0F;
    float dpiScale = 1.0F;
};

class AERO_API IFontProvider {
public:
    virtual ~IFontProvider() = default;

    virtual FontProviderIdentity Identity() const noexcept = 0;
    virtual Base::Result<void> LoadFace(
        const FontSource& source,
        const Typeface& typeface,
        FontFace& output) noexcept = 0;
    virtual Base::Result<void> ResolveFace(
        const FontQuery& query,
        FontFace& output) noexcept = 0;
    virtual Base::Result<bool> HasCodePoint(
        FontFaceHandle face,
        std::uint32_t codePoint) noexcept = 0;
    virtual void ReleaseFace(FontFaceHandle face) noexcept = 0;
};

class AERO_API ITextShaper {
public:
    virtual ~ITextShaper() = default;

    virtual bool Supports(
        FontProviderIdentity provider) const noexcept = 0;
    virtual Base::Result<void> Shape(
        const ShapingRequest& request,
        ShapedTextRun& output) noexcept = 0;
};

class AERO_API IGlyphRasterizer {
public:
    virtual ~IGlyphRasterizer() = default;

    virtual bool Supports(
        FontProviderIdentity provider) const noexcept = 0;
    virtual Base::Result<void> GetMetrics(
        const GlyphRequest& request,
        GlyphMetrics& output) noexcept = 0;
    virtual Base::Result<void> Rasterize(
        const GlyphRequest& request,
        GlyphBitmap& output) noexcept = 0;
    virtual Base::Result<void> ExtractOutline(
        const GlyphRequest& request,
        GlyphOutline& output) noexcept = 0;
};

struct TextProviderRegistration final {
    IFontProvider* fonts = nullptr;
    ITextShaper* shaper = nullptr;
    IGlyphRasterizer* rasterizer = nullptr;
};

} // namespace Aero::Text
