#include <Aero/Gui.hpp>
#include <Aero/Gui/View.hpp>
#include <Aero/Gui/Button.hpp>
#include <Aero/Documents.hpp>
#include <Aero/Gui/FrameworkElement.hpp>
#include <Aero/Events.hpp>
#include <Aero/Triggers/Triggers.hpp>

#include <Aero/Gui/Application.hpp>
#include <Aero/Gui/Binding.hpp>
#include <Aero/Gui/BindingBase.hpp>
#include <Aero/Gui/Border.hpp>
#include <Aero/Gui/Brush.hpp>
#include <Aero/Gui/Button.hpp>
#include <Aero/Gui/ButtonBase.hpp>
#include <Aero/Gui/ContentControl.hpp>
#include <Aero/Gui/Control.hpp>
#include <Aero/Gui/ControlTemplate.hpp>
#include <Aero/Gui/DataTemplate.hpp>
#include <Aero/Gui/DependencyObject.hpp>
#include <Aero/Gui/DependencyProperty.hpp>
#include <Aero/Gui/FrameworkContentElement.hpp>
#include <Aero/Gui/FrameworkElement.hpp>
#include <Aero/Gui/Geometry.hpp>
#include <Aero/Gui/Grid.hpp>
#include <Aero/Gui/IRenderer.hpp>
#include <Aero/Gui/ItemsControl.hpp>
#include <Aero/Gui/ListBox.hpp>
#include <Aero/Gui/Panel.hpp>
#include <Aero/Gui/RenderDevice.hpp>
#include <Aero/Gui/RenderTarget.hpp>
#include <Aero/Gui/ResourceDictionary.hpp>
#include <Aero/Gui/RoutedEvent.hpp>
#include <Aero/Gui/StackPanel.hpp>
#include <Aero/Gui/Storyboard.hpp>
#include <Aero/Gui/Style.hpp>
#include <Aero/Gui/TextBlock.hpp>
#include <Aero/Gui/TextBox.hpp>
#include <Aero/Gui/TextBoxBase.hpp>
#include <Aero/Gui/ToggleButton.hpp>
#include <Aero/Gui/Transform.hpp>
#include <Aero/Gui/TreeView.hpp>
#include <Aero/Gui/UIElement.hpp>
#include <Aero/Gui/View.hpp>
#include <Aero/Gui/Visual.hpp>
#include <Aero/Gui/Window.hpp>
#include <Aero/Gui/XamlReader.hpp>

#include <type_traits>

namespace {

static_assert(
    std::is_base_of<
        Aero::FrameworkElement,
        Aero::Controls::Button>::value,
    "Gui target must expose the retained WPF control surface");

static_assert(
    std::is_base_of<
        Aero::Documents::TextElement,
        Aero::Documents::Inline>::value,
    "Documents Inline must preserve the WPF TextElement relationship");
static_assert(
    std::is_base_of<
        Aero::Documents::Inline,
        Aero::Documents::Run>::value,
    "Documents Run must derive from Inline");
static_assert(
    std::is_base_of<
        Aero::Documents::Span,
        Aero::Documents::Hyperlink>::value,
    "Documents Hyperlink must be inline content rather than ButtonBase");

static_assert(std::is_default_constructible<
    Aero::Documents::TextPointer>::value,
    "Gui target must expose TextPointer");
static_assert(std::is_default_constructible<
    Aero::Documents::TextRange>::value,
    "Gui target must expose TextRange");
static_assert(std::is_default_constructible<
    Aero::Documents::InlineCollectionView>::value,
    "Gui target must expose InlineCollectionView");
static_assert(
    !std::is_base_of<
        Aero::Controls::Primitives::ButtonBase,
        Aero::Documents::Hyperlink>::value,
    "Documents Hyperlink must not use the temporary Button hierarchy");

[[maybe_unused]] void ConsumeGui(
    Aero::Controls::Button& button,
    Aero::FrameworkElement& root) noexcept {
    static_cast<void>(button.RuntimeType());
    static_cast<void>(root.FindName("PART_Content"));
    static_cast<void>(root.InvalidateVisual());
}

} // namespace
