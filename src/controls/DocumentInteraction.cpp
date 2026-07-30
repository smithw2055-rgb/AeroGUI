#include <Aero/Documents/Documents.hpp>

#include "RuntimeManagers.hpp"

#include <utility>

namespace Aero::Detail {

using HyperlinkInteractionManager =
    ControlRuntimeAccess::HyperlinkInteractionManager;
using namespace Aero::Core;
using namespace Aero::Presentation;

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
    if (command == nullptr) return {};
    UIElement* target = link.CommandTarget();
    if (target == nullptr) target = &link;
    Base::Ref<Base::Object> parameter = link.CommandParameter();
    const Value value = Value::FromObject(
        TypeOf<Base::Object>(), std::move(parameter));
    if (command->RuntimeType() == RoutedCommand::StaticTypeId()) {
        Base::Result<bool> executed = commands_->Execute(
            static_cast<RoutedCommand&>(*command), value, *target);
        return executed ? Base::Result<void>()
                        : Base::Result<void>(executed.GetStatus());
    }
    Base::Result<bool> allowed =
        command->CanExecute(*commands_, value, *target);
    if (!allowed) return allowed.GetStatus();
    if (!allowed.Value()) return {};
    return command->Execute(*commands_, value, *target);
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

} // namespace Aero::Detail
