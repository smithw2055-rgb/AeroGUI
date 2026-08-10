#pragma once

#include <Aero/Controls/HeaderedItemsControl.hpp>
#include <Aero/Controls/TextBlock.hpp>

namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::TypeId;
class AERO_GUI_API TreeViewItem
    : public HeaderedItemsControl {
    AERO_DECLARE_TYPE(TreeViewItem, HeaderedItemsControl)
public:
    TreeViewItem() noexcept;
    ~TreeViewItem() override;

    Value GetHeader() const noexcept;
    void SetHeader(Value value) noexcept;
    Result<void> SetHeader(StringView value) noexcept;
    StringView GetIcon() const noexcept;
    void SetIcon(
        StringView value) noexcept;
    Ref<DataTemplate>
        GetHeaderTemplate() const noexcept;
    void SetHeaderTemplate(
        Ref<DataTemplate> value) noexcept;
    bool GetIsExpanded() const noexcept;
    void SetIsExpanded(
        bool value) noexcept;
    bool GetIsSelected() const noexcept;
    void SetIsSelected(
        bool value) noexcept;
    bool GetHasItems() const noexcept {
        return ItemsControl::GetHasItems();
    }
    inline static constexpr DependencyProperty<Value> HeaderProperty{"Header"};
    inline static constexpr DependencyProperty<String> IconProperty{"Icon"};
    inline static constexpr DependencyProperty<Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};
    inline static constexpr DependencyProperty<bool> IsExpandedProperty{"IsExpanded"};
    inline static constexpr DependencyProperty<bool> IsSelectedProperty{"IsSelected"};
    inline static constexpr RoutedEvent<RoutedEventArgs> ExpandedEvent{"Expanded"};
    inline static constexpr RoutedEvent<RoutedEventArgs> CollapsedEvent{"Collapsed"};
    inline static constexpr RoutedEvent<RoutedEventArgs> SelectedEvent{"Selected"};
    inline static constexpr RoutedEvent<RoutedEventArgs> UnselectedEvent{"Unselected"};

protected:
    explicit TreeViewItem(TypeId runtimeType) noexcept;
    Result<Ref<FrameworkElement>>
        CreateContainer(
            const Ref<Base::Object>& item)
            noexcept override;
    void
        OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;

private:
#if defined(AERO_GUI_IMPLEMENTATION)
    friend struct ::Aero::Media::Visual::Access;
#endif
    friend class ItemsControl;
    TextBlock* headerText_ = nullptr;
    TextBlock* iconText_ = nullptr;
    TextBlock* expanderGlyph_ = nullptr;
    ItemsControl* childItems_ = nullptr;
    Ref<Base::Object> hierarchicalItemsSource_;
    Ref<Data::Binding> hierarchicalItemsBinding_;
    Ref<Base::Object> hierarchicalBindingSource_;
    Ref<DataTemplate> hierarchicalItemTemplate_;
    DependencyPropertyChangedEventHandler
        headerChangedHandler_;
    DependencyPropertyChangedEventHandler
        iconChangedHandler_;
    DependencyPropertyChangedEventHandler
        expandedChangedHandler_;
    DependencyPropertyChangedEventHandler
        selectedChangedHandler_;

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
    Result<void>
        SynchronizeTemplate() noexcept;
    void SetHierarchicalContent(
        Ref<Base::Object> source,
        Ref<DataTemplate> itemTemplate) noexcept;
    void SetHierarchicalBinding(
        Ref<Data::Binding> binding,
        Ref<Base::Object> source,
        Ref<DataTemplate> itemTemplate) noexcept;
    void ClearHierarchicalContent() noexcept;
    void ActivateHierarchicalContent() noexcept;
};

class AERO_GUI_API TreeView
    : public ItemsControl {
    AERO_DECLARE_TYPE(TreeView, ItemsControl)
#if defined(AERO_GUI_IMPLEMENTATION)
public:
#else
private:
#endif
    struct Access;

public:

    TreeView() noexcept
        : ItemsControl(StaticTypeId()) {}
    ~TreeView() override;

    Ref<Base::Object>
        GetSelectedItem() const noexcept;
    bool SelectItem(
        TreeViewItem* item) noexcept;
    inline static constexpr ReadOnlyDependencyProperty<Ref<Base::Object>> SelectedItemProperty{"SelectedItem"};
    inline static constexpr RoutedEvent<RoutedEventArgs> SelectedItemChangedEvent{"SelectedItemChanged"};

protected:
    Result<Ref<FrameworkElement>>
        CreateContainer(
            const Ref<Base::Object>& item)
            noexcept override;

private:
    friend struct Access;
};
} // namespace Aero::Controls
