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
static_assert(
    !std::is_base_of<
        Aero::Controls::ButtonBase,
        Aero::Documents::Hyperlink>::value,
    "Documents Hyperlink must not use the temporary Button hierarchy");

[[maybe_unused]] void ConsumeGui(Aero::Controls::Button& button) noexcept {
    static_cast<void>(button.RuntimeType());
}

} // namespace
