#pragma once

namespace Aero {
class ElementTree;
class UIElement;

namespace Controls {
void AttachOwnedContentSubtree(::Aero::ElementTree& tree, ::Aero::UIElement& parent) noexcept;
} // namespace Controls
} // namespace Aero
