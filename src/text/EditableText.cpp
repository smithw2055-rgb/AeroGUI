#include <Aero/Text/EditableText.hpp>

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
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

Base::Status OutOfMemory(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfMemory, message);
}

Base::Status OutOfRange(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfRange, message);
}

Base::Status NotFound(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotFound, message);
}

struct DecodedCodePoint final {
    std::uint32_t value = 0U;
    std::uint32_t length = 0U;
};

template <typename ByteAccessor>
DecodedCodePoint Decode(
    std::uint32_t offset,
    ByteAccessor&& byteAt) noexcept {
    const std::uint8_t lead = byteAt(offset);
    if (lead <= 0x7FU) {
        return {lead, 1U};
    }
    std::uint32_t length = 0U;
    std::uint32_t value = 0U;
    if ((lead & 0xE0U) == 0xC0U) {
        length = 2U;
        value = lead & 0x1FU;
    } else if ((lead & 0xF0U) == 0xE0U) {
        length = 3U;
        value = lead & 0x0FU;
    } else {
        length = 4U;
        value = lead & 0x07U;
    }
    for (std::uint32_t index = 1U; index < length; ++index) {
        value = (value << 6U) |
            (byteAt(offset + index) & 0x3FU);
    }
    return {value, length};
}

bool IsCombiningMark(std::uint32_t codePoint) noexcept {
    return
        (codePoint >= 0x0300U && codePoint <= 0x036FU) ||
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

bool ShouldBreak(
    std::uint32_t previous,
    std::uint32_t current,
    std::uint32_t regionalIndicatorRun) noexcept {
    if (previous == 0x000DU && current == 0x000AU) {
        return false;
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
        const DecodedCodePoint decoded = Decode(offset, byteAt);
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

bool IsNewline(std::uint32_t codePoint) noexcept {
    return codePoint == 0x000AU ||
        codePoint == 0x000DU;
}

} // namespace

struct EditableTextModel::Impl final {
    struct EditRecord final {
        explicit EditRecord(
            Base::IAllocator* allocator = nullptr) noexcept
            : removed(allocator),
              inserted(allocator) {}

        std::uint32_t start = 0U;
        std::uint32_t removedGraphemes = 0U;
        std::uint32_t insertedGraphemes = 0U;
        Base::String removed;
        Base::String inserted;
        TextSelection before;
        TextSelection after;
    };

    explicit Impl(Base::IAllocator* allocator) noexcept
        : bytes(allocator),
          graphemeOffsets(allocator),
          lineStarts(allocator),
          undo(allocator),
          redo(allocator) {}

    Base::Vector<char> bytes;
    Base::Vector<std::uint32_t> graphemeOffsets;
    Base::Vector<std::uint32_t> lineStarts;
    Base::Vector<EditRecord> undo;
    Base::Vector<EditRecord> redo;
    std::uint32_t gapBegin = 0U;
    std::uint32_t gapEnd = 0U;
    std::uint32_t textBytes = 0U;
    std::uint32_t codePoints = 0U;
    std::uint32_t maximumLength = UINT32_MAX;
    TextSelection selection;
    std::uint64_t revision = 0U;
    bool readOnly = false;

    std::uint32_t GapSize() const noexcept {
        return gapEnd - gapBegin;
    }

    std::uint8_t ByteAt(std::uint32_t offset) const noexcept {
        const std::uint32_t physical =
            offset < gapBegin
            ? offset
            : offset + GapSize();
        return static_cast<std::uint8_t>(bytes[physical]);
    }

    std::uint32_t GraphemeCount() const noexcept {
        return graphemeOffsets.Empty()
            ? 0U
            : graphemeOffsets.Size() - 1U;
    }

    Base::Result<void> EnsureIndexCapacity(
        std::uint32_t newTextBytes) noexcept {
        if (newTextBytes == UINT32_MAX) {
            return OutOfRange(
                "Editable text size exceeds index capacity");
        }
        Base::Result<void> graphemeCapacity =
            graphemeOffsets.TryReserve(newTextBytes + 1U);
        if (!graphemeCapacity) {
            return graphemeCapacity;
        }
        return lineStarts.TryReserve(newTextBytes + 1U);
    }

    Base::Result<void> EnsureGap(
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
        Base::Result<void> resized =
            replacement.TryResize(capacity);
        if (!resized) {
            return resized;
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

    Base::Result<void> MoveGap(
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

    Base::Result<void> RebuildIndexes() noexcept {
        graphemeOffsets.Clear();
        lineStarts.Clear();
        Base::Result<void> start =
            graphemeOffsets.TryPushBack(0U);
        if (start) {
            start = lineStarts.TryPushBack(0U);
        }
        if (!start) {
            return start;
        }
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
        bool first = true;
        while (offset < textBytes) {
            const DecodedCodePoint decoded =
                Decode(offset, byteAt);
            const bool breakBefore = first ||
                ShouldBreak(
                    previous,
                    decoded.value,
                    regionalRun);
            if (breakBefore && !first) {
                Base::Result<void> boundary =
                    graphemeOffsets.TryPushBack(offset);
                if (!boundary) {
                    return boundary;
                }
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
        Base::Result<void> end =
            graphemeOffsets.TryPushBack(textBytes);
        if (!end) {
            return end;
        }
        codePoints = points;

        for (std::uint32_t grapheme = 0U;
             grapheme < GraphemeCount();
             ++grapheme) {
            const std::uint32_t clusterOffset =
                graphemeOffsets[grapheme];
            const DecodedCodePoint decoded =
                Decode(clusterOffset, byteAt);
            if (IsNewline(decoded.value)) {
                Base::Result<void> line =
                    lineStarts.TryPushBack(grapheme + 1U);
                if (!line) {
                    return line;
                }
            }
        }
        return {};
    }

    Base::Result<void> CopyByteRange(
        std::uint32_t start,
        std::uint32_t count,
        Base::String& output) const noexcept {
        if (start > textBytes ||
            count > textBytes - start) {
            return OutOfRange(
                "Editable text byte range is out of bounds");
        }
        output.Clear();
        Base::Result<void> reserved =
            output.TryReserve(count);
        if (!reserved) {
            return reserved;
        }
        if (count == 0U) {
            return {};
        }
        if (start < gapBegin) {
            const std::uint32_t prefix =
                std::min(count, gapBegin - start);
            Base::Result<void> appended =
                output.TryAppendUnchecked({
                    bytes.Data() + start, prefix});
            if (!appended) {
                return appended;
            }
            if (prefix == count) {
                return {};
            }
            return output.TryAppendUnchecked({
                bytes.Data() + gapEnd,
                count - prefix});
        }
        return output.TryAppendUnchecked({
            bytes.Data() + start + GapSize(), count});
    }

    Base::Result<void> ReplaceBytes(
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

    void TrimHistory(
        Base::Vector<EditRecord>& history) noexcept {
        if (history.Size() < HistoryLimit) {
            return;
        }
        for (std::uint32_t index = 1U;
             index < history.Size();
             ++index) {
            history[index - 1U] =
                std::move(history[index]);
        }
        history.PopBack();
    }

    Base::Result<void> Replace(
        TextRange range,
        Base::StringView replacement,
        bool recordHistory,
        const TextSelection* forcedSelection = nullptr) noexcept {
        const std::uint32_t currentGraphemes =
            GraphemeCount();
        if (range.start > currentGraphemes ||
            range.length >
                currentGraphemes - range.start) {
            return OutOfRange(
                "Editable text range is out of bounds");
        }
        Base::Result<std::uint32_t> insertedCount =
            CountGraphemes(replacement);
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
            graphemeOffsets[range.End()];
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
                record.inserted.TryAssign(replacement);
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
            Base::Result<void> history =
                undo.TryReserve(
                    std::min(
                        HistoryLimit,
                        undo.Size() + 1U));
            if (!history) {
                return history;
            }
        }

        Base::Result<void> replaced =
            ReplaceBytes(
                byteStart, removedBytes, replacement);
        if (!replaced) {
            return replaced;
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
            Base::Result<void> appended =
                undo.TryPushBack(std::move(record));
            if (!appended) {
                return appended;
            }
        }
        return {};
    }
};

EditableTextModel::EditableTextModel(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {}

EditableTextModel::~EditableTextModel() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    impl_->~Impl();
    allocator_->Deallocate(
        impl_,
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::General);
}

Base::Result<void> EditableTextModel::EnsureImpl() noexcept {
    if (impl_ != nullptr) {
        return {};
    }
    void* memory = allocator_->Allocate({
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::General});
    if (memory == nullptr) {
        return OutOfMemory(
            "Failed to allocate editable text state");
    }
    impl_ = new (memory) Impl(allocator_);
    Base::Result<void> indexes =
        impl_->EnsureIndexCapacity(0U);
    if (indexes) {
        indexes = impl_->RebuildIndexes();
    }
    if (!indexes) {
        impl_->~Impl();
        allocator_->Deallocate(
            impl_,
            sizeof(Impl),
            alignof(Impl),
            Base::MemoryTag::General);
        impl_ = nullptr;
        return indexes;
    }
    return {};
}

Base::Result<void> EditableTextModel::SetText(
    Base::StringView text) noexcept {
    Base::Result<std::uint32_t> graphemes =
        CountGraphemes(text);
    if (!graphemes) {
        return graphemes.GetStatus();
    }
    Base::Result<void> state = EnsureImpl();
    if (!state) {
        return state;
    }
    if (graphemes.Value() > impl_->maximumLength) {
        return OutOfRange(
            "Editable text exceeds maximum length");
    }
    Base::Result<void> indexes =
        impl_->EnsureIndexCapacity(text.SizeBytes());
    if (!indexes) {
        return indexes;
    }
    if (text.SizeBytes() >
        UINT32_MAX - InitialGapBytes) {
        return OutOfRange(
            "Editable text exceeds byte capacity");
    }
    Base::Vector<char> replacement(allocator_);
    Base::Result<void> resized =
        replacement.TryResize(
            text.SizeBytes() + InitialGapBytes);
    if (!resized) {
        return resized;
    }
    if (text.SizeBytes() != 0U) {
        std::memcpy(
            replacement.Data(),
            text.Data(),
            text.SizeBytes());
    }
    impl_->bytes = std::move(replacement);
    impl_->gapBegin = text.SizeBytes();
    impl_->gapEnd = impl_->bytes.Size();
    impl_->textBytes = text.SizeBytes();
    Base::Result<void> rebuilt =
        impl_->RebuildIndexes();
    if (!rebuilt) {
        return rebuilt;
    }
    impl_->selection = {
        graphemes.Value(), graphemes.Value()};
    impl_->undo.Clear();
    impl_->redo.Clear();
    ++impl_->revision;
    return {};
}

Base::Result<void> EditableTextModel::Snapshot(
    Base::String& output) const noexcept {
    if (impl_ == nullptr) {
        output.Clear();
        return {};
    }
    return impl_->CopyByteRange(
        0U, impl_->textBytes, output);
}

std::uint32_t EditableTextModel::SizeBytes() const noexcept {
    return impl_ != nullptr ? impl_->textBytes : 0U;
}

std::uint32_t
EditableTextModel::CodePointCount() const noexcept {
    return impl_ != nullptr ? impl_->codePoints : 0U;
}

std::uint32_t
EditableTextModel::GraphemeCount() const noexcept {
    return impl_ != nullptr ? impl_->GraphemeCount() : 0U;
}

std::uint32_t EditableTextModel::LineCount() const noexcept {
    return impl_ != nullptr
        ? impl_->lineStarts.Size()
        : 1U;
}

std::uint64_t EditableTextModel::Revision() const noexcept {
    return impl_ != nullptr ? impl_->revision : 0U;
}

TextSelection EditableTextModel::Selection() const noexcept {
    return impl_ != nullptr
        ? impl_->selection
        : TextSelection{};
}

std::uint32_t EditableTextModel::Caret() const noexcept {
    return Selection().caret;
}

Base::Result<void> EditableTextModel::SetSelection(
    std::uint32_t anchor,
    std::uint32_t caret) noexcept {
    Base::Result<void> state = EnsureImpl();
    if (!state) {
        return state;
    }
    if (anchor > impl_->GraphemeCount() ||
        caret > impl_->GraphemeCount()) {
        return OutOfRange(
            "Editable text selection is out of range");
    }
    impl_->selection = {anchor, caret};
    return {};
}

Base::Result<void> EditableTextModel::SelectAll() noexcept {
    Base::Result<void> state = EnsureImpl();
    if (!state) {
        return state;
    }
    impl_->selection = {0U, impl_->GraphemeCount()};
    return {};
}

Base::Result<void> EditableTextModel::ReplaceRange(
    TextRange range,
    Base::StringView replacement) noexcept {
    Base::Result<void> state = EnsureImpl();
    if (!state) {
        return state;
    }
    if (impl_->readOnly) {
        return InvalidState(
            "Editable text model is read-only");
    }
    return impl_->Replace(range, replacement, true);
}

Base::Result<void> EditableTextModel::ReplaceSelection(
    Base::StringView replacement) noexcept {
    const TextSelection selection = Selection();
    return ReplaceRange(
        {selection.Start(), selection.Length()},
        replacement);
}

Base::Result<void> EditableTextModel::DeleteBackward() noexcept {
    const TextSelection selection = Selection();
    if (!selection.Empty()) {
        return ReplaceRange(
            {selection.Start(), selection.Length()},
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
    if (!selection.Empty()) {
        return ReplaceRange(
            {selection.Start(), selection.Length()},
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
    if (impl_ == nullptr) {
        return graphemeIndex == 0U
            ? Base::Result<std::uint32_t>(0U)
            : Base::Result<std::uint32_t>(
                OutOfRange(
                    "Editable text grapheme index is out of range"));
    }
    if (graphemeIndex > impl_->GraphemeCount()) {
        return OutOfRange(
            "Editable text grapheme index is out of range");
    }
    return impl_->graphemeOffsets[graphemeIndex];
}

Base::Result<std::uint32_t>
EditableTextModel::GraphemeIndexForByteOffset(
    std::uint32_t byteOffset) const noexcept {
    if (impl_ == nullptr) {
        return byteOffset == 0U
            ? Base::Result<std::uint32_t>(0U)
            : Base::Result<std::uint32_t>(
                OutOfRange(
                    "Editable text byte offset is out of range"));
    }
    for (std::uint32_t index = 0U;
         index < impl_->graphemeOffsets.Size();
         ++index) {
        if (impl_->graphemeOffsets[index] == byteOffset) {
            return index;
        }
    }
    return InvalidArgument(
        "Editable text byte offset is not a grapheme boundary");
}

Base::Result<TextRange> EditableTextModel::LineRange(
    std::uint32_t lineIndex) const noexcept {
    if (impl_ == nullptr) {
        return lineIndex == 0U
            ? Base::Result<TextRange>(TextRange{})
            : Base::Result<TextRange>(
                OutOfRange(
                    "Editable text line index is out of range"));
    }
    if (lineIndex >= impl_->lineStarts.Size()) {
        return OutOfRange(
            "Editable text line index is out of range");
    }
    const std::uint32_t start =
        impl_->lineStarts[lineIndex];
    std::uint32_t end =
        lineIndex + 1U < impl_->lineStarts.Size()
        ? impl_->lineStarts[lineIndex + 1U]
        : impl_->GraphemeCount();
    if (end > start) {
        const std::uint32_t byteOffset =
            impl_->graphemeOffsets[end - 1U];
        const auto byteAt =
            [&](std::uint32_t index) noexcept {
                return impl_->ByteAt(index);
            };
        if (IsNewline(
                Decode(byteOffset, byteAt).value)) {
            --end;
        }
    }
    return TextRange{start, end - start};
}

Base::Result<void> EditableTextModel::SetMaximumLength(
    std::uint32_t graphemeCount) noexcept {
    Base::Result<void> state = EnsureImpl();
    if (!state) {
        return state;
    }
    if (impl_->GraphemeCount() > graphemeCount) {
        return OutOfRange(
            "Maximum length is shorter than current text");
    }
    impl_->maximumLength = graphemeCount;
    return {};
}

std::uint32_t
EditableTextModel::MaximumLength() const noexcept {
    return impl_ != nullptr
        ? impl_->maximumLength
        : UINT32_MAX;
}

Base::Result<void> EditableTextModel::SetReadOnly(
    bool value) noexcept {
    Base::Result<void> state = EnsureImpl();
    if (!state) {
        return state;
    }
    impl_->readOnly = value;
    return {};
}

bool EditableTextModel::IsReadOnly() const noexcept {
    return impl_ != nullptr && impl_->readOnly;
}

bool EditableTextModel::CanUndo() const noexcept {
    return impl_ != nullptr && !impl_->undo.Empty();
}

bool EditableTextModel::CanRedo() const noexcept {
    return impl_ != nullptr && !impl_->redo.Empty();
}

Base::Result<void> EditableTextModel::Undo() noexcept {
    if (impl_ == nullptr || impl_->undo.Empty()) {
        return NotFound(
            "Editable text undo history is empty");
    }
    if (impl_->readOnly) {
        return InvalidState(
            "Editable text model is read-only");
    }
    Base::Result<void> capacity =
        impl_->redo.TryReserve(
            std::min(
                HistoryLimit,
                impl_->redo.Size() + 1U));
    if (!capacity) {
        return capacity;
    }
    Impl::EditRecord& record = impl_->undo.Back();
    const TextSelection selection = record.before;
    Base::Result<void> undone = impl_->Replace(
        {record.start, record.insertedGraphemes},
        record.removed.View(),
        false,
        &selection);
    if (!undone) {
        return undone;
    }
    Impl::EditRecord moved = std::move(record);
    impl_->undo.PopBack();
    impl_->TrimHistory(impl_->redo);
    return impl_->redo.TryPushBack(std::move(moved));
}

Base::Result<void> EditableTextModel::Redo() noexcept {
    if (impl_ == nullptr || impl_->redo.Empty()) {
        return NotFound(
            "Editable text redo history is empty");
    }
    if (impl_->readOnly) {
        return InvalidState(
            "Editable text model is read-only");
    }
    Base::Result<void> capacity =
        impl_->undo.TryReserve(
            std::min(
                HistoryLimit,
                impl_->undo.Size() + 1U));
    if (!capacity) {
        return capacity;
    }
    Impl::EditRecord& record = impl_->redo.Back();
    const TextSelection selection = record.after;
    Base::Result<void> redone = impl_->Replace(
        {record.start, record.removedGraphemes},
        record.inserted.View(),
        false,
        &selection);
    if (!redone) {
        return redone;
    }
    Impl::EditRecord moved = std::move(record);
    impl_->redo.PopBack();
    impl_->TrimHistory(impl_->undo);
    return impl_->undo.TryPushBack(std::move(moved));
}

void EditableTextModel::ClearHistory() noexcept {
    if (impl_ != nullptr) {
        impl_->undo.Clear();
        impl_->redo.Clear();
    }
}

} // namespace Aero::Text
