#pragma once

#include <Aero/Controls.hpp>
#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>

#include <cstdint>

namespace Aero::Text {

using FontProviderId = std::uint64_t;
using FontProviderVersion = std::uint64_t;
using FontFaceId = std::uint64_t;
using GlyphId = std::uint32_t;

constexpr FontProviderId InvalidFontProviderId = 0U;
constexpr FontFaceId InvalidFontFaceId = 0U;
constexpr GlyphId InvalidGlyphId = UINT32_MAX;

struct FontProviderIdentity  {
    FontProviderId id = InvalidFontProviderId;
    FontProviderVersion version = 0U;

    constexpr bool IsValid() const noexcept {
        return id != InvalidFontProviderId && version != 0U;
    }
};

constexpr bool operator==(
    FontProviderIdentity left,
    FontProviderIdentity right) noexcept {
    return left.id == right.id && left.version == right.version;
}

constexpr bool operator!=(
    FontProviderIdentity left,
    FontProviderIdentity right) noexcept {
    return !(left == right);
}

class Typeface  {
public:
    explicit Typeface(Base::IAllocator* allocator = nullptr) noexcept
        : family_(allocator), language_(allocator) {}

    Base::StringView Family() const noexcept { return family_.View(); }
    Base::StringView Language() const noexcept { return language_.View(); }
    std::uint16_t Weight() const noexcept { return weight_; }
    FontStyle Style() const noexcept { return style_; }
    FontStretch Stretch() const noexcept { return stretch_; }

    Base::Result<void> SetFamily(Base::StringView family) noexcept;
    Base::Result<void> SetLanguage(Base::StringView language) noexcept;
    Base::Result<void> SetWeight(std::uint16_t weight) noexcept;
    void SetStyle(FontStyle style) noexcept { style_ = style; }
    void SetStretch(FontStretch stretch) noexcept { stretch_ = stretch; }

private:
    Base::String family_;
    Base::String language_;
    std::uint16_t weight_ = 400U;
    FontStyle style_ = FontStyle::Normal;
    FontStretch stretch_ = FontStretch::Normal;
};

enum class FontSourceKind : std::uint8_t {
    File = 0U,
    Memory
};

struct FontSource  {
    FontSourceKind kind = FontSourceKind::File;
    Base::StringView identifier;
    Base::Span<const std::uint8_t> bytes;
    std::uint32_t faceIndex = 0U;
};

struct FontFaceHandle  {
    FontProviderIdentity provider;
    FontFaceId face = InvalidFontFaceId;
    std::uint32_t generation = 0U;

    constexpr bool IsValid() const noexcept {
        return provider.IsValid() &&
            face != InvalidFontFaceId &&
            generation != 0U;
    }
};

constexpr bool operator==(
    FontFaceHandle left,
    FontFaceHandle right) noexcept {
    return left.provider == right.provider &&
        left.face == right.face &&
        left.generation == right.generation;
}

constexpr bool operator!=(
    FontFaceHandle left,
    FontFaceHandle right) noexcept {
    return !(left == right);
}

struct FontMetrics  {
    float unitsPerEm = 0.0F;
    float ascent = 0.0F;
    float descent = 0.0F;
    float lineGap = 0.0F;
    float underlinePosition = 0.0F;
    float underlineThickness = 0.0F;
};

struct FontFace  {
    FontFaceHandle handle;
    FontMetrics metrics;
    bool hasColorGlyphs = false;
};

struct FontQuery  {
    const Typeface* typeface = nullptr;
    std::uint32_t codePoint = 0U;
    bool requireCodePoint = false;
    FontProviderId preferredProvider = InvalidFontProviderId;
};

enum class TextDirection : std::uint8_t {
    Auto = 0U,
    LeftToRight,
    RightToLeft
};

enum class Script : std::uint16_t {
    Unknown = 0U,
    Common,
    Inherited,
    Latin,
    Han,
    Arabic,
    Cyrillic,
    Greek,
    Hebrew
};

struct ShapedGlyph  {
    GlyphId glyph = InvalidGlyphId;
    std::uint32_t cluster = 0U;
    float advanceX = 0.0F;
    float offsetX = 0.0F;
    float offsetY = 0.0F;
};

struct ShapedTextRun  {
    explicit ShapedTextRun(
        Base::IAllocator* allocator = nullptr) noexcept
        : glyphs(allocator) {}

    FontFaceHandle face;
    TextDirection direction = TextDirection::LeftToRight;
    Script script = Script::Unknown;
    Base::Vector<ShapedGlyph> glyphs;
};

struct GlyphMetrics  {
    float width = 0.0F;
    float height = 0.0F;
    float bearingX = 0.0F;
    float bearingY = 0.0F;
    float advanceX = 0.0F;
};

enum class GlyphPixelFormat : std::uint8_t {
    // Rasterizers return coverage; the atlas stores SDF values for stable
    // filtering across Viewbox and DPI scaling.
    Gray8 = 0U,
    Sdf8
};

struct GlyphBitmap  {
    explicit GlyphBitmap(
        Base::IAllocator* allocator = nullptr) noexcept
        : pixels(allocator) {}

    GlyphPixelFormat format = GlyphPixelFormat::Gray8;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint32_t strideBytes = 0U;
    std::int32_t bearingX = 0;
    std::int32_t bearingY = 0;
    Base::Vector<std::uint8_t> pixels;
};

struct OutlinePoint  {
    float x = 0.0F;
    float y = 0.0F;
};

enum class OutlineCommandKind : std::uint8_t {
    MoveTo = 0U,
    LineTo,
    QuadraticTo,
    CubicTo,
    Close
};

struct OutlineCommand  {
    OutlineCommandKind kind = OutlineCommandKind::MoveTo;
    OutlinePoint points[3]{};
    std::uint8_t pointCount = 0U;
};

struct GlyphOutline  {
    explicit GlyphOutline(
        Base::IAllocator* allocator = nullptr) noexcept
        : commands(allocator) {}

    Base::Vector<OutlineCommand> commands;
};

} // namespace Aero::Text
