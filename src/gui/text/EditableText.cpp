#include "EditableText.hpp"

#include <Aero/Base/Utf8.hpp>
#include <Aero/Base/Vector.hpp>

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero::Text {
namespace {

constexpr std::uint32_t InitialGapBytes = 64U;
constexpr std::uint32_t HistoryLimit = 128U;

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::InvalidArgument(message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::InvalidState(message);
}

[[maybe_unused]] Base::Status OutOfMemory(const char* message = "Out of memory") noexcept {
    return Base::Status::OutOfMemory(message);
}

Base::Status OutOfRange(const char* message) noexcept {
    return Base::Status::OutOfRange(message);
}

Base::Status NotFound(const char* message) noexcept {
    return Base::Status::NotFound(message);
}

template <typename ByteAccessor>
Base::DecodedCodePoint Decode(
    std::uint32_t offset,
    std::uint32_t totalBytes,
    ByteAccessor&& byteAt) noexcept {
    char buf[4];
    const std::uint32_t count = std::min(4U, totalBytes - offset);
    for (std::uint32_t i = 0; i < count; ++i) {
        buf[i] = static_cast<char>(byteAt(offset + i));
    }
    return Base::DecodeCodePoint(buf, count);
}

bool IsCombiningMark(std::uint32_t codePoint) noexcept {
    return
        (codePoint >= 0x0300U && codePoint <= 0x036FU) ||
        (codePoint >= 0x064BU && codePoint <= 0x065FU) ||
        codePoint == 0x0670U ||
        (codePoint >= 0x1AB0U && codePoint <= 0x1AFFU) ||
        (codePoint >= 0x1DC0U && codePoint <= 0x1DFFU) ||
        (codePoint >= 0x20D0U && codePoint <= 0x20FFU) ||
        (codePoint >= 0xFE20U && codePoint <= 0xFE2FU);
}

bool IsVariationSelector(std::uint32_t codePoint) noexcept {
    return
        (codePoint >= 0xFE00U && codePoint <= 0xFE0FU) ||
        (codePoint >= 0xE0100U && codePoint <= 0xE01EFU);
}

bool IsEmojiModifier(std::uint32_t codePoint) noexcept {
    return codePoint >= 0x1F3FBU &&
        codePoint <= 0x1F3FFU;
}

bool IsRegionalIndicator(std::uint32_t codePoint) noexcept {
    return codePoint >= 0x1F1E6U &&
        codePoint <= 0x1F1FFU;
}

bool IsGraphemeExtend(std::uint32_t codePoint) noexcept {
    return IsCombiningMark(codePoint) ||
        IsVariationSelector(codePoint) ||
        IsEmojiModifier(codePoint);
}

bool IsNewline(std::uint32_t codePoint) noexcept {
    return codePoint == 0x000AU ||
        codePoint == 0x000DU;
}

bool ShouldBreak(
    std::uint32_t previous,
    std::uint32_t current,
    std::uint32_t regionalIndicatorRun) noexcept {
    if (previous == 0x000DU && current == 0x000AU) {
        return false;
    }
    if (IsNewline(previous) || IsNewline(current)) {
        return true;
    }
    if (IsGraphemeExtend(current) || current == 0x200DU ||
        previous == 0x200DU) {
        return false;
    }
    if (IsRegionalIndicator(previous) &&
        IsRegionalIndicator(current) &&
        regionalIndicatorRun % 2U == 1U) {
        return false;
    }
    return true;
}

Base::Result<std::uint32_t> CountGraphemes(
    Base::StringView text,
    std::uint32_t* codePointCount = nullptr) noexcept {
    const Base::Utf8Validation validation =
        Base::ValidateUtf8(text);
    if (!validation.valid) {
        return InvalidArgument(
            "Editable text replacement is not valid UTF-8");
    }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(
        text.Data());
    const auto byteAt = [&](std::uint32_t index) noexcept {
        return bytes[index];
    };
    std::uint32_t offset = 0U;
    std::uint32_t graphemes = 0U;
    std::uint32_t codePoints = 0U;
    std::uint32_t previous = 0U;
    std::uint32_t regionalRun = 0U;
    bool first = true;
    while (offset < text.SizeBytes()) {
        const Base::DecodedCodePoint decoded = Decode(offset, text.SizeBytes(), byteAt);
        const bool breakBefore = first ||
            ShouldBreak(previous, decoded.value, regionalRun);
        if (breakBefore) {
            ++graphemes;
        }
        if (IsRegionalIndicator(decoded.value)) {
            regionalRun =
                !breakBefore &&
                IsRegionalIndicator(previous)
                ? regionalRun + 1U
                : 1U;
        } else if (!IsGraphemeExtend(decoded.value) &&
                   decoded.value != 0x200DU) {
            regionalRun = 0U;
        }
        previous = decoded.value;
        first = false;
        ++codePoints;
        offset += decoded.length;
    }
    if (codePointCount != nullptr) {
        *codePointCount = codePoints;
    }
    return graphemes;
}

} // namespace

std::uint8_t EditableTextState::ByteAt(std::uint32_t offset) const noexcept {
    const std::uint32_t physical =
        offset < gapBegin
        ? offset
        : offset + GapSize();
    return static_cast<std::uint8_t>(bytes[physical]);
}

Base::Result<void> EditableTextState::EnsureIndexCapacity(
    std::uint32_t newTextBytes) noexcept {
    if (newTextBytes == UINT32_MAX) {
        return OutOfRange(
            "Editable text size exceeds index capacity");
    }
    graphemeOffsets.Reserve(newTextBytes + 1U);
    lineStarts.Reserve(newTextBytes + 1U);
    return {};
}

Base::Result<void> EditableTextState::EnsureGap(
    std::uint32_t requiredBytes) noexcept {
    if (GapSize() >= requiredBytes) {
        return {};
    }
    if (requiredBytes >
        UINT32_MAX - textBytes - InitialGapBytes) {
        return OutOfRange(
            "Editable text buffer exceeds 32-bit capacity");
    }
    const std::uint32_t minimum =
        textBytes + requiredBytes + InitialGapBytes;
    std::uint32_t capacity =
        bytes.Empty() ? InitialGapBytes : bytes.Size();
    while (capacity < minimum) {
        if (capacity > UINT32_MAX / 2U) {
            capacity = minimum;
            break;
        }
        capacity *= 2U;
    }
    Base::Vector<char> replacement(&bytes.Allocator());
    replacement.Resize(capacity);
    if (capacity != 0U && replacement.Data() == nullptr) {
        return OutOfMemory(
            "Failed to allocate text replacement buffer");
    }
    if (gapBegin != 0U) {
        std::memcpy(
            replacement.Data(),
            bytes.Data(),
            gapBegin);
    }
    const std::uint32_t suffixBytes =
        textBytes - gapBegin;
    const std::uint32_t replacementGapEnd =
        capacity - suffixBytes;
    if (suffixBytes != 0U) {
        std::memcpy(
            replacement.Data() + replacementGapEnd,
            bytes.Data() + gapEnd,
            suffixBytes);
    }
    bytes = std::move(replacement);
    gapEnd = replacementGapEnd;
    return {};
}

Base::Result<void> EditableTextState::MoveGap(
    std::uint32_t byteOffset) noexcept {
    if (byteOffset > textBytes) {
        return OutOfRange(
            "Editable text gap offset is out of range");
    }
    if (byteOffset < gapBegin) {
        const std::uint32_t count =
            gapBegin - byteOffset;
        std::memmove(
            bytes.Data() + gapEnd - count,
            bytes.Data() + byteOffset,
            count);
        gapBegin = byteOffset;
        gapEnd -= count;
    } else if (byteOffset > gapBegin) {
        const std::uint32_t count =
            byteOffset - gapBegin;
        std::memmove(
            bytes.Data() + gapBegin,
            bytes.Data() + gapEnd,
            count);
        gapBegin += count;
        gapEnd += count;
    }
    return {};
}

Base::Result<void> EditableTextState::RebuildIndexes() noexcept {
    graphemeOffsets.Clear();
    lineStarts.Clear();
    graphemeOffsets.PushBack(0U);
    lineStarts.PushBack(0U);
    if (textBytes == 0U) {
        codePoints = 0U;
        return {};
    }

    const auto byteAt = [&](std::uint32_t index) noexcept {
        return ByteAt(index);
    };
    std::uint32_t offset = 0U;
    std::uint32_t previous = 0U;
    std::uint32_t regionalRun = 0U;
    std::uint32_t points = 0U;
    std::uint32_t currentGrapheme = 0U;
    bool currentClusterIsNewline = false;
    bool first = true;
    while (offset < textBytes) {
        const Base::DecodedCodePoint decoded =
            Decode(offset, textBytes, byteAt);
        const bool breakBefore = first ||
            ShouldBreak(
                previous,
                decoded.value,
                regionalRun);
        if (breakBefore) {
            if (!first) {
                graphemeOffsets.PushBack(offset);
                ++currentGrapheme;
                if (currentClusterIsNewline) {
                    lineStarts.PushBack(currentGrapheme);
                }
            }
            currentClusterIsNewline = IsNewline(decoded.value);
        }
        if (IsRegionalIndicator(decoded.value)) {
            regionalRun =
                !breakBefore &&
                IsRegionalIndicator(previous)
                ? regionalRun + 1U
                : 1U;
        } else if (!IsGraphemeExtend(decoded.value) &&
                   decoded.value != 0x200DU) {
            regionalRun = 0U;
        }
        previous = decoded.value;
        first = false;
        ++points;
        offset += decoded.length;
    }
    if (!first && currentClusterIsNewline) {
        lineStarts.PushBack(currentGrapheme + 1U);
    }
    graphemeOffsets.PushBack(textBytes);
    codePoints = points;
    return {};
}

Base::Result<void> EditableTextState::CopyByteRange(
    std::uint32_t start,
    std::uint32_t count,
    Base::String& output) const noexcept {
    if (start > textBytes ||
        count > textBytes - start) {
        return OutOfRange(
            "Editable text byte range is out of bounds");
    }
    output.Clear();
    output.Reserve(count);
    if (count == 0U) {
        return {};
    }
    if (start < gapBegin) {
        const std::uint32_t prefix =
            std::min(count, gapBegin - start);
        Base::Result<void> appended =
            output.AppendUnchecked({
                bytes.Data() + start, prefix});
        if (!appended) {
            return appended;
        }
        if (prefix == count) {
            return {};
        }
        return output.AppendUnchecked({
            bytes.Data() + gapEnd,
            count - prefix});
    }
    return output.AppendUnchecked({
        bytes.Data() + start + GapSize(), count});
}

Base::Result<void> EditableTextState::ReplaceBytes(
    std::uint32_t byteStart,
    std::uint32_t removedBytes,
    Base::StringView replacement) noexcept {
    Base::Result<void> moved = MoveGap(byteStart);
    if (!moved) {
        return moved;
    }
    gapEnd += removedBytes;
    textBytes -= removedBytes;
    Base::Result<void> gap =
        EnsureGap(replacement.SizeBytes());
    if (!gap) {
        return gap;
    }
    if (replacement.SizeBytes() != 0U) {
        std::memcpy(
            bytes.Data() + gapBegin,
            replacement.Data(),
            replacement.SizeBytes());
        gapBegin += replacement.SizeBytes();
        textBytes += replacement.SizeBytes();
    }
    return RebuildIndexes();
}

bool EditableTextState::TryIncrementalReplace(
    TextRange range,
    std::uint32_t byteStart,
    std::uint32_t removedBytes,
    Base::StringView replacement,
    std::uint32_t insertedCount,
    std::uint32_t insertedCodePoints) noexcept {
    const std::uint32_t currentGraphemes = GraphemeCount();
    const std::uint32_t byteEnd = byteStart + removedBytes;

    for (std::uint32_t i = 0U; i < removedBytes; ++i) {
        const char c = static_cast<char>(ByteAt(byteStart + i));
        if (c == '\r' || c == '\n') {
            return false;
        }
    }
    for (std::uint32_t i = 0U; i < replacement.SizeBytes(); ++i) {
        const char c = replacement[i];
        if (c == '\r' || c == '\n') {
            return false;
        }
    }

    if (range.start > 0U) {
        std::uint32_t leftOffset = graphemeOffsets[range.start - 1U];
        std::uint32_t leftCp = 0U;
        while (leftOffset < byteStart) {
            const Base::DecodedCodePoint dec =
                Decode(leftOffset, byteStart, [&](std::uint32_t idx) { return ByteAt(idx); });
            leftCp = dec.value;
            leftOffset += dec.length;
        }
        if (leftCp == 0x200DU || leftCp == 0x000DU || IsRegionalIndicator(leftCp) || IsGraphemeExtend(leftCp)) {
            return false;
        }
    }

    const auto* repBytes = reinterpret_cast<const std::uint8_t*>(replacement.Data());
    if (replacement.SizeBytes() > 0U) {
        const Base::DecodedCodePoint firstDec =
            Decode(0U, replacement.SizeBytes(), [&](std::uint32_t idx) { return repBytes[idx]; });
        if (firstDec.value == 0x200DU || IsGraphemeExtend(firstDec.value) || IsRegionalIndicator(firstDec.value)) {
            return false;
        }
        std::uint32_t repOff = 0U;
        std::uint32_t lastCp = 0U;
        while (repOff < replacement.SizeBytes()) {
            const Base::DecodedCodePoint dec =
                Decode(repOff, replacement.SizeBytes(), [&](std::uint32_t idx) { return repBytes[idx]; });
            lastCp = dec.value;
            repOff += dec.length;
        }
        if (lastCp == 0x200DU || lastCp == 0x000DU || IsRegionalIndicator(lastCp) || IsGraphemeExtend(lastCp)) {
            return false;
        }
    }

    if (range.GetEnd() < currentGraphemes) {
        const Base::DecodedCodePoint rightDec =
            Decode(byteEnd, textBytes, [&](std::uint32_t idx) { return ByteAt(idx); });
        if (rightDec.value == 0x200DU || rightDec.value == 0x000AU || IsGraphemeExtend(rightDec.value) || IsRegionalIndicator(rightDec.value)) {
            return false;
        }
    }

    std::uint32_t removedCodePoints = 0U;
    std::uint32_t off = byteStart;
    while (off < byteEnd) {
        const Base::DecodedCodePoint dec =
            Decode(off, byteEnd, [&](std::uint32_t idx) { return ByteAt(idx); });
        ++removedCodePoints;
        off += dec.length;
    }

    Base::Vector<std::uint32_t> relOffsets(&bytes.Allocator());
    relOffsets.Reserve(insertedCount);
    std::uint32_t rOff = 0U;
    std::uint32_t prev = 0U;
    std::uint32_t regRun = 0U;
    bool first = true;
    while (rOff < replacement.SizeBytes()) {
        const Base::DecodedCodePoint dec =
            Decode(rOff, replacement.SizeBytes(), [&](std::uint32_t idx) { return repBytes[idx]; });
        const bool brk = first || ShouldBreak(prev, dec.value, regRun);
        if (brk) {
            relOffsets.PushBack(rOff);
        }
        if (IsRegionalIndicator(dec.value)) {
            regRun = !brk && IsRegionalIndicator(prev) ? regRun + 1U : 1U;
        } else if (!IsGraphemeExtend(dec.value) && dec.value != 0x200DU) {
            regRun = 0U;
        }
        prev = dec.value;
        first = false;
        rOff += dec.length;
    }
    if (relOffsets.Size() != insertedCount) {
        return false;
    }

    Base::Result<void> moved = MoveGap(byteStart);
    if (!moved) {
        return false;
    }
    gapEnd += removedBytes;
    textBytes -= removedBytes;
    Base::Result<void> gap = EnsureGap(replacement.SizeBytes());
    if (!gap) {
        return false;
    }
    if (replacement.SizeBytes() != 0U) {
        std::memcpy(
            bytes.Data() + gapBegin,
            replacement.Data(),
            replacement.SizeBytes());
        gapBegin += replacement.SizeBytes();
        textBytes += replacement.SizeBytes();
    }

    codePoints = codePoints - removedCodePoints + insertedCodePoints;

    const std::int32_t deltaGraphemes =
        static_cast<std::int32_t>(insertedCount) - static_cast<std::int32_t>(range.length);
    if (deltaGraphemes != 0) {
        for (std::uint32_t i = 1U; i < lineStarts.Size(); ++i) {
            if (lineStarts[i] > range.start) {
                lineStarts[i] = static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(lineStarts[i]) + deltaGraphemes);
            }
        }
    }

    const std::uint32_t oldSize = graphemeOffsets.Size();
    const std::int32_t deltaBytes =
        static_cast<std::int32_t>(replacement.SizeBytes()) - static_cast<std::int32_t>(removedBytes);
    const std::uint32_t oldTailStart = range.start + range.length;
    const std::uint32_t newTailStart = range.start + insertedCount;
    const std::uint32_t tailCount = oldSize - oldTailStart;

    if (deltaGraphemes > 0) {
        graphemeOffsets.Resize(oldSize + static_cast<std::uint32_t>(deltaGraphemes));
        std::memmove(
            graphemeOffsets.Data() + newTailStart,
            graphemeOffsets.Data() + oldTailStart,
            tailCount * sizeof(std::uint32_t));
        for (std::uint32_t k = 0U; k < insertedCount; ++k) {
            graphemeOffsets[range.start + k] = byteStart + relOffsets[k];
        }
        for (std::uint32_t k = 0U; k < tailCount; ++k) {
            graphemeOffsets[newTailStart + k] = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(graphemeOffsets[newTailStart + k]) + deltaBytes);
        }
    } else if (deltaGraphemes < 0) {
        std::memmove(
            graphemeOffsets.Data() + newTailStart,
            graphemeOffsets.Data() + oldTailStart,
            tailCount * sizeof(std::uint32_t));
        for (std::uint32_t k = 0U; k < insertedCount; ++k) {
            graphemeOffsets[range.start + k] = byteStart + relOffsets[k];
        }
        for (std::uint32_t k = 0U; k < tailCount; ++k) {
            graphemeOffsets[newTailStart + k] = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(graphemeOffsets[newTailStart + k]) + deltaBytes);
        }
        static_cast<void>(graphemeOffsets.Resize(
            oldSize - static_cast<std::uint32_t>(-deltaGraphemes)));
    } else {
        for (std::uint32_t k = 0U; k < insertedCount; ++k) {
            graphemeOffsets[range.start + k] = byteStart + relOffsets[k];
        }
        for (std::uint32_t k = 0U; k < tailCount; ++k) {
            graphemeOffsets[newTailStart + k] = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(graphemeOffsets[newTailStart + k]) + deltaBytes);
        }
    }

    return true;
}

void EditableTextState::TrimHistory(
    Base::Vector<EditRecord>& history) noexcept {
    if (history.Size() < HistoryLimit) {
        return;
    }
    constexpr std::uint32_t DropBatch = 16U;
    const std::uint32_t drop = std::min(DropBatch, static_cast<std::uint32_t>(history.Size()));
    std::move(history.Data() + drop, history.Data() + history.Size(), history.Data());
    static_cast<void>(history.Resize(history.Size() - drop));
}

Base::Result<void> EditableTextState::Replace(
    TextRange range,
    Base::StringView replacement,
    bool recordHistory,
    const TextSelection* forcedSelection) noexcept {
    const std::uint32_t currentGraphemes =
        GraphemeCount();
    if (range.start > currentGraphemes ||
        range.length >
            currentGraphemes - range.start) {
        return OutOfRange(
            "Editable text range is out of bounds");
    }
    std::uint32_t insertedPoints = 0U;
    Base::Result<std::uint32_t> insertedCount =
        CountGraphemes(replacement, &insertedPoints);
    if (!insertedCount) {
        return insertedCount.GetStatus();
    }
    const std::uint32_t retained =
        currentGraphemes - range.length;
    if (insertedCount.Value() >
        maximumLength - retained) {
        return OutOfRange(
            "Editable text replacement exceeds maximum length");
    }
    const std::uint32_t byteStart =
        graphemeOffsets[range.start];
    const std::uint32_t byteEnd =
        graphemeOffsets[range.GetEnd()];
    const std::uint32_t removedBytes =
        byteEnd - byteStart;
    if (replacement.SizeBytes() >
        UINT32_MAX - (textBytes - removedBytes)) {
        return OutOfRange(
            "Editable text replacement exceeds byte capacity");
    }
    const std::uint32_t newTextBytes =
        textBytes - removedBytes +
        replacement.SizeBytes();
    Base::Result<void> indexes =
        EnsureIndexCapacity(newTextBytes);
    if (!indexes) {
        return indexes;
    }
    Base::Result<void> gap =
        EnsureGap(replacement.SizeBytes());
    if (!gap) {
        return gap;
    }

    EditRecord record(&bytes.Allocator());
    if (recordHistory) {
        Base::Result<void> removed =
            CopyByteRange(
                byteStart, removedBytes, record.removed);
        if (!removed) {
            return removed;
        }
        Base::Result<void> inserted =
            record.inserted.Assign(replacement);
        if (!inserted) {
            return inserted;
        }
        record.start = range.start;
        record.removedGraphemes = range.length;
        record.insertedGraphemes =
            insertedCount.Value();
        record.before = selection;
        record.after = {
            range.start + insertedCount.Value(),
            range.start + insertedCount.Value()};
        undo.Reserve(
                std::min(
                    HistoryLimit,
                    undo.Size() + 1U));
    }

    const bool incremental = TryIncrementalReplace(
        range, byteStart, removedBytes, replacement,
        insertedCount.Value(), insertedPoints);
    if (!incremental) {
        Base::Result<void> replaced =
            ReplaceBytes(
                byteStart, removedBytes, replacement);
        if (!replaced) {
            return replaced;
        }
    }
    selection = forcedSelection != nullptr
        ? *forcedSelection
        : TextSelection{
            range.start + insertedCount.Value(),
            range.start + insertedCount.Value()};
    ++revision;
    if (recordHistory) {
        redo.Clear();
        TrimHistory(undo);
        undo.PushBack(std::move(record));
    }
    return {};
}

EditableTextModel::EditableTextModel(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {}

EditableTextModel::~EditableTextModel() noexcept = default;

Base::Result<void> EditableTextModel::EnsureState() noexcept {
    if (state_.has_value()) {
        return {};
    }
    state_.emplace(allocator_);
    Base::Result<void> indexes =
        state_->EnsureIndexCapacity(0U);
    if (indexes) {
        indexes = state_->RebuildIndexes();
    }
    if (!indexes) {
        state_.reset();
        return indexes;
    }
    return {};
}

Base::Result<void> EditableTextModel::SetText(
    Base::StringView text) noexcept {
    const Base::Utf8Validation validation =
        Base::ValidateUtf8(text);
    if (!validation.valid) {
        return InvalidArgument(
            "Editable text replacement is not valid UTF-8");
    }
    Base::Result<void> state = EnsureState();
    if (!state) {
        return state;
    }
    Base::Result<void> indexes =
        state_->EnsureIndexCapacity(text.SizeBytes());
    if (!indexes) {
        return indexes;
    }
    if (text.SizeBytes() >
        UINT32_MAX - InitialGapBytes) {
        return OutOfRange(
            "Editable text exceeds byte capacity");
    }
    Base::Vector<char> replacement(allocator_);
    replacement.Resize(
            text.SizeBytes() + InitialGapBytes);
    if (text.SizeBytes() != 0U) {
        std::memcpy(
            replacement.Data(),
            text.Data(),
            text.SizeBytes());
    }
    state_->bytes = std::move(replacement);
    state_->gapBegin = text.SizeBytes();
    state_->gapEnd = state_->bytes.Size();
    state_->textBytes = text.SizeBytes();
    Base::Result<void> rebuilt =
        state_->RebuildIndexes();
    if (!rebuilt) {
        return rebuilt;
    }
    if (state_->GraphemeCount() > state_->maximumLength) {
        state_->bytes.Clear();
        state_->textBytes = 0U;
        state_->gapBegin = 0U;
        state_->gapEnd = 0U;
        static_cast<void>(state_->RebuildIndexes());
        return OutOfRange(
            "Editable text exceeds maximum length");
    }
    const std::uint32_t graphemeCount = state_->GraphemeCount();
    state_->selection = {
        graphemeCount, graphemeCount};
    state_->undo.Clear();
    state_->redo.Clear();
    ++state_->revision;
    return {};
}

Base::Result<void> EditableTextModel::Snapshot(
    Base::String& output) const noexcept {
    if (!state_.has_value()) {
        output.Clear();
        return {};
    }
    return state_->CopyByteRange(
        0U, state_->textBytes, output);
}

std::uint32_t EditableTextModel::SizeBytes() const noexcept {
    return state_.has_value() ? state_->textBytes : 0U;
}

std::uint32_t
EditableTextModel::CodePointCount() const noexcept {
    return state_.has_value() ? state_->codePoints : 0U;
}

std::uint32_t
EditableTextModel::GraphemeCount() const noexcept {
    return state_.has_value() ? state_->GraphemeCount() : 0U;
}

std::uint32_t EditableTextModel::LineCount() const noexcept {
    return state_.has_value()
        ? state_->lineStarts.Size()
        : 1U;
}

std::uint64_t EditableTextModel::Revision() const noexcept {
    return state_.has_value() ? state_->revision : 0U;
}

TextSelection EditableTextModel::Selection() const noexcept {
    return state_.has_value()
        ? state_->selection
        : TextSelection{};
}

std::uint32_t EditableTextModel::Caret() const noexcept {
    return Selection().caret;
}

Base::Result<void> EditableTextModel::SetSelection(
    std::uint32_t anchor,
    std::uint32_t caret) noexcept {
    Base::Result<void> state = EnsureState();
    if (!state) {
        return state;
    }
    if (anchor > state_->GraphemeCount() ||
        caret > state_->GraphemeCount()) {
        return OutOfRange(
            "Editable text selection is out of range");
    }
    state_->selection = {anchor, caret};
    return {};
}

Base::Result<void> EditableTextModel::SelectAll() noexcept {
    Base::Result<void> state = EnsureState();
    if (!state) {
        return state;
    }
    state_->selection = {0U, state_->GraphemeCount()};
    return {};
}

Base::Result<void> EditableTextModel::ReplaceRange(
    TextRange range,
    Base::StringView replacement) noexcept {
    Base::Result<void> state = EnsureState();
    if (!state) {
        return state;
    }
    if (state_->readOnly) {
        return InvalidState(
            "Editable text model is read-only");
    }
    return state_->Replace(range, replacement, true);
}

Base::Result<void> EditableTextModel::ReplaceSelection(
    Base::StringView replacement) noexcept {
    const TextSelection selection = Selection();
    return ReplaceRange(
        {selection.GetStart(), selection.GetLength()},
        replacement);
}

Base::Result<void> EditableTextModel::DeleteBackward() noexcept {
    const TextSelection selection = Selection();
    if (!selection.GetIsEmpty()) {
        return ReplaceRange(
            {selection.GetStart(), selection.GetLength()},
            {});
    }
    if (selection.caret == 0U) {
        return {};
    }
    return ReplaceRange(
        {selection.caret - 1U, 1U}, {});
}

Base::Result<void> EditableTextModel::DeleteForward() noexcept {
    const TextSelection selection = Selection();
    if (!selection.GetIsEmpty()) {
        return ReplaceRange(
            {selection.GetStart(), selection.GetLength()},
            {});
    }
    if (selection.caret >= GraphemeCount()) {
        return {};
    }
    return ReplaceRange(
        {selection.caret, 1U}, {});
}

Base::Result<std::uint32_t>
EditableTextModel::ByteOffsetForGrapheme(
    std::uint32_t graphemeIndex) const noexcept {
    if (!state_.has_value()) {
        return graphemeIndex == 0U
            ? Base::Result<std::uint32_t>(0U)
            : Base::Result<std::uint32_t>(
                OutOfRange(
                    "Editable text grapheme index is out of range"));
    }
    if (graphemeIndex > state_->GraphemeCount()) {
        return OutOfRange(
            "Editable text grapheme index is out of range");
    }
    return state_->graphemeOffsets[graphemeIndex];
}

Base::Result<std::uint32_t>
EditableTextModel::GraphemeIndexForByteOffset(
    std::uint32_t byteOffset) const noexcept {
    if (!state_.has_value()) {
        return byteOffset == 0U
            ? Base::Result<std::uint32_t>(0U)
            : Base::Result<std::uint32_t>(
                OutOfRange(
                    "Editable text byte offset is out of range"));
    }
    const auto* begin = state_->graphemeOffsets.Data();
    const auto* end = begin + state_->graphemeOffsets.Size();
    const auto* it = std::lower_bound(begin, end, byteOffset);
    if (it != end && *it == byteOffset) {
        return static_cast<std::uint32_t>(it - begin);
    }
    return InvalidArgument(
        "Editable text byte offset is not a grapheme boundary");
}

Base::Result<TextRange> EditableTextModel::LineRange(
    std::uint32_t lineIndex) const noexcept {
    if (!state_.has_value()) {
        return lineIndex == 0U
            ? Base::Result<TextRange>(TextRange{})
            : Base::Result<TextRange>(
                OutOfRange(
                    "Editable text line index is out of range"));
    }
    if (lineIndex >= state_->lineStarts.Size()) {
        return OutOfRange(
            "Editable text line index is out of range");
    }
    const std::uint32_t start =
        state_->lineStarts[lineIndex];
    std::uint32_t end =
        lineIndex + 1U < state_->lineStarts.Size()
        ? state_->lineStarts[lineIndex + 1U]
        : state_->GraphemeCount();
    if (end > start) {
        const std::uint32_t byteOffset =
            state_->graphemeOffsets[end - 1U];
        const auto byteAt =
            [&](std::uint32_t index) noexcept {
                return state_->ByteAt(index);
            };
        if (IsNewline(
                Decode(byteOffset, state_->textBytes, byteAt).value)) {
            --end;
        }
    }
    return TextRange{start, end - start};
}

Base::Result<void> EditableTextModel::SetMaximumLength(
    std::uint32_t graphemeCount) noexcept {
    Base::Result<void> state = EnsureState();
    if (!state) {
        return state;
    }
    if (state_->GraphemeCount() > graphemeCount) {
        return OutOfRange(
            "Maximum length is shorter than current text");
    }
    state_->maximumLength = graphemeCount;
    return {};
}

std::uint32_t
EditableTextModel::MaximumLength() const noexcept {
    return state_.has_value()
        ? state_->maximumLength
        : UINT32_MAX;
}

Base::Result<void> EditableTextModel::SetReadOnly(
    bool value) noexcept {
    Base::Result<void> state = EnsureState();
    if (!state) {
        return state;
    }
    state_->readOnly = value;
    return {};
}

bool EditableTextModel::IsReadOnly() const noexcept {
    return state_.has_value() && state_->readOnly;
}

bool EditableTextModel::CanUndo() const noexcept {
    return state_.has_value() && !state_->undo.Empty();
}

bool EditableTextModel::CanRedo() const noexcept {
    return state_.has_value() && !state_->redo.Empty();
}

Base::Result<void> EditableTextModel::Undo() noexcept {
    if (!state_.has_value() || state_->undo.Empty()) {
        return NotFound(
            "Editable text undo history is empty");
    }
    if (state_->readOnly) {
        return InvalidState(
            "Editable text model is read-only");
    }
    state_->redo.Reserve(
            std::min(
                HistoryLimit,
                state_->redo.Size() + 1U));
    EditableTextState::EditRecord& record = state_->undo.Back();
    const TextSelection selection = record.before;
    Base::Result<void> undone = state_->Replace(
        {record.start, record.insertedGraphemes},
        record.removed.View(),
        false,
        &selection);
    if (!undone) {
        return undone;
    }
    EditableTextState::EditRecord moved = std::move(record);
    state_->undo.PopBack();
    state_->TrimHistory(state_->redo);
    state_->redo.PushBack(std::move(moved));
    return {};
}

Base::Result<void> EditableTextModel::Redo() noexcept {
    if (!state_.has_value() || state_->redo.Empty()) {
        return NotFound(
            "Editable text redo history is empty");
    }
    if (state_->readOnly) {
        return InvalidState(
            "Editable text model is read-only");
    }
    state_->undo.Reserve(
            std::min(
                HistoryLimit,
                state_->undo.Size() + 1U));
    EditableTextState::EditRecord& record = state_->redo.Back();
    const TextSelection selection = record.after;
    Base::Result<void> redone = state_->Replace(
        {record.start, record.removedGraphemes},
        record.inserted.View(),
        false,
        &selection);
    if (!redone) {
        return redone;
    }
    EditableTextState::EditRecord moved = std::move(record);
    state_->redo.PopBack();
    state_->TrimHistory(state_->undo);
    state_->undo.PushBack(std::move(moved));
    return {};
}

void EditableTextModel::ClearHistory() noexcept {
    if (state_.has_value()) {
        state_->undo.Clear();
        state_->redo.Clear();
    }
}

} // namespace Aero::Text
