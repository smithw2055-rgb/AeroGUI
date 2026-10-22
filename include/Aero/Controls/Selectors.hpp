#pragma once

// List, combo, and tab controls plus their items. Selector stays separate.
#include <Aero/Controls/Control.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Events.hpp>
#include <Aero/Controls/Popup.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/DataTemplate.hpp>

namespace Aero::Data { class CollectionView; }

namespace Aero::Controls {

using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::PropertyValue;
using ::Aero::Meta::TypeId;

enum class SelectionMode : std::uint8_t {
    Single = 0U,
    Multiple,
    Extended,
};

namespace Primitives {

class AERO_GUI_API Selector : public ItemsControl {
    AERO_DECLARE_TYPE(Selector, ItemsControl)

public:
    Selector() noexcept;
    ~Selector() override;

    bool Select(std::uint32_t index) noexcept;
    bool Unselect(std::uint32_t index) noexcept;
    bool Toggle(std::uint32_t index) noexcept;
    bool SelectRange(std::uint32_t first, std::uint32_t last, bool preserveExisting = false) noexcept;
    void AddSelectionChanged(const SelectionChangedHandler& handler) noexcept { selectionChanged_.Add(handler); }
    bool RemoveSelectionChanged(const SelectionChangedHandler& handler) noexcept {
        return selectionChanged_.Remove(handler);
    }
    void SyncContainers() noexcept;

    SelectionMode GetSelectionMode() const noexcept;
    void SetSelectionMode(SelectionMode value) noexcept;
    std::uint32_t GetSelectedIndex() const noexcept;
    void SetSelectedIndex(std::uint32_t index) noexcept;
    Ref<Base::Object> GetSelectedItem() const noexcept;
    void SetSelectedItem(Ref<Base::Object> item) noexcept;
    Ref<Base::Object> GetSelectedValue() const noexcept;
    void SetSelectedValue(Ref<Base::Object> value) noexcept;
    StringView GetSelectedValuePath() const noexcept { return GetValue(SelectedValuePathProperty); }
    Span<const std::uint32_t> GetSelectedIndices() const noexcept {
        return {selectedIndices_.Data(), selectedIndices_.Size()};
    }
    std::uint32_t GetSelectedCount() const noexcept { return selectedIndices_.Size(); }
    bool GetIsSelected(std::uint32_t index) const noexcept;
    std::uint32_t GetIndexOfItem(const Base::Object* item) const noexcept;
    bool GetIsSynchronizedWithCurrentItem() const noexcept;
    void SetIsSynchronizedWithCurrentItem(bool value) noexcept;
    void ClearSelection() noexcept;

    inline static constexpr RoutedEvent<RoutedEventArgs> SelectionChangedRoutedEvent{"SelectionChanged"};
    UIElement::Event<RoutedEventArgs> SelectionChanged() noexcept { return GetEvent(SelectionChangedRoutedEvent); }

    AERO_DEPENDENCY_PROPERTY(SelectionMode, SelectionMode);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, SelectedIndex);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, SelectedItem);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, SelectedValue);
    AERO_DEPENDENCY_PROPERTY(String, SelectedValuePath);
    AERO_ATTACHED_PROPERTY(bool, IsSelected);
    AERO_DEPENDENCY_PROPERTY(bool, IsSynchronizedWithCurrentItem);

    Base::Status LastSelectionError() const noexcept { return lastSelectionError_; }

protected:
    explicit Selector(TypeId runtimeType) noexcept;

    Result<void> PrepareContainerForItemOverride(FrameworkElement& container, const Ref<Base::Object>& item,
        std::uint32_t index) noexcept override;
    void OnContainersChanged() noexcept override;
    void OnItemsChanged(const ItemsChangedEvent& event) noexcept override;
    PropertyValue CoerceValueCore(DependencyPropertyHandle property, const PropertyValue& baseValue) noexcept override;
    [[deprecated("Use OnItemsChanged()")]]
    void OnItemsSourceCoreChanged() noexcept override;
    virtual void OnSelectionChanged(const SelectionChangedEvent& event);
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;

    void ClearContainerForItemOverride(FrameworkElement& container) noexcept override;

private:
    void PushSelectionToCurrent() noexcept;
    void OnViewCurrentChanged() noexcept;
    Result<bool> ApplySelection(Span<const std::uint32_t> indices, std::uint32_t primaryIndex) noexcept;
    void PublishProperties() noexcept;
    void HookCurrentView() noexcept;
    void UnhookCurrentView() noexcept;

    Base::Vector<std::uint32_t> selectedIndices_;
    std::uint32_t primaryIndex_ = UINT32_MAX;
    std::uint32_t pendingIndex_ = UINT32_MAX;
    Ref<Base::Object> pendingSelectedItem_;
    SelectionChangedHandler selectionChanged_;
    ItemsChangedHandler itemsChangedHandler_;
    Base::Status lastSelectionError_;
    DependencyPropertyHandle activeProperty_;
    bool synchronizingProperties_ = false;
    bool synchronizingCurrent_ = false;
    Data::CollectionView* subscribedView_ = nullptr;
    Base::Delegate<void()> currentChangedHandler_;
};

} // namespace Primitives

using Primitives::Selector;

class TabControl;

class AERO_GUI_API ListBoxItem : public ContentControl {
    AERO_DECLARE_TYPE(ListBoxItem, ContentControl)

public:
    ListBoxItem() noexcept;
    ~ListBoxItem() override;

    bool GetIsSelected() const noexcept;
    void SetIsSelected(bool value) noexcept;

    void UpdateVisualState(bool useTransitions = true) noexcept;

    inline static constexpr RoutedEvent<RoutedEventArgs> SelectedEvent{"Selected"};
    inline static constexpr RoutedEvent<RoutedEventArgs> UnselectedEvent{"Unselected"};
    UIElement::Event<RoutedEventArgs> Selected() noexcept { return GetEvent(SelectedEvent); }
    UIElement::Event<RoutedEventArgs> Unselected() noexcept { return GetEvent(UnselectedEvent); }

    AERO_DEPENDENCY_PROPERTY(bool, IsSelected);

protected:
    explicit ListBoxItem(TypeId runtimeType) noexcept;
    void OnApplyTemplate() noexcept override;
    virtual void OnSelected(RoutedEventArgs& e);
    virtual void OnUnselected(RoutedEventArgs& e);
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
};

class AERO_GUI_API ComboBoxItem : public ListBoxItem {
    AERO_DECLARE_TYPE(ComboBoxItem, ListBoxItem)

public:
    ComboBoxItem() noexcept : ListBoxItem(StaticTypeId()) {}
    ~ComboBoxItem() override = default;

    bool GetIsSelected() const noexcept;
    void SetIsSelected(bool value) noexcept;

    AERO_DEPENDENCY_PROPERTY(bool, IsSelected);

protected:
    void OnMouseLeftButtonDown(MouseButtonEventArgs& args);
    void OnMouseLeftButtonUp(MouseButtonEventArgs& args);
};

class AERO_GUI_API ListBox : public Primitives::Selector {
    AERO_DECLARE_TYPE(ListBox, Primitives::Selector)

public:
    ListBox() noexcept;
    ~ListBox() override;

    Result<bool> BringIntoView(std::uint32_t index) noexcept;

protected:
    explicit ListBox(TypeId runtimeType) noexcept;
    Result<Ref<FrameworkElement>> GetContainerForItemOverride() const noexcept override;
    void OnMouseLeftButtonDown(MouseButtonEventArgs& args);
    void OnKeyDown(KeyEventArgs& args);

private:
    std::uint32_t anchorIndex_ = UINT32_MAX;

    std::uint32_t FindContainerIndex(Base::Object* source) const noexcept;
    Result<bool> ApplyUserSelection(std::uint32_t index, std::uint32_t modifiers) noexcept;
};

class AERO_GUI_API ComboBox : public Primitives::Selector {
    AERO_DECLARE_TYPE(ComboBox, Primitives::Selector)

public:
    ComboBox() noexcept;
    ~ComboBox() override;

    bool GetIsDropDownOpen() const noexcept;
    void SetIsDropDownOpen(bool value) noexcept;
    double GetMaxDropDownHeight() const noexcept;
    void SetMaxDropDownHeight(double value) noexcept;
    bool GetIsEditable() const noexcept;
    void SetIsEditable(bool value) noexcept;
    bool GetIsReadOnly() const noexcept;
    void SetIsReadOnly(bool value) noexcept;
    StringView GetText() const noexcept;
    void SetText(StringView value) noexcept;
    StringView GetPlaceholder() const noexcept { return GetValue(PlaceholderProperty); }
    void SetPlaceholder(StringView value) noexcept { SetValue(PlaceholderProperty, value); }
    StringView GetSelectionBoxText() const noexcept;
    Value GetSelectionBoxItem() const noexcept { return GetValue(SelectionBoxItemProperty); }
    Ref<Base::Object> GetSelectionBoxItemTemplate() const noexcept { return GetValue(SelectionBoxItemTemplateProperty); }

    inline static constexpr RoutedEvent<RoutedEventArgs> DropDownOpenedEvent{"DropDownOpened"};
    inline static constexpr RoutedEvent<RoutedEventArgs> DropDownClosedEvent{"DropDownClosed"};
    UIElement::Event<RoutedEventArgs> DropDownOpened() noexcept { return GetEvent(DropDownOpenedEvent); }
    UIElement::Event<RoutedEventArgs> DropDownClosed() noexcept { return GetEvent(DropDownClosedEvent); }

    AERO_DEPENDENCY_PROPERTY(bool, IsDropDownOpen);
    AERO_DEPENDENCY_PROPERTY(double, MaxDropDownHeight);
    AERO_DEPENDENCY_PROPERTY(bool, IsEditable);
    AERO_DEPENDENCY_PROPERTY(bool, IsReadOnly);
    AERO_DEPENDENCY_PROPERTY(String, Text);
    AERO_DEPENDENCY_PROPERTY(String, Placeholder);
    AERO_READONLY_PROPERTY(String, SelectionBoxText);
    AERO_READONLY_PROPERTY(Value, SelectionBoxItem);
    AERO_READONLY_PROPERTY(Ref<Base::Object>, SelectionBoxItemTemplate);

protected:
    Result<Ref<FrameworkElement>> GetContainerForItemOverride() const noexcept override;
    Result<void> PrepareContainerForItemOverride(FrameworkElement& container, const Ref<Base::Object>& item,
        std::uint32_t index) noexcept override;
    void ClearContainerForItemOverride(FrameworkElement& container) noexcept override;
    void OnContainersChanged() noexcept override;
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    void OnSelectionChanged(const SelectionChangedEvent& event) override;
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    void OnMouseLeftButtonDown(MouseButtonEventArgs& args);
    void OnKeyDown(KeyEventArgs& args);

private:
    TextBlock* selectionBox_ = nullptr;
    ContentPresenter* selectionPresenter_ = nullptr;
    TextBox* editableTextBox_ = nullptr;
    Primitives::Popup* popup_ = nullptr;
    FrameworkElement* dropDownBorder_ = nullptr;
    DependencyPropertyChangedEventHandler popupIsOpenChangedHandler_;
    DependencyPropertyChangedEventHandler selectedProjectionChangedHandler_;
    RoutedEventHandler editableTextChangedHandler_;
    TextBlock* selectedProjection_ = nullptr;
    bool synchronizingEditableText_ = false;

    void OnPopupIsOpenChanged(DependencyObject& object, const DependencyPropertyChangedEventArgs& args) noexcept;
    void OnSelectedProjectionChanged(DependencyObject& object, const DependencyPropertyChangedEventArgs& args) noexcept;
    void OnEditableTextChanged(Base::Object* sender, RoutedEventArgs& args) noexcept;
    Result<void> UpdateSelectionBox() noexcept;
    void UpdateEditableVisualState() noexcept;
    void ObserveSelectedProjection(TextBlock* projection) noexcept;
    void SynchronizeContainers() noexcept;
    std::uint32_t FindContainerIndex(Base::Object* source) const noexcept;
    void UpdateVisualState(bool useTransitions = true) noexcept;
};

class AERO_GUI_API TabItem : public HeaderedContentControl {
    AERO_DECLARE_TYPE(TabItem, HeaderedContentControl)

public:
    TabItem() noexcept : HeaderedContentControl(StaticTypeId()) {}
    ~TabItem() override = default;

    bool GetIsSelected() const noexcept;
    void SetIsSelected(bool value) noexcept;
    AERO_DEPENDENCY_PROPERTY(bool, IsSelected);

protected:
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    void OnMouseLeftButtonDown(MouseButtonEventArgs& args);

private:
    TabControl* FindParentTabControl() const noexcept;
};

// Wraps tab headers according to the nearest templated TabControl's strip
// placement, matching the WPF TabPanel layout contract.
class AERO_GUI_API TabPanel : public Panel {
    AERO_DECLARE_TYPE(TabPanel, Panel)

public:
    TabPanel() noexcept : Panel(StaticTypeId()) {}
    ~TabPanel() override = default;

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;

private:
    bool GetIsVertical() const noexcept;
};

class AERO_GUI_API TabControl : public Primitives::Selector {
    AERO_DECLARE_TYPE(TabControl, Primitives::Selector)

public:
    TabControl() noexcept;
    ~TabControl() override;

    std::uint32_t GetTabCount() const noexcept { return GetCount(); }
    TabItem* GetSelectedTab() const noexcept;
    Value GetSelectedContent() const noexcept { return GetValue(SelectedContentProperty); }
    Ref<Base::Object> GetSelectedContentTemplate() const noexcept { return GetValue(SelectedContentTemplateProperty); }
    Ref<DataTemplate> GetContentTemplate() const noexcept { return GetValue(ContentTemplateProperty); }
    void SetContentTemplate(Ref<DataTemplate> value) noexcept { SetValue(ContentTemplateProperty, std::move(value)); }
    Dock GetTabStripPlacement() const noexcept { return GetValue(TabStripPlacementProperty); }
    void SetTabStripPlacement(Dock value) noexcept { SetValue(TabStripPlacementProperty, value); }

    AERO_READONLY_PROPERTY(Value, SelectedContent);
    AERO_READONLY_PROPERTY(Ref<Base::Object>, SelectedContentTemplate);
    AERO_DEPENDENCY_PROPERTY(Ref<DataTemplate>, ContentTemplate);
    AERO_DEPENDENCY_PROPERTY(Dock, TabStripPlacement);

protected:
    Result<Ref<FrameworkElement>> GetContainerForItemOverride() const noexcept override;
    Result<void> PrepareContainerForItemOverride(FrameworkElement& container,
        const Ref<Base::Object>& item, std::uint32_t index) noexcept override;
    void OnApplyTemplate() noexcept override;
    void OnSelectionChanged(const SelectionChangedEvent& event) override;
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    void OnItemsChanged(const ItemsChangedEvent& event) noexcept override;
    void OnContainersChanged() noexcept override;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;

private:
    friend class TabItem;
    void SynchronizeSelection() noexcept;
    void EnsureDefaultSelection() noexcept;
};

} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::SelectionMode)

