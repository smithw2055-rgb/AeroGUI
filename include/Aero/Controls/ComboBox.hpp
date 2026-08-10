#pragma once

#include <Aero/Controls/ListBox.hpp>
#include <Aero/Controls/Popup.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/ContentPresenter.hpp>

namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::TypeId;
class AERO_GUI_API ComboBoxItem
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

class AERO_GUI_API ComboBox : public Primitives::Selector {
    AERO_DECLARE_TYPE(ComboBox, Primitives::Selector)
#if defined(AERO_GUI_IMPLEMENTATION)
public:
#else
private:
#endif
    struct Access;

public:

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
    StringView GetText() const noexcept;
    void SetText(
        StringView value) noexcept;
    StringView GetPlaceholder() const noexcept {
        return GetValueOr(
            PlaceholderProperty, StringView{});
    }
    void SetPlaceholder(
        StringView value) noexcept {
        SetValue(PlaceholderProperty, value);
    }
    String GetSelectionBoxText() const noexcept;
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
    inline static constexpr DependencyProperty<String> TextProperty{"Text"};
    inline static constexpr DependencyProperty<String> PlaceholderProperty{"Placeholder"};
    inline static constexpr ReadOnlyDependencyProperty<String> SelectionBoxTextProperty{"SelectionBoxText"};
    inline static constexpr ReadOnlyDependencyProperty<Value> SelectionBoxItemProperty{"SelectionBoxItem"};

protected:
    Result<Ref<FrameworkElement>>
        CreateContainer(
            const Ref<Base::Object>& item)
            noexcept override;
    Result<void> PrepareContainer(
        FrameworkElement& container,
        const Ref<Base::Object>& item,
        std::uint32_t index) noexcept override;
    void ClearContainer(
        FrameworkElement& container) noexcept override;
    void OnContainersChanged() noexcept override;
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;

private:
    friend struct Access;
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
    DependencyPropertyChangedEventHandler
        selectedProjectionChangedHandler_;
    RoutedEventHandler editableTextChangedHandler_;
    TextBlock* selectedProjection_ = nullptr;
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
    void OnSelectedProjectionChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void OnEditableTextChanged(
        Base::Object* sender,
        RoutedEventArgs& args) noexcept;
    Result<void>
        UpdateSelectionBox() noexcept;
    Result<void>
        UpdateEditableVisualState() noexcept;
    void ObserveSelectedProjection(
        TextBlock* projection) noexcept;
    void SynchronizeContainers() noexcept;
    std::uint32_t FindContainerIndex(
        Base::Object* source) const noexcept;
};
} // namespace Aero::Controls
