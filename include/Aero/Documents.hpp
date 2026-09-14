#pragma once

#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Documents/Bold.hpp>
#include <Aero/Documents/Hyperlink.hpp>
#include <Aero/Documents/Inline.hpp>
#include <Aero/Documents/InlineCollection.hpp>
#include <Aero/Documents/InlineCollectionView.hpp>
#include <Aero/Documents/Italic.hpp>
#include <Aero/Documents/LineBreak.hpp>
#include <Aero/Documents/NavigationService.hpp>
#include <Aero/Documents/Run.hpp>
#include <Aero/Documents/Span.hpp>
#include <Aero/Documents/TextElement.hpp>
#include <Aero/Documents/TextPointer.hpp>
#include <Aero/Documents/TextRange.hpp>
#include <Aero/Documents/Underline.hpp>
#include <Aero/Documents/InlineUIContainer.hpp>
#include <Aero/Documents/Adorner.hpp>
#include <Aero/Documents/AdornerLayer.hpp>
#include <Aero/Documents/AdornerDecorator.hpp>

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
