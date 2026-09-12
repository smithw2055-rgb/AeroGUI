#pragma once

#include <Aero/Controls/Menu.hpp>

namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;

class AERO_GUI_API ContextMenu
    : public Menu {
    AERO_DECLARE_TYPE(ContextMenu, Menu)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    ContextMenu() noexcept;
    ~ContextMenu() override;

    bool GetIsOpen() const noexcept;
    void SetIsOpen(bool value) noexcept;
    Ref<UIElement>
        GetPlacementTarget() const noexcept;
    void SetPlacementTarget(Ref<UIElement> value) noexcept;

    AERO_DEPENDENCY_PROPERTY(bool, IsOpen);
    AERO_DEPENDENCY_PROPERTY(Ref<UIElement>, PlacementTarget);
    inline static constexpr RoutedEvent<RoutedEventArgs> OpenedEvent{"Opened"};
    inline static constexpr RoutedEvent<RoutedEventArgs> ClosedEvent{"Closed"};

protected:
    virtual void OnOpened(RoutedEventArgs& e);
    virtual void OnClosed(RoutedEventArgs& e);
    void OnPropertyChanged(
        const DependencyPropertyChangedEventArgs& args) noexcept override;
    void OnApplyTemplate() noexcept override;
};
} // namespace Aero::Controls
