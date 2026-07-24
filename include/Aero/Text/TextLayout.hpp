#pragma once

#include <Aero/Text/TextTypes.hpp>

namespace Aero::Text {

struct PositionedGlyph final {
    GlyphId glyph = InvalidGlyphId;
    std::uint32_t cluster = 0U;
    float x = 0.0F;
    float y = 0.0F;
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

    Base::Result<void> TryAddRun(GlyphRun&& run) noexcept;
    Base::Result<void> TryAddLine(const TextLine& line) noexcept;
    Base::Result<void> SetSize(TextLayoutSize size) noexcept;
    void Clear() noexcept;

private:
    Base::Vector<GlyphRun> runs_;
    Base::Vector<TextLine> lines_;
    TextLayoutSize size_;
};

} // namespace Aero::Text
