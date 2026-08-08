#pragma once

#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Controls/TextBlock.hpp>

namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::TypeId;
class AERO_API TreeViewItem
    : public HeaderedItemsControl,
      private Collections::IItemsSource {
    AERO_DECLARE_TYPE(TreeViewItem, HeaderedItemsControl)
public:
    TreeViewItem() noexcept;
    ~TreeViewItem() override;

    Value GetHeader() const noexcept;
    void SetHeader(Value value) noexcept;
    Base::Result<void> SetHeader(Base::StringView value) noexcept;
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
    inline static constexpr DependencyProperty<Value> HeaderProperty{"Header"};
    inline static constexpr DependencyProperty<Base::String> IconProperty{"Icon"};
    inline static constexpr DependencyProperty<Base::Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};
    inline static constexpr DependencyProperty<bool> IsExpandedProperty{"IsExpanded"};
    inline static constexpr DependencyProperty<bool> IsSelectedProperty{"IsSelected"};
    inline static constexpr ReadOnlyDependencyProperty<bool> HasItemsProperty{"HasItems"};
    // WPF item hosts accept an ItemsPanelTemplate from a style. The current
    // tree realization retains the value while it supplies its own host.
    inline static constexpr DependencyProperty<Base::Ref<ItemsPanelTemplate>> ItemsPanelProperty{"ItemsPanel"};
    inline static constexpr RoutedEvent<RoutedEventArgs> ExpandedEvent{"Expanded"};
    inline static constexpr RoutedEvent<RoutedEventArgs> CollapsedEvent{"Collapsed"};
    inline static constexpr RoutedEvent<RoutedEventArgs> SelectedEvent{"Selected"};
    inline static constexpr RoutedEvent<RoutedEventArgs> UnselectedEvent{"Unselected"};

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
    inline static constexpr ReadOnlyDependencyProperty<Base::Ref<Base::Object>> SelectedItemProperty{"SelectedItem"};
    inline static constexpr RoutedEvent<RoutedEventArgs> SelectedItemChangedEvent{"SelectedItemChanged"};

protected:
    Base::Result<Base::Ref<FrameworkElement>>
        CreateContainer(
            const Base::Ref<Base::Object>& item)
            noexcept override;

private:
    friend struct Impl;
};
} // namespace Aero::Controls
