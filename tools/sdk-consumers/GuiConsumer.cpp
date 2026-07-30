#include <Aero/Gui.hpp>

#include <type_traits>

namespace {

static_assert(
    std::is_base_of<
        Aero::FrameworkElement,
        Aero::Controls::Button>::value,
    "Gui target must expose the retained WPF control surface");

[[maybe_unused]] void ConsumeGui(Aero::Controls::Button& button) noexcept {
    static_cast<void>(button.RuntimeType());
}

} // namespace
