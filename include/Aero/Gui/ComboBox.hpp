#pragma once

#include <Aero/Gui/ListBox.hpp>
#include <Aero/Gui/Popup.hpp>
#include <Aero/Gui/TextBlock.hpp>
#include <Aero/Gui/TextBox.hpp>
#include <Aero/Gui/ContentPresenter.hpp>

namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::TypeId;
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

    inline static constexpr DependencyProperty<bool> IsSelectedProperty{"IsSelected"};
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
    Value GetSelectionBoxItem() const noexcept {
        return GetValueOr(
            SelectionBoxItemProperty,
            Value::NullObject(
                Meta::TypeOf<Base::Object>()));
    }

    inline static constexpr RoutedEvent<RoutedEventArgs> DropDownOpenedEvent{"DropDownOpened"};
    inline static constexpr RoutedEvent<RoutedEventArgs> DropDownClosedEvent{"DropDownClosed"};
    UIElement::Event<RoutedEventArgs>
        DropDownOpened() noexcept {
        return GetEvent(DropDownOpenedEvent);
    }
    UIElement::Event<RoutedEventArgs>
        DropDownClosed() noexcept {
        return GetEvent(DropDownClosedEvent);
    }

    inline static constexpr DependencyProperty<bool> IsDropDownOpenProperty{"IsDropDownOpen"};
    inline static constexpr DependencyProperty<double> MaxDropDownHeightProperty{"MaxDropDownHeight"};
    inline static constexpr DependencyProperty<bool> IsEditableProperty{"IsEditable"};
    inline static constexpr DependencyProperty<bool> IsReadOnlyProperty{"IsReadOnly"};
    inline static constexpr DependencyProperty<Base::String> TextProperty{"Text"};
    inline static constexpr DependencyProperty<Base::String> PlaceholderProperty{"Placeholder"};
    inline static constexpr ReadOnlyDependencyProperty<Base::String> SelectionBoxTextProperty{"SelectionBoxText"};
    inline static constexpr ReadOnlyDependencyProperty<Value> SelectionBoxItemProperty{"SelectionBoxItem"};

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
