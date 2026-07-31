#pragma once

// Internal runtime type ownership. Installed headers use these aliases only to
// preserve source spelling for opaque pointers and runtime-only signatures.
// Complete manager definitions remain under src/ and are not SDK authoring API.
namespace Aero::Controls {
class Control;
}

namespace Aero::Detail {

class UiRuntimeAccess final {
public:
    class RoutedEventManager;
    class CommandManager;
    class HitTestManager;
    class PointerInputManager;
    class FocusManager;
    class KeyboardInputManager;
    class TextInputManager;
    class LayoutManager;
    class BindingManager;
    class AnimationManager;
    class StyleManager;
    class ThemeStyleManager;
};

} // namespace Aero::Detail

namespace Aero::Detail {
using RoutedEventManager = UiRuntimeAccess::RoutedEventManager;
using CommandManager = UiRuntimeAccess::CommandManager;
using HitTestManager = UiRuntimeAccess::HitTestManager;
using PointerInputManager = UiRuntimeAccess::PointerInputManager;
using FocusManager = UiRuntimeAccess::FocusManager;
using KeyboardInputManager = UiRuntimeAccess::KeyboardInputManager;
using TextInputManager = UiRuntimeAccess::TextInputManager;
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

    static void Attach(
        Controls::Control& control,
        Aero::Detail::RoutedEventManager* events) noexcept;
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
