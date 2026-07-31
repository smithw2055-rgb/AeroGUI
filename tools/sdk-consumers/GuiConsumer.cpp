#include <Aero/Gui.hpp>

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
        Aero::Controls::ButtonBase,
        Aero::Documents::Hyperlink>::value,
    "Documents Hyperlink must not use the temporary Button hierarchy");

static_assert(std::is_default_constructible<
    Aero::Documents::TextSelection>::value,
    "Gui target must expose Documents TextSelection");
static_assert(std::is_same<
    decltype(std::declval<Aero::Controls::TextBlock&>().Selection()),
    Aero::Documents::TextSelection>::value,
    "TextBlock must expose Documents selection state");

[[maybe_unused]] void ConsumeGui(Aero::Controls::Button& button) noexcept {
    static_cast<void>(button.RuntimeType());
}

} // namespace
