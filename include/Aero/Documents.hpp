#pragma once

#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Documents/Inlines.hpp>
#include <Aero/Documents/NavigationService.hpp>
#include <Aero/Documents/TextPointer.hpp>
#include <Aero/Documents/TextRange.hpp>
#include <Aero/Documents/Adorners.hpp>

namespace Aero::Meta { class Registration; }

namespace Aero::Documents {

void CopyText(const Controls::TextBlock& container, String& output) noexcept;
TextPointer GetPositionFromPoint(Controls::TextBlock& container, Aero::Base::Point point,
    bool snapToText = true) noexcept;
Aero::Base::Rect GetCharacterRect(const TextPointer& position) noexcept;
AERO_GUI_API void PopulateDocumentsMetadata(::Aero::Meta::Registration& context) noexcept;

} // namespace Aero::Documents
