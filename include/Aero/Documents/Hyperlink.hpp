#pragma once

#include <Aero/Documents/Span.hpp>
#include <Aero/Events/NavigationEventArgs.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>

namespace Aero::Documents {

class AERO_GUI_API Hyperlink : public Span {
    AERO_DECLARE_TYPE(Hyperlink, Span)
public:
    Hyperlink() noexcept : Span(StaticTypeId()) {}
    ~Hyperlink() override = default;

    inline static constexpr RoutedEvent<Aero::RoutedEventArgs> ClickEvent{"Click"};
    ContentElement::Event<Aero::RoutedEventArgs> Click() noexcept {
        return GetEvent(ClickEvent);
    }
    inline static constexpr RoutedEvent<RequestNavigateEventArgs> RequestNavigateEvent{"RequestNavigate"};
    ContentElement::Event<RequestNavigateEventArgs> RequestNavigate() noexcept {
        return GetEvent(RequestNavigateEvent);
    }

    StringView GetNavigateUri() const noexcept;
    Aero::Input::ICommand* GetCommand() const noexcept;
    Value GetCommandParameter() const noexcept;
    Aero::UIElement* GetCommandTarget() const noexcept;

    void SetNavigateUri(StringView value) noexcept;
    void SetCommand(
        Ref<Aero::Input::ICommand> command) noexcept;
    void SetCommandParameter(Value parameter) noexcept;
    void SetCommandTarget(
        Ref<Aero::UIElement> target) noexcept;

    inline static constexpr DependencyProperty<String> NavigateUriProperty{"NavigateUri"};
    inline static constexpr DependencyProperty<Ref<Aero::Input::ICommand>> CommandProperty{"Command"};
    inline static constexpr DependencyProperty<Value> CommandParameterProperty{"CommandParameter"};
    inline static constexpr DependencyProperty<Ref<Aero::UIElement>> CommandTargetProperty{"CommandTarget"};
};

} // namespace Aero::Documents
