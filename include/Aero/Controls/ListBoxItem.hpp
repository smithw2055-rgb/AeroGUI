#pragma once

#include <Aero/Controls/ContentControl.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Controls {

using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::TypeId;

class AERO_GUI_API ListBoxItem : public ContentControl {
    AERO_DECLARE_TYPE(ListBoxItem, ContentControl)
public:
    ListBoxItem() noexcept;
    ~ListBoxItem() override;

    bool GetIsSelected() const noexcept;
    void SetIsSelected(bool value) noexcept;

    void UpdateVisualState(bool useTransitions = true) noexcept;

    inline static constexpr RoutedEvent<RoutedEventArgs> SelectedEvent{"Selected"};
    inline static constexpr RoutedEvent<RoutedEventArgs> UnselectedEvent{"Unselected"};
    UIElement::Event<RoutedEventArgs> Selected() noexcept {
        return GetEvent(SelectedEvent);
    }
    UIElement::Event<RoutedEventArgs> Unselected() noexcept {
        return GetEvent(UnselectedEvent);
    }

    AERO_DEPENDENCY_PROPERTY(bool, IsSelected);
protected:
    explicit ListBoxItem(TypeId runtimeType) noexcept;
    virtual void OnSelected(RoutedEventArgs& e);
    virtual void OnUnselected(RoutedEventArgs& e);
    void OnPropertyChanged(
        const DependencyPropertyChangedEventArgs& args) noexcept override;
};

} // namespace Aero::Controls
