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
#include "gui/media/BrushRendering.hpp"
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
        VisualTree(textBox) != tree_) {
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
    records_.PushBack(record);
    if (!captureSubscribed_) {
        input_->AddPointerCaptureChanged(captureChangedHandler_);
        captureSubscribed_ = true;
    }
    textBox.AddHandler(UIElement::MouseDownEvent, mouseDownHandler_);
    textBox.AddHandler(UIElement::MouseMoveEvent, mouseMoveHandler_);
    textBox.AddHandler(UIElement::MouseUpEvent, mouseUpHandler_);
    textBox.AddHandler(UIElement::KeyDownEvent, keyDownHandler_);
    textBox.AddHandler(UIElement::TextInputEvent, textInputHandler_);
    textBox.AddHandler(UIElement::LostKeyboardFocusEvent, focusChangedHandler_);
    textBox.AddValueChangedHandler(TextBox::TextProperty, propertyChangedHandler_);
    textBox.AddValueChangedHandler(TextBox::IsReadOnlyProperty, propertyChangedHandler_);
    textBox.AddValueChangedHandler(TextBox::MaxLengthProperty, propertyChangedHandler_);
    textBox.AddValueChangedHandler(UIElement::IsEnabledProperty, propertyChangedHandler_);
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
        VisualTree(passwordBox) != tree_) {
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
    records_.PushBack(record);
    if (!captureSubscribed_) {
        input_->AddPointerCaptureChanged(captureChangedHandler_);
        captureSubscribed_ = true;
    }

    passwordBox.AddHandler(UIElement::MouseDownEvent, mouseDownHandler_);
    passwordBox.AddHandler(UIElement::MouseMoveEvent, mouseMoveHandler_);
    passwordBox.AddHandler(UIElement::MouseUpEvent, mouseUpHandler_);
    passwordBox.AddHandler(UIElement::KeyDownEvent, keyDownHandler_);
    passwordBox.AddHandler(UIElement::TextInputEvent, textInputHandler_);
    passwordBox.AddHandler(UIElement::LostKeyboardFocusEvent, focusChangedHandler_);
    passwordBox.AddValueChangedHandler(PasswordBox::PasswordCharProperty, propertyChangedHandler_);
    passwordBox.AddValueChangedHandler(PasswordBox::MaxLengthProperty, propertyChangedHandler_);
    passwordBox.AddValueChangedHandler(PasswordBox::ForegroundProperty, propertyChangedHandler_);
    passwordBox.AddValueChangedHandler(PasswordBox::SelectionBrushProperty, propertyChangedHandler_);
    passwordBox.AddValueChangedHandler(PasswordBox::CaretBrushProperty, propertyChangedHandler_);
    passwordBox.AddValueChangedHandler(UIElement::IsEnabledProperty, propertyChangedHandler_);
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
            passwordBox.editor_.InvalidateMeasure();
            passwordBox.editor_.InvalidateVisual();
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

// ---- Section: Selection/caret (merged from TextBoxSelection.cpp) ----

using namespace Primitives;
using namespace ::Aero::Render;

Base::Result<void> TextBox::MoveCaretHorizontal(
    double direction,
    bool extend) noexcept {
    const TextSelection old =
        Model(model_).Selection();
    std::uint32_t next = old.caret;
    if (!extend && !old.GetIsEmpty()) {
        next = direction < 0.0
            ? old.GetStart() : old.GetEnd();
    } else if (direction < 0.0) {
        if (next != 0U) {
            --next;
        }
    } else if (next < Model(model_).GraphemeCount()) {
        ++next;
    }
    SetSelection(extend ? old.anchor : next, next);
    return {};
}

Base::Result<void>
TextBox::MoveCaretLineBoundary(
    bool end,
    bool extend) noexcept {
    const TextSelection old =
        Model(model_).Selection();
    std::uint32_t lineIndex = 0U;
    for (std::uint32_t line = 0U;
         line < Model(model_).LineCount(); ++line) {
        Base::Result<TextRange> range =
            Model(model_).LineRange(line);
        if (!range) {
            return range.GetStatus();
        }
        const std::uint32_t nextStart =
            line + 1U < Model(model_).LineCount()
            ? range.Value().GetEnd() + 1U
            : Model(model_).GraphemeCount() + 1U;
        if (old.caret < nextStart) {
            lineIndex = line;
            break;
        }
    }
    Base::Result<TextRange> range =
        Model(model_).LineRange(lineIndex);
    if (!range) {
        return range.GetStatus();
    }
    const std::uint32_t next = end
        ? range.Value().GetEnd()
        : range.Value().start;
    SetSelection(extend ? old.anchor : next, next);
    return {};
}

double TextBox::GetLineHeight() const noexcept {
    if (!caretStops_.Empty() &&
        caretStops_[0].height > 0.0) {
        return caretStops_[0].height;
    }
    return GetFontSize() * 1.6 /
        std::max(1.0, GetDpiScale());
}

Rect TextBox::GetCaretRectangle() const noexcept {
    if (caretStops_.Empty()) {
        return {
            -scroll_.horizontalOffset,
            -scroll_.verticalOffset,
            CaretWidth / std::max(1.0, GetDpiScale()),
            GetLineHeight()};
    }
    const std::uint32_t index =
        std::min(
            Model(GetActiveModel()).Caret(),
            caretStops_.Size() - 1U);
    const CaretStop& stop =
        caretStops_[index];
    return {
        stop.x - scroll_.horizontalOffset,
        stop.y - scroll_.verticalOffset,
        CaretWidth / std::max(1.0, GetDpiScale()),
        stop.height};
}

std::uint32_t TextBox::HitTestText(
    Point position) const noexcept {
    if (caretStops_.Empty()) {
        return 0U;
    }
    const double x =
        position.x -
            GetPadding().left +
            scroll_.horizontalOffset;
    const double y =
        position.y -
            GetPadding().top +
            scroll_.verticalOffset;
    std::uint32_t best = 0U;
    double bestDistance =
        std::numeric_limits<double>::infinity();
    for (std::uint32_t index = 0U;
         index < caretStops_.Size(); ++index) {
        const CaretStop& stop =
            caretStops_[index];
        const double vertical =
            y < stop.y
            ? stop.y - y
            : (y > stop.y + stop.height
                ? y - (stop.y + stop.height)
                : 0.0);
        const double distance =
            vertical * 10000.0 +
            std::abs(x - stop.x);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = index;
        }
    }
    return best;
}

Base::Result<void>
TextBox::RebuildCaretStops() noexcept {
    caretStops_.Clear();
    const auto& active = Model(GetActiveModel());
    const std::uint32_t graphemes =
        active.GraphemeCount();
    caretStops_.Reserve(
            graphemes + 1U);
    const std::uint32_t lines =
        std::max(1U, active.LineCount());
    std::uint32_t maximumLineLength = 0U;
    for (std::uint32_t line = 0U;
         line < lines; ++line) {
        Base::Result<TextRange> range =
            active.LineRange(line);
        if (!range) {
            return range.GetStatus();
        }
        maximumLineLength =
            std::max(
                maximumLineLength,
                range.Value().length);
    }
    std::uint32_t visualLines = 0U;
    for (std::uint32_t line = 0U;
         line < lines; ++line) {
        Base::Result<TextRange> range =
            active.LineRange(line);
        if (!range) {
            return range.GetStatus();
        }
        const std::uint32_t length =
            range.Value().length;
        const std::uint32_t wrapped =
            wrapColumns_ == UINT32_MAX
            ? 1U
            : std::max(
                  1U,
                  (length + wrapColumns_ - 1U) /
                      wrapColumns_);
        visualLines += wrapped;
    }
    visualLines = std::max(1U, visualLines);
    const double lineHeight =
        textSize_.height > 0.0
        ? textSize_.height /
            static_cast<double>(visualLines)
        : GetFontSize() * 1.6 /
            std::max(1.0, GetDpiScale());
    const double advance =
        wrapColumns_ != UINT32_MAX
        ? DefaultAdvance *
              GetFontSize() / 16.0 /
              std::max(1.0, GetDpiScale())
        : maximumLineLength != 0U &&
            textSize_.width > 0.0
        ? textSize_.width /
            static_cast<double>(
                maximumLineLength)
        : DefaultAdvance *
              GetFontSize() / 16.0 /
            std::max(1.0, GetDpiScale());

    caretStops_.Resize(
            graphemes + 1U);
    std::uint32_t visualLineBase = 0U;
    for (std::uint32_t line = 0U;
         line < lines; ++line) {
        Base::Result<TextRange> range =
            active.LineRange(line);
        if (!range) {
            return range.GetStatus();
        }
        const std::uint32_t start =
            range.Value().start;
        const std::uint32_t end =
            range.Value().GetEnd();
        for (std::uint32_t index = start;
             index <= end; ++index) {
            const std::uint32_t offset =
                index - start;
            std::uint32_t visualLineOffset = 0U;
            std::uint32_t column = offset;
            if (wrapColumns_ != UINT32_MAX) {
                visualLineOffset =
                    offset / wrapColumns_;
                column = offset % wrapColumns_;
                if (offset == range.Value().length &&
                    offset != 0U &&
                    column == 0U) {
                    --visualLineOffset;
                    column = wrapColumns_;
                }
            }
            caretStops_[index] = {
                static_cast<double>(column) *
                    advance,
                static_cast<double>(
                    visualLineBase +
                    visualLineOffset) *
                    lineHeight,
                lineHeight,
                visualLineBase +
                    visualLineOffset};
        }
        const std::uint32_t wrappedLines =
            wrapColumns_ == UINT32_MAX
            ? 1U
            : std::max(
                  1U,
                  (range.Value().length +
                   wrapColumns_ - 1U) /
                      wrapColumns_);
        visualLineBase += wrappedLines;
        if (line + 1U < lines &&
            end < graphemes) {
            caretStops_[end + 1U] = {
                0.0,
                static_cast<double>(
                    visualLineBase) *
                    lineHeight,
                lineHeight,
                visualLineBase};
        }
    }
    return {};
}

Size TextBox::MeasureOverride(
    Size availableSize) noexcept {
    Base::Result<void> display =
        DisplayPolicy(displayPolicy_)->BuildDisplayText(
            Model(GetActiveModel()), displayText_);
    if (!display) {
        return Size{};
    }
    ReleaseGlyphRuns();
    textSize_ = {};
    wrapColumns_ = UINT32_MAX;
    showingPlaceholder_ =
        displayText_.Empty() &&
        !GetPlaceholder().Empty();
    if (showingPlaceholder_) {
        display = displayText_.Assign(
            GetPlaceholder());
        if (!display) {
            return Size{};
        }
    }
    const Thickness padding = GetPadding();
    const Size contentAvailable =
        Deflate(availableSize, padding);
    if (GetTextWrapping() !=
            TextWrapping::NoWrap &&
        contentAvailable.width > 0.0) {
        const double fallbackAdvance =
            DefaultAdvance *
            GetFontSize() / 16.0 /
            std::max(1.0, GetDpiScale());
        const double columns =
            std::floor(
                contentAvailable.width /
                fallbackAdvance);
        wrapColumns_ = static_cast<std::uint32_t>(
            std::min(
                static_cast<double>(UINT32_MAX),
                std::max(1.0, columns)));
    }
    auto* layoutService = LayoutService(*this);
    if (layoutService != nullptr &&
        !displayText_.Empty()) {
        TextLayoutRequest request;
        request.text = displayText_.View();
        request.availableSize =
            contentAvailable;
        request.dpiScale = GetDpiScale();
        request.pixelSize =
            static_cast<float>(GetFontSize());
        request.lineHeight =
            static_cast<float>(
                GetFontSize() * 1.6);
        const Base::Ref<Media::FontFamily> configuredFamily =
            GetFontFamily();
        Base::StringView family = configuredFamily
            ? configuredFamily->GetSource()
            : Base::StringView{};
        const bool defaultFamily =
            family.Empty() ||
            family == Base::StringView(
                "Segoe UI");
        if (defaultFamily) {
            const bool bold =
                GetFontWeight() ==
                    FontWeight::Bold ||
                GetFontWeight() ==
                    FontWeight::SemiBold;
            const bool italic =
                GetFontStyle() !=
                    FontStyle::Normal;
            if (bold && italic) {
                family = Base::StringView(
                    "Segoe UI Bold Italic");
            } else if (bold) {
                family = Base::StringView(
                    "Segoe UI Bold");
            } else if (italic) {
                family = Base::StringView(
                    "Segoe UI Italic");
            }
        }
        request.fontFamily = family;
        request.wrapping = GetTextWrapping();
        request.alignment = GetTextAlignment();
        request.direction = GetFlowDirection() == FlowDirection::RightToLeft
            ? Text::TextDirection::RightToLeft
            : Text::TextDirection::LeftToRight;
        TextLayoutResult result;
        Base::Result<void> prepared =
            layoutService->ShapeAndPrepare(
                request, result);
        if (!prepared) {
            return Size{};
        }
        for (RenderGlyphRunId glyph :
             result.glyphRuns) {
            if (glyph ==
                InvalidRenderGlyphRunId) {
                for (RenderGlyphRunId release :
                     result.glyphRuns) {
                    if (release !=
                        InvalidRenderGlyphRunId) {
                        layoutService->
                            ReleaseGlyphRun(release);
                    }
                }
                return Size{};
            }
        }
        glyphRuns_ =
            std::move(result.glyphRuns);
        serviceOwnsGlyphRuns_ =
            !glyphRuns_.Empty();
        textSize_ = result.desiredSize;
    } else {
        std::uint32_t maximumLine = 0U;
        std::uint32_t visualLineCount = 0U;
        const auto& active = Model(GetActiveModel());
        for (std::uint32_t line = 0U;
             line < active.LineCount();
             ++line) {
            Base::Result<TextRange> range =
                active.LineRange(line);
            if (!range) {
                return Size{};
            }
            maximumLine = std::max(
                maximumLine,
                range.Value().length);
            visualLineCount +=
                wrapColumns_ == UINT32_MAX
                ? 1U
                : std::max(
                      1U,
                      (range.Value().length +
                       wrapColumns_ - 1U) /
                          wrapColumns_);
        }
        const std::uint32_t visibleColumns =
            wrapColumns_ == UINT32_MAX
            ? maximumLine
            : std::min(
                  maximumLine,
                  wrapColumns_);
        textSize_ = {
            static_cast<double>(visibleColumns) *
                DefaultAdvance *
                GetFontSize() / 16.0 /
                std::max(1.0, GetDpiScale()),
            static_cast<double>(
                std::max(1U, visualLineCount)) *
                GetFontSize() * 1.6 /
                std::max(1.0, GetDpiScale())};
    }
    Base::Result<void> stops =
        RebuildCaretStops();
    if (!stops) {
        return Size{};
    }
    scroll_.extentWidth = textSize_.width;
    scroll_.extentHeight =
        std::max(textSize_.height, GetLineHeight());
    scroll_.horizontalOffset = ClampOffset(
        scroll_.horizontalOffset,
        scroll_.extentWidth,
        scroll_.viewportWidth);
    scroll_.verticalOffset = ClampOffset(
        scroll_.verticalOffset,
        scroll_.extentHeight,
        scroll_.viewportHeight);
    if (GetIsKeyboardFocused() ||
        compositionActive_) {
        Base::Result<void> visible =
            EnsureCaretVisible();
        if (!visible) {
            return Size{};
        }
    }
    static_cast<void>(
        UpdateCandidateWindow());
    const double minimumWidth =
        DefaultAdvance *
        GetFontSize() / 16.0 /
        std::max(1.0, GetDpiScale());
    Size desired{
        std::min(
            std::max(minimumWidth, textSize_.width),
            contentAvailable.width),
        std::min(
            std::max(GetLineHeight(), textSize_.height),
            contentAvailable.height)};
    const std::uint32_t maximumLines =
        GetMaxLines();
    const std::uint32_t minimumLines =
        GetMinLines();
    if (maximumLines != 0U) {
        const double lineBoxHeight =
            std::max(
                GetLineHeight(),
                GetFontSize() * 1.6);
        const double maximumHeight =
            lineBoxHeight *
            static_cast<double>(
                maximumLines);
        if (GetTextWrapping() !=
                TextWrapping::NoWrap &&
            !displayText_.Empty()) {
            desired.height = std::min(
                contentAvailable.height,
                maximumHeight);
        } else {
            desired.height = std::min(
                desired.height,
                maximumHeight);
        }
    }
    desired.height = std::max(
        desired.height,
        std::min(
            contentAvailable.height,
            std::max(
                GetLineHeight(),
                GetFontSize() * 1.6) *
                static_cast<double>(
                    minimumLines)));
    const Size textInflated = Inflate(desired, padding);
    Size templateSize{};
    if (GetTemplateRoot() != nullptr) {
        templateSize = Control::MeasureOverride(availableSize);
    }
    return Size{
        std::max(templateSize.width, textInflated.width),
        std::max(templateSize.height, textInflated.height)};
}

Size TextBox::ArrangeOverride(
    Size finalSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        Control::ArrangeOverride(finalSize);
    }
    const Size contentViewport =
        Deflate(finalSize, GetPadding());
    SetViewport(contentViewport);
    if (GetIsKeyboardFocused() ||
        compositionActive_) {
        Base::Result<void> visible =
            EnsureCaretVisible();
        if (!visible) {
            return finalSize;
        }
    }
    static_cast<void>(
        UpdateCandidateWindow());
    return finalSize;
}

void TextBox::OnApplyTemplate() noexcept {
    Control::OnApplyTemplate();
    DependencyObject* part = GetTemplateChild(Base::StringView("PART_ContentHost"));
    if (part != nullptr && AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
            part->RuntimeType(), ScrollViewer::StaticTypeId())) {
        static_cast<void>(AttachScrollViewer(static_cast<ScrollViewer*>(part)));
    } else {
        static_cast<void>(AttachScrollViewer(nullptr));
    }
}

void TextBox::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    if (GetTemplateRoot() != nullptr) return;
    auto& builder = Aero::Render::DrawingBridge::Builder(context);
    const Rect bounds{
        0.0, 0.0,
        GetRenderSize().width,
        GetRenderSize().height};
    const Thickness border =
        GetBorderThickness();
    const double borderThickness = std::max(
        std::max(border.left, border.right),
        std::max(border.top, border.bottom));
    Color borderBrush =
        ::Aero::Media::SampleBrush(GetBorderBrush());
    if (GetIsKeyboardFocused() && GetIsEnabled()) {
        borderBrush = Color{
            11.0F / 255.0F,
            128.0F / 255.0F,
            193.0F / 255.0F,
            1.0F};
    } else if (GetIsMouseOver() && GetIsEnabled()) {
        borderBrush = Color{
            93.0F / 255.0F,
            100.0F / 255.0F,
            105.0F / 255.0F,
            1.0F};
    }
    Base::Result<void> chrome =
        builder.FillRoundedRect(
            bounds,
            ::Aero::Media::SampleBrush(GetBackground()),
            1.75);
    if (!chrome) {
        return;
    }
    if (borderThickness > 0.0 &&
        borderBrush.alpha > 0.0F) {
        chrome = builder.StrokeRect(
            bounds,
            borderBrush,
            GetIsKeyboardFocused()
                ? std::max(1.0, borderThickness)
                : borderThickness);
        if (!chrome) {
            return;
        }
    }
    static_cast<void>(RenderEditor(
        context,
        GetRenderSize(),
        GetIsKeyboardFocused()));
}

Base::Result<void>
TextBox::RenderEditor(
    ::Aero::Media::DrawingContext& context,
    Size viewport,
    bool drawCaret) noexcept {
    auto& builder = Aero::Render::DrawingBridge::Builder(context);
    Thickness padding = GetPadding();
    if (scrollViewer_ != nullptr) {
        const Rect svSlot = scrollViewer_->GetLayoutSlot();
        padding.left += svSlot.x;
        padding.top += svSlot.y;
    }
    const Rect contentBounds{
        padding.left,
        padding.top,
        std::max(
            0.0,
            viewport.width -
                padding.left -
                padding.right),
        std::max(
            0.0,
            viewport.height -
                padding.top -
                padding.bottom)};
    Base::Result<void> clip =
        builder.PushClip(contentBounds);
    if (!clip) {
        return clip;
    }
    Base::Result<void> transform =
        builder.PushTransform(Transform2D{
            1.0, 0.0, 0.0, 1.0,
            padding.left -
                scroll_.horizontalOffset,
            padding.top -
                scroll_.verticalOffset});
    if (!transform) {
        return transform;
    }
    const TextSelection selection =
        Model(GetActiveModel()).Selection();
    if (!selection.GetIsEmpty() &&
        !caretStops_.Empty()) {
        const std::uint32_t begin =
            selection.GetStart();
        const std::uint32_t end =
            selection.GetEnd();
        std::uint32_t index = begin;
        while (index < end) {
            const CaretStop& first =
                caretStops_[index];
            std::uint32_t lineEnd =
                index + 1U;
            while (lineEnd < end &&
                lineEnd < caretStops_.Size() &&
                caretStops_[lineEnd].line ==
                    first.line) {
                ++lineEnd;
            }
            const CaretStop& last =
                caretStops_[
                    std::min(
                        lineEnd,
                        caretStops_.Size() - 1U)];
            const double width =
                last.line == first.line
                ? std::max(
                    0.0,
                    last.x - first.x)
                : std::max(
                    DefaultAdvance,
                    textSize_.width - first.x);
            Color selectionColor =
                ::Aero::Media::SampleBrush(
                    GetSelectionBrush(),
                    0.5,
                    Color{
                        46.0F / 255.0F,
                        174.0F / 255.0F,
                        235.0F / 255.0F,
                        1.0F});
            selectionColor.alpha *=
                static_cast<float>(
                    GetSelectionOpacity());
            Base::Result<void> filled =
                builder.FillRect(
                    {first.x, first.y,
                     width, first.height},
                    selectionColor);
            if (!filled) {
                return filled;
            }
            index = lineEnd;
        }
    }
    for (RenderGlyphRunId glyph :
         glyphRuns_) {
            Base::Result<void> drawn =
                builder.DrawGlyphRun(
                    glyph,
                    showingPlaceholder_
                    ? ::Aero::Media::SampleBrush(
                        GetPlaceholderForeground(),
                        0.5,
                        Color{
                            123.0F / 255.0F,
                            128.0F / 255.0F,
                            133.0F / 255.0F,
                            1.0F})
                    : ::Aero::Media::SampleBrush(
                        GetForeground(),
                        0.5,
                        Color{
                            0.0F, 0.0F, 0.0F, 1.0F}));
        if (!drawn) {
            return drawn;
        }
    }
    if (drawCaret &&
        !showingPlaceholder_) {
        Rect caret = GetCaretRectangle();
        caret.x += scroll_.horizontalOffset;
        caret.y += scroll_.verticalOffset;
        Base::Result<void> drawn =
            builder.FillRect(
                caret,
                ::Aero::Media::SampleBrush(
                    GetCaretBrush(),
                    0.5,
                    Color{
                        0.0F, 0.0F, 0.0F, 1.0F}));
        if (!drawn) {
            return drawn;
        }
    }
    Base::Result<void> popTransform =
        builder.PopTransform();
    if (!popTransform) {
        return popTransform;
    }
    return builder.PopClip();
}

void TextBox::ReleaseGlyphRuns() noexcept {
    auto* layoutService = LayoutService(*this);
    if (serviceOwnsGlyphRuns_ && layoutService != nullptr) {
        for (RenderGlyphRunId glyph :
             glyphRuns_) {
            layoutService->ReleaseGlyphRun(glyph);
        }
    }
    glyphRuns_.Clear();
    serviceOwnsGlyphRuns_ = false;
}

void TextBox::SetViewport(
    Size viewport) noexcept {
    if (!IsValidLayoutSize(viewport)) {
        return;
    }
    const ScrollData old = scroll_;
    scroll_.viewportWidth = viewport.width;
    scroll_.viewportHeight = viewport.height;
    scroll_.horizontalOffset = ClampOffset(
        scroll_.horizontalOffset,
        scroll_.extentWidth,
        scroll_.viewportWidth);
    scroll_.verticalOffset = ClampOffset(
        scroll_.verticalOffset,
        scroll_.extentHeight,
        scroll_.viewportHeight);
    const bool changed =
        old.viewportWidth !=
            scroll_.viewportWidth ||
        old.viewportHeight !=
            scroll_.viewportHeight ||
        old.horizontalOffset !=
            scroll_.horizontalOffset ||
        old.verticalOffset !=
            scroll_.verticalOffset;
    if (changed) {
        InvalidateVisual();
    }
}

void TextBox::SetHorizontalOffset(
    double value) noexcept {
    if (!std::isfinite(value)) {
        return;
    }
    const double next = ClampOffset(
        value,
        scroll_.extentWidth,
        scroll_.viewportWidth);
    if (next == scroll_.horizontalOffset) {
        return;
    }
    scroll_.horizontalOffset = next;
    InvalidateVisual();
}

void TextBox::SetVerticalOffset(
    double value) noexcept {
    if (!std::isfinite(value)) {
        return;
    }
    const double next = ClampOffset(
        value,
        scroll_.extentHeight,
        scroll_.viewportHeight);
    if (next == scroll_.verticalOffset) {
        return;
    }
    scroll_.verticalOffset = next;
    InvalidateVisual();
}

Base::Result<bool> TextBox::LineHorizontal(
    double direction) noexcept {
    const double old = scroll_.horizontalOffset;
    SetHorizontalOffset(old + direction * ScrollLine);
    return old != scroll_.horizontalOffset;
}

Base::Result<bool> TextBox::LineVertical(
    double direction) noexcept {
    const double old = scroll_.verticalOffset;
    SetVerticalOffset(old + direction * GetLineHeight());
    return old != scroll_.verticalOffset;
}

Base::Result<bool> TextBox::PageHorizontal(
    double direction) noexcept {
    const double old = scroll_.horizontalOffset;
    SetHorizontalOffset(old + direction * scroll_.viewportWidth);
    return old != scroll_.horizontalOffset;
}

Base::Result<bool> TextBox::PageVertical(
    double direction) noexcept {
    const double old = scroll_.verticalOffset;
    SetVerticalOffset(old + direction * scroll_.viewportHeight);
    return old != scroll_.verticalOffset;
}

Base::Result<void>
TextBox::EnsureCaretVisible() noexcept {
    const Rect caret = GetCaretRectangle();
    double horizontal =
        scroll_.horizontalOffset;
    double vertical =
        scroll_.verticalOffset;
    const double contentX =
        caret.x + scroll_.horizontalOffset;
    const double contentY =
        caret.y + scroll_.verticalOffset;
    if (contentX < horizontal) {
        horizontal = contentX;
    } else if (contentX + caret.width >
        horizontal + scroll_.viewportWidth) {
        horizontal = contentX + caret.width -
            scroll_.viewportWidth;
    }
    if (contentY < vertical) {
        vertical = contentY;
    } else if (contentY + caret.height >
        vertical + scroll_.viewportHeight) {
        vertical = contentY + caret.height -
            scroll_.viewportHeight;
    }
    SetHorizontalOffset(horizontal);
    SetVerticalOffset(vertical);
    return {};
}

Base::Result<void>
TextBox::UpdateCandidateWindow() noexcept {
    if (inputMethodHost_ == nullptr ||
        !compositionActive_) {
        return {};
    }
    Input::ImeCandidateWindow candidate;
    Rect caret = GetCaretRectangle();
    caret.x += GetPadding().left;
    caret.y += GetPadding().top;
    UIElement& owner =
        coordinateOwner_ != nullptr
        ? *coordinateOwner_
        : static_cast<UIElement&>(*this);
    candidate.caret =
        ToRootRect(owner, caret);
    candidate.dpiScale = GetDpiScale();
    inputMethodHost_->SetCandidateWindow(candidate);
    return {};
}

} // namespace Aero::Controls
