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

    AERO_DEPENDENCY_PROPERTY(bool, IsSelected);
protected:
    explicit ListBoxItem(TypeId runtimeType) noexcept;

private:
    DependencyPropertyChangedEventHandler selectedChangedHandler_;
    DependencyPropertyChangedEventHandler mouseOverChangedHandler_;
    DependencyPropertyChangedEventHandler isEnabledChangedHandler_;

    void OnIsSelectedChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void OnIsMouseOverChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void OnIsEnabledChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
};

} // namespace Aero::Controls
