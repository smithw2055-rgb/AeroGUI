#pragma once

#include <Aero/Controls/HeaderedItemsControl.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Base/Delegate.hpp>

namespace Aero::Controls {
namespace Primitives { class ToggleButton; }
using ::Aero::DependencyProperty;
using ::Aero::RoutedEvent;
using ::Aero::RoutedEventArgs;
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
    void SetHeader(StringView value) noexcept;
    StringView GetIcon() const noexcept;
    void SetIcon(StringView value) noexcept;
    Ref<DataTemplate>
        GetHeaderTemplate() const noexcept;
    void SetHeaderTemplate(Ref<DataTemplate> value) noexcept;
    bool GetIsExpanded() const noexcept;
    void SetIsExpanded(bool value) noexcept;
    bool GetIsSelected() const noexcept;
    void SetIsSelected(bool value) noexcept;
    bool GetHasItems() const noexcept {
        return ItemsControl::GetHasItems();
    }
    void BeginExpanderGesture() noexcept;
    void ApplyExpanderGesture() noexcept;
    // Same DP as HeaderedItemsControl so ContentSource="Header" TemplateBindings
    // and SetHeader share one store. A second Header DP left PART_Header empty
    // for generated SampleTemplate visuals.
    inline static constexpr auto HeaderProperty = HeaderedItemsControl::HeaderProperty;
    AERO_DEPENDENCY_PROPERTY(String, Icon);
    inline static constexpr auto HeaderTemplateProperty = HeaderedItemsControl::HeaderTemplateProperty;
    AERO_DEPENDENCY_PROPERTY(bool, IsExpanded);
    AERO_DEPENDENCY_PROPERTY(bool, IsSelected);
    inline static constexpr RoutedEvent<RoutedEventArgs> ExpandedEvent{"Expanded"};
    inline static constexpr RoutedEvent<RoutedEventArgs> CollapsedEvent{"Collapsed"};
    inline static constexpr RoutedEvent<RoutedEventArgs> SelectedEvent{"Selected"};
    inline static constexpr RoutedEvent<RoutedEventArgs> UnselectedEvent{"Unselected"};

protected:
    virtual void OnExpanded(RoutedEventArgs& e);
    virtual void OnCollapsed(RoutedEventArgs& e);
    virtual void OnSelected(RoutedEventArgs& e);
    virtual void OnUnselected(RoutedEventArgs& e);
    explicit TreeViewItem(TypeId runtimeType) noexcept;
    Result<Ref<FrameworkElement>>
        CreateContainer(
            const Ref<Base::Object>& item)
            noexcept override;
    void
        OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    void OnPropertyChanged(
        const DependencyPropertyChangedEventArgs& args) noexcept override;

private:
    friend class ItemsControl;
    friend class TreeView;
    TextBlock* headerText_ = nullptr;
    TextBlock* iconText_ = nullptr;
    TextBlock* expanderGlyph_ = nullptr;
    ItemsControl* childItems_ = nullptr;
    UIElement* itemsBorder_ = nullptr;
    UIElement* itemsHostPresenter_ = nullptr;
    Primitives::ToggleButton* expandButton_ = nullptr;
    Ref<Base::Object> hierarchicalItemsSource_;
    Ref<Data::Binding> hierarchicalItemsBinding_;
    Ref<Base::Object> hierarchicalBindingSource_;
    Ref<DataTemplate> hierarchicalItemTemplate_;
    Base::Delegate<void(Base::Object*, RoutedEventArgs&)>
        expandClickHandler_;
    bool expanderGestureActive_ = false;
    bool expanderGestureTarget_ = false;
    bool expanderGestureArmed_ = false;
    void OnExpandButtonClick(
        Base::Object* sender,
        RoutedEventArgs& args) noexcept;
    Result<void>
        SynchronizeTemplate() noexcept;
    void ProjectHeaderContent() noexcept;
    void ProjectRealizedHeaders() noexcept;
    void SetHierarchicalContent(Ref<Base::Object> source, Ref<DataTemplate> itemTemplate) noexcept;
    void SetHierarchicalBinding(Ref<Data::Binding> binding, Ref<Base::Object> source, Ref<DataTemplate> itemTemplate) noexcept;
    void ClearHierarchicalContent() noexcept;
    void ActivateHierarchicalContent() noexcept;
};

} // namespace Aero::Controls
