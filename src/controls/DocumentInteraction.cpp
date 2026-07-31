#include <Aero/Documents/Documents.hpp>
#include <Aero/Presentation/Transforms.hpp>

#include "RuntimeManagers.hpp"

#include <algorithm>
#include <utility>

namespace Aero::Detail {

using HyperlinkInteractionManager =
    ControlRuntimeAccess::HyperlinkInteractionManager;
using DocumentSelectionManager =
    ControlRuntimeAccess::DocumentSelectionManager;
using namespace Aero::Core;
using namespace Aero::Presentation;
using namespace Aero::Documents;

namespace {
bool TryToDocumentLocalPoint(
    TextBlock& owner, Base::Object* originalSource,
    Point sourcePoint, Point& documentPoint) noexcept {
    UIElement* current = &owner;
    if (originalSource != nullptr) {
        Visual* sourceVisual = static_cast<Visual*>(originalSource);
        current = sourceVisual->AsUIElement();
        if (current == nullptr) return false;
    }
    Point point = sourcePoint;
    while (current != &owner) {
        FrameworkElement* framework = current->AsFrameworkElement();
        if (framework != nullptr) {
            point = TransformPoint(framework->LocalVisualTransform(), point);
        }
        const Rect slot = current->LayoutSlot();
        point.x += slot.x;
        point.y += slot.y;
        Visual* parent = current->VisualParent();
        current = parent != nullptr ? parent->AsUIElement() : nullptr;
        if (current == nullptr) return false;
    }
    documentPoint = point;
    return true;
}
} // namespace

HyperlinkInteractionManager::HyperlinkInteractionManager(
    ObjectTree& tree,
    RoutedEventManager& events,
    PointerInputManager& pointer,
    FocusManager& focus,
    CommandManager& commands) noexcept
    : tree_(&tree),
      events_(&events),
      pointer_(&pointer),
      focus_(&focus),
      commands_(&commands),
      links_(&Base::GetDefaultAllocator()),
      mouseDownHandler_(this, &HyperlinkInteractionManager::OnMouseDown),
      mouseUpHandler_(this, &HyperlinkInteractionManager::OnMouseUp),
      keyDownHandler_(this, &HyperlinkInteractionManager::OnKeyDown),
      keyUpHandler_(this, &HyperlinkInteractionManager::OnKeyUp),
      focusChangedHandler_(this, &HyperlinkInteractionManager::OnFocusChanged),
      propertyChangedHandler_(this, &HyperlinkInteractionManager::OnPropertyChanged),
      requeryHandler_(this, &HyperlinkInteractionManager::OnRequerySuggested) {}

HyperlinkInteractionManager::~HyperlinkInteractionManager() noexcept {
    if (initialized_) {
        static_cast<void>(
            commands_->RemoveRequerySuggested(requeryHandler_));
    }
    while (!links_.Empty()) {
        const std::uint32_t index = links_.Size() - 1U;
        Documents::Hyperlink* link = Resolve(index);
        if (link != nullptr) {
            static_cast<void>(link->RemoveHandler(
                UIElement::MouseDownEvent, mouseDownHandler_));
            static_cast<void>(link->RemoveHandler(
                UIElement::MouseUpEvent, mouseUpHandler_));
            static_cast<void>(link->RemoveHandler(
                UIElement::KeyDownEvent, keyDownHandler_));
            static_cast<void>(link->RemoveHandler(
                UIElement::KeyUpEvent, keyUpHandler_));
            static_cast<void>(link->RemoveHandler(
                UIElement::GotKeyboardFocusEvent,
                focusChangedHandler_));
            static_cast<void>(link->RemoveHandler(
                UIElement::LostKeyboardFocusEvent,
                focusChangedHandler_));
            static_cast<void>(link->RemoveValueChangedHandler(
                Documents::Hyperlink::CommandProperty,
                propertyChangedHandler_));
            static_cast<void>(link->RemoveValueChangedHandler(
                UIElement::IsEnabledProperty,
                propertyChangedHandler_));
        }
        UnsubscribeCommand(links_[index]);
        links_.PopBack();
    }
}

Base::Result<void> HyperlinkInteractionManager::Initialize() noexcept {
    if (initialized_) return {};
    Base::Result<void> result =
        commands_->TryAddRequerySuggested(requeryHandler_);
    if (!result) return result.GetStatus();
    initialized_ = true;
    return {};
}

std::uint32_t HyperlinkInteractionManager::Find(
    const Documents::Hyperlink& link) const noexcept {
    for (std::uint32_t index = 0U; index < links_.Size(); ++index) {
        if (tree_->ResolveHandle(links_[index].handle) == &link) {
            return index;
        }
    }
    return UINT32_MAX;
}

Documents::Hyperlink* HyperlinkInteractionManager::Resolve(
    std::uint32_t index) noexcept {
    Visual* visual = tree_->ResolveHandle(links_[index].handle);
    return visual != nullptr
        ? static_cast<Documents::Hyperlink*>(visual->AsUIElement())
        : nullptr;
}

void HyperlinkInteractionManager::UnsubscribeCommand(
    Record& record) noexcept {
    if (record.command) {
        static_cast<void>(record.command->RemoveCanExecuteChanged(
            requeryHandler_));
        record.command.Reset();
    }
}

Base::Result<void> HyperlinkInteractionManager::SubscribeCommand(
    Documents::Hyperlink& link,
    Record& record) noexcept {
    UnsubscribeCommand(record);
    ICommand* command = link.Command();
    if (command == nullptr) return {};
    record.command = Base::Ref<ICommand>::FromBorrowed(*command);
    Base::Result<void> result =
        record.command->TryAddCanExecuteChanged(requeryHandler_);
    if (!result) record.command.Reset();
    return result;
}

Base::Result<void> HyperlinkInteractionManager::Attach(
    Documents::Hyperlink& link) noexcept {
    Base::Result<void> ready = Initialize();
    if (!ready) return ready.GetStatus();
    if (Find(link) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Hyperlink is already attached to interaction services");
    }
    if (!link.IsLoaded() || link.OwningTree() != tree_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Hyperlink must be loaded in the interaction tree");
    }
    Base::Result<VisualHandle> handle = tree_->GetHandle(link);
    if (!handle) return handle.GetStatus();
    Record record;
    record.handle = handle.Value();
    Base::Result<void> appended = links_.TryPushBack(std::move(record));
    if (!appended) return appended.GetStatus();

    Base::Result<void> result = link.TryAddHandler(
        UIElement::MouseDownEvent, mouseDownHandler_);
    if (result) result = link.TryAddHandler(
        UIElement::MouseUpEvent, mouseUpHandler_);
    if (result) result = link.TryAddHandler(
        UIElement::KeyDownEvent, keyDownHandler_);
    if (result) result = link.TryAddHandler(
        UIElement::KeyUpEvent, keyUpHandler_);
    if (result) result = link.TryAddHandler(
        UIElement::GotKeyboardFocusEvent, focusChangedHandler_);
    if (result) result = link.TryAddHandler(
        UIElement::LostKeyboardFocusEvent, focusChangedHandler_);
    if (result) result = link.TryAddValueChangedHandler(
        Documents::Hyperlink::CommandProperty,
        propertyChangedHandler_);
    if (result) result = link.TryAddValueChangedHandler(
        UIElement::IsEnabledProperty,
        propertyChangedHandler_);
    if (result) result = SubscribeCommand(link, links_.Back());
    if (result) result = RefreshCanExecute(link);
    if (!result) {
        const Base::Status status = result.GetStatus();
        static_cast<void>(Detach(link));
        return status;
    }
    return {};
}

Base::Result<bool> HyperlinkInteractionManager::Detach(
    Documents::Hyperlink& link) noexcept {
    const std::uint32_t index = Find(link);
    if (index == UINT32_MAX) return false;
    Record& record = links_[index];
    if (record.pointerDown) {
        static_cast<void>(pointer_->ReleasePointer(record.pointerId));
    }
    static_cast<void>(link.RemoveHandler(
        UIElement::MouseDownEvent, mouseDownHandler_));
    static_cast<void>(link.RemoveHandler(
        UIElement::MouseUpEvent, mouseUpHandler_));
    static_cast<void>(link.RemoveHandler(
        UIElement::KeyDownEvent, keyDownHandler_));
    static_cast<void>(link.RemoveHandler(
        UIElement::KeyUpEvent, keyUpHandler_));
    static_cast<void>(link.RemoveHandler(
        UIElement::GotKeyboardFocusEvent, focusChangedHandler_));
    static_cast<void>(link.RemoveHandler(
        UIElement::LostKeyboardFocusEvent, focusChangedHandler_));
    static_cast<void>(link.RemoveValueChangedHandler(
        Documents::Hyperlink::CommandProperty,
        propertyChangedHandler_));
    static_cast<void>(link.RemoveValueChangedHandler(
        UIElement::IsEnabledProperty,
        propertyChangedHandler_));
    UnsubscribeCommand(record);
    if (index + 1U != links_.Size()) {
        links_[index] = std::move(links_.Back());
    }
    links_.PopBack();
    return true;
}

Base::Result<void> HyperlinkInteractionManager::RefreshCanExecute(
    Documents::Hyperlink& link) noexcept {
    const std::uint32_t index = Find(link);
    if (index == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Hyperlink is not attached to interaction services");
    }
    bool enabled = true;
    ICommand* command = link.Command();
    if (command != nullptr) {
        UIElement* target = link.CommandTarget();
        if (target == nullptr) target = &link;
        Base::Ref<Base::Object> parameter = link.CommandParameter();
        const Value value = Value::FromObject(
            TypeOf<Base::Object>(), std::move(parameter));
        Base::Result<bool> allowed =
            command->CanExecute(*commands_, value, *target);
        if (!allowed) return allowed.GetStatus();
        enabled = allowed.Value();
    }
    links_[index].commandEnabled = enabled;
    if (!enabled && focus_->FocusedNode() == &link) {
        Base::Result<bool> cleared = focus_->ClearFocus();
        if (!cleared) return cleared.GetStatus();
    }
    return {};
}

Base::Result<void> HyperlinkInteractionManager::Invoke(
    Documents::Hyperlink& link) noexcept {
    const std::uint32_t index = Find(link);
    if (index == UINT32_MAX || !link.IsEnabled() ||
        !links_[index].commandEnabled) {
        return {};
    }
    RoutedEventArgs args;
    Base::Result<void> raised = events_->RaiseEvent(
        link, Documents::Hyperlink::ClickEvent, &args);
    if (!raised) return raised.GetStatus();

    ICommand* command = link.Command();
    if (command != nullptr) {
        UIElement* target = link.CommandTarget();
        if (target == nullptr) target = &link;
        Base::Ref<Base::Object> parameter = link.CommandParameter();
        const Value value = Value::FromObject(
            TypeOf<Base::Object>(), std::move(parameter));
        if (command->RuntimeType() == RoutedCommand::StaticTypeId()) {
            Base::Result<bool> executed = commands_->Execute(
                static_cast<RoutedCommand&>(*command), value, *target);
            if (!executed) return executed.GetStatus();
        } else {
            Base::Result<bool> allowed =
                command->CanExecute(*commands_, value, *target);
            if (!allowed) return allowed.GetStatus();
            if (allowed.Value()) {
                Base::Result<void> executed =
                    command->Execute(*commands_, value, *target);
                if (!executed) return executed.GetStatus();
            }
        }
    }

    const Base::StringView uri = link.NavigateUri();
    if (!uri.Empty()) {
        Documents::RequestNavigateEventArgs navigate(uri, &link);
        raised = events_->RaiseEvent(
            link, Documents::Hyperlink::RequestNavigateEvent,
            &navigate);
        if (!raised) return raised.GetStatus();
    }
    return {};
}

void HyperlinkInteractionManager::OnMouseDown(
    Base::Object* sender,
    const MouseButtonEventArgs& args) noexcept {
    auto& link = *static_cast<Documents::Hyperlink*>(sender);
    if (args.changedButton != MouseButton::Left || !link.IsEnabled()) {
        return;
    }
    const std::uint32_t index = Find(link);
    if (index == UINT32_MAX || !links_[index].commandEnabled) return;
    Record& record = links_[index];
    record.pointerId = args.pointerId;
    record.pointerDown = true;
    static_cast<void>(pointer_->CapturePointer(args.pointerId, link));
    static_cast<void>(focus_->SetFocus(&link));
    args.handled = true;
}

void HyperlinkInteractionManager::OnMouseUp(
    Base::Object* sender,
    const MouseButtonEventArgs& args) noexcept {
    auto& link = *static_cast<Documents::Hyperlink*>(sender);
    if (args.changedButton != MouseButton::Left) return;
    const std::uint32_t index = Find(link);
    if (index == UINT32_MAX) return;
    Record& record = links_[index];
    if (!record.pointerDown || record.pointerId != args.pointerId) return;
    record.pointerDown = false;
    static_cast<void>(pointer_->ReleasePointer(args.pointerId));
    args.handled = true;
    if (link.IsEnabled() && link.IsMouseOver()) {
        static_cast<void>(Invoke(link));
    }
}

void HyperlinkInteractionManager::OnKeyDown(
    Base::Object* sender,
    const KeyEventArgs& args) noexcept {
    auto& link = *static_cast<Documents::Hyperlink*>(sender);
    if (!link.IsEnabled() ||
        (args.key != KeyboardKeySpace && args.key != KeyboardKeyEnter)) {
        return;
    }
    const std::uint32_t index = Find(link);
    if (index == UINT32_MAX || !links_[index].commandEnabled) return;
    links_[index].keyboardDown = true;
    args.handled = true;
}

void HyperlinkInteractionManager::OnKeyUp(
    Base::Object* sender,
    const KeyEventArgs& args) noexcept {
    auto& link = *static_cast<Documents::Hyperlink*>(sender);
    if (args.key != KeyboardKeySpace && args.key != KeyboardKeyEnter) return;
    const std::uint32_t index = Find(link);
    if (index == UINT32_MAX || !links_[index].keyboardDown) return;
    links_[index].keyboardDown = false;
    args.handled = true;
    static_cast<void>(Invoke(link));
}

void HyperlinkInteractionManager::OnFocusChanged(
    Base::Object* sender,
    const KeyboardFocusChangedEventArgs& args) noexcept {
    auto& link = *static_cast<Documents::Hyperlink*>(sender);
    const std::uint32_t index = Find(link);
    if (index != UINT32_MAX && args.newFocus != &link) {
        links_[index].keyboardDown = false;
    }
}

void HyperlinkInteractionManager::OnPropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    auto& link = static_cast<Documents::Hyperlink&>(object);
    const std::uint32_t index = Find(link);
    if (index == UINT32_MAX) return;
    if (args.property == Documents::Hyperlink::CommandProperty) {
        if (!SubscribeCommand(link, links_[index])) return;
        static_cast<void>(RefreshCanExecute(link));
    }
}

void HyperlinkInteractionManager::OnRequerySuggested() noexcept {
    for (std::uint32_t index = 0U; index < links_.Size(); ++index) {
        Documents::Hyperlink* link = Resolve(index);
        if (link != nullptr) {
            static_cast<void>(RefreshCanExecute(*link));
        }
    }
}


DocumentSelectionManager::DocumentSelectionManager(
    ObjectTree& tree, RoutedEventManager& events,
    PointerInputManager& pointer, FocusManager& focus,
    Platform::IClipboard* clipboard) noexcept
    : tree_(&tree), events_(&events), pointer_(&pointer), focus_(&focus),
      clipboard_(clipboard), records_(&Base::GetDefaultAllocator()),
      mouseDownHandler_(this, &DocumentSelectionManager::OnMouseDown),
      mouseMoveHandler_(this, &DocumentSelectionManager::OnMouseMove),
      mouseUpHandler_(this, &DocumentSelectionManager::OnMouseUp),
      keyDownHandler_(this, &DocumentSelectionManager::OnKeyDown),
      focusChangedHandler_(this, &DocumentSelectionManager::OnFocusChanged),
      propertyChangedHandler_(this, &DocumentSelectionManager::OnPropertyChanged),
      captureChangedHandler_(this, &DocumentSelectionManager::OnCaptureChanged) {}

DocumentSelectionManager::~DocumentSelectionManager() noexcept {
    while (!records_.Empty()) {
        TextBlock* text = Resolve(records_.Size() - 1U);
        if (text == nullptr) records_.PopBack();
        else static_cast<void>(Detach(*text));
    }
    if (captureSubscribed_) {
        static_cast<void>(
            pointer_->RemoveCaptureChanged(captureChangedHandler_));
        captureSubscribed_ = false;
    }
}

std::uint32_t DocumentSelectionManager::Find(const TextBlock& text) const noexcept {
    for (std::uint32_t index = 0U; index < records_.Size(); ++index) {
        if (tree_->ResolveHandle(records_[index].handle) == &text) return index;
    }
    return UINT32_MAX;
}

TextBlock* DocumentSelectionManager::Resolve(std::uint32_t index) noexcept {
    if (index >= records_.Size()) return nullptr;
    Visual* visual = tree_->ResolveHandle(records_[index].handle);
    return visual != nullptr ? static_cast<TextBlock*>(visual->AsUIElement()) : nullptr;
}

void DocumentSelectionManager::RemoveAt(std::uint32_t index) noexcept {
    if (index + 1U != records_.Size()) records_[index] = std::move(records_.Back());
    records_.PopBack();
}

Base::Result<void> DocumentSelectionManager::SynchronizeSelectionEnabled(
    TextBlock& text) noexcept {
    if (text.IsTextSelectionEnabled()) {
        Base::Result<void> result = text.SetCurrentValue(UIElement::FocusableProperty, true);
        if (result) result = text.SetCurrentValue(UIElement::IsTabStopProperty, true);
        if (result && text.IsKeyboardFocused()) result = text.SetCaretBlinkVisible(true);
        return result;
    }
    Base::Result<void> cleared = text.ClearSelection();
    return cleared ? text.SetCaretBlinkVisible(false) : cleared;
}

Base::Result<void> DocumentSelectionManager::Attach(TextBlock& text) noexcept {
    if (Find(text) != UINT32_MAX) {
        return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
            "TextBlock selection is already attached");
    }
    if (!text.IsLoaded() || text.OwningTree() != tree_) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "TextBlock selection requires a loaded document root");
    }
    Record record; record.handle = text.Handle();
    Base::Result<void> added = records_.TryPushBack(record);
    if (!added) return added.GetStatus();
    if (!captureSubscribed_) {
        added = pointer_->TryAddCaptureChanged(captureChangedHandler_);
        if (!added) { records_.PopBack(); return added.GetStatus(); }
        captureSubscribed_ = true;
    }
    Base::Result<void> result = text.TryAddHandler(UIElement::MouseDownEvent, mouseDownHandler_);
    if (result) result = text.TryAddHandler(UIElement::MouseMoveEvent, mouseMoveHandler_);
    if (result) result = text.TryAddHandler(UIElement::MouseUpEvent, mouseUpHandler_);
    if (result) result = text.TryAddHandler(UIElement::KeyDownEvent, keyDownHandler_);
    if (result) result = text.TryAddHandler(UIElement::GotKeyboardFocusEvent, focusChangedHandler_);
    if (result) result = text.TryAddHandler(UIElement::LostKeyboardFocusEvent, focusChangedHandler_);
    if (result) result = text.TryAddValueChangedHandler(
        TextBlock::IsTextSelectionEnabledProperty, propertyChangedHandler_);
    if (result) result = text.TryAddValueChangedHandler(
        UIElement::IsEnabledProperty, propertyChangedHandler_);
    if (result) result = SynchronizeSelectionEnabled(text);
    if (!result) {
        const Base::Status failure = result.GetStatus();
        static_cast<void>(Detach(text));
        return failure;
    }
    return {};
}

Base::Result<bool> DocumentSelectionManager::Detach(TextBlock& text) noexcept {
    const std::uint32_t index = Find(text);
    if (index == UINT32_MAX) return false;
    if (records_[index].dragging) {
        Base::Result<bool> released = pointer_->ReleasePointer(records_[index].pointerId);
        if (!released) return released.GetStatus();
    }
    static_cast<void>(text.RemoveHandler(UIElement::MouseDownEvent, mouseDownHandler_));
    static_cast<void>(text.RemoveHandler(UIElement::MouseMoveEvent, mouseMoveHandler_));
    static_cast<void>(text.RemoveHandler(UIElement::MouseUpEvent, mouseUpHandler_));
    static_cast<void>(text.RemoveHandler(UIElement::KeyDownEvent, keyDownHandler_));
    static_cast<void>(text.RemoveHandler(UIElement::GotKeyboardFocusEvent, focusChangedHandler_));
    static_cast<void>(text.RemoveHandler(UIElement::LostKeyboardFocusEvent, focusChangedHandler_));
    static_cast<void>(text.RemoveValueChangedHandler(
        TextBlock::IsTextSelectionEnabledProperty, propertyChangedHandler_));
    static_cast<void>(text.RemoveValueChangedHandler(
        UIElement::IsEnabledProperty, propertyChangedHandler_));
    static_cast<void>(text.SetCaretBlinkVisible(false));
    RemoveAt(index);
    if (records_.Empty() && captureSubscribed_) {
        static_cast<void>(pointer_->RemoveCaptureChanged(captureChangedHandler_));
        captureSubscribed_ = false;
    }
    return true;
}

Base::Result<void> DocumentSelectionManager::MoveCaret(
    TextBlock& text, Documents::LogicalDirection direction, bool extend) noexcept {
    const Documents::TextSelection selection = text.Selection();
    Documents::TextPointer target = selection.CaretPosition();
    if (!extend && !selection.IsEmpty()) {
        target = direction == LogicalDirection::Backward
            ? selection.Start() : selection.End();
    } else {
        Base::Result<Documents::TextPointer> next =
            target.GetNextInsertionPosition(direction);
        if (!next) return next.GetStatus();
        target = next.Value();
    }
    return text.SetSelection(extend ? selection.AnchorPosition() : target, target);
}

Base::Result<void> DocumentSelectionManager::MoveLine(
    TextBlock& text, double direction, bool extend) noexcept {
    Base::Result<Rect> caret = text.CaretRectangle();
    if (!caret) return caret.GetStatus();
    Point point{caret.Value().x, direction < 0.0
        ? caret.Value().y - 1.0
        : caret.Value().y + caret.Value().height + 1.0};
    Base::Result<Documents::TextPointer> target =
        Documents::GetPositionFromPoint(text, point, true);
    if (!target) return target.GetStatus();
    const Documents::TextSelection selection = text.Selection();
    return text.SetSelection(
        extend ? selection.AnchorPosition() : target.Value(), target.Value());
}

void DocumentSelectionManager::OnMouseDown(
    Base::Object* sender, const MouseButtonEventArgs& args) noexcept {
    auto& text = *static_cast<TextBlock*>(sender);
    if (args.changedButton != MouseButton::Left || !text.IsEnabled() ||
        !text.IsTextSelectionEnabled()) return;
    const std::uint32_t index = Find(text);
    if (index == UINT32_MAX) return;
    Point local;
    if (!TryToDocumentLocalPoint(
            text, args.originalSource, args.position, local)) return;
    Base::Result<Documents::TextPointer> position =
        Documents::GetPositionFromPoint(text, local, true);
    if (!position) return;
    const bool extend = HasKeyboardModifier(args.modifiers, KeyboardModifiers::Shift);
    Documents::TextPointer anchor = extend
        ? text.SelectionAnchor() : position.Value();
    if (!text.SetSelection(anchor, position.Value())) return;
    Base::Result<bool> focused = focus_->SetFocus(&text);
    if (!focused) return;
    Base::Result<void> captured = pointer_->CapturePointer(args.pointerId, text);
    if (captured) {
        records_[index].pointerId = args.pointerId;
        records_[index].anchor = anchor;
        records_[index].dragging = true;
        records_[index].blinkElapsed = 0U;
    }
    args.handled = true;
}

void DocumentSelectionManager::OnMouseMove(
    Base::Object* sender, const MouseEventArgs& args) noexcept {
    auto& text = *static_cast<TextBlock*>(sender);
    const std::uint32_t index = Find(text);
    if (index == UINT32_MAX || !records_[index].dragging ||
        records_[index].pointerId != args.pointerId) return;
    Point local;
    if (!TryToDocumentLocalPoint(
            text, args.originalSource, args.position, local)) return;
    Base::Result<Documents::TextPointer> position =
        Documents::GetPositionFromPoint(text, local, true);
    if (!position) return;
    const Documents::TextPointer anchor = records_[index].anchor;
    if (text.SetSelection(anchor, position.Value())) args.handled = true;
}

void DocumentSelectionManager::OnMouseUp(
    Base::Object* sender, const MouseButtonEventArgs& args) noexcept {
    auto& text = *static_cast<TextBlock*>(sender);
    const std::uint32_t index = Find(text);
    if (index == UINT32_MAX || args.changedButton != MouseButton::Left ||
        !records_[index].dragging || records_[index].pointerId != args.pointerId) return;
    Point local;
    if (TryToDocumentLocalPoint(
            text, args.originalSource, args.position, local)) {
        Base::Result<Documents::TextPointer> position =
            Documents::GetPositionFromPoint(text, local, true);
        if (position) {
            static_cast<void>(text.SetSelection(
                records_[index].anchor, position.Value()));
        }
    }
    records_[index].dragging = false;
    static_cast<void>(pointer_->ReleasePointer(args.pointerId));
    args.handled = true;
}

void DocumentSelectionManager::OnKeyDown(
    Base::Object* sender, const KeyEventArgs& args) noexcept {
    auto& text = *static_cast<TextBlock*>(sender);
    if (!text.IsEnabled() || !text.IsTextSelectionEnabled()) return;
    const bool shift = HasKeyboardModifier(args.modifiers, KeyboardModifiers::Shift);
    const bool control = HasKeyboardModifier(args.modifiers, KeyboardModifiers::Control);
    Base::Result<void> result; bool handled = true;
    if (control && args.key == KeyboardKeyA) result = text.SelectAll();
    else if (control && args.key == KeyboardKeyC) {
        result = clipboard_ != nullptr ? text.CopySelection(*clipboard_)
            : Base::Result<void>(Base::Status::Failure(
                Base::ErrorCode::NotInitialized, "Document clipboard is unavailable"));
    } else if (args.key == KeyboardKeyLeft) {
        result = MoveCaret(text, LogicalDirection::Backward, shift);
    } else if (args.key == KeyboardKeyRight) {
        result = MoveCaret(text, LogicalDirection::Forward, shift);
    } else if (args.key == KeyboardKeyHome) {
        Documents::TextPointer target = text.ContentStart();
        const Documents::TextSelection selection = text.Selection();
        result = text.SetSelection(shift ? selection.AnchorPosition() : target, target);
    } else if (args.key == KeyboardKeyEnd) {
        Documents::TextPointer target = text.ContentEnd();
        const Documents::TextSelection selection = text.Selection();
        result = text.SetSelection(shift ? selection.AnchorPosition() : target, target);
    } else if (args.key == KeyboardKeyUp) result = MoveLine(text, -1.0, shift);
    else if (args.key == KeyboardKeyDown) result = MoveLine(text, 1.0, shift);
    else handled = false;
    if (handled && result) {
        const std::uint32_t index = Find(text);
        if (index != UINT32_MAX) records_[index].blinkElapsed = 0U;
        args.handled = true;
    }
}

void DocumentSelectionManager::OnFocusChanged(
    Base::Object* sender, const KeyboardFocusChangedEventArgs& args) noexcept {
    auto& text = *static_cast<TextBlock*>(sender);
    const std::uint32_t index = Find(text);
    if (index == UINT32_MAX) return;
    records_[index].blinkElapsed = 0U;
    static_cast<void>(text.SetCaretBlinkVisible(args.newFocus == &text));
    if (args.newFocus != &text && records_[index].dragging) {
        records_[index].dragging = false;
        static_cast<void>(pointer_->ReleasePointer(records_[index].pointerId));
    }
}

void DocumentSelectionManager::OnPropertyChanged(
    DependencyObject& object, const DependencyPropertyChangedEventArgs&) noexcept {
    auto& text = static_cast<TextBlock&>(object);
    static_cast<void>(SynchronizeSelectionEnabled(text));
}

void DocumentSelectionManager::OnCaptureChanged(
    std::uint32_t pointerId, UIElement* target, bool captured) noexcept {
    if (captured || target == nullptr) return;
    for (Record& record : records_) {
        if (record.pointerId == pointerId && tree_->ResolveHandle(record.handle) == target) {
            record.dragging = false;
            return;
        }
    }
}

Base::Result<std::uint32_t> DocumentSelectionManager::AdvanceTime(
    std::uint32_t elapsedMilliseconds) noexcept {
    std::uint32_t changed = 0U;
    std::uint32_t index = 0U;
    while (index < records_.Size()) {
        Record& record = records_[index];
        Visual* visual = tree_->ResolveHandle(record.handle);
        if (visual == nullptr) {
            RemoveAt(index);
            continue;
        }
        TextBlock* text = static_cast<TextBlock*>(visual);
        const bool active = text->IsEnabled() &&
            text->IsTextSelectionEnabled() &&
            text->IsKeyboardFocused() && text->Selection().IsEmpty();
        if (!active) {
            record.blinkElapsed = 0U;
            Base::Result<void> hidden = text->SetCaretBlinkVisible(false);
            if (!hidden) return hidden.GetStatus();
            ++index;
            continue;
        }
        const std::uint64_t total =
            static_cast<std::uint64_t>(record.blinkElapsed) +
            elapsedMilliseconds;
        const std::uint32_t toggles =
            static_cast<std::uint32_t>(total / 500U);
        record.blinkElapsed = static_cast<std::uint32_t>(total % 500U);
        if ((toggles & 1U) != 0U) {
            Base::Result<void> toggled =
                text->SetCaretBlinkVisible(!text->caretBlinkVisible_);
            if (!toggled) return toggled.GetStatus();
            ++changed;
        }
        ++index;
    }
    if (records_.Empty() && captureSubscribed_) {
        static_cast<void>(
            pointer_->RemoveCaptureChanged(captureChangedHandler_));
        captureSubscribed_ = false;
    }
    return changed;
}
} // namespace Aero::Detail
