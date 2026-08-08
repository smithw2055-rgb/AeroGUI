#pragma once

#include <Aero/TextFormatting.hpp>

namespace Aero::Controls {

// Inline formatting value shared by retained text controls and the
// Aero::Documents inline hierarchy.
enum class TextDecorations : std::uint8_t {
    None = 0U,
    Underline
};

} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::TextDecorations)
