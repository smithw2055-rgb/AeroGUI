#pragma once

#include "FontManager.hpp"

#include <limits>

namespace Aero::Text {

struct PositionedGlyph final {
    GlyphId glyph = InvalidGlyphId;
    std::uint32_t cluster = 0U;
    float x = 0.0F;
    float y = 0.0F;
    float advanceX = 0.0F;
};

struct GlyphRun final {
    explicit GlyphRun(
        Base::IAllocator* allocator = nullptr) noexcept
        : glyphs(allocator) {}

    FontFaceHandle face;
    float pixelSize = 0.0F;
    TextDirection direction = TextDirection::LeftToRight;
    Script script = Script::Unknown;
    Base::Vector<PositionedGlyph> glyphs;
};

struct TextLine final {
    std::uint32_t firstRun = 0U;
    std::uint32_t runCount = 0U;
    std::uint32_t textStart = 0U;
    std::uint32_t textLength = 0U;
    float x = 0.0F;
    float width = 0.0F;
    float ascent = 0.0F;
    float descent = 0.0F;
    float baseline = 0.0F;
    float y = 0.0F;
};

struct TextLayoutSize final {
    float width = 0.0F;
    float height = 0.0F;
};

struct TextLayoutRequest final {
    FontFace face;
    Base::Span<const FontFace> fallbackFaces;
    Base::StringView text;
    Base::StringView language;
    float pixelSize = 0.0F;
    float maxWidth = std::numeric_limits<float>::infinity();
    float lineHeight = 0.0F;
    TextDirection direction = TextDirection::Auto;
    Script script = Script::Unknown;
    TextWrapping wrapping = TextWrapping::NoWrap;
    TextTrimming trimming = TextTrimming::None;
    TextAlignment alignment = TextAlignment::Start;
};

class AERO_API TextLayout final {
public:
    explicit TextLayout(
        Base::IAllocator* allocator = nullptr) noexcept
        : runs_(allocator), lines_(allocator) {}

    Base::Span<const GlyphRun> Runs() const noexcept {
        return runs_.AsSpan();
    }
    Base::Span<const TextLine> Lines() const noexcept {
        return lines_.AsSpan();
    }
    TextLayoutSize Size() const noexcept { return size_; }
    TextLayoutSize NaturalSize() const noexcept {
        return naturalSize_;
    }
    TextAlignment Alignment() const noexcept {
        return alignment_;
    }
    bool IsTrimmed() const noexcept { return trimmed_; }

    Base::Result<void> TryAddRun(GlyphRun&& run) noexcept;
    Base::Result<void> TryAddLine(const TextLine& line) noexcept;
    Base::Result<void> SetSize(TextLayoutSize size) noexcept;
    Base::Result<void> ShapeAndMeasure(
        FontManager& fonts,
        const TextLayoutRequest& request) noexcept;
    Base::Result<void> Arrange(float finalWidth) noexcept;
    void Clear() noexcept;

private:
    Base::Vector<GlyphRun> runs_;
    Base::Vector<TextLine> lines_;
    TextLayoutSize size_;
    TextLayoutSize naturalSize_;
    TextAlignment alignment_ = TextAlignment::Start;
    bool trimmed_ = false;
};

} // namespace Aero::Text
