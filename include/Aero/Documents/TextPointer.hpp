#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>

#include <cstdint>

namespace Aero::Controls { class TextBlock; struct TextBlockDocumentHelper; }

namespace Aero::Documents {

class TextRange;

enum class LogicalDirection : std::uint8_t {
    Backward = 0U,
    Forward,
};

// Borrowed text position in a formatted text container. Storage offsets remain
// private so the public contract is independent of the engine's UTF encoding.
class AERO_GUI_API TextPointer {
public:
    TextPointer() noexcept = default;
    bool GetIsValid() const noexcept { return container_ != nullptr; }
    Controls::TextBlock* GetTextContainer() const noexcept { return container_; }
    LogicalDirection GetLogicalDirection() const noexcept { return direction_; }
    bool GetIsAtInsertionPosition() const noexcept { return GetIsValid(); }

    Result<std::int32_t> CompareTo(
        const TextPointer& other) const noexcept;
    Result<TextPointer> GetPositionAtOffset(
        std::int32_t delta,
        LogicalDirection direction) const noexcept;
    friend bool operator==(
        const TextPointer& left,
        const TextPointer& right) noexcept {
        return left.container_ == right.container_ &&
            left.offset_ == right.offset_ &&
            left.direction_ == right.direction_;
    }
    friend bool operator!=(
        const TextPointer& left,
        const TextPointer& right) noexcept {
        return !(left == right);
    }

private:
    friend class Aero::Controls::TextBlock;
    friend class TextRange;
#if defined(AERO_GUI_IMPLEMENTATION)
    friend struct Aero::Controls::TextBlockDocumentHelper;
#endif
    TextPointer(
        Controls::TextBlock& container,
        std::uint32_t offset,
        LogicalDirection direction) noexcept
        : container_(&container), offset_(offset), direction_(direction) {}

    Controls::TextBlock* container_ = nullptr;
    std::uint32_t offset_ = 0U;
    LogicalDirection direction_ = LogicalDirection::Forward;
};

} // namespace Aero::Documents
