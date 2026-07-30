#include <Aero/Documents/Documents.hpp>

#include <utility>

namespace Aero::Documents {

Base::StringView Hyperlink::NavigateUri() const noexcept {
    return GetValueOr(NavigateUriProperty, Base::StringView{});
}

Presentation::ICommand* Hyperlink::Command() const noexcept {
    return GetValueOr(
        CommandProperty,
        Base::Ref<Presentation::ICommand>{}).Get();
}

Base::Ref<Base::Object> Hyperlink::CommandParameter() const noexcept {
    return GetValueOr(
        CommandParameterProperty,
        Base::Ref<Base::Object>{});
}

Presentation::UIElement* Hyperlink::CommandTarget() const noexcept {
    return GetValueOr(
        CommandTargetProperty,
        Base::Ref<Presentation::UIElement>{}).Get();
}

Base::Result<void> Hyperlink::SetNavigateUri(
    Base::StringView value) noexcept {
    return SetValue(NavigateUriProperty, value);
}

Base::Result<void> Hyperlink::SetCommand(
    Base::Ref<Presentation::ICommand> command) noexcept {
    return SetValue(CommandProperty, std::move(command));
}

Base::Result<void> Hyperlink::SetCommandParameter(
    Base::Ref<Base::Object> parameter) noexcept {
    return SetValue(CommandParameterProperty, std::move(parameter));
}

Base::Result<void> Hyperlink::SetCommandTarget(
    Base::Ref<Presentation::UIElement> target) noexcept {
    return SetValue(CommandTargetProperty, std::move(target));
}

} // namespace Aero::Documents
