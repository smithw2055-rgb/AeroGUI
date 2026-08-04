#pragma once

#include <Aero/Collections.hpp>
#include <Aero/Styling.hpp>
#include <utility>
#include <Aero/Input.hpp>
#include <Aero/Data.hpp>
#include <Aero/Controls/Core.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Controls/Primitives.hpp>
#include <Aero/Events/ControlEventArgs.hpp>

namespace Aero::Controls {

using ItemsChangeAction = Collections::ItemsChangeAction;
using ItemsChangedEvent = Collections::ItemsChangedEvent;
using ItemsChangedHandler = Collections::ItemsChangedHandler;

class AERO_API ItemCollection : public Collections::IItemsSource {
public:
    std::uint32_t GetCount() const noexcept override {
        return items_.Size();
    }
    Base::Ref<Base::Object> GetItem(
        std::uint32_t index) const noexcept override;
    Base::Result<void> Add(
        Base::Ref<Base::Object> item) noexcept;
    Base::Result<void> Insert(
        std::uint32_t index,
        Base::Ref<Base::Object> item) noexcept;
    Base::Result<Base::Ref<Base::Object>> RemoveAt(
        std::uint32_t index) noexcept;
    Base::Result<void> Replace(
        std::uint32_t index,
        Base::Ref<Base::Object> item) noexcept;
    Base::Result<void> Move(
        std::uint32_t oldIndex,
        std::uint32_t newIndex) noexcept;
    void Reset() noexcept;
    bool Reset(
        Base::Span<const Base::Ref<Base::Object>>
            items) noexcept;
    void AddItemsChanged(
        const ItemsChangedHandler& handler) noexcept override {
        changed_.Add(handler);
    }
    bool RemoveItemsChanged(
        const ItemsChangedHandler& handler) noexcept override {
        return changed_.Remove(handler);
    }
private:
    Base::Vector<Base::Ref<Base::Object>> items_;
    ItemsChangedHandler changed_;
    void Notify(const ItemsChangedEvent& event) noexcept;
};

// WPF AlternationConverter selects an authored value by alternation index.
// Keeping object values intact lets a binding later return brushes, strings,
// and other resources without lossy text conversion.
class AERO_API AlternationConverter : public Base::Object {
    AERO_DECLARE_TYPE(AlternationConverter, Base::Object)
public:
    AlternationConverter() noexcept
        : values_(&Base::GetDefaultAllocator()) {}
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Span<const Base::Ref<Base::Object>> GetValues() const noexcept {
        return values_.AsSpan();
    }
    Base::Result<void> AddValue(
        Base::Ref<Base::Object> value) noexcept {
        if (!value) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "AlternationConverter values cannot be null");
        }
        return values_.PushBack(std::move(value));
    }
    void ClearValues() noexcept { values_.Clear(); }
private:
    Base::Vector<Base::Ref<Base::Object>> values_;
};

// Adds a scalar value to an ItemsSource without exposing the internal boxing
// object used by the generator.
AERO_API Base::Result<void> AddBoxedItem(
    Collections::ObservableCollection& source,
    Meta::Value value) noexcept;

// Convenience overload for the common string item case.
AERO_API Base::Result<void> AddBoxedStringItem(
    Collections::ObservableCollection& source,
    Base::StringView value) noexcept;

class AERO_API DataTemplate : public Base::Object {
    AERO_DECLARE_TYPE(DataTemplate, Base::Object)
public:
    struct Impl;

    DataTemplate() noexcept;
    ~DataTemplate() noexcept override;
    DataTemplate(const DataTemplate&) = delete;
    DataTemplate& operator=(const DataTemplate&) = delete;

    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    TypeId GetDataType() const noexcept;
    void SetDataType(TypeId value) noexcept;
    Base::Ref<Base::Object> GetHierarchicalItemsSource() const noexcept;
    void SetHierarchicalItemsSource(Base::Ref<Base::Object> value) noexcept;
    Base::Ref<Base::Object> GetHierarchicalItemTemplate() const noexcept;
    void SetHierarchicalItemTemplate(Base::Ref<Base::Object> value) noexcept;
    ResourceKey GetImplicitKey() const noexcept;
    ResourceDictionary& GetResources() noexcept;
    const ResourceDictionary& GetResources() const noexcept;
    void SetResources(Base::Ref<ResourceDictionary> value) noexcept;
    bool GetIsSealed() const noexcept;

private:
    friend struct Impl;
    void* state_ = nullptr;
};

class AERO_API ItemsPanelTemplate : public Base::Object {
    AERO_DECLARE_TYPE(ItemsPanelTemplate, Base::Object)
public:
    struct Impl;

    ItemsPanelTemplate() noexcept;
    ~ItemsPanelTemplate() noexcept override;
    ItemsPanelTemplate(const ItemsPanelTemplate&) = delete;
    ItemsPanelTemplate& operator=(const ItemsPanelTemplate&) = delete;

    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    ResourceDictionary& GetResources() noexcept;
    const ResourceDictionary& GetResources() const noexcept;
    void SetResources(Base::Ref<ResourceDictionary> value) noexcept;
    bool GetIsSealed() const noexcept;

private:
    friend struct Impl;
    void* state_ = nullptr;
};

class AERO_API ItemsPresenter : public Decorator {
    AERO_DECLARE_TYPE(ItemsPresenter, Decorator)
public:
    ItemsPresenter() noexcept
        : Decorator(StaticTypeId()) {}
    ~ItemsPresenter() override = default;
    Panel* GetItemsHost() const noexcept;
    void SetItemsHost(
        const Base::Ref<Base::Object>& owner,
        Panel& panel) noexcept;
};

class ItemContainerGenerator;
class VirtualizingStackPanel;

class AERO_API ItemsControl : public Control {
    AERO_DECLARE_TYPE(ItemsControl, Control)
public:
    ItemsControl() noexcept;
    ~ItemsControl() override;

    ItemCollection& GetItems() noexcept {
        return items_;
    }
    const ItemCollection& GetItems() const noexcept {
        return items_;
    }
    Base::Ref<Base::Object> GetItemsSource() const noexcept {
        return GetValueOr(
            ItemsSourceProperty,
            Base::Ref<Base::Object>{});
    }
    bool GetHasItems() const noexcept {
        return GetValueOr(HasItemsProperty, false);
    }
    std::uint32_t GetCount() const noexcept;
    Base::Ref<Base::Object> GetItem(
        std::uint32_t index) const noexcept;
    void SetItemsSource(
        Base::Ref<Base::Object> source) noexcept {
        SetValue(ItemsSourceProperty, std::move(source));
    }
    void SetItemsSource(
        Collections::IItemsSource* source) noexcept;
    std::uint32_t GetAlternationCount() const noexcept {
        return GetValueOr(AlternationCountProperty, 0U);
    }
    void SetAlternationCount(
        std::uint32_t value) noexcept {
        SetValue(AlternationCountProperty, value);
    }

    const DataTemplate* GetItemTemplate() const noexcept {
        return itemTemplate_;
    }
    void SetItemTemplate(
        const DataTemplate* value) noexcept;
    const ItemsPanelTemplate* GetItemsPanel() const noexcept {
        return itemsPanel_;
    }
    void SetItemsPanel(
        const ItemsPanelTemplate* value) noexcept;
    const Style* GetItemContainerStyle() const noexcept {
        return itemContainerStyle_;
    }
    void SetItemContainerStyle(
        const Style* value) noexcept;

    void AddItemsChanged(
        const ItemsChangedHandler& handler) noexcept {
        changed_.Add(handler);
    }
    bool RemoveItemsChanged(
        const ItemsChangedHandler& handler) noexcept {
        return changed_.Remove(handler);
    }
    Panel* GetItemsHost() const noexcept {
        return itemsHost_;
    }
    std::uint32_t GetRealizedItemCount() const noexcept;
    std::uint32_t GetCreatedContainerCount() const noexcept;
    std::uint32_t GetRecycledContainerUseCount() const noexcept;

    inline static constexpr Members::ReadOnlyProperty<std::uint32_t> ItemCountProperty{"ItemCount"};
    inline static constexpr Members::ReadOnlyProperty<bool> HasItemsProperty{"HasItems"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ItemsSourceProperty{"ItemsSource"};
    inline static constexpr Members::Property<std::uint32_t> AlternationCountProperty{"AlternationCount"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> ItemTemplateProperty{"ItemTemplate"};
    inline static constexpr Members::Property<Base::Ref<ItemsPanelTemplate>> ItemsPanelProperty{"ItemsPanel"};
    inline static constexpr Members::Property<Base::Ref<Style>> ItemContainerStyleProperty{"ItemContainerStyle"};

protected:
    explicit ItemsControl(TypeId runtimeType) noexcept;
    ItemContainerGenerator* AttachedGenerator() const noexcept {
        return generator_;
    }
    virtual Base::Result<
        Base::Ref<FrameworkElement>>
        CreateContainer(
            const Base::Ref<Base::Object>& item) noexcept;
    virtual Base::Result<void> PrepareContainer(
        FrameworkElement& container,
        const Base::Ref<Base::Object>& item,
        std::uint32_t index) noexcept;
    virtual void ClearContainer(
        FrameworkElement& container) noexcept;
    virtual void OnContainersChanged() noexcept {}
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;

private:
    friend class ItemContainerGenerator;
    friend struct ::Aero::Visual::Impl;
    ItemCollection items_;
    Collections::IItemsSource* source_ = nullptr;
    const DataTemplate* itemTemplate_ = nullptr;
    const ItemsPanelTemplate* itemsPanel_ = nullptr;
    const Style* itemContainerStyle_ = nullptr;
    ItemContainerGenerator* generator_ = nullptr;
    Panel* itemsHost_ = nullptr;
    ItemsChangedHandler changed_;
    ItemsChangedHandler localHandler_;
    ItemsChangedHandler sourceHandler_;

    void OnLocalChanged(
        const ItemsChangedEvent& event) noexcept;
    void OnSourceChanged(
        const ItemsChangedEvent& event) noexcept;
    void PublishReset() noexcept;
    void PublishItemCount() noexcept;
};

// WPF's header-bearing items base. It retains ItemsControl's generation and
// layout behavior while exposing the header metadata consumed by theme styles.
class AERO_API HeaderedItemsControl : public ItemsControl {
    AERO_DECLARE_TYPE(HeaderedItemsControl, ItemsControl)
public:
    HeaderedItemsControl() noexcept
        : ItemsControl(StaticTypeId()) {}
    ~HeaderedItemsControl() override = default;

    Base::StringView GetHeader() const noexcept {
        return GetValueOr(HeaderProperty, Base::StringView{});
    }
    void SetHeader(Base::StringView value) noexcept {
        SetValue(HeaderProperty, value);
    }
    Base::Ref<DataTemplate> GetHeaderTemplate() const noexcept {
        return GetValueOr(
            HeaderTemplateProperty, Base::Ref<DataTemplate>{});
    }
    void SetHeaderTemplate(
        Base::Ref<DataTemplate> value) noexcept {
        SetValue(HeaderTemplateProperty, std::move(value));
    }

    inline static constexpr Members::Property<Base::String> HeaderProperty{"Header"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};

protected:
    explicit HeaderedItemsControl(TypeId runtimeType) noexcept
        : ItemsControl(runtimeType) {}
};

class AERO_API ItemContainerGenerator {
public:
    struct Impl;

    ~ItemContainerGenerator() noexcept;

    ItemContainerGenerator(const ItemContainerGenerator&) = delete;
    ItemContainerGenerator& operator=(const ItemContainerGenerator&) = delete;

    Base::Result<void> Attach(
        ItemsControl& owner,
        Panel& itemsHost) noexcept;
    Base::Result<void> AttachVirtualized(
        ItemsControl& owner,
        VirtualizingStackPanel& itemsHost) noexcept;
    Base::Result<bool> Detach() noexcept;
    Base::Result<void> Refresh() noexcept;
    void SetRealizationRange(
        std::uint32_t firstIndex,
        std::uint32_t count) noexcept;

    std::uint32_t GetGeneratedCount() const noexcept;
    std::uint32_t GetFirstGeneratedIndex() const noexcept;
    std::uint32_t GetCreatedContainerCount() const noexcept;
    std::uint32_t GetRecycledContainerUseCount() const noexcept;
    FrameworkElement* ContainerFromIndex(
        std::uint32_t index) const noexcept;
    std::uint32_t IndexFromContainer(
        const FrameworkElement& container) const noexcept;
    Base::Ref<Base::Object> ItemFromContainer(
        const FrameworkElement& container) const noexcept;
    Base::Status LastError() const noexcept;

private:
    friend struct ::Aero::Controls::Control::Impl;
    friend struct Impl;

    ItemContainerGenerator() noexcept = default;

    void* impl_ = nullptr;
};
} // namespace Aero::Controls

namespace Aero::Controls {

enum class ExpandDirection : std::uint8_t {
    Down = 0U,
    Up,
    Left,
    Right,
};

namespace Primitives {

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

    bool GetIsOpen() const noexcept;
    void SetIsOpen(bool value) noexcept;
    PlacementMode GetPlacement() const noexcept;
    void SetPlacement(
        PlacementMode value) noexcept;
    double GetHorizontalOffset() const noexcept;
    void SetHorizontalOffset(
        double value) noexcept;
    double GetVerticalOffset() const noexcept;
    void SetVerticalOffset(
        double value) noexcept;
    bool GetStaysOpen() const noexcept;
    void SetStaysOpen(
        bool value) noexcept;
    bool GetMatchPlacementTargetWidth() const noexcept;
    void SetMatchPlacementTargetWidth(
        bool value) noexcept;
    Base::Ref<UIElement>
        GetPlacementTarget() const noexcept;
    void SetPlacementTarget(
        Base::Ref<UIElement> value) noexcept;
    PopupAnimation GetPopupAnimation() const noexcept;
    void SetPopupAnimation(
        PopupAnimation value) noexcept;
    bool GetAllowsTransparency() const noexcept;
    void SetAllowsTransparency(
        bool value) noexcept;

    inline static constexpr Members::RoutedEvent<RoutedEventArgs> OpenedEvent{"Opened"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> ClosedEvent{"Closed"};
    UIElement::Event<RoutedEventArgs>
        Opened() noexcept {
        return GetEvent(OpenedEvent);
    }
    UIElement::Event<RoutedEventArgs>
        Closed() noexcept {
        return GetEvent(ClosedEvent);
    }

    inline static constexpr Members::Property<bool> IsOpenProperty{"IsOpen"};
    inline static constexpr Members::Property<PlacementMode> PlacementProperty{"Placement"};
    inline static constexpr Members::Property<double> HorizontalOffsetProperty{"HorizontalOffset"};
    inline static constexpr Members::Property<double> VerticalOffsetProperty{"VerticalOffset"};
    inline static constexpr Members::Property<bool> StaysOpenProperty{"StaysOpen"};
    inline static constexpr Members::Property<bool> MatchPlacementTargetWidthProperty{"MatchPlacementTargetWidth"};
    inline static constexpr Members::Property<Base::Ref<UIElement>> PlacementTargetProperty{"PlacementTarget"};
    inline static constexpr Members::Property<PopupAnimation> PopupAnimationProperty{"PopupAnimation"};
    inline static constexpr Members::Property<bool> AllowsTransparencyProperty{"AllowsTransparency"};

protected:
    explicit Popup(TypeId runtimeType) noexcept;
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
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

} // namespace Primitives

class AERO_API HeaderedContentControl
    : public ContentControl {
    AERO_DECLARE_TYPE(
        HeaderedContentControl,
        ContentControl)
public:
    Meta::Value GetHeader() const noexcept;
    void SetHeader(
        const Meta::Value& value) noexcept;
    Base::Ref<DataTemplate> GetHeaderTemplate() const noexcept;
    void SetHeaderTemplate(
        Base::Ref<DataTemplate> value) noexcept;

    // WPF headers are content, not just text. They can hold an element, a
    // resource object, a scalar, or x:Null and are consumed by a
    // ContentPresenter through ContentSource="Header".
    inline static constexpr Members::Property<Meta::Value> HeaderProperty{"Header"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};

protected:
    explicit HeaderedContentControl(
        TypeId runtimeType) noexcept
        : ContentControl(runtimeType) {}
    ~HeaderedContentControl() override = default;
};

class AERO_API GroupBox
    : public HeaderedContentControl {
    AERO_DECLARE_TYPE(
        GroupBox,
        HeaderedContentControl)
public:
    GroupBox() noexcept
        : HeaderedContentControl(StaticTypeId()) {}
    ~GroupBox() override = default;
};

class AERO_API Label : public ContentControl {
    AERO_DECLARE_TYPE(Label, ContentControl)
public:
    Label() noexcept : ContentControl(StaticTypeId()) {}
};

class AERO_API Expander
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

    inline static constexpr Members::RoutedEvent<RoutedEventArgs> ExpandedEvent{"Expanded"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> CollapsedEvent{"Collapsed"};
    UIElement::Event<RoutedEventArgs>
        Expanded() noexcept {
        return GetEvent(ExpandedEvent);
    }
    UIElement::Event<RoutedEventArgs>
        Collapsed() noexcept {
        return GetEvent(CollapsedEvent);
    }
    inline static constexpr Members::Property<bool> IsExpandedProperty{"IsExpanded"};
    inline static constexpr Members::Property<ExpandDirection> ExpandDirectionProperty{"ExpandDirection"};

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

class AERO_API TabItem
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
    inline static constexpr Members::Property<bool> IsSelectedProperty{"IsSelected"};
};

class AERO_API TabControl : public Control {
    AERO_DECLARE_TYPE(TabControl, Control)
public:
    TabControl() noexcept;
    ~TabControl() override;

    std::uint32_t GetTabCount() const noexcept {
        return tabs_.Size();
    }
    std::uint32_t GetSelectedIndex() const noexcept;
    TabItem* GetSelectedTab() const noexcept;
    Meta::Value GetSelectedContent() const noexcept {
        return GetValueOr(
            SelectedContentProperty,
            Meta::Value::NullObject(
                Meta::TypeOf<Base::Object>()));
    }
    Base::Result<void> AddOwnedTab(
        Base::Ref<TabItem> tab) noexcept;
    void ClearOwnedTabs() noexcept;
    void SetSelectedIndex(
        std::uint32_t value) noexcept;
    // Kept as dependency properties even while the lightweight tab host is
    // being upgraded to the full selector pipeline. This preserves authored
    // ItemsControl binding/template declarations instead of reducing them to
    // loader-only markup.
    Base::Ref<Base::Object> GetItemsSource() const noexcept {
        return GetValueOr(
            ItemsSourceProperty,
            Base::Ref<Base::Object>{});
    }
    void SetItemsSource(
        Base::Ref<Base::Object> value) noexcept {
        SetValue(ItemsSourceProperty, std::move(value));
    }
    Base::Ref<DataTemplate> GetItemTemplate() const noexcept {
        return GetValueOr(
            ItemTemplateProperty,
            Base::Ref<DataTemplate>{});
    }
    void SetItemTemplate(
        Base::Ref<DataTemplate> value) noexcept {
        SetValue(ItemTemplateProperty, std::move(value));
    }
    Base::Ref<DataTemplate> GetContentTemplate() const noexcept {
        return GetValueOr(
            ContentTemplateProperty,
            Base::Ref<DataTemplate>{});
    }
    void SetContentTemplate(
        Base::Ref<DataTemplate> value) noexcept {
        SetValue(ContentTemplateProperty, std::move(value));
    }
    Dock GetTabStripPlacement() const noexcept {
        return GetValueOr(TabStripPlacementProperty, Dock::Top);
    }
    void SetTabStripPlacement(Dock value) noexcept {
        SetValue(TabStripPlacementProperty, value);
    }

    inline static constexpr Members::RoutedEvent<RoutedEventArgs> SelectionChangedEvent{"SelectionChanged"};
    UIElement::Event<RoutedEventArgs>
        SelectionChanged() noexcept {
        return GetEvent(SelectionChangedEvent);
    }
    inline static constexpr Members::Property<std::uint32_t> SelectedIndexProperty{"SelectedIndex"};
    inline static constexpr Members::ReadOnlyProperty<Meta::Value> SelectedContentProperty{"SelectedContent"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ItemsSourceProperty{"ItemsSource"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> ItemTemplateProperty{"ItemTemplate"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> ContentTemplateProperty{"ContentTemplate"};
    inline static constexpr Members::Property<Dock> TabStripPlacementProperty{"TabStripPlacement"};

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
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
class AERO_API TabPanel : public Panel {
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
AERO_DECLARE_TYPE_ENUM(Aero::Controls::Primitives::PlacementMode)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::Primitives::PopupAnimation)

namespace Aero::Controls {

class VisualStateManager;
namespace Primitives { class Popup; }
class TextBox;

enum class SelectionMode : std::uint8_t {
    Single = 0U,
    Multiple,
    Extended,
};

namespace Primitives { class Selector; }

class AERO_API ListBoxItem : public ContentControl {
    AERO_DECLARE_TYPE(ListBoxItem, ContentControl)
public:
    ListBoxItem() noexcept : ContentControl(StaticTypeId()) {}
    ~ListBoxItem() override = default;

    bool GetIsSelected() const noexcept;
    void SetIsSelected(bool value) noexcept;

    inline static constexpr Members::Property<bool> IsSelectedProperty{"IsSelected"};
protected:
    explicit ListBoxItem(TypeId runtimeType) noexcept
        : ContentControl(runtimeType) {}
};

namespace Primitives {

class AERO_API Selector : public ItemsControl {
    AERO_DECLARE_TYPE(Selector, ItemsControl)
public:
    Selector() noexcept;
    ~Selector() override;

    SelectionMode GetSelectionMode() const noexcept;
    std::uint32_t GetSelectedIndex() const noexcept;
    Base::Ref<Base::Object> GetSelectedItem() const noexcept;
    Base::Ref<Base::Object> GetSelectedValue() const noexcept;
    Base::StringView GetSelectedValuePath() const noexcept {
        return GetValueOr(
            SelectedValuePathProperty,
            Base::StringView{});
    }
    Base::Span<const std::uint32_t> GetSelectedIndices() const noexcept {
        return {selectedIndices_.Data(), selectedIndices_.Size()};
    }
    std::uint32_t GetSelectedCount() const noexcept {
        return selectedIndices_.Size();
    }
    bool GetIsSelected(std::uint32_t index) const noexcept;
    std::uint32_t GetIndexOfItem(
        const Base::Object* item) const noexcept;

    void SetSelectionMode(
        SelectionMode value) noexcept;
    void SetSelectedIndex(
        std::uint32_t index) noexcept;
    void SetSelectedItem(
        Base::Ref<Base::Object> item) noexcept;
    void SetSelectedValue(
        Base::Ref<Base::Object> value) noexcept;
    bool Select(
        std::uint32_t index) noexcept;
    bool Unselect(
        std::uint32_t index) noexcept;
    bool Toggle(
        std::uint32_t index) noexcept;
    bool SelectRange(
        std::uint32_t first,
        std::uint32_t last,
        bool preserveExisting = false) noexcept;
    void ClearSelection() noexcept;

    void AddSelectionChanged(
        const SelectionChangedHandler& handler) noexcept {
        selectionChanged_.Add(handler);
    }
    bool RemoveSelectionChanged(
        const SelectionChangedHandler& handler) noexcept {
        return selectionChanged_.Remove(handler);
    }
    Base::Status LastSelectionError() const noexcept {
        return lastSelectionError_;
    }

    inline static constexpr Members::Property<SelectionMode> SelectionModeProperty{"SelectionMode"};
    inline static constexpr Members::Property<std::uint32_t> SelectedIndexProperty{"SelectedIndex"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> SelectedItemProperty{"SelectedItem"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> SelectedValueProperty{"SelectedValue"};
    inline static constexpr Members::Property<Base::String> SelectedValuePathProperty{"SelectedValuePath"};
    inline static constexpr Members::AttachedProperty<bool> IsSelectedProperty{"IsSelected"};
    // WPF Selector.SelectionChanged is a bubbling routed event. Keep the
    // strongly typed selection notification above for model-facing code while
    // also publishing the routed surface used by EventTrigger.
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> SelectionChangedRoutedEvent{"SelectionChanged"};
    UIElement::Event<RoutedEventArgs>
        SelectionChanged() noexcept {
        return GetEvent(
            SelectionChangedRoutedEvent);
    }

protected:
    explicit Selector(TypeId runtimeType) noexcept;
    Base::Result<void> PrepareContainer(
        FrameworkElement& container,
        const Base::Ref<Base::Object>& item,
        std::uint32_t index) noexcept override;
    void ClearContainer(
        FrameworkElement& container) noexcept override;
    void OnContainersChanged() noexcept override;

private:
    friend struct ::Aero::Visual::Impl;
    Base::Vector<std::uint32_t> selectedIndices_;
    std::uint32_t primaryIndex_ = UINT32_MAX;
    std::uint32_t pendingIndex_ = UINT32_MAX;
    // A bound SelectedItem can arrive before a delayed ItemsSource. Retain it
    // until its matching item materializes instead of writing null back
    // through the TwoWay binding.
    Base::Ref<Base::Object> pendingSelectedItem_;
    SelectionChangedHandler selectionChanged_;
    ItemsChangedHandler itemsChangedHandler_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    Base::Status lastSelectionError_;
    VisualStateManager* states_ = nullptr;
    DependencyPropertyHandle activeProperty_;
    bool synchronizingProperties_ = false;

    void OnItemsChanged(
        const ItemsChangedEvent& event) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    Base::Result<bool> ApplySelection(
        Base::Span<const std::uint32_t> indices,
        std::uint32_t primaryIndex) noexcept;
    Base::Result<void> PublishProperties() noexcept;
    void SyncContainers() noexcept;
};

} // namespace Primitives

class AERO_API ListBox : public Primitives::Selector {
    AERO_DECLARE_TYPE(ListBox, Primitives::Selector)
public:
    struct Impl;

    ListBox() noexcept : Primitives::Selector(StaticTypeId()) {}
    ~ListBox() override;

    Base::Result<bool> BringIntoView(
        std::uint32_t index) noexcept;

protected:
    explicit ListBox(TypeId runtimeType) noexcept
        : Primitives::Selector(runtimeType) {}
    Base::Result<Base::Ref<FrameworkElement>>
        CreateContainer(
            const Base::Ref<Base::Object>& item) noexcept override;

private:
    friend struct Impl;
    void* interactions_ = nullptr;
};

class AERO_API ComboBoxItem
    : public ListBoxItem {
    AERO_DECLARE_TYPE(ComboBoxItem, ListBoxItem)
public:
    ComboBoxItem() noexcept
        : ListBoxItem(StaticTypeId()) {}
    ~ComboBoxItem() override = default;

    bool GetIsSelected() const noexcept;
    void SetIsSelected(
        bool value) noexcept;

    inline static constexpr Members::Property<bool> IsSelectedProperty{"IsSelected"};
};


class AERO_API ComboBox : public Primitives::Selector {
    AERO_DECLARE_TYPE(ComboBox, Primitives::Selector)
public:
    struct Impl;

    ComboBox() noexcept;
    ~ComboBox() override;

    bool GetIsDropDownOpen() const noexcept;
    void SetIsDropDownOpen(
        bool value) noexcept;
    double GetMaxDropDownHeight() const noexcept;
    void SetMaxDropDownHeight(
        double value) noexcept;
    bool GetIsEditable() const noexcept;
    void SetIsEditable(
        bool value) noexcept;
    bool GetIsReadOnly() const noexcept;
    void SetIsReadOnly(
        bool value) noexcept;
    Base::StringView GetText() const noexcept;
    void SetText(
        Base::StringView value) noexcept;
    Base::StringView GetPlaceholder() const noexcept {
        return GetValueOr(
            PlaceholderProperty, Base::StringView{});
    }
    void SetPlaceholder(
        Base::StringView value) noexcept {
        SetValue(PlaceholderProperty, value);
    }
    Base::String GetSelectionBoxText() const noexcept;
    Meta::Value GetSelectionBoxItem() const noexcept {
        return GetValueOr(
            SelectionBoxItemProperty,
            Meta::Value::NullObject(
                Meta::TypeOf<Base::Object>()));
    }

    inline static constexpr Members::RoutedEvent<RoutedEventArgs> DropDownOpenedEvent{"DropDownOpened"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> DropDownClosedEvent{"DropDownClosed"};
    UIElement::Event<RoutedEventArgs>
        DropDownOpened() noexcept {
        return GetEvent(DropDownOpenedEvent);
    }
    UIElement::Event<RoutedEventArgs>
        DropDownClosed() noexcept {
        return GetEvent(DropDownClosedEvent);
    }

    inline static constexpr Members::Property<bool> IsDropDownOpenProperty{"IsDropDownOpen"};
    inline static constexpr Members::Property<double> MaxDropDownHeightProperty{"MaxDropDownHeight"};
    inline static constexpr Members::Property<bool> IsEditableProperty{"IsEditable"};
    inline static constexpr Members::Property<bool> IsReadOnlyProperty{"IsReadOnly"};
    inline static constexpr Members::Property<Base::String> TextProperty{"Text"};
    inline static constexpr Members::Property<Base::String> PlaceholderProperty{"Placeholder"};
    inline static constexpr Members::ReadOnlyProperty<Base::String> SelectionBoxTextProperty{"SelectionBoxText"};
    inline static constexpr Members::ReadOnlyProperty<Meta::Value> SelectionBoxItemProperty{"SelectionBoxItem"};

protected:
    Base::Result<Base::Ref<FrameworkElement>>
        CreateContainer(
            const Base::Ref<Base::Object>& item)
            noexcept override;
    Base::Result<void> PrepareContainer(
        FrameworkElement& container,
        const Base::Ref<Base::Object>& item,
        std::uint32_t index) noexcept override;
    void ClearContainer(
        FrameworkElement& container) noexcept override;
    void OnContainersChanged() noexcept override;
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;

private:
    friend struct Impl;
    void* interactions_ = nullptr;
    TextBlock* selectionBox_ = nullptr;
    ContentPresenter* selectionPresenter_ =
        nullptr;
    TextBox* editableTextBox_ = nullptr;
    Primitives::Popup* popup_ = nullptr;
    FrameworkElement* dropDownBorder_ = nullptr;
    SelectionChangedHandler selectionChangedHandler_;
    DependencyPropertyChangedEventHandler
        dropDownChangedHandler_;
    DependencyPropertyChangedEventHandler
        maxDropDownHeightChangedHandler_;
    DependencyPropertyChangedEventHandler
        editableChangedHandler_;
    DependencyPropertyChangedEventHandler
        textChangedHandler_;
    DependencyPropertyChangedEventHandler
        foregroundChangedHandler_;
    DependencyPropertyChangedEventHandler
        selectedValueChangedHandler_;
    RoutedEventHandler editableTextChangedHandler_;
    bool synchronizingEditableText_ = false;

    void OnSelectionChanged(
        Selector& selector,
        const SelectionChangedEvent& event)
        noexcept;
    void OnDropDownPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnMaxDropDownHeightPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnEditablePropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnTextPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnForegroundPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnSelectedValuePropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnEditableTextChanged(
        Base::Object* sender,
        RoutedEventArgs& args) noexcept;
    Base::Result<void>
        UpdateSelectionBox() noexcept;
    Base::Result<void>
        UpdateEditableVisualState() noexcept;
    void SynchronizeContainers() noexcept;
    std::uint32_t FindContainerIndex(
        Base::Object* source) const noexcept;
};



} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::SelectionMode)

namespace Aero::Controls {

enum class GridViewColumnHeaderRole : std::uint8_t {
    Normal = 0U,
    Floating,
    Padding
};

// Standard GridView header container. The presenter owns header generation
// and interaction; this control supplies the WPF-visible Role state used by
// the default templates.
class AERO_API GridViewColumnHeader
    : public ContentControl {
    AERO_DECLARE_TYPE(GridViewColumnHeader, ContentControl)
public:
    GridViewColumnHeader() noexcept
        : ContentControl(StaticTypeId()) {}

    GridViewColumnHeaderRole GetRole() const noexcept {
        return GetValueOr(
            RoleProperty, GridViewColumnHeaderRole::Normal);
    }
    void SetRole(
        GridViewColumnHeaderRole value) noexcept {
        SetValue(RoleProperty, value);
    }

    inline static constexpr Members::Property<GridViewColumnHeaderRole> RoleProperty{"Role"};
};

class AERO_API GridViewColumn
    : public DependencyObject {
    AERO_DECLARE_TYPE(GridViewColumn, DependencyObject)
public:
    GridViewColumn() noexcept
        : DependencyObject(StaticTypeId()) {}
    Base::StringView GetHeader() const noexcept;
    void SetHeader(
        Base::StringView value) noexcept;
    double GetWidth() const noexcept;
    void SetWidth(
        double value) noexcept;
    Base::Ref<DataTemplate>
        GetCellTemplate() const noexcept;
    void SetCellTemplate(
        Base::Ref<DataTemplate> value) noexcept;
    Base::Ref<DataTemplate>
        GetHeaderTemplate() const noexcept;
    void SetHeaderTemplate(
        Base::Ref<DataTemplate> value) noexcept;
    Base::StringView GetDisplayMemberPath()
        const noexcept;
    void SetDisplayMemberPath(
        Base::StringView value) noexcept;
    Base::Ref<Aero::Data::Binding>
        GetDisplayMemberBinding() const noexcept;
    void SetDisplayMemberBinding(
        Base::Ref<Aero::Data::Binding> value) noexcept;
    Base::Ref<Style> GetHeaderContainerStyle() const noexcept {
        return GetValueOr(
            HeaderContainerStyleProperty,
            Base::Ref<Style>{});
    }
    void SetHeaderContainerStyle(
        Base::Ref<Style> value) noexcept {
        SetValue(HeaderContainerStyleProperty, std::move(value));
    }

    inline static constexpr Members::Property<Base::String> HeaderProperty{"Header"};
    inline static constexpr Members::Property<double> WidthProperty{"Width"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> CellTemplateProperty{"CellTemplate"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};
    inline static constexpr Members::Property<Base::String> DisplayMemberPathProperty{"DisplayMemberPath"};
    inline static constexpr Members::Property<Base::Ref<Aero::Data::Binding>> DisplayMemberBindingProperty{"DisplayMemberBinding"};
    inline static constexpr Members::Property<Base::Ref<Style>> HeaderContainerStyleProperty{"HeaderContainerStyle"};
};

class AERO_API GridView
    : public Base::Object {
    AERO_DECLARE_TYPE(GridView, Base::Object)
public:
    GridView() noexcept = default;
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Span<const Base::Ref<GridViewColumn>>
        GetColumns() const noexcept {
        return {
            columns_.Data(),
            columns_.Size()};
    }
    Base::Result<void> AddColumn(
        Base::Ref<GridViewColumn> column)
        noexcept;
    void ClearColumns() noexcept {
        columns_.Clear();
    }
    Base::Ref<Style> GetColumnHeaderContainerStyle() const noexcept {
        return columnHeaderContainerStyle_;
    }
    void SetColumnHeaderContainerStyle(
        Base::Ref<Style> value) noexcept {
        columnHeaderContainerStyle_ = std::move(value);
        return;
    }

private:
    Base::Vector<Base::Ref<GridViewColumn>>
        columns_;
    Base::Ref<Style> columnHeaderContainerStyle_;
};

// Hosts GridView column headers inside the ListView ScrollViewer template.
// The column collection is normally supplied by a template binding from the
// owning ListView's GridView and is consumed by the view implementation.
class AERO_API GridViewHeaderRowPresenter
    : public Aero::FrameworkElement {
    AERO_DECLARE_TYPE(
        GridViewHeaderRowPresenter,
        Aero::FrameworkElement)
public:
    GridViewHeaderRowPresenter() noexcept
        : FrameworkElement(StaticTypeId()) {}

    bool GetAllowsColumnReorder() const noexcept {
        return GetValueOr(AllowsColumnReorderProperty, false);
    }
    void SetAllowsColumnReorder(bool value) noexcept {
        SetValue(AllowsColumnReorderProperty, value);
    }

    inline static constexpr Members::Property<bool> AllowsColumnReorderProperty{"AllowsColumnReorder"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ColumnHeaderContainerStyleProperty{"ColumnHeaderContainerStyle"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ColumnHeaderContextMenuProperty{"ColumnHeaderContextMenu"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ColumnHeaderTemplateProperty{"ColumnHeaderTemplate"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ColumnHeaderTemplateSelectorProperty{"ColumnHeaderTemplateSelector"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ColumnHeaderToolTipProperty{"ColumnHeaderToolTip"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ColumnsProperty{"Columns"};
};

// The row counterpart to GridViewHeaderRowPresenter. It is instantiated by
// ListViewItem templates and receives the active GridView columns/content
// during ListView container realization.
class AERO_API GridViewRowPresenter
    : public Aero::FrameworkElement {
    AERO_DECLARE_TYPE(
        GridViewRowPresenter,
        Aero::FrameworkElement)
public:
    GridViewRowPresenter() noexcept
        : FrameworkElement(StaticTypeId()) {}

    inline static constexpr Members::Property<Base::Ref<Base::Object>> ColumnsProperty{"Columns"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ContentProperty{"Content"};
};

class AERO_API ListViewItem
    : public ListBoxItem {
    AERO_DECLARE_TYPE(ListViewItem, ListBoxItem)
public:
    ListViewItem() noexcept
        : ListBoxItem(StaticTypeId()) {}
    ~ListViewItem() override = default;
};

class AERO_API ListView
    : public ListBox {
    AERO_DECLARE_TYPE(ListView, ListBox)
public:
    ListView() noexcept
        : ListBox(StaticTypeId()) {}
    ~ListView() override = default;

    Base::Ref<GridView> GetView() const noexcept;
    void SetView(
        Base::Ref<GridView> value) noexcept;

    inline static constexpr Members::Property<Base::Ref<GridView>> ViewProperty{"View"};

protected:
    void
        OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    Base::Result<Base::Ref<FrameworkElement>>
        CreateContainer(
            const Base::Ref<Base::Object>& item)
            noexcept override;

private:
    TextBlock* columnHeaders_ = nullptr;
    Base::Result<void>
        SynchronizeColumnHeaders() noexcept;
};

} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::GridViewColumnHeaderRole)

namespace Aero::Controls {

class VisualStateManager;

class AERO_API TreeViewItem
    : public HeaderedItemsControl,
      private Collections::IItemsSource {
    AERO_DECLARE_TYPE(TreeViewItem, HeaderedItemsControl)
public:
    TreeViewItem() noexcept;
    ~TreeViewItem() override;

    Base::StringView GetHeader() const noexcept;
    void SetHeader(
        Base::StringView value) noexcept;
    Base::StringView GetIcon() const noexcept;
    void SetIcon(
        Base::StringView value) noexcept;
    Base::Ref<DataTemplate>
        GetHeaderTemplate() const noexcept;
    void SetHeaderTemplate(
        Base::Ref<DataTemplate> value) noexcept;
    bool GetIsExpanded() const noexcept;
    void SetIsExpanded(
        bool value) noexcept;
    bool GetIsSelected() const noexcept;
    void SetIsSelected(
        bool value) noexcept;
    bool GetHasItems() const noexcept {
        return GetValueOr(HasItemsProperty, false);
    }

    ItemCollection& GetItems() noexcept {
        return items_;
    }
    const ItemCollection& GetItems() const noexcept {
        return items_;
    }
    inline static constexpr Members::Property<Base::String> HeaderProperty{"Header"};
    inline static constexpr Members::Property<Base::String> IconProperty{"Icon"};
    inline static constexpr Members::Property<Base::Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};
    inline static constexpr Members::Property<bool> IsExpandedProperty{"IsExpanded"};
    inline static constexpr Members::Property<bool> IsSelectedProperty{"IsSelected"};
    inline static constexpr Members::ReadOnlyProperty<bool> HasItemsProperty{"HasItems"};
    // WPF item hosts accept an ItemsPanelTemplate from a style. The current
    // tree realization retains the value while it supplies its own host.
    inline static constexpr Members::Property<Base::Ref<ItemsPanelTemplate>> ItemsPanelProperty{"ItemsPanel"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> ExpandedEvent{"Expanded"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> CollapsedEvent{"Collapsed"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> SelectedEvent{"Selected"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> UnselectedEvent{"Unselected"};

protected:
    explicit TreeViewItem(TypeId runtimeType) noexcept;
    void
        OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;

private:
    friend struct ::Aero::Visual::Impl;
    // The collection protocol is an implementation detail used by the
    // generated child ItemsControl; it is intentionally not part of the
    // TreeViewItem SDK surface.
    std::uint32_t GetCount() const noexcept override {
        return items_.GetCount();
    }
    Base::Ref<Base::Object> GetItem(
        std::uint32_t index) const noexcept override {
        return items_.GetItem(index);
    }
    void AddItemsChanged(
        const ItemsChangedHandler& handler)
        noexcept override {
        items_.AddItemsChanged(handler);
    }
    bool RemoveItemsChanged(
        const ItemsChangedHandler& handler)
        noexcept override {
        return items_.RemoveItemsChanged(handler);
    }
    ItemCollection items_;
    TextBlock* headerText_ = nullptr;
    TextBlock* iconText_ = nullptr;
    TextBlock* expanderGlyph_ = nullptr;
    ItemsControl* childItems_ = nullptr;
    DependencyPropertyChangedEventHandler
        headerChangedHandler_;
    DependencyPropertyChangedEventHandler
        iconChangedHandler_;
    DependencyPropertyChangedEventHandler
        expandedChangedHandler_;
    DependencyPropertyChangedEventHandler
        selectedChangedHandler_;
    ItemsChangedHandler itemsChangedHandler_;

    void OnHeaderChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnExpandedChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnSelectedChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnItemsChanged(
        const ItemsChangedEvent& event) noexcept;
    Base::Result<void>
        SynchronizeTemplate() noexcept;
};

class AERO_API TreeView
    : public ItemsControl {
    AERO_DECLARE_TYPE(TreeView, ItemsControl)
public:
    struct Impl;

    TreeView() noexcept
        : ItemsControl(StaticTypeId()) {}
    ~TreeView() override;

    Base::Ref<Base::Object>
        GetSelectedItem() const noexcept;
    bool SelectItem(
        TreeViewItem* item) noexcept;
    inline static constexpr Members::ReadOnlyProperty<Base::Ref<Base::Object>> SelectedItemProperty{"SelectedItem"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> SelectedItemChangedEvent{"SelectedItemChanged"};

protected:
    Base::Result<Base::Ref<FrameworkElement>>
        CreateContainer(
            const Base::Ref<Base::Object>& item)
            noexcept override;

private:
    friend struct Impl;
    void* interactions_ =
        nullptr;
    VisualStateManager* states_ = nullptr;
};


} // namespace Aero::Controls

namespace Aero::Controls {

enum class ScrollUnit : std::uint8_t { Item = 0U, Pixel };
enum class VirtualizationMode : std::uint8_t { Standard = 0U, Recycling };

// WPF attached-property owner shared by all virtualizing panels. The current
// panel implementation is pixel-based; exposing this owner preserves the
// authored contract while item-unit realization is added.
class AERO_API VirtualizingPanel : public Base::Object {
    AERO_DECLARE_TYPE(VirtualizingPanel, Base::Object)
public:
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    inline static constexpr Members::AttachedProperty<ScrollUnit> ScrollUnitProperty{"ScrollUnit"};
    inline static constexpr Members::AttachedProperty<VirtualizationMode> VirtualizationModeProperty{"VirtualizationMode"};
};

class AERO_API VirtualizingStackPanel
    : public Panel,
      public IScrollInfo {
    AERO_DECLARE_TYPE(VirtualizingStackPanel, Panel)
public:
    VirtualizingStackPanel() noexcept;
    ~VirtualizingStackPanel() override;

    Orientation GetOrientation() const noexcept;
    void SetOrientation(
        Orientation value) noexcept;
    std::uint32_t GetOverscanCount() const noexcept;
    void SetOverscanCount(
        std::uint32_t value) noexcept;
    double GetEstimatedItemExtent() const noexcept;
    void SetEstimatedItemExtent(
        double value) noexcept;

    std::uint32_t GetVisibleFirstIndex() const noexcept {
        return visibleFirstIndex_;
    }
    std::uint32_t GetVisibleCount() const noexcept {
        return visibleCount_;
    }
    std::uint32_t GetRealizedFirstIndex() const noexcept {
        return desiredFirstIndex_;
    }
    std::uint32_t GetRealizedCount() const noexcept {
        return desiredCount_;
    }
    double GetItemExtent(
        std::uint32_t index) const noexcept;
    double GetItemOffset(
        std::uint32_t index) const noexcept;

    ScrollData GetData() const noexcept override {
        return data_;
    }
    void SetViewport(
        Size viewport) noexcept override;
    void SetHorizontalOffset(
        double value) noexcept override;
    void SetVerticalOffset(
        double value) noexcept override;
    Base::Result<bool> LineHorizontal(
        double direction) noexcept override;
    Base::Result<bool> LineVertical(
        double direction) noexcept override;
    Base::Result<bool> PageHorizontal(
        double direction) noexcept override;
    Base::Result<bool> PageVertical(
        double direction) noexcept override;

    inline static constexpr Members::Property<Orientation> OrientationProperty{"Orientation"};
    inline static constexpr Members::Property<std::uint32_t> OverscanCountProperty{"OverscanCount"};
    inline static constexpr Members::Property<double> EstimatedItemExtentProperty{"EstimatedItemExtent"};

protected:
    void OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept override;
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;

private:
    friend class ItemContainerGenerator;
    friend struct ::Aero::Visual::Impl;

    ItemContainerGenerator* generator_ = nullptr;
    Base::Vector<double> itemExtents_;
    Base::Vector<double> extentTree_;
    ScrollData data_;
    double crossExtent_ = 0.0;
    double estimatedItemExtent_ = 24.0;
    std::uint32_t overscanCount_ = 2U;
    Orientation orientation_ = Orientation::Vertical;
    std::uint32_t visibleFirstIndex_ = 0U;
    std::uint32_t visibleCount_ = 0U;
    std::uint32_t desiredFirstIndex_ = 0U;
    std::uint32_t desiredCount_ = 0U;

    Base::Result<void> AttachGenerator(
        ItemContainerGenerator& generator,
        std::uint32_t itemCount) noexcept;
    void DetachGenerator(
        ItemContainerGenerator& generator) noexcept;
    Base::Result<void> HandleItemsChanged(
        const ItemsChangedEvent& event,
        std::uint32_t itemCount) noexcept;
    Base::Result<void> ResizeExtentCache(
        std::uint32_t itemCount) noexcept;
    Base::Result<void> ApplyExtentDelta(
        const ItemsChangedEvent& event,
        std::uint32_t itemCount) noexcept;
    Base::Result<void> UpdateRealization(
        bool notifyGenerator) noexcept;
    void CalculateRealizationRange() noexcept;
    std::uint32_t ItemIndexAtOffset(
        double offset) const noexcept;
    double MainOffset() const noexcept;
    double MainViewport() const noexcept;
    double MainExtent() const noexcept;
    void SetMainOffset(double value) noexcept;
    void SetMainExtent(double value) noexcept;
    void ClampOffsets() noexcept;
    double ExtentForIndex(
        std::uint32_t index) const noexcept;
    Base::Result<void> RebuildExtentTree() noexcept;
    void AddExtentDeviation(
        std::uint32_t index,
        double delta) noexcept;
    double PrefixDeviation(
        std::uint32_t count) const noexcept;
    void SetMeasuredExtent(
        std::uint32_t index,
        double value) noexcept;
    void SetMainScrollOffset(
        double value) noexcept;
    void SetCrossScrollOffset(
        double value) noexcept;
};

} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::ScrollUnit)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::VirtualizationMode)
