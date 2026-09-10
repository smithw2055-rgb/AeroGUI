#pragma once

#include <Aero/Controls/TextBox.hpp>


namespace Aero::Controls {

using ::Aero::Meta::TypeId;
class AERO_GUI_API PasswordBox : public Primitives::TextBoxBase {
    AERO_DECLARE_TYPE(PasswordBox, Primitives::TextBoxBase)
public:

    PasswordBox() noexcept;
    ~PasswordBox() override;

    StringView GetPassword() const noexcept {
        return password_.View();
    }
    void SetPassword(StringView value) noexcept;
    StringView GetPasswordChar() const noexcept;
    void SetPasswordChar(StringView value) noexcept;
    std::uint32_t GetMaxLength() const noexcept;
    void SetMaxLength(std::uint32_t value) noexcept;
    void SetSelectionBrush(Ref<Media::Brush> value) noexcept override;
    void SetSelectionOpacity(double value) noexcept override;
    void SetCaretBrush(Ref<Media::Brush> value) noexcept override;
    TextSelection GetSelection() const noexcept;
    std::uint32_t GetCaret() const noexcept;
    void SetSelection(std::uint32_t anchor, std::uint32_t caret) noexcept;
    Result<void> SelectAll() noexcept;
    void SetInputMethodHost(Input::ITextInputMethodHost* host) noexcept;
    Input::ITextInputMethodHost*
    GetInputMethodHost() const noexcept;
    bool GetIsComposing() const noexcept;

    inline static constexpr RoutedEvent<RoutedEventArgs> PasswordChangedEvent{"PasswordChanged"};
    UIElement::Event<RoutedEventArgs>
        PasswordChanged() noexcept {
        return GetEvent(PasswordChangedEvent);
    }
    AERO_DEPENDENCY_PROPERTY(String, PasswordChar);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, MaxLength);
    AERO_DEPENDENCY_PROPERTY(String, Placeholder);
    StringView GetPlaceholder() const noexcept {
        return GetValue(PlaceholderProperty);
    }
    void SetPlaceholder(StringView value) noexcept {
        SetValue(PlaceholderProperty, value);
    }
    inline static constexpr auto ForegroundProperty = Control::ForegroundProperty;

protected:
    void OnApplyTemplate() noexcept override;
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;
    void OnRender(
        ::Aero::Media::DrawingContext& context) noexcept override;

    virtual void OnMouseDown(MouseButtonEventArgs& args) noexcept;
    virtual void OnMouseMove(MouseEventArgs& args) noexcept;
    virtual void OnMouseUp(MouseButtonEventArgs& args) noexcept;
    virtual void OnKeyDown(KeyEventArgs& args) noexcept;
    virtual void OnTextInput(TextCompositionEventArgs& args) noexcept;
    virtual void OnLostKeyboardFocus(KeyboardFocusChangedEventArgs& args) noexcept;

private:
    friend class TextBox;

    MouseButtonEventHandler mouseDownHandler_;
    MouseEventHandler mouseMoveHandler_;
    MouseButtonEventHandler mouseUpHandler_;
    KeyEventHandler keyDownHandler_;
    TextCompositionEventHandler textInputHandler_;
    KeyboardFocusChangedEventHandler focusChangedHandler_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;

    std::uint32_t pointerId_ = 0U;
    std::uint32_t dragAnchor_ = 0U;
    bool isDragging_ = false;

    void OnMouseDownHandler(Base::Object* sender, MouseButtonEventArgs& args) noexcept;
    void OnMouseMoveHandler(Base::Object* sender, MouseEventArgs& args) noexcept;
    void OnMouseUpHandler(Base::Object* sender, MouseButtonEventArgs& args) noexcept;
    void OnKeyDownHandler(Base::Object* sender, KeyEventArgs& args) noexcept;
    void OnTextInputHandler(Base::Object* sender, TextCompositionEventArgs& args) noexcept;
    void OnLostKeyboardFocusHandler(Base::Object* sender, KeyboardFocusChangedEventArgs& args) noexcept;

    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;

    String password_;
    void* validation_ = nullptr;
    void* passwordPolicy_ = nullptr;
    TextBox editor_;
    bool synchronizingEditor_ = false;

    Result<void>
        SynchronizeEditorFromPassword() noexcept;
    Result<void>
        SynchronizePasswordFromEditor() noexcept;
};
} // namespace Aero::Controls
