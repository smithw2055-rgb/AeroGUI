#pragma once

#include <Aero/Controls/Core.hpp>
#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>

#include <cstdint>

namespace Aero::Text::Detail {

// UTF-8 gap-buffer editor model. Public positions are extended grapheme
// cluster indices; UTF-8 bytes and code-point counts remain queryable for
// platform text adapters and diagnostics.
class AERO_API EditableTextModel {
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
    struct Impl;

    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;

    Base::Result<void> EnsureImpl() noexcept;
};

} // namespace Aero::Text::Detail
