#pragma once

#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/TabItem.hpp>
#include <Aero/DataTemplate.hpp>

namespace Aero::Controls {

class AERO_GUI_API TabControl : public Control {
    AERO_DECLARE_TYPE(TabControl, Control)
public:
    TabControl() noexcept;
    ~TabControl() override;

    std::uint32_t GetTabCount() const noexcept {
        return tabs_.Size();
    }
    std::uint32_t GetSelectedIndex() const noexcept;
    TabItem* GetSelectedTab() const noexcept;
    Value GetSelectedContent() const noexcept {
        return GetValueOr(
            SelectedContentProperty,
            Value::NullObject(
                Meta::TypeOf<Base::Object>()));
    }
    Result<void> AddOwnedTab(
        Ref<TabItem> tab) noexcept;
    void ClearOwnedTabs() noexcept;
    void SetSelectedIndex(
        std::uint32_t value) noexcept;
    // Kept as dependency properties even while the lightweight tab host is
    // being upgraded to the full selector pipeline. This preserves authored
    // ItemsControl binding/template declarations instead of reducing them to
    // loader-only markup.
    Ref<Base::Object> GetItemsSource() const noexcept {
        return GetValueOr(
            ItemsSourceProperty,
            Ref<Base::Object>{});
    }
    void SetItemsSource(
        Ref<Base::Object> value) noexcept {
        SetValue(ItemsSourceProperty, std::move(value));
    }
    Ref<DataTemplate> GetItemTemplate() const noexcept {
        return GetValueOr(
            ItemTemplateProperty,
            Ref<DataTemplate>{});
    }
    void SetItemTemplate(
        Ref<DataTemplate> value) noexcept {
        SetValue(ItemTemplateProperty, std::move(value));
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

    inline static constexpr RoutedEvent<RoutedEventArgs> SelectionChangedEvent{"SelectionChanged"};
    UIElement::Event<RoutedEventArgs>
        SelectionChanged() noexcept {
        return GetEvent(SelectionChangedEvent);
    }
    inline static constexpr DependencyProperty<std::uint32_t> SelectedIndexProperty{"SelectedIndex"};
    inline static constexpr ReadOnlyDependencyProperty<Value> SelectedContentProperty{"SelectedContent"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ItemsSourceProperty{"ItemsSource"};
    inline static constexpr DependencyProperty<Ref<DataTemplate>> ItemTemplateProperty{"ItemTemplate"};
    inline static constexpr DependencyProperty<Ref<DataTemplate>> ContentTemplateProperty{"ContentTemplate"};
    inline static constexpr DependencyProperty<Dock> TabStripPlacementProperty{"TabStripPlacement"};

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;

private:
    Base::Vector<Ref<TabItem>> tabs_;
    DependencyPropertyChangedEventHandler
        selectionChangedHandler_;
    void OnSelectionPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    Result<void> SynchronizeSelection() noexcept;
};

} // namespace Aero::Controls
