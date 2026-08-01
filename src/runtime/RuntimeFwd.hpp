#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/RoutedEvent.hpp>

// Internal runtime type ownership. Installed headers use these aliases only to
// preserve source spelling for opaque pointers and runtime-only signatures.
// Complete manager definitions remain under src/ and are not SDK authoring API.
namespace Aero {
class UIElement;
}
namespace Aero::Controls {
class Control;
class VisualStateManager;
}

namespace Aero::Detail {

class EventRouter;
class InputService;

class UiRuntimeAccess final {
public:
    class LayoutManager;
    class BindingManager;
    class AnimationManager;
    class StyleManager;
    class ThemeStyleManager;

    static void SetEventRouter(Aero::UIElement& element, EventRouter* router) noexcept;
    static void SetCommandRouter(Aero::UIElement& element, InputService* service) noexcept;
    static Base::Result<void> SetMouseOver(Aero::UIElement& element, bool value) noexcept;
    static Base::Result<void> SetPressed(Aero::UIElement& element, bool value) noexcept;
    static Base::Result<void> SetKeyboardFocused(Aero::UIElement& element, bool value) noexcept;
    static Base::Result<void> SetKeyboardFocusWithin(Aero::UIElement& element, bool value) noexcept;
    static void InvokeHandlers(Aero::UIElement& element, RoutedEventHandle event, RoutedEventArgs& args) noexcept;
};

} // namespace Aero::Detail

namespace Aero::Detail {
using LayoutManager = UiRuntimeAccess::LayoutManager;
using BindingManager = UiRuntimeAccess::BindingManager;
using AnimationManager = UiRuntimeAccess::AnimationManager;
using StyleManager = UiRuntimeAccess::StyleManager;
using ThemeStyleManager = UiRuntimeAccess::ThemeStyleManager;
} // namespace Aero::Detail

namespace Aero::Detail {

class ControlRuntimeAccess final {
public:
    class ControlInteractionManager;
    class HyperlinkInteractionManager;
    class TextBoxInteractionManager;
    class ScrollInteractionManager;
    class SliderInteractionManager;
    class TreeViewInteractionManager;
    class ComboBoxInteractionManager;
    class ListBoxInteractionManager;
    class TemplateManager;
    class MenuInteractionManager;

    static void SetVisualStateManager(Controls::Control& control, Controls::VisualStateManager* visualStates) noexcept;
};

} // namespace Aero::Detail

namespace Aero::Controls {

using ControlInteractionManager =
    Aero::Detail::ControlRuntimeAccess::ControlInteractionManager;
using TextBoxInteractionManager =
    Aero::Detail::ControlRuntimeAccess::TextBoxInteractionManager;
using ScrollInteractionManager =
    Aero::Detail::ControlRuntimeAccess::ScrollInteractionManager;
using SliderInteractionManager =
    Aero::Detail::ControlRuntimeAccess::SliderInteractionManager;
using TreeViewInteractionManager =
    Aero::Detail::ControlRuntimeAccess::TreeViewInteractionManager;
using ComboBoxInteractionManager =
    Aero::Detail::ControlRuntimeAccess::ComboBoxInteractionManager;
using ListBoxInteractionManager =
    Aero::Detail::ControlRuntimeAccess::ListBoxInteractionManager;
using TemplateManager =
    Aero::Detail::ControlRuntimeAccess::TemplateManager;
using MenuInteractionManager =
    Aero::Detail::ControlRuntimeAccess::MenuInteractionManager;

} // namespace Aero::Controls
