#pragma once

#include <Aero/Controls/Primitives/Selector.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/TabItem.hpp>
#include <Aero/DataTemplate.hpp>

namespace Aero::Controls {

class AERO_GUI_API TabControl : public Primitives::Selector {
    AERO_DECLARE_TYPE(TabControl, Primitives::Selector)
public:
    TabControl() noexcept;
    ~TabControl() override;

    std::uint32_t GetTabCount() const noexcept {
        return GetCount();
    }
    TabItem* GetSelectedTab() const noexcept;
    Value GetSelectedContent() const noexcept {
        return GetValueOr(
            SelectedContentProperty,
            Value::NullObject(
                Meta::TypeOf<Base::Object>()));
    }
    Ref<DataTemplate> GetContentTemplate() const noexcept {
        return GetValueOr(
            ContentTemplateProperty,
            Ref<DataTemplate>{});
    }
    void SetContentTemplate(
        Ref<DataTemplate> value) noexcept {
        SetValue(ContentTemplateProperty, std::move(value));
    }
    Dock GetTabStripPlacement() const noexcept {
        return GetValueOr(TabStripPlacementProperty, Dock::Top);
    }
    void SetTabStripPlacement(Dock value) noexcept {
        SetValue(TabStripPlacementProperty, value);
    }

    inline static constexpr ReadOnlyDependencyProperty<Value> SelectedContentProperty{"SelectedContent"};
    inline static constexpr DependencyProperty<Ref<DataTemplate>> ContentTemplateProperty{"ContentTemplate"};
    inline static constexpr DependencyProperty<Dock> TabStripPlacementProperty{"TabStripPlacement"};

protected:
    Result<Ref<FrameworkElement>> CreateContainer(
        const Ref<Base::Object>& item) noexcept override;
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;

private:
    DependencyPropertyChangedEventHandler
        selectionChangedHandler_;
    void OnSelectionPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    Result<void> SynchronizeSelection() noexcept;
};

} // namespace Aero::Controls
