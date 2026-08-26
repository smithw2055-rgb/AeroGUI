#pragma once

#include <Aero/Controls/Menu.hpp>

namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;

class AERO_GUI_API ContextMenu
    : public Menu {
    AERO_DECLARE_TYPE(ContextMenu, Menu)
public:
    ContextMenu() noexcept;
    ~ContextMenu() override;

    bool GetIsOpen() const noexcept;
    void SetIsOpen(
        bool value) noexcept;
    Ref<UIElement>
        GetPlacementTarget() const noexcept;
    void SetPlacementTarget(
        Ref<UIElement> value) noexcept;

    inline static constexpr DependencyProperty<bool> IsOpenProperty{"IsOpen"};
    inline static constexpr DependencyProperty<Ref<UIElement>> PlacementTargetProperty{"PlacementTarget"};
    inline static constexpr RoutedEvent<RoutedEventArgs> OpenedEvent{"Opened"};
    inline static constexpr RoutedEvent<RoutedEventArgs> ClosedEvent{"Closed"};

protected:
    void
        OnApplyTemplate() noexcept override;

private:
    DependencyPropertyChangedEventHandler
        openChangedHandler_;
    void OnOpenChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
};
} // namespace Aero::Controls
