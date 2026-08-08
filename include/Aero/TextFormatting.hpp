#pragma once

#include <Aero/FrameworkElement.hpp>

namespace Aero {

enum class FontStyle : std::uint8_t {
    Normal = 0U,
    Italic,
    Oblique
};

enum class FontStretch : std::uint8_t {
    UltraCondensed = 0U,
    ExtraCondensed,
    Condensed,
    SemiCondensed,
    Normal,
    SemiExpanded,
    Expanded,
    ExtraExpanded,
    UltraExpanded
};

enum class TextWrapping : std::uint8_t {
    NoWrap = 0U,
    Wrap,
    WrapWithOverflow
};

enum class TextTrimming : std::uint8_t {
    None = 0U,
    CharacterEllipsis,
    WordEllipsis
};

enum class TextAlignment : std::uint8_t {
    Left = 0U,
    Center,
    Right,
    Justify
};

struct TextRange {
    std::uint32_t start = 0U;
    std::uint32_t length = 0U;

    std::uint32_t GetEnd() const noexcept { return start + length; }
    bool GetIsEmpty() const noexcept { return length == 0U; }
};

struct TextSelection {
    std::uint32_t anchor = 0U;
    std::uint32_t caret = 0U;

    std::uint32_t GetStart() const noexcept {
        return anchor < caret ? anchor : caret;
    }
    std::uint32_t GetEnd() const noexcept {
        return anchor < caret ? caret : anchor;
    }
    std::uint32_t GetLength() const noexcept { return GetEnd() - GetStart(); }
    bool GetIsEmpty() const noexcept { return anchor == caret; }
};

struct TextHitRegion {
    std::uint32_t textOffset = 0U;
    std::uint32_t textLength = 0U;
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

} // namespace Aero

AERO_DECLARE_TYPE_ENUM(Aero::FontWeight)
AERO_DECLARE_TYPE_ENUM(Aero::FontStyle)
AERO_DECLARE_TYPE_ENUM(Aero::TextWrapping)
AERO_DECLARE_TYPE_ENUM(Aero::TextTrimming)
AERO_DECLARE_TYPE_ENUM(Aero::TextAlignment)
