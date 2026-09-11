#pragma once

#include <Aero/Controls/ComboBoxItem.hpp>
#include <Aero/Controls/Primitives/Selector.hpp>
#include <Aero/Controls/Popup.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/ContentPresenter.hpp>

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
    StringView GetPlaceholder() const noexcept {
        return GetValue(PlaceholderProperty);
    }
    void SetPlaceholder(StringView value) noexcept {
        SetValue(PlaceholderProperty, value);
    }
    StringView GetSelectionBoxText() const noexcept;
    Value GetSelectionBoxItem() const noexcept {
        return GetValue(SelectionBoxItemProperty);
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

    AERO_DEPENDENCY_PROPERTY(bool, IsDropDownOpen);
    AERO_DEPENDENCY_PROPERTY(double, MaxDropDownHeight);
    AERO_DEPENDENCY_PROPERTY(bool, IsEditable);
    AERO_DEPENDENCY_PROPERTY(bool, IsReadOnly);
    AERO_DEPENDENCY_PROPERTY(String, Text);
    AERO_DEPENDENCY_PROPERTY(String, Placeholder);
    AERO_READONLY_PROPERTY(String, SelectionBoxText);
    AERO_READONLY_PROPERTY(Value, SelectionBoxItem);

protected:
    Result<Ref<FrameworkElement>>
        GetContainerForItemOverride() const
            noexcept override;
    Result<void> PrepareContainerForItemOverride(
        FrameworkElement& container,
        const Ref<Base::Object>& item,
        std::uint32_t index) noexcept override;
    void ClearContainerForItemOverride(
        FrameworkElement& container) noexcept override;
    void OnContainersChanged() noexcept override;
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    void OnSelectionChanged(const SelectionChangedEvent& event) override;
    void OnPropertyChanged(
        const DependencyPropertyChangedEventArgs& args) noexcept override;
    void OnMouseLeftButtonDown(MouseButtonEventArgs& args) override;
    void OnKeyDown(KeyEventArgs& args) override;

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

    void OnPopupIsOpenChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void OnSelectedProjectionChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void OnEditableTextChanged(
        Base::Object* sender,
        RoutedEventArgs& args) noexcept;
    Result<void> UpdateSelectionBox() noexcept;
    Result<void> UpdateEditableVisualState() noexcept;
    void ObserveSelectedProjection(
        TextBlock* projection) noexcept;
    void SynchronizeContainers() noexcept;
    std::uint32_t FindContainerIndex(
        Base::Object* source) const noexcept;
    void UpdateVisualState(bool useTransitions = true) noexcept;
};
} // namespace Aero::Controls
