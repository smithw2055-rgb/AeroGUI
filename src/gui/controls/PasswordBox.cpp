#include "gui/controls/TextBoxCommon.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/PasswordBox.hpp>
#include "gui/text/EditableText.hpp"
#include "gui/core/State.hpp"
#include "gui/input/InputState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/media/MediaState.hpp"
#include "TextBlockLayout.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <utility>
#include "ControlBehavior.hpp"

namespace Aero::Controls {
using namespace Primitives;
using namespace ::Aero::Render;

PasswordBox::PasswordBox() noexcept
    : TextBoxBase(StaticTypeId()),
      validation_(new (std::nothrow) ::Aero::Text::EditableTextModel()),
      passwordPolicy_(new (std::nothrow) PasswordTextDisplayPolicy()) {
    editor_.displayPolicy_ =
        passwordPolicy_;
    editor_.coordinateOwner_ = this;
    editor_.passwordOwner_ = this;
}

PasswordBox::~PasswordBox() {
    delete static_cast<::Aero::Text::EditableTextModel*>(validation_);
    validation_ = nullptr;
    delete static_cast<::Aero::Controls::PasswordTextDisplayPolicy*>(passwordPolicy_);
    passwordPolicy_ = nullptr;
}

void PasswordBox::SetPassword(
    Base::StringView value) noexcept {
    if (password_.View() == value) return;
    ::Aero::Text::EditableTextModel next;
    Base::Result<void> limited =
        next.SetMaximumLength(
            EffectiveMaximumLength(
                GetMaxLength()));
    if (limited) limited = next.SetText(value);
    if (!limited) {
        return;
    }
    Base::String nextPassword;
    Base::Result<void> copied =
        nextPassword.Assign(value);
    if (!copied) return;
    Base::Result<void> modelLimit =
        Model(validation_).SetMaximumLength(
            EffectiveMaximumLength(
                GetMaxLength()));
    if (modelLimit) {
        modelLimit = Model(validation_).SetText(value);
    }
    if (!modelLimit) return;
    synchronizingEditor_ = true;
    editor_.SetText(value);
    synchronizingEditor_ = false;
    password_ = std::move(nextPassword);
    (void)InvalidateMeasure();
    (void)InvalidateVisual();
    RoutedEventArgs args;
    RaiseEvent(PasswordChangedEvent, &args);
}

Base::StringView PasswordBox::GetPasswordChar() const noexcept {
    return GetValue(PasswordCharProperty);
}

void PasswordBox::SetPasswordChar(
    Base::StringView value) noexcept {
    PasswordTextDisplayPolicy validation;
    Base::Result<void> valid =
        validation.SetMask(value);
    if (!valid) return;
    if (passwordPolicy_ == nullptr) {
        return;
    }
    Base::Result<void> mask =
        PasswordPolicy(passwordPolicy_)->SetMask(value);
    if (!mask) return;
    SetValue(PasswordCharProperty, value);
    (void)editor_.InvalidateMeasure();
    (void)InvalidateMeasure();
    (void)InvalidateVisual();
}

std::uint32_t PasswordBox::GetMaxLength() const noexcept {
    return GetValue(MaxLengthProperty);
}

void PasswordBox::SetMaxLength(
    std::uint32_t value) noexcept {
    const std::uint32_t effective =
        EffectiveMaximumLength(value);
    if (Model(validation_).GraphemeCount() > effective) {
        return;
    }
    Base::Result<void> validation =
        Model(validation_).SetMaximumLength(effective);
    if (!validation) return;
    editor_.SetMaxLength(value);
    SetValue(MaxLengthProperty, value);
}

void PasswordBox::SetSelectionBrush(
    Base::Ref<Media::Brush> value) noexcept {
    TextBoxBase::SetSelectionBrush(std::move(value));
    (void)editor_.SetSelectionBrush(GetSelectionBrush());
}

void PasswordBox::SetSelectionOpacity(
    double value) noexcept {
    TextBoxBase::SetSelectionOpacity(value);
    (void)editor_.SetSelectionOpacity(value);
}

void PasswordBox::SetCaretBrush(
    Base::Ref<Media::Brush> value) noexcept {
    TextBoxBase::SetCaretBrush(std::move(value));
    (void)editor_.SetCaretBrush(GetCaretBrush());
}

TextSelection
PasswordBox::GetSelection() const noexcept {
    return editor_.GetSelection();
}

std::uint32_t PasswordBox::GetCaret() const noexcept {
    return editor_.GetCaret();
}

void PasswordBox::SetSelection(
    std::uint32_t anchor,
    std::uint32_t caret) noexcept {
    (void)editor_.SetSelection(anchor, caret);
}

Base::Result<void> PasswordBox::SelectAll() noexcept {
    return editor_.SelectAll();
}

void PasswordBox::SetInputMethodHost(
    Input::ITextInputMethodHost* host) noexcept {
    (void)editor_.SetInputMethodHost(host);
}

Input::ITextInputMethodHost*
PasswordBox::GetInputMethodHost() const noexcept {
    return editor_.GetInputMethodHost();
}

bool PasswordBox::GetIsComposing() const noexcept {
    return editor_.GetIsComposing();
}

Size PasswordBox::MeasureOverride(
    Size availableSize) noexcept {
    Size templateSize{};
    if (GetTemplateRoot() != nullptr) {
        templateSize = Control::MeasureOverride(availableSize);
    }
    editor_.SetUseLayoutRounding(GetUseLayoutRounding(), GetDpiScale());
    editor_.SetForeground(GetForeground());
    editor_.SetSelectionBrush(GetSelectionBrush());
    editor_.SetSelectionOpacity(GetSelectionOpacity());
    editor_.SetCaretBrush(GetCaretBrush());
    editor_.SetPlaceholder(GetPlaceholder());
    Base::Result<Size> measuredEditor =
        editor_.MeasureOverride(availableSize);
    if (!measuredEditor) {
        return Size{};
    }
    const Size editorSize = measuredEditor.Value();
    return Size{
        std::max(templateSize.width, editorSize.width),
        std::max(templateSize.height, editorSize.height)};
}

void PasswordBox::OnApplyTemplate() noexcept {
    Control::OnApplyTemplate();
    DependencyObject* part = GetTemplateChild(Base::StringView("PART_ContentHost"));
    if (part != nullptr && PropertyRegistry().Types().IsDerivedFrom(
            part->RuntimeType(), ScrollViewer::StaticTypeId())) {
        static_cast<void>(editor_.AttachScrollViewer(static_cast<ScrollViewer*>(part)));
    } else {
        static_cast<void>(editor_.AttachScrollViewer(nullptr));
    }
}

Size PasswordBox::ArrangeOverride(
    Size finalSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        Control::ArrangeOverride(finalSize);
    }
    editor_.SetViewport(finalSize);
    return finalSize;
}

void PasswordBox::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    DependencyObject* part = GetTemplateChild(Base::StringView("PART_ContentHost"));
    if (part != nullptr && PropertyRegistry().Types().IsDerivedFrom(
            part->RuntimeType(), ScrollViewer::StaticTypeId())) {
        return;
    }
    static_cast<void>(editor_.RenderEditor(
        context,
        GetRenderSize(),
        GetIsKeyboardFocused()));
}

Base::Result<void>
PasswordBox::SynchronizeEditorFromPassword()
    noexcept {
    if (synchronizingEditor_) {
        return {};
    }
    synchronizingEditor_ = true;
    editor_.SetMaxLength(GetMaxLength());
    editor_.SetText(password_.View());
    synchronizingEditor_ = false;
    return {};
}

Base::Result<void>
PasswordBox::SynchronizePasswordFromEditor()
    noexcept {
    if (synchronizingEditor_ ||
        password_.View() == editor_.GetText()) {
        return {};
    }
    Base::String next;
    Base::Result<void> copied =
        next.Assign(editor_.GetText());
    if (!copied) return copied.GetStatus();
    Base::Result<void> model =
        Model(validation_).SetMaximumLength(
            EffectiveMaximumLength(
                GetMaxLength()));
    if (model) {
        model = Model(validation_).SetText(
            next.View());
    }
    if (!model) return model.GetStatus();
    password_ = std::move(next);
    Base::Result<void> measure =
        InvalidateMeasure();
    if (!measure) return measure.GetStatus();
    Base::Result<void> render =
        InvalidateVisual();
    if (!render) return render.GetStatus();
    RoutedEventArgs args;
    RaiseEvent(PasswordChangedEvent, &args);
    return {};
}

} // namespace Aero::Controls
