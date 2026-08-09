#pragma once

#include <Aero/DataTemplate.hpp>
#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Controls/Panel.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;
enum class ExpandDirection : std::uint8_t {
    Down = 0U,
    Up,
    Left,
    Right,
};

class AERO_GUI_API HeaderedContentControl
    : public ContentControl {
    AERO_DECLARE_TYPE(
        HeaderedContentControl,
        ContentControl)
public:
    Value GetHeader() const noexcept;
    void SetHeader(
        const Value& value) noexcept;
    Result<void> SetHeader(StringView value) noexcept;
    Ref<DataTemplate> GetHeaderTemplate() const noexcept;
    void SetHeaderTemplate(
        Ref<DataTemplate> value) noexcept;

    // WPF headers are content, not just text. They can hold an element, a
    // resource object, a scalar, or x:Null and are consumed by a
    // ContentPresenter through ContentSource="Header".
    inline static constexpr DependencyProperty<Value> HeaderProperty{"Header"};
    inline static constexpr DependencyProperty<Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};

protected:
    explicit HeaderedContentControl(
        TypeId runtimeType) noexcept
        : ContentControl(runtimeType) {}
    ~HeaderedContentControl() override = default;
};

class AERO_GUI_API GroupBox
    : public HeaderedContentControl {
    AERO_DECLARE_TYPE(
        GroupBox,
        HeaderedContentControl)
public:
    GroupBox() noexcept
        : HeaderedContentControl(StaticTypeId()) {}
    ~GroupBox() override = default;
};

class AERO_GUI_API Label : public ContentControl {
    AERO_DECLARE_TYPE(Label, ContentControl)
public:
    Label() noexcept : ContentControl(StaticTypeId()) {}
};

class AERO_GUI_API Expander
    : public HeaderedContentControl {
    AERO_DECLARE_TYPE(
        Expander,
        HeaderedContentControl)
public:
    Expander() noexcept;
    ~Expander() override;

    bool GetIsExpanded() const noexcept;
    void SetIsExpanded(
        bool value) noexcept;
    ExpandDirection GetDirection() const noexcept;
    void SetDirection(
        ExpandDirection value) noexcept;

    inline static constexpr RoutedEvent<RoutedEventArgs> ExpandedEvent{"Expanded"};
    inline static constexpr RoutedEvent<RoutedEventArgs> CollapsedEvent{"Collapsed"};
    UIElement::Event<RoutedEventArgs>
        Expanded() noexcept {
        return GetEvent(ExpandedEvent);
    }
    UIElement::Event<RoutedEventArgs>
        Collapsed() noexcept {
        return GetEvent(CollapsedEvent);
    }
    inline static constexpr DependencyProperty<bool> IsExpandedProperty{"IsExpanded"};
    inline static constexpr DependencyProperty<ExpandDirection> ExpandDirectionProperty{"ExpandDirection"};

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;

private:
    DependencyPropertyChangedEventHandler
        expandedChangedHandler_;
    void OnExpandedPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
};

class AERO_GUI_API TabItem
    : public HeaderedContentControl {
    AERO_DECLARE_TYPE(
        TabItem,
        HeaderedContentControl)
public:
    TabItem() noexcept
        : HeaderedContentControl(StaticTypeId()) {}
    ~TabItem() override = default;

    bool GetIsSelected() const noexcept;
    void SetIsSelected(
        bool value) noexcept;
    inline static constexpr DependencyProperty<bool> IsSelectedProperty{"IsSelected"};
};

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

// Wraps tab headers according to the nearest templated TabControl's strip
// placement, matching the WPF TabPanel layout contract.
class AERO_GUI_API TabPanel : public Panel {
    AERO_DECLARE_TYPE(TabPanel, Panel)
public:
    TabPanel() noexcept : Panel(StaticTypeId()) {}
    ~TabPanel() override = default;

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;

private:
    bool GetIsVertical() const noexcept;
};
} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::ExpandDirection)
