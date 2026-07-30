#pragma once

#include <Aero/Controls/Items.hpp>
#include <Aero/Presentation/Input.hpp>

namespace Aero::Controls {

class TreeViewInteractionManager;
class VisualStateManager;

class AERO_API TreeViewItem
    : public ItemContainer,
      public IItemsSource {
    AERO_DECLARE_TYPE(TreeViewItem, ItemContainer)
public:
    TreeViewItem() noexcept;
    ~TreeViewItem() override;

    Base::StringView Header() const noexcept;
    Base::Result<void> SetHeader(
        Base::StringView value) noexcept;
    Base::StringView Icon() const noexcept;
    Base::Result<void> SetIcon(
        Base::StringView value) noexcept;
    Base::Ref<DataTemplate>
        HeaderTemplate() const noexcept;
    Base::Result<void> SetHeaderTemplate(
        Base::Ref<DataTemplate> value) noexcept;
    bool IsExpanded() const noexcept;
    Base::Result<void> SetIsExpanded(
        bool value) noexcept;
    bool IsSelected() const noexcept;
    Base::Result<void> SetIsSelected(
        bool value) noexcept;
    bool HasItems() const noexcept {
        return GetValueOr(HasItemsProperty, false);
    }

    ItemsCollection& Items() noexcept {
        return items_;
    }
    const ItemsCollection& Items() const noexcept {
        return items_;
    }
    std::uint32_t Count() const noexcept override {
        return items_.Count();
    }
    Base::Ref<Base::Object> ItemAt(
        std::uint32_t index) const noexcept override {
        return items_.ItemAt(index);
    }
    Base::Result<void> TryAddItemsChanged(
        const ItemsChangedHandler& handler)
        noexcept override {
        return items_.TryAddItemsChanged(handler);
    }
    bool RemoveItemsChanged(
        const ItemsChangedHandler& handler)
        noexcept override {
        return items_.RemoveItemsChanged(handler);
    }

    inline static constexpr Members::Property<
        Base::String> HeaderProperty{"Header"};
    inline static constexpr Members::Property<
        Base::String> IconProperty{"Icon"};
    inline static constexpr Members::Property<
        Base::Ref<DataTemplate>>
        HeaderTemplateProperty{"HeaderTemplate"};
    inline static constexpr Members::Property<bool>
        IsExpandedProperty{"IsExpanded"};
    inline static constexpr Members::Property<bool>
        IsSelectedProperty{"IsSelected"};
    inline static constexpr Members::ReadOnlyProperty<bool>
        HasItemsProperty{"HasItems"};
    // WPF item hosts accept an ItemsPanelTemplate from a style. The current
    // tree realization retains the value while it supplies its own host.
    inline static constexpr Members::Property<
        Base::Ref<ItemsPanelTemplate>>
        ItemsPanelProperty{"ItemsPanel"};
    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs> ExpandedEvent{"Expanded"};
    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs> CollapsedEvent{"Collapsed"};
    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs> SelectedEvent{"Selected"};
    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs> UnselectedEvent{"Unselected"};

protected:
    explicit TreeViewItem(TypeId runtimeType) noexcept;
    Base::Result<void>
        OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;

private:
    ItemsCollection items_;
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

class AERO_API TreeView final
    : public ItemsControl {
    AERO_DECLARE_TYPE(TreeView, ItemsControl)
public:
    TreeView() noexcept
        : ItemsControl(StaticTypeId()) {}
    ~TreeView() override;

    Base::Ref<Base::Object>
        SelectedItem() const noexcept;
    Base::Result<bool> SelectItem(
        TreeViewItem* item) noexcept;
    inline static constexpr Members::ReadOnlyProperty<
        Base::Ref<Base::Object>>
        SelectedItemProperty{"SelectedItem"};
    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs>
        SelectedItemChangedEvent{
            "SelectedItemChanged"};

protected:
    Base::Result<Base::Ref<ItemContainer>>
        CreateContainer(
            const Base::Ref<Base::Object>& item)
            noexcept override;

private:
    friend class TreeViewInteractionManager;
    TreeViewInteractionManager* interactions_ =
        nullptr;
    VisualStateManager* states_ = nullptr;
};

class AERO_API TreeViewInteractionManager final {
public:
    TreeViewInteractionManager(
        ObjectTree& tree,
        RoutedEventManager& events,
        FocusManager& focus,
        VisualStateManager* states = nullptr)
        noexcept;
    ~TreeViewInteractionManager() noexcept;

    Base::Result<void> Attach(
        TreeView& treeView) noexcept;
    Base::Result<bool> Detach(
        TreeView& treeView) noexcept;

private:
    ObjectTree* tree_ = nullptr;
    [[maybe_unused]]
    RoutedEventManager* events_ = nullptr;
    FocusManager* focus_ = nullptr;
    VisualStateManager* states_ = nullptr;
    Base::Vector<VisualHandle> records_;
    MouseButtonEventHandler mouseDownHandler_;
    KeyEventHandler keyDownHandler_;

    std::uint32_t FindTreeView(
        const TreeView& treeView) const noexcept;
    TreeView* ResolveTreeView(
        std::uint32_t index) noexcept;
    TreeViewItem* FindItem(
        TreeView& treeView,
        Base::Object* source) const noexcept;
    Base::Result<void> CollectVisibleItems(
        Visual& parent,
        Base::Vector<TreeViewItem*>& items)
        noexcept;
    void OnMouseDown(
        Base::Object* sender,
        const MouseButtonEventArgs& args)
        noexcept;
    void OnKeyDown(
        Base::Object* sender,
        const KeyEventArgs& args) noexcept;
};

} // namespace Aero::Controls
