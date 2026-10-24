#pragma once

#include <Aero/Controls.hpp>
#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace Aero::Text {

struct EditableTextState {
    struct EditRecord {
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

    explicit EditableTextState(Base::IAllocator* allocator) noexcept
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

    std::uint32_t GapSize() const noexcept { return gapEnd - gapBegin; }
    std::uint8_t ByteAt(std::uint32_t offset) const noexcept;
    std::uint32_t GraphemeCount() const noexcept {
        return graphemeOffsets.Empty() ? 0U : graphemeOffsets.Size() - 1U;
    }
    Base::Result<void> EnsureIndexCapacity(std::uint32_t newTextBytes) noexcept;
    Base::Result<void> EnsureGap(std::uint32_t requiredBytes) noexcept;
    Base::Result<void> MoveGap(std::uint32_t byteOffset) noexcept;
    Base::Result<void> RebuildIndexes() noexcept;
    Base::Result<void> CopyByteRange(
        std::uint32_t start,
        std::uint32_t count,
        Base::String& output) const noexcept;
    Base::Result<void> ReplaceBytes(
        std::uint32_t byteStart,
        std::uint32_t removedBytes,
        Base::StringView replacement) noexcept;
    bool TryIncrementalReplace(
        TextRange range,
        std::uint32_t byteStart,
        std::uint32_t removedBytes,
        Base::StringView replacement,
        std::uint32_t insertedCount,
        std::uint32_t insertedCodePoints) noexcept;
    void TrimHistory(Base::Vector<EditRecord>& history) noexcept;
    Base::Result<void> Replace(
        TextRange range,
        Base::StringView replacement,
        bool recordHistory,
        const TextSelection* forcedSelection = nullptr) noexcept;
};

// UTF-8 gap-buffer editor model. Public positions are extended grapheme
// cluster indices; UTF-8 bytes and code-point counts remain queryable for
// platform text adapters and diagnostics.
class EditableTextModel {
public:
    explicit EditableTextModel(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~EditableTextModel() noexcept;

    EditableTextModel(const EditableTextModel&) = delete;
    EditableTextModel& operator=(const EditableTextModel&) = delete;

    Base::Result<void> SetText(Base::StringView text) noexcept;
    Base::Result<void> Snapshot(Base::String& output) const noexcept;

    std::uint32_t SizeBytes() const noexcept;
    std::uint32_t CodePointCount() const noexcept;
    std::uint32_t GraphemeCount() const noexcept;
    std::uint32_t LineCount() const noexcept;
    std::uint64_t Revision() const noexcept;

    TextSelection Selection() const noexcept;
    std::uint32_t Caret() const noexcept;
    Base::Result<void> SetSelection(
        std::uint32_t anchor,
        std::uint32_t caret) noexcept;
    Base::Result<void> SelectAll() noexcept;

    Base::Result<void> ReplaceRange(
        TextRange range,
        Base::StringView replacement) noexcept;
    Base::Result<void> ReplaceSelection(
        Base::StringView replacement) noexcept;
    Base::Result<void> DeleteBackward() noexcept;
    Base::Result<void> DeleteForward() noexcept;

    Base::Result<std::uint32_t> ByteOffsetForGrapheme(
        std::uint32_t graphemeIndex) const noexcept;
    Base::Result<std::uint32_t> GraphemeIndexForByteOffset(
        std::uint32_t byteOffset) const noexcept;
    Base::Result<TextRange> LineRange(
        std::uint32_t lineIndex) const noexcept;

    Base::Result<void> SetMaximumLength(
        std::uint32_t graphemeCount) noexcept;
    std::uint32_t MaximumLength() const noexcept;
    Base::Result<void> SetReadOnly(bool value) noexcept;
    bool IsReadOnly() const noexcept;

    bool CanUndo() const noexcept;
    bool CanRedo() const noexcept;
    Base::Result<void> Undo() noexcept;
    Base::Result<void> Redo() noexcept;
    void ClearHistory() noexcept;

private:
    Base::IAllocator* allocator_ = nullptr;
    std::optional<EditableTextState> state_;

    Base::Result<void> EnsureState() noexcept;
};

} // namespace Aero::Text
