#include "gui/controls/TextBoxInternal.hpp"
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

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Controls;
using namespace ::Aero::Controls;
using namespace ::Aero;

TextEditBehavior::
TextEditBehavior(
    ElementTree& tree,
    EventRouter& events,
    InputRouter& input,
    Input::IClipboard& clipboard) noexcept
    : tree_(&tree),
      events_(&events),
      input_(&input),
      clipboard_(&clipboard),
      mouseDownHandler_(
          this,
          &TextEditBehavior::
              OnMouseDown),
      mouseMoveHandler_(
          this,
          &TextEditBehavior::
              OnMouseMove),
      mouseUpHandler_(
          this,
          &TextEditBehavior::
              OnMouseUp),
      keyDownHandler_(
          this,
          &TextEditBehavior::
              OnKeyDown),
      textInputHandler_(
          this,
          &TextEditBehavior::
              OnTextInput),
      focusChangedHandler_(
          this,
          &TextEditBehavior::
              OnFocusChanged),
      propertyChangedHandler_(
          this,
          &TextEditBehavior::
              OnPropertyChanged),
      captureChangedHandler_(
          this,
          &TextEditBehavior::
              OnCaptureChanged) {}

TextEditBehavior::
~TextEditBehavior() noexcept {
    while (!records_.Empty()) {
        UIElement* owner =
            ResolveOwner(records_.Size() - 1U);
        if (owner == nullptr) {
            records_.PopBack();
        } else if (records_[
                       records_.Size() - 1U].
                       password) {
            static_cast<void>(Detach(
                *static_cast<PasswordBox*>(
                    owner)));
        } else {
            static_cast<void>(Detach(
                *static_cast<TextBox*>(owner)));
        }
    }
}

std::uint32_t TextEditBehavior::Find(
    const UIElement& owner) const noexcept {
    for (std::uint32_t index = 0U;
         index < records_.Size(); ++index) {
        if (tree_->ResolveHandle(
                records_[index].handle) ==
            &owner) {
            return index;
        }
    }
    return UINT32_MAX;
}

UIElement*
TextEditBehavior::ResolveOwner(
    std::uint32_t index) noexcept {
    if (index >= records_.Size()) {
        return nullptr;
    }
    ::Aero::Media::Visual* visual =
        tree_->ResolveHandle(
            records_[index].handle);
    if (visual == nullptr) {
        return nullptr;
    }
    const TypeId expected =
        records_[index].password
        ? PasswordBox::StaticTypeId()
        : TextBox::StaticTypeId();
    if (visual->RuntimeType() != expected) {
        return nullptr;
    }
    return static_cast<UIElement*>(visual);
}

TextBox*
TextEditBehavior::ResolveEditor(
    std::uint32_t index) noexcept {
    UIElement* owner = ResolveOwner(index);
    if (owner == nullptr) return nullptr;
    return records_[index].password
        ? &static_cast<PasswordBox*>(
              owner)->editor_
        : static_cast<TextBox*>(owner);
}

void TextEditBehavior::RemoveAt(
    std::uint32_t index) noexcept {
    if (index + 1U != records_.Size()) {
        records_[index] = std::move(
            records_[records_.Size() - 1U]);
    }
    records_.PopBack();
}

Base::Result<void>
TextEditBehavior::Attach(
    TextBox& textBox) noexcept {
    if (Find(textBox) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "TextBox is already attached");
    }
    if (!textBox.GetIsLoaded() ||
        textBox.GetTree() != tree_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TextBox must be loaded in the interaction tree");
    }
    Base::Result<void> synced =
        textBox.SynchronizeModel();
    if (!synced) {
        return synced;
    }
    Record record;
    record.handle = AeroGuiInternal::Handle(textBox);
    Base::Result<void> appended =
        records_.PushBack(record);
    if (!appended) {
        return appended;
    }
    if (!captureSubscribed_) {
        input_->AddPointerCaptureChanged(captureChangedHandler_);
        captureSubscribed_ = true;
    }
    Base::Result<void> result =
        textBox.AddHandlerChecked(
            UIElement::MouseDownEvent,
            mouseDownHandler_);
    if (result) {
        result = textBox.AddHandlerChecked(
            UIElement::MouseMoveEvent,
            mouseMoveHandler_);
    }
    if (result) {
        result = textBox.AddHandlerChecked(
            UIElement::MouseUpEvent,
            mouseUpHandler_);
    }
    if (result) {
        result = textBox.AddHandlerChecked(
            UIElement::KeyDownEvent,
            keyDownHandler_);
    }
    if (result) {
        result = textBox.AddHandlerChecked(
            UIElement::TextInputEvent,
            textInputHandler_);
    }
    if (result) {
        result = textBox.AddHandlerChecked(
            UIElement::LostKeyboardFocusEvent,
            focusChangedHandler_);
    }
    if (result) {
        result =
            textBox.AddValueChangedHandlerChecked(
                TextBox::TextProperty,
                propertyChangedHandler_);
    }
    if (result) {
        result =
            textBox.AddValueChangedHandlerChecked(
                TextBox::IsReadOnlyProperty,
                propertyChangedHandler_);
    }
    if (result) {
        result =
            textBox.AddValueChangedHandlerChecked(
                TextBox::MaxLengthProperty,
                propertyChangedHandler_);
    }
    if (result) {
        result =
            textBox.AddValueChangedHandlerChecked(
                UIElement::IsEnabledProperty,
                propertyChangedHandler_);
    }
    if (!result) {
        const Base::Status failure =
            result.GetStatus();
        static_cast<void>(Detach(textBox));
        return failure;
    }
    return {};
}

Base::Result<void>
TextEditBehavior::Attach(
    PasswordBox& passwordBox) noexcept {
    if (Find(passwordBox) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "PasswordBox is already attached");
    }
    if (!passwordBox.GetIsLoaded() ||
        passwordBox.GetTree() != tree_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "PasswordBox must be loaded in the interaction tree");
    }
    Base::Result<void> synced =
        PasswordPolicy(passwordBox.passwordPolicy_)->SetMask(
            passwordBox.GetPasswordChar());
    if (synced) {
        synced =
            passwordBox.
                SynchronizeEditorFromPassword();
    }
    if (synced) {
        passwordBox.editor_.SetForeground(passwordBox.GetForeground());
        passwordBox.editor_.SetSelectionBrush(passwordBox.GetSelectionBrush());
        passwordBox.editor_.SetSelectionOpacity(passwordBox.GetSelectionOpacity());
        passwordBox.editor_.SetCaretBrush(passwordBox.GetCaretBrush());
    }
    if (!synced) return synced.GetStatus();

    Record record;
    record.handle = AeroGuiInternal::Handle(passwordBox);
    record.password = true;
    Base::Result<void> appended =
        records_.PushBack(record);
    if (!appended) return appended.GetStatus();
    if (!captureSubscribed_) {
        input_->AddPointerCaptureChanged(captureChangedHandler_);
        captureSubscribed_ = true;
    }

    Base::Result<void> result =
        passwordBox.AddHandlerChecked(
            UIElement::MouseDownEvent,
            mouseDownHandler_);
    if (result) {
        result = passwordBox.AddHandlerChecked(
            UIElement::MouseMoveEvent,
            mouseMoveHandler_);
    }
    if (result) {
        result = passwordBox.AddHandlerChecked(
            UIElement::MouseUpEvent,
            mouseUpHandler_);
    }
    if (result) {
        result = passwordBox.AddHandlerChecked(
            UIElement::KeyDownEvent,
            keyDownHandler_);
    }
    if (result) {
        result = passwordBox.AddHandlerChecked(
            UIElement::TextInputEvent,
            textInputHandler_);
    }
    if (result) {
        result = passwordBox.AddHandlerChecked(
            UIElement::LostKeyboardFocusEvent,
            focusChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                AddValueChangedHandlerChecked(
                    PasswordBox::
                        PasswordCharProperty,
                    propertyChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                AddValueChangedHandlerChecked(
                    PasswordBox::
                        MaxLengthProperty,
                    propertyChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                AddValueChangedHandlerChecked(
                    PasswordBox::
                        ForegroundProperty,
                    propertyChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                AddValueChangedHandlerChecked(
                    PasswordBox::
                        SelectionBrushProperty,
                    propertyChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                AddValueChangedHandlerChecked(
                    PasswordBox::
                        CaretBrushProperty,
                    propertyChangedHandler_);
    }
    if (result) {
        result =
            passwordBox.
                AddValueChangedHandlerChecked(
                    UIElement::
                        IsEnabledProperty,
                    propertyChangedHandler_);
    }
    if (!result) {
        const Base::Status failure =
            result.GetStatus();
        static_cast<void>(
            Detach(passwordBox));
        return failure;
    }
    return {};
}

Base::Result<bool>
TextEditBehavior::Detach(
    TextBox& textBox) noexcept {
    const std::uint32_t index = Find(textBox);
    if (index == UINT32_MAX) {
        return false;
    }
    Record& record = records_[index];
    if (record.dragging) {
        Base::Result<bool> released =
            input_->ReleasePointer(
                record.pointerId);
        if (!released) {
            return released.GetStatus();
        }
    }
    static_cast<void>(textBox.RemoveHandler(
        UIElement::MouseDownEvent,
        mouseDownHandler_));
    static_cast<void>(textBox.RemoveHandler(
        UIElement::MouseMoveEvent,
        mouseMoveHandler_));
    static_cast<void>(textBox.RemoveHandler(
        UIElement::MouseUpEvent,
        mouseUpHandler_));
    static_cast<void>(textBox.RemoveHandler(
        UIElement::KeyDownEvent,
        keyDownHandler_));
    static_cast<void>(textBox.RemoveHandler(
        UIElement::TextInputEvent,
        textInputHandler_));
    static_cast<void>(textBox.RemoveHandler(
        UIElement::LostKeyboardFocusEvent,
        focusChangedHandler_));
    static_cast<void>(
        textBox.RemoveValueChangedHandler(
            TextBox::TextProperty,
            propertyChangedHandler_));
    static_cast<void>(
        textBox.RemoveValueChangedHandler(
            TextBox::IsReadOnlyProperty,
            propertyChangedHandler_));
    static_cast<void>(
        textBox.RemoveValueChangedHandler(
            TextBox::MaxLengthProperty,
            propertyChangedHandler_));
    static_cast<void>(
        textBox.RemoveValueChangedHandler(
            UIElement::IsEnabledProperty,
            propertyChangedHandler_));
    RemoveAt(index);
    if (records_.Empty() &&
        captureSubscribed_) {
        static_cast<void>(
            input_->RemovePointerCaptureChanged(
                captureChangedHandler_));
        captureSubscribed_ = false;
    }
    return true;
}

Base::Result<bool>
TextEditBehavior::Detach(
    PasswordBox& passwordBox) noexcept {
    const std::uint32_t index =
        Find(passwordBox);
    if (index == UINT32_MAX) {
        return false;
    }
    Record& record = records_[index];
    if (record.dragging) {
        Base::Result<bool> released =
            input_->ReleasePointer(
                record.pointerId);
        if (!released) {
            return released.GetStatus();
        }
    }
    static_cast<void>(
        passwordBox.RemoveHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_));
    static_cast<void>(
        passwordBox.RemoveHandler(
            UIElement::MouseMoveEvent,
            mouseMoveHandler_));
    static_cast<void>(
        passwordBox.RemoveHandler(
            UIElement::MouseUpEvent,
            mouseUpHandler_));
    static_cast<void>(
        passwordBox.RemoveHandler(
            UIElement::KeyDownEvent,
            keyDownHandler_));
    static_cast<void>(
        passwordBox.RemoveHandler(
            UIElement::TextInputEvent,
            textInputHandler_));
    static_cast<void>(
        passwordBox.RemoveHandler(
            UIElement::
                LostKeyboardFocusEvent,
            focusChangedHandler_));
    static_cast<void>(
        passwordBox.
            RemoveValueChangedHandler(
                PasswordBox::
                    PasswordCharProperty,
                propertyChangedHandler_));
    static_cast<void>(
        passwordBox.
            RemoveValueChangedHandler(
                PasswordBox::
                    MaxLengthProperty,
                propertyChangedHandler_));
    static_cast<void>(
        passwordBox.
            RemoveValueChangedHandler(
                PasswordBox::
                    ForegroundProperty,
                propertyChangedHandler_));
    static_cast<void>(
        passwordBox.
            RemoveValueChangedHandler(
                PasswordBox::
                    SelectionBrushProperty,
                propertyChangedHandler_));
    static_cast<void>(
        passwordBox.
            RemoveValueChangedHandler(
                PasswordBox::
                    CaretBrushProperty,
                propertyChangedHandler_));
    static_cast<void>(
        passwordBox.
            RemoveValueChangedHandler(
                UIElement::IsEnabledProperty,
                propertyChangedHandler_));
    passwordBox.SetInputMethodHost(nullptr);
    RemoveAt(index);
    if (records_.Empty() &&
        captureSubscribed_) {
        static_cast<void>(
            input_->RemovePointerCaptureChanged(
                    captureChangedHandler_));
        captureSubscribed_ = false;
    }
    return true;
}

void TextEditBehavior::OnMouseDown(
    Base::Object* sender,
    MouseButtonEventArgs& args) noexcept {
    auto& owner =
        *static_cast<UIElement*>(sender);
    if (args.GetChangedButton() !=
            MouseButton::Left ||
        !owner.GetIsEnabled()) {
        return;
    }
    const std::uint32_t index =
        Find(owner);
    TextBox* editor =
        index != UINT32_MAX
        ? ResolveEditor(index)
        : nullptr;
    if (editor == nullptr) {
        return;
    }
    const Point local =
        ToLocalPoint(
            owner, args.GetPosition());
    const std::uint32_t caret =
        editor->HitTestText(local);
    static_cast<void>(
        editor->SetSelection(caret, caret));
    static_cast<void>(
        input_->SetFocus(&owner));
    Base::Result<void> captured =
        input_->CapturePointer(
            args.GetPointerId(), owner);
    if (captured) {
        records_[index].pointerId =
            args.GetPointerId();
        records_[index].anchor = caret;
        records_[index].dragging = true;
    }
    args.SetHandled(true);
}

void TextEditBehavior::OnMouseMove(
    Base::Object* sender,
    MouseEventArgs& args) noexcept {
    auto& owner =
        *static_cast<UIElement*>(sender);
    const std::uint32_t index =
        Find(owner);
    if (index == UINT32_MAX ||
        !records_[index].dragging ||
        records_[index].pointerId !=
            args.GetPointerId()) {
        return;
    }
    TextBox* editor =
        ResolveEditor(index);
    if (editor == nullptr) return;
    const Point local =
        ToLocalPoint(
            owner, args.GetPosition());
    static_cast<void>(
        editor->SetSelection(
            records_[index].anchor,
            editor->HitTestText(local)));
    args.SetHandled(true);
}

void TextEditBehavior::OnMouseUp(
    Base::Object* sender,
    MouseButtonEventArgs& args) noexcept {
    auto& owner =
        *static_cast<UIElement*>(sender);
    const std::uint32_t index =
        Find(owner);
    if (index == UINT32_MAX ||
        args.GetChangedButton() !=
            MouseButton::Left ||
        !records_[index].dragging ||
        records_[index].pointerId !=
            args.GetPointerId()) {
        return;
    }
    TextBox* editor =
        ResolveEditor(index);
    if (editor == nullptr) return;
    const Point local =
        ToLocalPoint(
            owner, args.GetPosition());
    static_cast<void>(
        editor->SetSelection(
            records_[index].anchor,
            editor->HitTestText(local)));
    records_[index].dragging = false;
    static_cast<void>(
        input_->ReleasePointer(
            args.GetPointerId()));
    args.SetHandled(true);
}

void TextEditBehavior::OnKeyDown(
    Base::Object* sender,
    KeyEventArgs& args) noexcept {
    auto& owner =
        *static_cast<UIElement*>(sender);
    const std::uint32_t index =
        Find(owner);
    TextBox* editor =
        index != UINT32_MAX
        ? ResolveEditor(index)
        : nullptr;
    if (!owner.GetIsEnabled() ||
        editor == nullptr) {
        return;
    }
    const bool password =
        records_[index].password;
    const bool shift =
        HasKeyboardModifier(
            args.GetModifiers(),
            KeyboardModifiers::Shift);
    const bool control =
        HasKeyboardModifier(
            args.GetModifiers(),
            KeyboardModifiers::Control);
    Base::Result<void> result;
    bool handled = true;
    if (control &&
        args.GetKey() == KeyboardKeyA) {
        result = editor->SelectAll();
    } else if (control &&
        args.GetKey() == KeyboardKeyC) {
        result = password
            ? Base::Result<void>{}
            : editor->CopySelection(
                  *clipboard_);
    } else if (control &&
        args.GetKey() == KeyboardKeyX) {
        result = password
            ? editor->ReplaceSelection(
                  Base::StringView{})
            : editor->CutSelection(
                  *clipboard_);
    } else if (control &&
        args.GetKey() == KeyboardKeyV) {
        result = editor->Paste(
            *clipboard_);
    } else if (control &&
        args.GetKey() == KeyboardKeyZ) {
        result = shift
            ? editor->Redo()
            : editor->Undo();
    } else if (control &&
        args.GetKey() == KeyboardKeyY) {
        result = editor->Redo();
    } else if (args.GetKey() ==
        KeyboardKeyLeft) {
        result =
            editor->MoveCaretHorizontal(
                -1.0, shift);
    } else if (args.GetKey() ==
        KeyboardKeyRight) {
        result =
            editor->MoveCaretHorizontal(
                1.0, shift);
    } else if (args.GetKey() ==
        KeyboardKeyHome) {
        result =
            editor->MoveCaretLineBoundary(
                false, shift);
    } else if (args.GetKey() ==
        KeyboardKeyEnd) {
        result =
            editor->MoveCaretLineBoundary(
                true, shift);
    } else if (args.GetKey() ==
        KeyboardKeyBackspace) {
        result =
            editor->DeleteBackward();
    } else if (args.GetKey() ==
        KeyboardKeyDelete) {
        result =
            editor->DeleteForward();
    } else if (args.GetKey() ==
            KeyboardKeyEnter &&
        editor->GetAcceptsReturn()) {
        result = editor->ReplaceSelection(
            Base::StringView("\n"));
    } else {
        handled = false;
    }
    if (handled && result) {
        args.SetHandled(true);
    }
}

void TextEditBehavior::OnTextInput(
    Base::Object* sender,
    TextCompositionEventArgs& args) noexcept {
    auto& owner =
        *static_cast<UIElement*>(sender);
    const std::uint32_t index =
        Find(owner);
    TextBox* editor =
        index != UINT32_MAX
        ? ResolveEditor(index)
        : nullptr;
    if (!owner.GetIsEnabled() ||
        editor == nullptr ||
        editor->GetIsReadOnly()) {
        return;
    }
    if (editor->GetIsComposing()) {
        Base::Result<void> cancelled =
            editor->
                CancelCompositionForFocusLoss();
        if (!cancelled) {
            return;
        }
    }
    Base::Result<void> inserted =
        editor->ReplaceSelection(args.GetText());
    if (inserted) {
        args.SetHandled(true);
    }
}

void TextEditBehavior::OnFocusChanged(
    Base::Object* sender,
    KeyboardFocusChangedEventArgs& args) noexcept {
    auto& owner =
        *static_cast<UIElement*>(sender);
    if (args.GetNewFocus() == &owner) {
        return;
    }
    const std::uint32_t index =
        Find(owner);
    TextBox* editor =
        index != UINT32_MAX
        ? ResolveEditor(index)
        : nullptr;
    if (editor == nullptr) return;
    static_cast<void>(
        editor->
            CancelCompositionForFocusLoss());
    if (!records_[index].dragging) {
        return;
    }
    records_[index].dragging = false;
    static_cast<void>(
        input_->ReleasePointer(
            records_[index].pointerId));
}

void TextEditBehavior::OnPropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (object.RuntimeType() ==
        PasswordBox::StaticTypeId()) {
        auto& passwordBox =
            static_cast<PasswordBox&>(object);
        if (args.GetProperty() ==
                PasswordBox::
                    PasswordCharProperty) {
            static_cast<void>(
                PasswordPolicy(passwordBox.passwordPolicy_)->
                    SetMask(
                        passwordBox.
                            GetPasswordChar()));
            static_cast<void>(
                passwordBox.editor_.
                    InvalidateMeasure());
            static_cast<void>(
                passwordBox.editor_.
                    InvalidateVisual());
        } else if (args.GetProperty() ==
                PasswordBox::
                    MaxLengthProperty) {
            static_cast<void>(
                passwordBox.editor_.
                    CancelCompositionForFocusLoss());
            static_cast<void>(
                Model(passwordBox.validation_).
                    SetMaximumLength(
                        EffectiveMaximumLength(
                            passwordBox.
                                GetMaxLength())));
            static_cast<void>(
                passwordBox.editor_.
                        SetMaxLength(
                        passwordBox.
                            GetMaxLength()));
        } else if (args.GetProperty() ==
                PasswordBox::
                    ForegroundProperty) {
            static_cast<void>(
                passwordBox.editor_.
                    SetForeground(
                        passwordBox.
                            GetForeground()));
        } else if (args.GetProperty() ==
                PasswordBox::
                    SelectionBrushProperty) {
            static_cast<void>(
                passwordBox.editor_.
                    SetSelectionBrush(
                        passwordBox.
                            GetSelectionBrush()));
        } else if (args.GetProperty() ==
                PasswordBox::
                    SelectionOpacityProperty) {
            static_cast<void>(
                passwordBox.editor_.SetSelectionOpacity(
                    passwordBox.GetSelectionOpacity()));
        } else if (args.GetProperty() ==
                PasswordBox::
                    CaretBrushProperty) {
            static_cast<void>(
                passwordBox.editor_.
                    SetCaretBrush(
                        passwordBox.
                            GetCaretBrush()));
        } else if (args.GetProperty() ==
                PasswordBox::
                    PlaceholderProperty) {
            passwordBox.editor_.SetPlaceholder(
                passwordBox.GetPlaceholder());
        } else if (args.GetProperty() ==
                       UIElement::
                           IsEnabledProperty &&
                   !args.GetNewValue().
                       AsBoolean()) {
            static_cast<void>(
                passwordBox.editor_.
                    CancelCompositionForFocusLoss());
        }
        return;
    }
    auto& textBox =
        static_cast<TextBox&>(object);
    if (args.GetProperty() ==
            TextBox::TextProperty) {
        if (!textBox.updatingTextProperty_) {
            static_cast<void>(
                textBox.SynchronizeModel());
        }
    } else if (args.GetProperty() ==
            TextBox::IsReadOnlyProperty) {
        if (args.GetNewValue().AsBoolean()) {
            static_cast<void>(
                textBox.
                    CancelCompositionForFocusLoss());
        }
        static_cast<void>(
                Model(textBox.model_).SetReadOnly(
                args.GetNewValue().AsBoolean()));
    } else if (args.GetProperty() ==
            TextBox::MaxLengthProperty) {
        static_cast<void>(
            textBox.
                CancelCompositionForFocusLoss());
    } else if (args.GetProperty() ==
                   UIElement::IsEnabledProperty &&
               !args.GetNewValue().AsBoolean()) {
        static_cast<void>(
            textBox.
                CancelCompositionForFocusLoss());
    }
}

void TextEditBehavior::OnCaptureChanged(
    std::uint32_t pointerId,
    UIElement* target,
    bool captured) noexcept {
    if (captured || target == nullptr) {
        return;
    }
    for (std::uint32_t index = 0U;
         index < records_.Size(); ++index) {
        UIElement* owner =
            ResolveOwner(index);
        if (owner != target ||
            records_[index].pointerId !=
                pointerId) {
            continue;
        }
        records_[index].dragging = false;
        return;
    }
}

} // namespace Aero::Controls
