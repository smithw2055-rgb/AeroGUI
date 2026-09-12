#pragma once

#include <Aero/Controls/TextBox.hpp>


namespace Aero::Meta { class Registration; }

namespace Aero::Controls {

using ::Aero::Meta::TypeId;
class AERO_GUI_API PasswordBox : public Primitives::TextBoxBase {
    AERO_DECLARE_TYPE(PasswordBox, Primitives::TextBoxBase)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

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

    void OnMouseDown(MouseButtonEventArgs& args) override;
    void OnMouseMove(MouseEventArgs& args) override;
    void OnMouseUp(MouseButtonEventArgs& args) override;
    void OnKeyDown(KeyEventArgs& args) override;
    void OnTextInput(TextCompositionEventArgs& args) override;
    void OnLostKeyboardFocus(KeyboardFocusChangedEventArgs& args) override;
    bool ValidateValueCore(
        Meta::DependencyPropertyHandle property,
        const PropertyValue& value) const noexcept override;
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;

private:
    friend class TextBox;

    String password_;
    void* passwordPolicy_ = nullptr;
    TextBox editor_;
    bool synchronizingEditor_ = false;

    Result<void>
        SynchronizeEditorFromPassword() noexcept;
    Result<void>
        SynchronizePasswordFromEditor() noexcept;
};
} // namespace Aero::Controls
