#pragma once

#include <Aero/Base/String.hpp>
#include <Aero/Documents/TextPointer.hpp>

namespace Aero::Documents {

class AERO_GUI_API TextRange {
public:
    TextRange() noexcept = default;
    static Result<TextRange> Create(
        TextPointer start,
        TextPointer end) noexcept;

    bool GetIsValid() const noexcept {
        return start_.GetIsValid() && end_.GetIsValid();
    }
    bool GetIsEmpty() const noexcept {
        return GetIsValid() && start_.offset_ == end_.offset_;
    }
    std::uint32_t GetLength() const noexcept {
        return GetIsValid() ? end_.offset_ - start_.offset_ : 0U;
    }
    const TextPointer& GetStart() const noexcept { return start_; }
    const TextPointer& GetEnd() const noexcept { return end_; }
    Result<String> GetText() const noexcept;
    Result<void> CopyText(String& output) const noexcept;

private:
    TextRange(TextPointer start, TextPointer end) noexcept
        : start_(start), end_(end) {}
    TextPointer start_;
    TextPointer end_;
};

} // namespace Aero::Documents
