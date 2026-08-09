#include <Aero/Gui.hpp>
#include <Aero/View.hpp>
#include <Aero/Controls/Button.hpp>
#include <Aero/Documents.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Events.hpp>
#include <Aero/Triggers/Triggers.hpp>

#include <Aero/Data/Binding.hpp>
#include <Aero/Controls/Border.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Controls/Button.hpp>
#include <Aero/Controls/ButtonBase.hpp>
#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/DependencyObject.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Controls/Grid.hpp>
#include <Aero/IRenderer.hpp>
#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Controls/ListBox.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Resources.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Controls/StackPanel.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Style.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/ToggleButton.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Controls/TreeView.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/Visual.hpp>
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
