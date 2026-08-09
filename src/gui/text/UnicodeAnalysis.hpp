#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>

#include <cstdint>

namespace Aero::Text {

enum class UnicodeScript : std::uint8_t {
    Unknown = 0U,
    Common,
    Inherited,
    Latin,
    Arabic,
    Hebrew,
    Han,
    Hiragana,
    Katakana,
};

enum class UnicodeBidiClass : std::uint8_t {
    LeftToRight = 0U,
    RightToLeft,
    ArabicLetter,
    EuropeanNumber,
    ArabicNumber,
    NonspacingMark,
    Whitespace,
    Neutral,
};

enum class UnicodeLineBreakClass : std::uint8_t {
    Alphabetic = 0U,
    Numeric,
    Space,
    Mandatory,
    Ideograph,
    CombiningMark,
    OpeningPunctuation,
    ClosingPunctuation,
    Hyphen,
};

enum class ParagraphDirection : std::uint8_t {
    Auto = 0U,
    LeftToRight,
    RightToLeft,
};

struct UnicodeScalar  {
    std::uint32_t value = 0U;
    std::uint32_t byteOffset = 0U;
    std::uint8_t byteLength = 0U;
    std::uint32_t cluster = 0U;
    UnicodeScript script = UnicodeScript::Unknown;
    UnicodeBidiClass bidi = UnicodeBidiClass::Neutral;
    UnicodeLineBreakClass lineBreak =
        UnicodeLineBreakClass::Alphabetic;
    std::uint8_t embeddingLevel = 0U;
};

struct UnicodeRun  {
    std::uint32_t firstScalar = 0U;
    std::uint32_t scalarCount = 0U;
    UnicodeScript script = UnicodeScript::Unknown;
    std::uint8_t embeddingLevel = 0U;
    bool rightToLeft = false;
};

struct UnicodeBreakOpportunity  {
    std::uint32_t scalarIndex = 0U;
    bool allowed = false;
    bool mandatory = false;
};

struct UnicodeClusterMap  {
    std::uint32_t logicalCluster = 0U;
    std::uint32_t visualCluster = 0U;
};

// Deterministic provider-neutral Unicode analysis used by TextBlock/TextBox.
// The implementation covers the desktop runtime baseline without depending on
// ICU and keeps the data model compatible with a generated Unicode table.
class UnicodeTextAnalysis  {
public:
    explicit UnicodeTextAnalysis(
        Base::IAllocator* allocator = nullptr) noexcept;

    Base::Result<void> Analyze(
        Base::StringView text,
        ParagraphDirection direction =
            ParagraphDirection::Auto) noexcept;
    void Clear() noexcept;

    Base::Span<const UnicodeScalar> Scalars() const noexcept {
        return scalars_.AsSpan();
    }
    Base::Span<const UnicodeRun> LogicalRuns() const noexcept {
        return logicalRuns_.AsSpan();
    }
    Base::Span<const std::uint32_t> VisualScalarOrder() const noexcept {
        return visualOrder_.AsSpan();
    }
    Base::Span<const UnicodeBreakOpportunity>
    BreakOpportunities() const noexcept {
        return breaks_.AsSpan();
    }
    Base::Span<const UnicodeClusterMap> ClusterMap() const noexcept {
        return clusterMap_.AsSpan();
    }

    std::uint8_t ParagraphLevel() const noexcept {
        return paragraphLevel_;
    }
    std::uint32_t ClusterCount() const noexcept {
        return clusterCount_;
    }

    std::uint32_t LogicalScalarFromVisual(
        std::uint32_t visualIndex) const noexcept;
    std::uint32_t VisualScalarFromLogical(
        std::uint32_t logicalIndex) const noexcept;

private:
    Base::Vector<UnicodeScalar> scalars_;
    Base::Vector<UnicodeRun> logicalRuns_;
    Base::Vector<std::uint32_t> visualOrder_;
    Base::Vector<UnicodeBreakOpportunity> breaks_;
    Base::Vector<UnicodeClusterMap> clusterMap_;
    std::uint8_t paragraphLevel_ = 0U;
    std::uint32_t clusterCount_ = 0U;

    Base::Result<void> Decode(Base::StringView text) noexcept;
    Base::Result<void> Resolve(
        ParagraphDirection direction) noexcept;
    Base::Result<void> BuildRuns() noexcept;
    Base::Result<void> BuildVisualOrder() noexcept;
    Base::Result<void> BuildBreaks() noexcept;
    Base::Result<void> BuildClusterMap() noexcept;
};

UnicodeScript ClassifyScript(
    std::uint32_t scalar) noexcept;
UnicodeBidiClass ClassifyBidi(
    std::uint32_t scalar) noexcept;
UnicodeLineBreakClass ClassifyLineBreak(
    std::uint32_t scalar) noexcept;
bool IsUnicodeCombiningMark(
    std::uint32_t scalar) noexcept;

} // namespace Aero::Text
