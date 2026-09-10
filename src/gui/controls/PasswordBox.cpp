#include "gui/controls/TextBoxCommon.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/PasswordBox.hpp>
#include "gui/text/EditableText.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/input/InputManager.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "TextBlockLayout.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <utility>

namespace Aero::Controls {
using namespace Primitives;
using namespace ::Aero::Render;

PasswordBox::PasswordBox() noexcept
    : TextBoxBase(StaticTypeId()),
      validation_(new (std::nothrow) ::Aero::Text::EditableTextModel()),
      passwordPolicy_(new (std::nothrow) PasswordTextDisplayPolicy()),
      mouseDownHandler_(this, &PasswordBox::OnMouseDownHandler),
      mouseMoveHandler_(this, &PasswordBox::OnMouseMoveHandler),
      mouseUpHandler_(this, &PasswordBox::OnMouseUpHandler),
      keyDownHandler_(this, &PasswordBox::OnKeyDownHandler),
      textInputHandler_(this, &PasswordBox::OnTextInputHandler),
      focusChangedHandler_(this, &PasswordBox::OnLostKeyboardFocusHandler),
      propertyChangedHandler_(this, &PasswordBox::OnPropertyChanged) {
    editor_.displayPolicy_ = passwordPolicy_;
    editor_.coordinateOwner_ = this;
    editor_.passwordOwner_ = this;

    AddHandler(UIElement::MouseDownEvent, mouseDownHandler_);
    AddHandler(UIElement::MouseMoveEvent, mouseMoveHandler_);
    AddHandler(UIElement::MouseUpEvent, mouseUpHandler_);
    AddHandler(UIElement::KeyDownEvent, keyDownHandler_);
    AddHandler(UIElement::TextInputEvent, textInputHandler_);
    AddHandler(UIElement::LostKeyboardFocusEvent, focusChangedHandler_);
    AddValueChangedHandler(PasswordCharProperty, propertyChangedHandler_);
    AddValueChangedHandler(MaxLengthProperty, propertyChangedHandler_);
    AddValueChangedHandler(ForegroundProperty, propertyChangedHandler_);
    AddValueChangedHandler(SelectionBrushProperty, propertyChangedHandler_);
    AddValueChangedHandler(SelectionOpacityProperty, propertyChangedHandler_);
    AddValueChangedHandler(CaretBrushProperty, propertyChangedHandler_);
    AddValueChangedHandler(PlaceholderProperty, propertyChangedHandler_);
    AddValueChangedHandler(UIElement::IsEnabledProperty, propertyChangedHandler_);

    if (passwordPolicy_ != nullptr) {
        static_cast<void>(PasswordPolicy(passwordPolicy_)->SetMask(GetPasswordChar()));
    }
    static_cast<void>(SynchronizeEditorFromPassword());
    editor_.SetForeground(GetForeground());
    editor_.SetSelectionBrush(GetSelectionBrush());
    editor_.SetSelectionOpacity(GetSelectionOpacity());
    editor_.SetCaretBrush(GetCaretBrush());
}

PasswordBox::~PasswordBox() {
    RemoveHandler(UIElement::MouseDownEvent, mouseDownHandler_);
    RemoveHandler(UIElement::MouseMoveEvent, mouseMoveHandler_);
    RemoveHandler(UIElement::MouseUpEvent, mouseUpHandler_);
    RemoveHandler(UIElement::KeyDownEvent, keyDownHandler_);
    RemoveHandler(UIElement::TextInputEvent, textInputHandler_);
    RemoveHandler(UIElement::LostKeyboardFocusEvent, focusChangedHandler_);
    RemoveValueChangedHandler(PasswordCharProperty, propertyChangedHandler_);
    RemoveValueChangedHandler(MaxLengthProperty, propertyChangedHandler_);
    RemoveValueChangedHandler(ForegroundProperty, propertyChangedHandler_);
    RemoveValueChangedHandler(SelectionBrushProperty, propertyChangedHandler_);
    RemoveValueChangedHandler(SelectionOpacityProperty, propertyChangedHandler_);
    RemoveValueChangedHandler(CaretBrushProperty, propertyChangedHandler_);
    RemoveValueChangedHandler(PlaceholderProperty, propertyChangedHandler_);
    RemoveValueChangedHandler(UIElement::IsEnabledProperty, propertyChangedHandler_);

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
    InvalidateMeasure();
    InvalidateVisual();
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
    editor_.InvalidateMeasure();
    InvalidateMeasure();
    InvalidateVisual();
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
    if (part != nullptr && AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
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
    if (part != nullptr && AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
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
    InvalidateMeasure();
    InvalidateVisual();
    RoutedEventArgs args;
    RaiseEvent(PasswordChangedEvent, &args);
    return {};
}

void PasswordBox::OnMouseDownHandler(Base::Object*, MouseButtonEventArgs& args) noexcept {
    OnMouseDown(args);
}

void PasswordBox::OnMouseMoveHandler(Base::Object*, MouseEventArgs& args) noexcept {
    OnMouseMove(args);
}

void PasswordBox::OnMouseUpHandler(Base::Object*, MouseButtonEventArgs& args) noexcept {
    OnMouseUp(args);
}

void PasswordBox::OnKeyDownHandler(Base::Object*, KeyEventArgs& args) noexcept {
    OnKeyDown(args);
}

void PasswordBox::OnTextInputHandler(Base::Object*, TextCompositionEventArgs& args) noexcept {
    OnTextInput(args);
}

void PasswordBox::OnLostKeyboardFocusHandler(Base::Object*, KeyboardFocusChangedEventArgs& args) noexcept {
    OnLostKeyboardFocus(args);
}

void PasswordBox::OnMouseDown(MouseButtonEventArgs& args) noexcept {
    if (args.GetChangedButton() != MouseButton::Left || !GetIsEnabled()) {
        return;
    }
    const Point local = ToLocalPoint(*this, args.GetPosition());
    const std::uint32_t caret = editor_.HitTestText(local);
    static_cast<void>(editor_.SetSelection(caret, caret));
    static_cast<void>(Focus());
    Base::Result<void> captured = CapturePointer(args.GetPointerId());
    if (captured) {
        pointerId_ = args.GetPointerId();
        dragAnchor_ = caret;
        isDragging_ = true;
    }
    args.SetHandled(true);
}

void PasswordBox::OnMouseMove(MouseEventArgs& args) noexcept {
    if (!isDragging_ || pointerId_ != args.GetPointerId()) {
        return;
    }
    const Point local = ToLocalPoint(*this, args.GetPosition());
    static_cast<void>(editor_.SetSelection(dragAnchor_, editor_.HitTestText(local)));
    args.SetHandled(true);
}

void PasswordBox::OnMouseUp(MouseButtonEventArgs& args) noexcept {
    if (args.GetChangedButton() != MouseButton::Left || !isDragging_ || pointerId_ != args.GetPointerId()) {
        return;
    }
    const Point local = ToLocalPoint(*this, args.GetPosition());
    static_cast<void>(editor_.SetSelection(dragAnchor_, editor_.HitTestText(local)));
    isDragging_ = false;
    static_cast<void>(ReleasePointer(args.GetPointerId()));
    args.SetHandled(true);
}

void PasswordBox::OnKeyDown(KeyEventArgs& args) noexcept {
    if (!GetIsEnabled()) {
        return;
    }
    const bool shift = HasKeyboardModifier(args.GetModifiers(), KeyboardModifiers::Shift);
    const bool control = HasKeyboardModifier(args.GetModifiers(), KeyboardModifiers::Control);
    Base::Result<void> result;
    bool handled = true;
    if (control && args.GetKey() == KeyboardKeyA) {
        result = editor_.SelectAll();
    } else if (control && args.GetKey() == KeyboardKeyC) {
        result = Base::Result<void>{};
    } else if (control && args.GetKey() == KeyboardKeyX) {
        result = editor_.ReplaceSelection(Base::StringView{});
    } else if (control && args.GetKey() == KeyboardKeyV) {
        Input::IClipboard* clipboard = AeroGuiInternal::ClipboardOf(*this);
        if (clipboard != nullptr) {
            result = editor_.Paste(*clipboard);
        }
    } else if (control && args.GetKey() == KeyboardKeyZ) {
        result = shift ? editor_.Redo() : editor_.Undo();
    } else if (control && args.GetKey() == KeyboardKeyY) {
        result = editor_.Redo();
    } else if (args.GetKey() == KeyboardKeyLeft) {
        result = editor_.MoveCaretHorizontal(-1.0, shift);
    } else if (args.GetKey() == KeyboardKeyRight) {
        result = editor_.MoveCaretHorizontal(1.0, shift);
    } else if (args.GetKey() == KeyboardKeyHome) {
        result = editor_.MoveCaretLineBoundary(false, shift);
    } else if (args.GetKey() == KeyboardKeyEnd) {
        result = editor_.MoveCaretLineBoundary(true, shift);
    } else if (args.GetKey() == KeyboardKeyBackspace) {
        result = editor_.DeleteBackward();
    } else if (args.GetKey() == KeyboardKeyDelete) {
        result = editor_.DeleteForward();
    } else if (args.GetKey() == KeyboardKeyEnter && editor_.GetAcceptsReturn()) {
        result = editor_.ReplaceSelection(Base::StringView("\n"));
    } else {
        handled = false;
    }
    if (handled && result) {
        args.SetHandled(true);
    }
}

void PasswordBox::OnTextInput(TextCompositionEventArgs& args) noexcept {
    if (!GetIsEnabled() || editor_.GetIsReadOnly()) {
        return;
    }
    if (editor_.GetIsComposing()) {
        Base::Result<void> cancelled = editor_.CancelCompositionForFocusLoss();
        if (!cancelled) {
            return;
        }
    }
    Base::Result<void> inserted = editor_.ReplaceSelection(args.GetText());
    if (inserted) {
        args.SetHandled(true);
    }
}

void PasswordBox::OnLostKeyboardFocus(KeyboardFocusChangedEventArgs&) noexcept {
    static_cast<void>(editor_.CancelCompositionForFocusLoss());
    if (!isDragging_) {
        return;
    }
    isDragging_ = false;
    static_cast<void>(ReleasePointer(pointerId_));
}

void PasswordBox::OnPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (args.GetProperty() == PasswordBox::PasswordCharProperty) {
        static_cast<void>(PasswordPolicy(passwordPolicy_)->SetMask(GetPasswordChar()));
        editor_.InvalidateMeasure();
        InvalidateMeasure();
        InvalidateVisual();
    } else if (args.GetProperty() == PasswordBox::MaxLengthProperty) {
        static_cast<void>(editor_.CancelCompositionForFocusLoss());
        static_cast<void>(Model(validation_).SetMaximumLength(EffectiveMaximumLength(GetMaxLength())));
        static_cast<void>(editor_.SetMaxLength(GetMaxLength()));
    } else if (args.GetProperty() == PasswordBox::ForegroundProperty) {
        static_cast<void>(editor_.SetForeground(GetForeground()));
    } else if (args.GetProperty() == PasswordBox::SelectionBrushProperty) {
        static_cast<void>(editor_.SetSelectionBrush(GetSelectionBrush()));
    } else if (args.GetProperty() == PasswordBox::SelectionOpacityProperty) {
        static_cast<void>(editor_.SetSelectionOpacity(GetSelectionOpacity()));
    } else if (args.GetProperty() == PasswordBox::CaretBrushProperty) {
        static_cast<void>(editor_.SetCaretBrush(GetCaretBrush()));
    } else if (args.GetProperty() == PasswordBox::PlaceholderProperty) {
        editor_.SetPlaceholder(GetPlaceholder());
    } else if (args.GetProperty() == UIElement::IsEnabledProperty && !args.GetNewValue().AsBoolean()) {
        static_cast<void>(editor_.CancelCompositionForFocusLoss());
    }
}

} // namespace Aero::Controls
