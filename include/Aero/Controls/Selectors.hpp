#pragma once

// List, combo, and tab controls plus their items. Selector stays separate.
#include <Aero/Controls/ContentControl.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Controls/Primitives/Selector.hpp>
#include <Aero/Controls/Popup.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/ContentPresenter.hpp>
#include <Aero/Controls/HeaderedContentControl.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/DataTemplate.hpp>

namespace Aero::Controls {

using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::TypeId;

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
    virtual void OnSelected(RoutedEventArgs& e);
    virtual void OnUnselected(RoutedEventArgs& e);
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
};

} // namespace Aero::Controls

namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::TypeId;

class AERO_GUI_API ComboBoxItem : public ListBoxItem {
    AERO_DECLARE_TYPE(ComboBoxItem, ListBoxItem)
public:
    ComboBoxItem() noexcept : ListBoxItem(StaticTypeId()) {}
    ~ComboBoxItem() override = default;

    bool GetIsSelected() const noexcept;
    void SetIsSelected(bool value) noexcept;

    AERO_DEPENDENCY_PROPERTY(bool, IsSelected);
};
} // namespace Aero::Controls

namespace Aero::Controls {

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

} // namespace Aero::Controls

namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::TypeId;

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
} // namespace Aero::Controls

namespace Aero::Controls {

class AERO_GUI_API TabItem : public HeaderedContentControl {
    AERO_DECLARE_TYPE(TabItem, HeaderedContentControl)
public:
    TabItem() noexcept : HeaderedContentControl(StaticTypeId()) {}
    ~TabItem() override = default;

    bool GetIsSelected() const noexcept;
    void SetIsSelected(bool value) noexcept;
    AERO_DEPENDENCY_PROPERTY(bool, IsSelected);
};

} // namespace Aero::Controls

namespace Aero::Controls {

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

} // namespace Aero::Controls

namespace Aero::Controls {

class AERO_GUI_API TabControl : public Primitives::Selector {
    AERO_DECLARE_TYPE(TabControl, Primitives::Selector)
public:
    TabControl() noexcept;
    ~TabControl() override;

    std::uint32_t GetTabCount() const noexcept { return GetCount(); }
    TabItem* GetSelectedTab() const noexcept;
    Value GetSelectedContent() const noexcept { return GetValue(SelectedContentProperty); }
    Ref<DataTemplate> GetContentTemplate() const noexcept { return GetValue(ContentTemplateProperty); }
    void SetContentTemplate(Ref<DataTemplate> value) noexcept { SetValue(ContentTemplateProperty, std::move(value)); }
    Dock GetTabStripPlacement() const noexcept { return GetValue(TabStripPlacementProperty); }
    void SetTabStripPlacement(Dock value) noexcept { SetValue(TabStripPlacementProperty, value); }

    AERO_READONLY_PROPERTY(Value, SelectedContent);
    AERO_DEPENDENCY_PROPERTY(Ref<DataTemplate>, ContentTemplate);
    AERO_DEPENDENCY_PROPERTY(Dock, TabStripPlacement);

protected:
    Result<Ref<FrameworkElement>> GetContainerForItemOverride() const noexcept override;
    void OnSelectionChanged(const SelectionChangedEvent& event) override;
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;

private:
    void SynchronizeSelection() noexcept;
};

} // namespace Aero::Controls
