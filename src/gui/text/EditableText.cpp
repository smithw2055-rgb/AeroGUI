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

bool CountGraphemes(
    Base::StringView text,
    std::uint32_t& outGraphemes,
    std::uint32_t* codePointCount = nullptr) noexcept {
    const Base::Utf8Validation validation =
        Base::ValidateUtf8(text);
    if (!validation.valid) {
        return false;
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
    outGraphemes = graphemes;
    return true;
}

} // namespace

std::uint8_t EditableTextState::ByteAt(std::uint32_t offset) const noexcept {
    const std::uint32_t physical =
        offset < gapBegin
        ? offset
        : offset + GapSize();
    return static_cast<std::uint8_t>(bytes[physical]);
}

void EditableTextState::EnsureIndexCapacity(
    std::uint32_t newTextBytes) noexcept {
    if (newTextBytes == UINT32_MAX) {
        return;
    }
    graphemeOffsets.Reserve(newTextBytes + 1U);
    lineStarts.Reserve(newTextBytes + 1U);
}

void EditableTextState::EnsureGap(
    std::uint32_t requiredBytes) noexcept {
    if (GapSize() >= requiredBytes) {
        return;
    }
    if (requiredBytes >
        UINT32_MAX - textBytes - InitialGapBytes) {
        return;
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
        return;
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
}

void EditableTextState::MoveGap(
    std::uint32_t byteOffset) noexcept {
    if (byteOffset > textBytes) {
        byteOffset = textBytes;
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
}

void EditableTextState::RebuildIndexes() noexcept {
    graphemeOffsets.Clear();
    lineStarts.Clear();
    graphemeOffsets.PushBack(0U);
    lineStarts.PushBack(0U);
    if (textBytes == 0U) {
        codePoints = 0U;
        return;
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
}

void EditableTextState::CopyByteRange(
    std::uint32_t start,
    std::uint32_t count,
    Base::String& output) const noexcept {
    if (start > textBytes) {
        output.Clear();
        return;
    }
    if (count > textBytes - start) {
        count = textBytes - start;
    }
    output.Clear();
    output.Reserve(count);
    if (count == 0U) {
        return;
    }
    if (start < gapBegin) {
        const std::uint32_t prefix =
            std::min(count, gapBegin - start);
        output.AppendUnchecked({
            bytes.Data() + start, prefix});
        if (prefix == count) {
            return;
        }
        output.AppendUnchecked({
            bytes.Data() + gapEnd,
            count - prefix});
        return;
    }
    output.AppendUnchecked({
        bytes.Data() + start + GapSize(), count});
}

void EditableTextState::ReplaceBytes(
    std::uint32_t byteStart,
    std::uint32_t removedBytes,
    Base::StringView replacement) noexcept {
    MoveGap(byteStart);
    gapEnd += removedBytes;
    textBytes -= removedBytes;
    EnsureGap(replacement.SizeBytes());
    if (replacement.SizeBytes() != 0U) {
        std::memcpy(
            bytes.Data() + gapBegin,
            replacement.Data(),
            replacement.SizeBytes());
        gapBegin += replacement.SizeBytes();
        textBytes += replacement.SizeBytes();
    }
    RebuildIndexes();
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

    MoveGap(byteStart);
    gapEnd += removedBytes;
    textBytes -= removedBytes;
    EnsureGap(replacement.SizeBytes());
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

bool EditableTextState::Replace(
    TextRange range,
    Base::StringView replacement,
    bool recordHistory,
    const TextSelection* forcedSelection) noexcept {
    const std::uint32_t currentGraphemes =
        GraphemeCount();
    if (range.start > currentGraphemes ||
        range.length >
            currentGraphemes - range.start) {
        return false;
    }
    std::uint32_t insertedPoints = 0U;
    std::uint32_t insertedCount = 0U;
    if (!CountGraphemes(replacement, insertedCount, &insertedPoints)) {
        return false;
    }
    const std::uint32_t retained =
        currentGraphemes - range.length;
    if (insertedCount >
        maximumLength - retained) {
        return false;
    }
    const std::uint32_t byteStart =
        graphemeOffsets[range.start];
    const std::uint32_t byteEnd =
        graphemeOffsets[range.GetEnd()];
    const std::uint32_t removedBytes =
        byteEnd - byteStart;
    if (replacement.SizeBytes() >
        UINT32_MAX - (textBytes - removedBytes)) {
        return false;
    }
    const std::uint32_t newTextBytes =
        textBytes - removedBytes +
        replacement.SizeBytes();
    EnsureIndexCapacity(newTextBytes);
    EnsureGap(replacement.SizeBytes());

    EditRecord record(&bytes.Allocator());
    if (recordHistory) {
        CopyByteRange(
            byteStart, removedBytes, record.removed);
        record.inserted.Assign(replacement);
        record.start = range.start;
        record.removedGraphemes = range.length;
        record.insertedGraphemes = insertedCount;
        record.before = selection;
        record.after = {
            range.start + insertedCount,
            range.start + insertedCount};
        undo.Reserve(
            std::min(
                HistoryLimit,
                undo.Size() + 1U));
    }

    const bool incremental = TryIncrementalReplace(
        range, byteStart, removedBytes, replacement,
        insertedCount, insertedPoints);
    if (!incremental) {
        ReplaceBytes(
            byteStart, removedBytes, replacement);
    }
    selection = forcedSelection != nullptr
        ? *forcedSelection
        : TextSelection{
            range.start + insertedCount,
            range.start + insertedCount};
    ++revision;
    if (recordHistory) {
        redo.Clear();
        TrimHistory(undo);
        undo.PushBack(std::move(record));
    }
    return true;
}

EditableTextModel::EditableTextModel(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {}

EditableTextModel::~EditableTextModel() noexcept = default;

void EditableTextModel::EnsureState() noexcept {
    if (state_.has_value()) {
        return;
    }
    state_.emplace(allocator_);
    state_->EnsureIndexCapacity(0U);
    state_->RebuildIndexes();
}

bool EditableTextModel::SetText(
    Base::StringView text) noexcept {
    const Base::Utf8Validation validation =
        Base::ValidateUtf8(text);
    if (!validation.valid) {
        return false;
    }
    EnsureState();
    if (!state_.has_value()) {
        return false;
    }
    state_->EnsureIndexCapacity(text.SizeBytes());
    if (text.SizeBytes() >
        UINT32_MAX - InitialGapBytes) {
        return false;
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
    state_->RebuildIndexes();
    if (state_->GraphemeCount() > state_->maximumLength) {
        state_->bytes.Clear();
        state_->textBytes = 0U;
        state_->gapBegin = 0U;
        state_->gapEnd = 0U;
        state_->RebuildIndexes();
        return false;
    }
    const std::uint32_t graphemeCount = state_->GraphemeCount();
    state_->selection = {
        graphemeCount, graphemeCount};
    state_->undo.Clear();
    state_->redo.Clear();
    ++state_->revision;
    return true;
}

void EditableTextModel::Snapshot(
    Base::String& output) const noexcept {
    if (!state_.has_value()) {
        output.Clear();
        return;
    }
    state_->CopyByteRange(
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

void EditableTextModel::SetSelection(
    std::uint32_t anchor,
    std::uint32_t caret) noexcept {
    EnsureState();
    if (!state_.has_value()) {
        return;
    }
    const std::uint32_t maxGraphemes = state_->GraphemeCount();
    state_->selection = {
        std::min(anchor, maxGraphemes),
        std::min(caret, maxGraphemes)};
}

void EditableTextModel::SelectAll() noexcept {
    EnsureState();
    if (!state_.has_value()) {
        return;
    }
    state_->selection = {0U, state_->GraphemeCount()};
}

bool EditableTextModel::ReplaceRange(
    TextRange range,
    Base::StringView replacement) noexcept {
    EnsureState();
    if (!state_.has_value() || state_->readOnly) {
        return false;
    }
    return state_->Replace(range, replacement, true);
}

bool EditableTextModel::ReplaceSelection(
    Base::StringView replacement) noexcept {
    const TextSelection selection = Selection();
    return ReplaceRange(
        {selection.GetStart(), selection.GetLength()},
        replacement);
}

bool EditableTextModel::DeleteBackward() noexcept {
    const TextSelection selection = Selection();
    if (!selection.GetIsEmpty()) {
        return ReplaceRange(
            {selection.GetStart(), selection.GetLength()},
            {});
    }
    if (selection.caret == 0U) {
        return false;
    }
    return ReplaceRange(
        {selection.caret - 1U, 1U}, {});
}

bool EditableTextModel::DeleteForward() noexcept {
    const TextSelection selection = Selection();
    if (!selection.GetIsEmpty()) {
        return ReplaceRange(
            {selection.GetStart(), selection.GetLength()},
            {});
    }
    if (selection.caret >= GraphemeCount()) {
        return false;
    }
    return ReplaceRange(
        {selection.caret, 1U}, {});
}

std::uint32_t
EditableTextModel::ByteOffsetForGrapheme(
    std::uint32_t graphemeIndex) const noexcept {
    if (!state_.has_value() || state_->graphemeOffsets.Empty()) {
        return 0U;
    }
    if (graphemeIndex >= state_->graphemeOffsets.Size()) {
        return state_->textBytes;
    }
    return state_->graphemeOffsets[graphemeIndex];
}

std::uint32_t
EditableTextModel::GraphemeIndexForByteOffset(
    std::uint32_t byteOffset) const noexcept {
    if (!state_.has_value() || state_->graphemeOffsets.Empty()) {
        return 0U;
    }
    const auto* begin = state_->graphemeOffsets.Data();
    const auto* end = begin + state_->graphemeOffsets.Size();
    const auto* it = std::lower_bound(begin, end, byteOffset);
    if (it != end) {
        return static_cast<std::uint32_t>(it - begin);
    }
    return state_->GraphemeCount();
}

TextRange EditableTextModel::LineRange(
    std::uint32_t lineIndex) const noexcept {
    if (!state_.has_value() || lineIndex >= state_->lineStarts.Size()) {
        return TextRange{};
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

bool EditableTextModel::SetMaximumLength(
    std::uint32_t graphemeCount) noexcept {
    EnsureState();
    if (!state_.has_value()) {
        return false;
    }
    if (state_->GraphemeCount() > graphemeCount) {
        return false;
    }
    state_->maximumLength = graphemeCount;
    return true;
}

std::uint32_t
EditableTextModel::MaximumLength() const noexcept {
    return state_.has_value()
        ? state_->maximumLength
        : UINT32_MAX;
}

void EditableTextModel::SetReadOnly(
    bool value) noexcept {
    EnsureState();
    if (!state_.has_value()) {
        return;
    }
    state_->readOnly = value;
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

bool EditableTextModel::Undo() noexcept {
    if (!state_.has_value() || state_->undo.Empty() || state_->readOnly) {
        return false;
    }
    state_->redo.Reserve(
            std::min(
                HistoryLimit,
                state_->redo.Size() + 1U));
    EditableTextState::EditRecord& record = state_->undo.Back();
    const TextSelection selection = record.before;
    if (!state_->Replace(
        {record.start, record.insertedGraphemes},
        record.removed.View(),
        false,
        &selection)) {
        return false;
    }
    EditableTextState::EditRecord moved = std::move(record);
    state_->undo.PopBack();
    state_->TrimHistory(state_->redo);
    state_->redo.PushBack(std::move(moved));
    return true;
}

bool EditableTextModel::Redo() noexcept {
    if (!state_.has_value() || state_->redo.Empty() || state_->readOnly) {
        return false;
    }
    state_->undo.Reserve(
            std::min(
                HistoryLimit,
                state_->undo.Size() + 1U));
    EditableTextState::EditRecord& record = state_->redo.Back();
    const TextSelection selection = record.after;
    if (!state_->Replace(
        {record.start, record.removedGraphemes},
        record.inserted.View(),
        false,
        &selection)) {
        return false;
    }
    EditableTextState::EditRecord moved = std::move(record);
    state_->redo.PopBack();
    state_->TrimHistory(state_->undo);
    state_->undo.PushBack(std::move(moved));
    return true;
}

void EditableTextModel::ClearHistory() noexcept {
    if (state_.has_value()) {
        state_->undo.Clear();
        state_->redo.Clear();
    }
}

} // namespace Aero::Text
