#pragma once

// Internal runtime type ownership. Installed headers use these aliases only to
// preserve source spelling for opaque pointers and runtime-only signatures.
// Complete manager definitions remain under src/ and are not SDK authoring API.
namespace Aero::Controls {
class Control;
}

namespace Aero::Detail {

class PresentationRuntimeAccess final {
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

namespace Aero::Presentation {

using RoutedEventManager =
    Aero::Detail::PresentationRuntimeAccess::RoutedEventManager;
using CommandManager =
    Aero::Detail::PresentationRuntimeAccess::CommandManager;
using HitTestManager =
    Aero::Detail::PresentationRuntimeAccess::HitTestManager;
using PointerInputManager =
    Aero::Detail::PresentationRuntimeAccess::PointerInputManager;
using FocusManager =
    Aero::Detail::PresentationRuntimeAccess::FocusManager;
using KeyboardInputManager =
    Aero::Detail::PresentationRuntimeAccess::KeyboardInputManager;
using TextInputManager =
    Aero::Detail::PresentationRuntimeAccess::TextInputManager;
using LayoutManager =
    Aero::Detail::PresentationRuntimeAccess::LayoutManager;
using BindingManager =
    Aero::Detail::PresentationRuntimeAccess::BindingManager;
using AnimationManager =
    Aero::Detail::PresentationRuntimeAccess::AnimationManager;
using StyleManager =
    Aero::Detail::PresentationRuntimeAccess::StyleManager;
using ThemeStyleManager =
    Aero::Detail::PresentationRuntimeAccess::ThemeStyleManager;

} // namespace Aero::Presentation

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
        Presentation::RoutedEventManager* events) noexcept;
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
