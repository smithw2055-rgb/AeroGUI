#pragma once

#include <Aero/Controls/Items.hpp>

namespace Aero::Controls {

enum class ExpandDirection : std::uint8_t {
    Down = 0U,
    Up,
    Left,
    Right,
};

enum class PlacementMode : std::uint8_t {
    Bottom = 0U,
    Top,
    Left,
    Right,
    Center,
    Mouse,
};

enum class PopupAnimation : std::uint8_t {
    None = 0U,
    Fade,
    Slide,
    Scroll,
};

class AERO_API Popup : public ContentControl {
    AERO_DECLARE_TYPE(Popup, ContentControl)
public:
    Popup() noexcept;
    ~Popup() override;

    bool IsOpen() const noexcept;
    Base::Result<void> SetIsOpen(bool value) noexcept;
    PlacementMode Placement() const noexcept;
    Base::Result<void> SetPlacement(
        PlacementMode value) noexcept;
    double HorizontalOffset() const noexcept;
    Base::Result<void> SetHorizontalOffset(
        double value) noexcept;
    double VerticalOffset() const noexcept;
    Base::Result<void> SetVerticalOffset(
        double value) noexcept;
    bool StaysOpen() const noexcept;
    Base::Result<void> SetStaysOpen(
        bool value) noexcept;
    bool MatchPlacementTargetWidth() const noexcept;
    Base::Result<void> SetMatchPlacementTargetWidth(
        bool value) noexcept;
    Base::Ref<UIElement>
        PlacementTarget() const noexcept;
    Base::Result<void> SetPlacementTarget(
        Base::Ref<UIElement> value) noexcept;
    PopupAnimation GetPopupAnimation() const noexcept;
    Base::Result<void> SetPopupAnimation(
        PopupAnimation value) noexcept;
    bool AllowsTransparency() const noexcept;
    Base::Result<void> SetAllowsTransparency(
        bool value) noexcept;

    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs>
        OpenedEvent{"Opened"};
    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs>
        ClosedEvent{"Closed"};
    UIElement::RoutedEvent_<RoutedEventHandler>
        Opened() noexcept {
        return Event(OpenedEvent);
    }
    UIElement::RoutedEvent_<RoutedEventHandler>
        Closed() noexcept {
        return Event(ClosedEvent);
    }

    inline static constexpr Members::Property<bool>
        IsOpenProperty{"IsOpen"};
    inline static constexpr Members::Property<
        PlacementMode>
        PlacementProperty{"Placement"};
    inline static constexpr Members::Property<double>
        HorizontalOffsetProperty{"HorizontalOffset"};
    inline static constexpr Members::Property<double>
        VerticalOffsetProperty{"VerticalOffset"};
    inline static constexpr Members::Property<bool>
        StaysOpenProperty{"StaysOpen"};
    inline static constexpr Members::Property<bool>
        MatchPlacementTargetWidthProperty{
            "MatchPlacementTargetWidth"};
    inline static constexpr Members::Property<
        Base::Ref<UIElement>>
        PlacementTargetProperty{
            "PlacementTarget"};
    inline static constexpr Members::Property<
        PopupAnimation>
        PopupAnimationProperty{"PopupAnimation"};
    inline static constexpr Members::Property<bool>
        AllowsTransparencyProperty{
            "AllowsTransparency"};

protected:
    explicit Popup(TypeId runtimeType) noexcept;
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override;

private:
    DependencyPropertyChangedEventHandler
        openChangedHandler_;
    Size popupDesiredSize_;
    void OnOpenPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
};

class AERO_API HeaderedContentControl
    : public ContentControl {
    AERO_DECLARE_TYPE(
        HeaderedContentControl,
        ContentControl)
public:
    Core::Value Header() const noexcept;
    Base::Result<void> SetHeader(
        const Core::Value& value) noexcept;
    Base::Ref<DataTemplate> HeaderTemplate() const noexcept;
    Base::Result<void> SetHeaderTemplate(
        Base::Ref<DataTemplate> value) noexcept;

    // WPF headers are content, not just text. They can hold an element, a
    // resource object, a scalar, or x:Null and are consumed by a
    // ContentPresenter through ContentSource="Header".
    inline static constexpr Members::Property<Core::Value>
        HeaderProperty{"Header"};
    inline static constexpr Members::Property<
        Base::Ref<DataTemplate>>
        HeaderTemplateProperty{"HeaderTemplate"};

protected:
    explicit HeaderedContentControl(
        TypeId runtimeType) noexcept
        : ContentControl(runtimeType) {}
    ~HeaderedContentControl() override = default;
};

class AERO_API GroupBox final
    : public HeaderedContentControl {
    AERO_DECLARE_TYPE(
        GroupBox,
        HeaderedContentControl)
public:
    GroupBox() noexcept
        : HeaderedContentControl(StaticTypeId()) {}
    ~GroupBox() override = default;
};

class AERO_API Label final : public ContentControl {
    AERO_DECLARE_TYPE(Label, ContentControl)
public:
    Label() noexcept : ContentControl(StaticTypeId()) {}
};

class AERO_API Expander final
    : public HeaderedContentControl {
    AERO_DECLARE_TYPE(
        Expander,
        HeaderedContentControl)
public:
    Expander() noexcept;
    ~Expander() override;

    bool IsExpanded() const noexcept;
    Base::Result<void> SetIsExpanded(
        bool value) noexcept;
    ExpandDirection Direction() const noexcept;
    Base::Result<void> SetDirection(
        ExpandDirection value) noexcept;

    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs>
        ExpandedEvent{"Expanded"};
    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs>
        CollapsedEvent{"Collapsed"};
    UIElement::RoutedEvent_<RoutedEventHandler>
        Expanded() noexcept {
        return Event(ExpandedEvent);
    }
    UIElement::RoutedEvent_<RoutedEventHandler>
        Collapsed() noexcept {
        return Event(CollapsedEvent);
    }
    inline static constexpr Members::Property<bool>
        IsExpandedProperty{"IsExpanded"};
    inline static constexpr Members::Property<
        ExpandDirection>
        ExpandDirectionProperty{"ExpandDirection"};

protected:
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override;

private:
    DependencyPropertyChangedEventHandler
        expandedChangedHandler_;
    void OnExpandedPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
};

class AERO_API TabItem final
    : public HeaderedContentControl {
    AERO_DECLARE_TYPE(
        TabItem,
        HeaderedContentControl)
public:
    TabItem() noexcept
        : HeaderedContentControl(StaticTypeId()) {}
    ~TabItem() override = default;

    bool IsSelected() const noexcept;
    Base::Result<void> SetIsSelected(
        bool value) noexcept;
    inline static constexpr Members::Property<bool>
        IsSelectedProperty{"IsSelected"};
};

class AERO_API TabControl final : public Control {
    AERO_DECLARE_TYPE(TabControl, Control)
public:
    TabControl() noexcept;
    ~TabControl() override;

    std::uint32_t TabCount() const noexcept {
        return tabs_.Size();
    }
    std::uint32_t SelectedIndex() const noexcept;
    TabItem* SelectedTab() const noexcept;
    Core::Value SelectedContent() const noexcept {
        return GetValueOr(
            SelectedContentProperty,
            Core::Value::NullObject(
                Core::TypeOf<Base::Object>()));
    }
    Base::Result<void> AddOwnedTab(
        Base::Ref<TabItem> tab) noexcept;
    Base::Result<void> ClearOwnedTabs() noexcept;
    Base::Result<bool> SetSelectedIndex(
        std::uint32_t value) noexcept;
    // Kept as dependency properties even while the lightweight tab host is
    // being upgraded to the full selector pipeline. This preserves authored
    // ItemsControl binding/template declarations instead of reducing them to
    // loader-only markup.
    Base::Ref<Base::Object> ItemsSource() const noexcept {
        return GetValueOr(
            ItemsSourceProperty,
            Base::Ref<Base::Object>{});
    }
    Base::Result<void> SetItemsSource(
        Base::Ref<Base::Object> value) noexcept {
        return SetValue(ItemsSourceProperty, std::move(value));
    }
    Base::Ref<DataTemplate> ItemTemplate() const noexcept {
        return GetValueOr(
            ItemTemplateProperty,
            Base::Ref<DataTemplate>{});
    }
    Base::Result<void> SetItemTemplate(
        Base::Ref<DataTemplate> value) noexcept {
        return SetValue(ItemTemplateProperty, std::move(value));
    }
    Base::Ref<DataTemplate> ContentTemplate() const noexcept {
        return GetValueOr(
            ContentTemplateProperty,
            Base::Ref<DataTemplate>{});
    }
    Base::Result<void> SetContentTemplate(
        Base::Ref<DataTemplate> value) noexcept {
        return SetValue(ContentTemplateProperty, std::move(value));
    }
    Dock TabStripPlacement() const noexcept {
        return GetValueOr(TabStripPlacementProperty, Dock::Top);
    }
    Base::Result<void> SetTabStripPlacement(Dock value) noexcept {
        return SetValue(TabStripPlacementProperty, value);
    }

    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs>
        SelectionChangedEvent{"SelectionChanged"};
    UIElement::RoutedEvent_<RoutedEventHandler>
        SelectionChanged() noexcept {
        return Event(SelectionChangedEvent);
    }
    inline static constexpr Members::Property<
        std::uint32_t>
        SelectedIndexProperty{"SelectedIndex"};
    inline static constexpr Members::ReadOnlyProperty<
        Core::Value>
        SelectedContentProperty{"SelectedContent"};
    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        ItemsSourceProperty{"ItemsSource"};
    inline static constexpr Members::Property<
        Base::Ref<DataTemplate>>
        ItemTemplateProperty{"ItemTemplate"};
    inline static constexpr Members::Property<
        Base::Ref<DataTemplate>>
        ContentTemplateProperty{"ContentTemplate"};
    inline static constexpr Members::Property<Dock>
        TabStripPlacementProperty{"TabStripPlacement"};

protected:
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override;

private:
    Base::Vector<Base::Ref<TabItem>> tabs_;
    DependencyPropertyChangedEventHandler
        selectionChangedHandler_;
    void OnSelectionPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    Base::Result<void> SynchronizeSelection() noexcept;
};

// Wraps tab headers according to the nearest templated TabControl's strip
// placement, matching the WPF TabPanel layout contract.
class AERO_API TabPanel final : public Panel {
    AERO_DECLARE_TYPE(TabPanel, Panel)
public:
    TabPanel() noexcept : Panel(StaticTypeId()) {}
    ~TabPanel() override = default;

protected:
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override;

private:
    bool IsVertical() const noexcept;
};

} // namespace Aero::Controls

namespace Aero::Core {

template<>
struct MetaTypeTraits<Controls::ExpandDirection> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("ExpandDirection");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "ExpandDirection";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Controls::PlacementMode> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("PlacementMode");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "PlacementMode";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Controls::PopupAnimation> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("PopupAnimation");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "PopupAnimation";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core
