#pragma once

#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Documents/Inlines.hpp>
#include <Aero/Documents/NavigationService.hpp>
#include <Aero/Documents/TextPointer.hpp>
#include <Aero/Documents/TextRange.hpp>
#include <Aero/Documents/Adorners.hpp>

namespace Aero::Documents {

Result<void> CopyText(
    const Controls::TextBlock& container,
    String& output) noexcept;
Result<TextPointer> GetPositionFromPoint(
    Controls::TextBlock& container,
    Aero::Base::Point point,
    bool snapToText = true) noexcept;
Result<Aero::Base::Rect> GetCharacterRect(
    const TextPointer& position) noexcept;

} // namespace Aero::Documents
