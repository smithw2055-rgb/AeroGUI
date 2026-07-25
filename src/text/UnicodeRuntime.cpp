#include <Aero/Text/UnicodeRuntime.hpp>

#include <limits>

namespace Aero::Text {
namespace {

constexpr std::uint32_t InvalidIndex = UINT32_MAX;

Base::Status InvalidUtf8(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidUtf8, message);
}

bool InRange(
    std::uint32_t value,
    std::uint32_t first,
    std::uint32_t last) noexcept {
    return value >= first && value <= last;
}

bool IsContinuation(std::uint8_t value) noexcept {
    return (value & 0xC0U) == 0x80U;
}

bool IsStrongRight(UnicodeBidiClass value) noexcept {
    return value == UnicodeBidiClass::RightToLeft ||
        value == UnicodeBidiClass::ArabicLetter;
}

bool IsStrongLeft(UnicodeBidiClass value) noexcept {
    return value == UnicodeBidiClass::LeftToRight;
}

bool IsIdeograph(UnicodeLineBreakClass value) noexcept {
    return value == UnicodeLineBreakClass::Ideograph;
}

} // namespace

bool IsUnicodeCombiningMark(
    std::uint32_t scalar) noexcept {
    return InRange(scalar, 0x0300U, 0x036FU) ||
        InRange(scalar, 0x1AB0U, 0x1AFFU) ||
        InRange(scalar, 0x1DC0U, 0x1DFFU) ||
        InRange(scalar, 0x20D0U, 0x20FFU) ||
        InRange(scalar, 0xFE20U, 0xFE2FU) ||
        InRange(scalar, 0x064BU, 0x065FU) ||
        scalar == 0x0670U;
}

UnicodeScript ClassifyScript(
    std::uint32_t scalar) noexcept {
    if (IsUnicodeCombiningMark(scalar)) {
        return UnicodeScript::Inherited;
    }
    if (InRange(scalar, 0x0041U, 0x005AU) ||
        InRange(scalar, 0x0061U, 0x007AU) ||
        InRange(scalar, 0x00C0U, 0x024FU)) {
        return UnicodeScript::Latin;
    }
    if (InRange(scalar, 0x0590U, 0x05FFU)) {
        return UnicodeScript::Hebrew;
    }
    if (InRange(scalar, 0x0600U, 0x06FFU) ||
        InRange(scalar, 0x0750U, 0x077FU) ||
        InRange(scalar, 0x08A0U, 0x08FFU) ||
        InRange(scalar, 0xFB50U, 0xFDFFU) ||
        InRange(scalar, 0xFE70U, 0xFEFFU)) {
        return UnicodeScript::Arabic;
    }
    if (InRange(scalar, 0x3040U, 0x309FU)) {
        return UnicodeScript::Hiragana;
    }
    if (InRange(scalar, 0x30A0U, 0x30FFU) ||
        InRange(scalar, 0x31F0U, 0x31FFU)) {
        return UnicodeScript::Katakana;
    }
    if (InRange(scalar, 0x3400U, 0x4DBFU) ||
        InRange(scalar, 0x4E00U, 0x9FFFU) ||
        InRange(scalar, 0xF900U, 0xFAFFU)) {
        return UnicodeScript::Han;
    }
    if (scalar <= 0x0040U ||
        InRange(scalar, 0x2000U, 0x206FU) ||
        InRange(scalar, 0x3000U, 0x303FU)) {
        return UnicodeScript::Common;
    }
    return UnicodeScript::Unknown;
}

UnicodeBidiClass ClassifyBidi(
    std::uint32_t scalar) noexcept {
    if (IsUnicodeCombiningMark(scalar)) {
        return UnicodeBidiClass::NonspacingMark;
    }
    if (scalar == 0x0009U || scalar == 0x000AU ||
        scalar == 0x000DU || scalar == 0x0020U ||
        scalar == 0x00A0U) {
        return UnicodeBidiClass::Whitespace;
    }
    if (InRange(scalar, 0x0030U, 0x0039U)) {
        return UnicodeBidiClass::EuropeanNumber;
    }
    if (InRange(scalar, 0x0660U, 0x0669U) ||
        InRange(scalar, 0x06F0U, 0x06F9U)) {
        return UnicodeBidiClass::ArabicNumber;
    }
    if (InRange(scalar, 0x0590U, 0x05FFU)) {
        return UnicodeBidiClass::RightToLeft;
    }
    if (ClassifyScript(scalar) == UnicodeScript::Arabic) {
        return UnicodeBidiClass::ArabicLetter;
    }
    const UnicodeScript script = ClassifyScript(scalar);
    if (script == UnicodeScript::Latin ||
        script == UnicodeScript::Han ||
        script == UnicodeScript::Hiragana ||
        script == UnicodeScript::Katakana) {
        return UnicodeBidiClass::LeftToRight;
    }
    return UnicodeBidiClass::Neutral;
}

UnicodeLineBreakClass ClassifyLineBreak(
    std::uint32_t scalar) noexcept {
    if (scalar == 0x000AU || scalar == 0x000DU ||
        scalar == 0x2028U || scalar == 0x2029U) {
        return UnicodeLineBreakClass::Mandatory;
    }
    if (scalar == 0x0009U || scalar == 0x0020U ||
        scalar == 0x00A0U || scalar == 0x3000U) {
        return UnicodeLineBreakClass::Space;
    }
    if (IsUnicodeCombiningMark(scalar)) {
        return UnicodeLineBreakClass::CombiningMark;
    }
    if (InRange(scalar, 0x0030U, 0x0039U) ||
        InRange(scalar, 0x0660U, 0x0669U) ||
        InRange(scalar, 0x06F0U, 0x06F9U)) {
        return UnicodeLineBreakClass::Numeric;
    }
    const UnicodeScript script = ClassifyScript(scalar);
    if (script == UnicodeScript::Han ||
        script == UnicodeScript::Hiragana ||
        script == UnicodeScript::Katakana) {
        return UnicodeLineBreakClass::Ideograph;
    }
    if (scalar == 0x002DU || scalar == 0x2010U ||
        scalar == 0x2013U) {
        return UnicodeLineBreakClass::Hyphen;
    }
    if (scalar == 0x0028U || scalar == 0x005BU ||
        scalar == 0x007BU || scalar == 0x3008U ||
        scalar == 0x300CU) {
        return UnicodeLineBreakClass::OpeningPunctuation;
    }
    if (scalar == 0x0029U || scalar == 0x005DU ||
        scalar == 0x007DU || scalar == 0x002CU ||
        scalar == 0x002EU || scalar == 0x003AU ||
        scalar == 0x003BU || scalar == 0x3001U ||
        scalar == 0x3002U || scalar == 0x3009U ||
        scalar == 0x300DU) {
        return UnicodeLineBreakClass::ClosingPunctuation;
    }
    return UnicodeLineBreakClass::Alphabetic;
}

UnicodeTextAnalysis::UnicodeTextAnalysis(
    Base::IAllocator* allocator) noexcept
    : scalars_(allocator),
      logicalRuns_(allocator),
      visualOrder_(allocator),
      breaks_(allocator),
      clusterMap_(allocator) {}

void UnicodeTextAnalysis::Clear() noexcept {
    scalars_.Clear();
    logicalRuns_.Clear();
    visualOrder_.Clear();
    breaks_.Clear();
    clusterMap_.Clear();
    paragraphLevel_ = 0U;
    clusterCount_ = 0U;
}

Base::Result<void> UnicodeTextAnalysis::Analyze(
    Base::StringView text,
    ParagraphDirection direction) noexcept {
    Clear();
    Base::Result<void> decoded = Decode(text);
    if (!decoded) return decoded.GetStatus();
    Base::Result<void> resolved = Resolve(direction);
    if (!resolved) return resolved.GetStatus();
    Base::Result<void> runs = BuildRuns();
    if (!runs) return runs.GetStatus();
    Base::Result<void> visual = BuildVisualOrder();
    if (!visual) return visual.GetStatus();
    Base::Result<void> breaks = BuildBreaks();
    if (!breaks) return breaks.GetStatus();
    return BuildClusterMap();
}

Base::Result<void> UnicodeTextAnalysis::Decode(
    Base::StringView text) noexcept {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(
        text.Data());
    std::uint32_t offset = 0U;
    std::uint32_t cluster = 0U;
    while (offset < text.SizeBytes()) {
        const std::uint8_t first = bytes[offset];
        std::uint32_t value = 0U;
        std::uint8_t length = 0U;
        if (first < 0x80U) {
            value = first;
            length = 1U;
        } else if ((first & 0xE0U) == 0xC0U) {
            length = 2U;
            value = first & 0x1FU;
        } else if ((first & 0xF0U) == 0xE0U) {
            length = 3U;
            value = first & 0x0FU;
        } else if ((first & 0xF8U) == 0xF0U) {
            length = 4U;
            value = first & 0x07U;
        } else {
            return InvalidUtf8(
                "Unicode analysis encountered an invalid UTF-8 lead byte");
        }
        if (offset + length > text.SizeBytes()) {
            return InvalidUtf8(
                "Unicode analysis encountered truncated UTF-8");
        }
        for (std::uint8_t index = 1U; index < length; ++index) {
            const std::uint8_t next = bytes[offset + index];
            if (!IsContinuation(next)) {
                return InvalidUtf8(
                    "Unicode analysis encountered an invalid continuation byte");
            }
            value = (value << 6U) | (next & 0x3FU);
        }
        const bool overlong =
            (length == 2U && value < 0x80U) ||
            (length == 3U && value < 0x800U) ||
            (length == 4U && value < 0x10000U);
        if (overlong || value > 0x10FFFFU ||
            InRange(value, 0xD800U, 0xDFFFU)) {
            return InvalidUtf8(
                "Unicode analysis encountered a non-scalar UTF-8 value");
        }

        UnicodeScalar scalar;
        scalar.value = value;
        scalar.byteOffset = offset;
        scalar.byteLength = length;
        scalar.script = ClassifyScript(value);
        scalar.bidi = ClassifyBidi(value);
        scalar.lineBreak = ClassifyLineBreak(value);
        if (!IsUnicodeCombiningMark(value) || scalars_.Empty()) {
            scalar.cluster = cluster++;
        } else {
            scalar.cluster = scalars_.Back().cluster;
        }
        Base::Result<void> appended =
            scalars_.TryPushBack(scalar);
        if (!appended) return appended.GetStatus();
        offset += length;
    }
    clusterCount_ = cluster;
    return {};
}

Base::Result<void> UnicodeTextAnalysis::Resolve(
    ParagraphDirection direction) noexcept {
    if (direction == ParagraphDirection::LeftToRight) {
        paragraphLevel_ = 0U;
    } else if (direction == ParagraphDirection::RightToLeft) {
        paragraphLevel_ = 1U;
    } else {
        paragraphLevel_ = 0U;
        for (const UnicodeScalar& scalar : scalars_) {
            if (IsStrongRight(scalar.bidi)) {
                paragraphLevel_ = 1U;
                break;
            }
            if (IsStrongLeft(scalar.bidi)) {
                paragraphLevel_ = 0U;
                break;
            }
        }
    }

    std::uint8_t previousStrong = paragraphLevel_;
    UnicodeScript previousScript = UnicodeScript::Common;
    for (UnicodeScalar& scalar : scalars_) {
        if (IsStrongRight(scalar.bidi)) {
            scalar.embeddingLevel =
                (paragraphLevel_ & 1U) != 0U ? 1U : 1U;
            previousStrong = scalar.embeddingLevel;
        } else if (IsStrongLeft(scalar.bidi)) {
            scalar.embeddingLevel =
                (paragraphLevel_ & 1U) != 0U ? 2U : 0U;
            previousStrong = scalar.embeddingLevel;
        } else if (scalar.bidi == UnicodeBidiClass::EuropeanNumber ||
                   scalar.bidi == UnicodeBidiClass::ArabicNumber) {
            scalar.embeddingLevel =
                (previousStrong & 1U) != 0U ? 2U : previousStrong;
        } else {
            scalar.embeddingLevel = previousStrong;
        }

        if (scalar.script == UnicodeScript::Inherited ||
            scalar.script == UnicodeScript::Common ||
            scalar.script == UnicodeScript::Unknown) {
            if (previousScript != UnicodeScript::Common) {
                scalar.script = previousScript;
            }
        } else {
            previousScript = scalar.script;
        }
    }
    return {};
}

Base::Result<void> UnicodeTextAnalysis::BuildRuns() noexcept {
    if (scalars_.Empty()) return {};
    UnicodeRun current;
    current.firstScalar = 0U;
    current.scalarCount = 1U;
    current.script = scalars_[0].script;
    current.embeddingLevel = scalars_[0].embeddingLevel;
    current.rightToLeft =
        (current.embeddingLevel & 1U) != 0U;
    for (std::uint32_t index = 1U;
         index < scalars_.Size();
         ++index) {
        const UnicodeScalar& scalar = scalars_[index];
        if (scalar.script == current.script &&
            scalar.embeddingLevel == current.embeddingLevel) {
            ++current.scalarCount;
            continue;
        }
        Base::Result<void> appended =
            logicalRuns_.TryPushBack(current);
        if (!appended) return appended.GetStatus();
        current.firstScalar = index;
        current.scalarCount = 1U;
        current.script = scalar.script;
        current.embeddingLevel = scalar.embeddingLevel;
        current.rightToLeft =
            (scalar.embeddingLevel & 1U) != 0U;
    }
    return logicalRuns_.TryPushBack(current);
}

Base::Result<void> UnicodeTextAnalysis::BuildVisualOrder() noexcept {
    Base::Result<void> reserve =
        visualOrder_.TryReserve(scalars_.Size());
    if (!reserve) return reserve.GetStatus();
    std::uint8_t maximum = paragraphLevel_;
    std::uint8_t minimumOdd = 255U;
    for (const UnicodeScalar& scalar : scalars_) {
        Base::Result<void> appended =
            visualOrder_.TryPushBack(visualOrder_.Size());
        if (!appended) return appended.GetStatus();
        if (scalar.embeddingLevel > maximum) {
            maximum = scalar.embeddingLevel;
        }
        if ((scalar.embeddingLevel & 1U) != 0U &&
            scalar.embeddingLevel < minimumOdd) {
            minimumOdd = scalar.embeddingLevel;
        }
    }
    if (minimumOdd == 255U) return {};

    for (std::uint32_t level = maximum + 1U;
         level-- > minimumOdd;) {
        std::uint32_t index = 0U;
        while (index < visualOrder_.Size()) {
            while (index < visualOrder_.Size() &&
                scalars_[visualOrder_[index]].embeddingLevel < level) {
                ++index;
            }
            const std::uint32_t begin = index;
            while (index < visualOrder_.Size() &&
                scalars_[visualOrder_[index]].embeddingLevel >= level) {
                ++index;
            }
            if (index > begin + 1U) {
                std::uint32_t left = begin;
                std::uint32_t right = index - 1U;
                while (left < right) {
                    const std::uint32_t value = visualOrder_[left];
                    visualOrder_[left] = visualOrder_[right];
                    visualOrder_[right] = value;
                    ++left;
                    --right;
                }
            }
        }
        if (level == 0U) break;
    }
    return {};
}

Base::Result<void> UnicodeTextAnalysis::BuildBreaks() noexcept {
    Base::Result<void> reserve =
        breaks_.TryReserve(scalars_.Size() + 1U);
    if (!reserve) return reserve.GetStatus();
    Base::Result<void> start = breaks_.TryPushBack({0U, true, false});
    if (!start) return start.GetStatus();
    for (std::uint32_t index = 0U;
         index < scalars_.Size();
         ++index) {
        const UnicodeLineBreakClass current =
            scalars_[index].lineBreak;
        const UnicodeLineBreakClass next =
            index + 1U < scalars_.Size()
                ? scalars_[index + 1U].lineBreak
                : UnicodeLineBreakClass::Mandatory;
        UnicodeBreakOpportunity opportunity;
        opportunity.scalarIndex = index + 1U;
        opportunity.mandatory =
            current == UnicodeLineBreakClass::Mandatory ||
            index + 1U == scalars_.Size();
        opportunity.allowed = opportunity.mandatory ||
            current == UnicodeLineBreakClass::Space ||
            current == UnicodeLineBreakClass::Hyphen ||
            (IsIdeograph(current) &&
             next != UnicodeLineBreakClass::CombiningMark &&
             next != UnicodeLineBreakClass::ClosingPunctuation) ||
            (IsIdeograph(next) &&
             current != UnicodeLineBreakClass::OpeningPunctuation);
        Base::Result<void> appended =
            breaks_.TryPushBack(opportunity);
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> UnicodeTextAnalysis::BuildClusterMap() noexcept {
    if (clusterCount_ == 0U) return {};
    Base::Result<void> reserve =
        clusterMap_.TryReserve(clusterCount_);
    if (!reserve) return reserve.GetStatus();
    for (std::uint32_t logical = 0U;
         logical < clusterCount_;
         ++logical) {
        UnicodeClusterMap map;
        map.logicalCluster = logical;
        map.visualCluster = InvalidIndex;
        Base::Result<void> appended =
            clusterMap_.TryPushBack(map);
        if (!appended) return appended.GetStatus();
    }
    std::uint32_t visualCluster = 0U;
    std::uint32_t previousLogical = InvalidIndex;
    for (std::uint32_t visual = 0U;
         visual < visualOrder_.Size();
         ++visual) {
        const std::uint32_t logicalScalar =
            visualOrder_[visual];
        const std::uint32_t logicalCluster =
            scalars_[logicalScalar].cluster;
        if (logicalCluster == previousLogical) continue;
        clusterMap_[logicalCluster].visualCluster = visualCluster++;
        previousLogical = logicalCluster;
    }
    return {};
}

std::uint32_t UnicodeTextAnalysis::LogicalScalarFromVisual(
    std::uint32_t visualIndex) const noexcept {
    return visualIndex < visualOrder_.Size()
        ? visualOrder_[visualIndex]
        : InvalidIndex;
}

std::uint32_t UnicodeTextAnalysis::VisualScalarFromLogical(
    std::uint32_t logicalIndex) const noexcept {
    if (logicalIndex >= scalars_.Size()) return InvalidIndex;
    for (std::uint32_t visual = 0U;
         visual < visualOrder_.Size();
         ++visual) {
        if (visualOrder_[visual] == logicalIndex) return visual;
    }
    return InvalidIndex;
}

} // namespace Aero::Text
