#pragma once

#include <Aero/Controls/TextBox.hpp>


namespace Aero::Controls {

class TextEditBehavior;

using ::Aero::Meta::TypeId;
class AERO_GUI_API PasswordBox : public Primitives::TextBoxBase {
    AERO_DECLARE_TYPE(PasswordBox, Primitives::TextBoxBase)
public:

    PasswordBox() noexcept;
    ~PasswordBox() override;

    StringView GetPassword() const noexcept {
        return password_.View();
    }
    void SetPassword(
        StringView value) noexcept;
    StringView GetPasswordChar() const noexcept;
    void SetPasswordChar(
        StringView value) noexcept;
    std::uint32_t GetMaxLength() const noexcept;
    void SetMaxLength(
        std::uint32_t value) noexcept;
    void SetSelectionBrush(
        Ref<Media::Brush> value) noexcept override;
    void SetSelectionOpacity(
        double value) noexcept override;
    void SetCaretBrush(
        Ref<Media::Brush> value) noexcept override;
    TextSelection GetSelection() const noexcept;
    std::uint32_t GetCaret() const noexcept;
    void SetSelection(
        std::uint32_t anchor,
        std::uint32_t caret) noexcept;
    Result<void> SelectAll() noexcept;
    void SetInputMethodHost(
        Input::ITextInputMethodHost* host) noexcept;
    Input::ITextInputMethodHost*
    GetInputMethodHost() const noexcept;
    bool GetIsComposing() const noexcept;

    inline static constexpr RoutedEvent<RoutedEventArgs> PasswordChangedEvent{"PasswordChanged"};
    UIElement::Event<RoutedEventArgs>
        PasswordChanged() noexcept {
        return GetEvent(PasswordChangedEvent);
    }
    inline static constexpr DependencyProperty<String> PasswordCharProperty{"PasswordChar"};
    inline static constexpr DependencyProperty<std::uint32_t> MaxLengthProperty{"MaxLength"};
    inline static constexpr DependencyProperty<String> PlaceholderProperty{"Placeholder"};
    inline static constexpr auto ForegroundProperty = Control::ForegroundProperty;

protected:
    void OnApplyTemplate() noexcept override;
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;
    void OnRender(
        ::Aero::Media::DrawingContext& context) noexcept override;

private:
    friend class TextBox;
    friend class TextEditBehavior;
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
#endif

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
