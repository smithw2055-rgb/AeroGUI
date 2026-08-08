#pragma once

#include <Aero/Controls/TextBox.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;
class AERO_API PasswordBox : public Primitives::TextBoxBase {
    AERO_DECLARE_TYPE(PasswordBox, Primitives::TextBoxBase)
public:
    struct Impl;

    PasswordBox() noexcept;
    ~PasswordBox() override;

    Base::StringView GetPassword() const noexcept {
        return password_.View();
    }
    void SetPassword(
        Base::StringView value) noexcept;
    Base::StringView GetPasswordChar() const noexcept;
    void SetPasswordChar(
        Base::StringView value) noexcept;
    std::uint32_t GetMaxLength() const noexcept;
    void SetMaxLength(
        std::uint32_t value) noexcept;
    void SetSelectionBrush(
        Base::Ref<Media::Brush> value) noexcept override;
    void SetSelectionOpacity(
        double value) noexcept override;
    void SetCaretBrush(
        Base::Ref<Media::Brush> value) noexcept override;
    TextSelection GetSelection() const noexcept;
    std::uint32_t GetCaret() const noexcept;
    void SetSelection(
        std::uint32_t anchor,
        std::uint32_t caret) noexcept;
    Base::Result<void> SelectAll() noexcept;
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
    inline static constexpr DependencyProperty<Base::String> PasswordCharProperty{"PasswordChar"};
    inline static constexpr DependencyProperty<std::uint32_t> MaxLengthProperty{"MaxLength"};
    inline static constexpr DependencyProperty<Base::String> PlaceholderProperty{"Placeholder"};
    inline static constexpr auto ForegroundProperty = Control::ForegroundProperty;

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;
    void OnRender(
        DrawingContext& context) noexcept override;

private:
    friend class TextBox;
    friend struct ::Aero::Controls::TextBox::Impl;
    friend struct ::Aero::Controls::Control::Impl;
    Base::String password_;
    void* validation_ = nullptr;
    void* passwordPolicy_ = nullptr;
    TextBox editor_;
    bool synchronizingEditor_ = false;

    Base::Result<void>
        SynchronizeEditorFromPassword() noexcept;
    Base::Result<void>
        SynchronizePasswordFromEditor() noexcept;
};
} // namespace Aero::Controls
