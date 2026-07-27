#include <Aero/Controls/Buttons.hpp>

#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>

#include <utility>

namespace Aero::Controls {

ClickMode ButtonBase::GetClickMode() const noexcept {
    return ClickModeProperty.GetOr(*this, ClickMode::Release);
}

ICommand* ButtonBase::Command() const noexcept {
    Base::Result<Base::Ref<ICommand>> value =
        CommandProperty.Get(*this);
    return value ? value.Value().Get() : nullptr;
}

Base::Ref<Base::Object> ButtonBase::CommandParameter() const noexcept {
    return CommandParameterProperty.GetOr(*this);
}

UIElement* ButtonBase::CommandTarget() const noexcept {
    Base::Result<Base::Ref<UIElement>> value =
        CommandTargetProperty.Get(*this);
    return value ? value.Value().Get() : nullptr;
}

Base::Result<void> ButtonBase::SetClickMode(
    ClickMode value) noexcept {
    if (value > ClickMode::Hover) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ButtonBase ClickMode is invalid");
    }
    return ClickModeProperty.Set(*this, value);
}

Base::Result<void> ButtonBase::SetCommand(
    Base::Ref<ICommand> command) noexcept {
    return CommandProperty.Set(*this, command);
}

Base::Result<void> ButtonBase::SetCommandParameter(
    Base::Ref<Base::Object> parameter) noexcept {
    return CommandParameterProperty.Set(*this, parameter);
}

Base::Result<void> ButtonBase::SetCommandTarget(
    Base::Ref<UIElement> target) noexcept {
    return CommandTargetProperty.Set(*this, target);
}

std::uint32_t RepeatButton::Delay() const noexcept {
    return DelayProperty.GetOr(*this, 400U);
}

std::uint32_t RepeatButton::Interval() const noexcept {
    return IntervalProperty.GetOr(*this, 100U);
}

Base::Result<void> RepeatButton::SetDelay(
    std::uint32_t value) noexcept {
    return DelayProperty.Set(*this, value);
}

Base::Result<void> RepeatButton::SetInterval(
    std::uint32_t value) noexcept {
    if (value == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "RepeatButton interval must be positive");
    }
    return IntervalProperty.Set(*this, value);
}

bool ToggleButton::IsChecked() const noexcept {
    return IsCheckedProperty.GetOr(*this, false);
}

bool ToggleButton::IsThreeState() const noexcept {
    return IsThreeStateProperty.GetOr(*this, false);
}

bool ToggleButton::IsIndeterminate() const noexcept {
    return IsIndeterminateProperty.GetOr(*this, false);
}

ToggleState ToggleButton::GetToggleState() const noexcept {
    if (IsIndeterminate()) return ToggleState::Indeterminate;
    return IsChecked()
        ? ToggleState::Checked
        : ToggleState::Unchecked;
}

Base::Result<void> ToggleButton::SetIsChecked(
    bool value) noexcept {
    Base::Result<void> checked = SetValue(
        IsCheckedProperty,
        Value::FromBoolean(BuiltinTypes::Boolean, value));
    if (!checked) return checked.GetStatus();
    return SetReadOnlyCurrentValue(
        IsIndeterminateProperty,
        Value::FromBoolean(BuiltinTypes::Boolean, false));
}

Base::Result<void> ToggleButton::SetIsThreeState(
    bool value) noexcept {
    Base::Result<void> state = SetValue(
        IsThreeStateProperty,
        Value::FromBoolean(BuiltinTypes::Boolean, value));
    if (!state || value || !IsIndeterminate()) {
        return state;
    }
    return SetReadOnlyCurrentValue(
        IsIndeterminateProperty,
        Value::FromBoolean(BuiltinTypes::Boolean, false));
}

Base::Result<void> ToggleButton::SetToggleState(
    ToggleState value) noexcept {
    if (value > ToggleState::Indeterminate ||
        (value == ToggleState::Indeterminate &&
            !IsThreeState())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ToggleButton state is invalid");
    }
    const Value enabled =
        Value::FromBoolean(BuiltinTypes::Boolean, true);
    const Value disabled =
        Value::FromBoolean(BuiltinTypes::Boolean, false);
    Base::Result<void> status;
    switch (value) {
    case ToggleState::Checked:
        status = SetValue(IsCheckedProperty, enabled);
        if (!status) return status.GetStatus();
        return SetReadOnlyCurrentValue(
            IsIndeterminateProperty, disabled);
    case ToggleState::Unchecked:
        status = SetReadOnlyCurrentValue(
            IsIndeterminateProperty, disabled);
        if (!status) return status.GetStatus();
        return SetValue(IsCheckedProperty, disabled);
    case ToggleState::Indeterminate:
        status = SetReadOnlyCurrentValue(
            IsIndeterminateProperty, enabled);
        if (!status) return status.GetStatus();
        return SetValue(IsCheckedProperty, disabled);
    }
    return {};
}

Base::StringView RadioButton::GroupName() const noexcept {
    Base::Result<Value> value = GetValue(GroupNameProperty);
    return value ? value.Value().AsString() : Base::StringView();
}

Base::Result<void> RadioButton::SetGroupName(
    Base::StringView value) noexcept {
    Base::Result<Value> stored =
        Value::TryFromString(BuiltinTypes::String, value);
    return stored
        ? SetValue(GroupNameProperty, stored.Value())
        : stored.GetStatus();
}

ControlInteractionManager::ControlInteractionManager(
    ObjectTree& tree,
    RoutedEventManager& events,
    PointerInputManager& pointer,
    FocusManager& focus,
    CommandManager& commands,
    VisualStateManager* states) noexcept
    : tree_(&tree),
      events_(&events),
      pointer_(&pointer),
      focus_(&focus),
      commands_(&commands),
      states_(states),
      buttons_(&Base::GetDefaultAllocator()),
      mouseDownHandler_(
          this, &ControlInteractionManager::OnMouseDown),
      mouseUpHandler_(
          this, &ControlInteractionManager::OnMouseUp),
      keyDownHandler_(
          this, &ControlInteractionManager::OnKeyDown),
      keyUpHandler_(
          this, &ControlInteractionManager::OnKeyUp),
      focusChangedHandler_(
          this, &ControlInteractionManager::OnFocusChanged),
      propertyChangedHandler_(
          this, &ControlInteractionManager::OnPropertyChanged),
      pointerStateChangedHandler_(
          this, &ControlInteractionManager::OnPointerStateChanged),
      captureChangedHandler_(
          this, &ControlInteractionManager::OnCaptureChanged),
      requeryHandler_(
          this, &ControlInteractionManager::OnRequerySuggested) {}

ControlInteractionManager::~ControlInteractionManager() noexcept {
    if (initialized_) {
        static_cast<void>(
            pointer_->RemoveStateChanged(
                pointerStateChangedHandler_));
        static_cast<void>(
            pointer_->RemoveCaptureChanged(
                captureChangedHandler_));
        static_cast<void>(
            commands_->RemoveRequerySuggested(requeryHandler_));
    }
    while (!buttons_.Empty()) {
        const std::uint32_t index = buttons_.Size() - 1U;
        ButtonBase* button = ResolveButton(index);
        if (button != nullptr) {
            static_cast<void>(button->RemoveHandler(
                UIElement::MouseDownEvent, mouseDownHandler_));
            static_cast<void>(button->RemoveHandler(
                UIElement::MouseUpEvent, mouseUpHandler_));
            static_cast<void>(button->RemoveHandler(
                UIElement::KeyDownEvent, keyDownHandler_));
            static_cast<void>(button->RemoveHandler(
                UIElement::KeyUpEvent, keyUpHandler_));
            static_cast<void>(button->RemoveHandler(
                UIElement::GotKeyboardFocusEvent,
                focusChangedHandler_));
            static_cast<void>(button->RemoveHandler(
                UIElement::LostKeyboardFocusEvent,
                focusChangedHandler_));
            static_cast<void>(button->RemoveValueChangedHandler(
                ButtonBase::CommandProperty,
                propertyChangedHandler_));
            static_cast<void>(button->RemoveValueChangedHandler(
                UIElement::IsEnabledProperty,
                propertyChangedHandler_));
            if (button->RuntimeType() ==
                    ToggleButton::StaticTypeId() ||
                button->RuntimeType() ==
                    CheckBox::StaticTypeId() ||
                button->RuntimeType() ==
                    RadioButton::StaticTypeId()) {
                static_cast<void>(button->RemoveValueChangedHandler(
                    ToggleButton::IsCheckedProperty,
                    propertyChangedHandler_));
                static_cast<void>(button->RemoveValueChangedHandler(
                    ToggleButton::IsThreeStateProperty,
                    propertyChangedHandler_));
                static_cast<void>(button->RemoveValueChangedHandler(
                    ToggleButton::IsIndeterminateProperty,
                    propertyChangedHandler_));
            }
            if (button->RuntimeType() ==
                RadioButton::StaticTypeId()) {
                static_cast<void>(button->RemoveValueChangedHandler(
                    RadioButton::GroupNameProperty,
                    propertyChangedHandler_));
            }
        }
        UnsubscribeCommand(buttons_[index]);
        buttons_.PopBack();
    }
}

Base::Result<void> ControlInteractionManager::Initialize() noexcept {
    if (initialized_) return {};
    Base::Result<void> state =
        pointer_->TryAddStateChanged(
            pointerStateChangedHandler_);
    if (!state) return state.GetStatus();
    Base::Result<void> capture =
        pointer_->TryAddCaptureChanged(captureChangedHandler_);
    if (!capture) {
        static_cast<void>(pointer_->RemoveStateChanged(
            pointerStateChangedHandler_));
        return capture.GetStatus();
    }
    Base::Result<void> requery =
        commands_->TryAddRequerySuggested(requeryHandler_);
    if (!requery) {
        static_cast<void>(pointer_->RemoveCaptureChanged(
            captureChangedHandler_));
        static_cast<void>(pointer_->RemoveStateChanged(
            pointerStateChangedHandler_));
        return requery.GetStatus();
    }
    initialized_ = true;
    return {};
}

std::uint32_t ControlInteractionManager::FindButton(
    const ButtonBase& button) const noexcept {
    for (std::uint32_t index = 0U;
        index < buttons_.Size(); ++index) {
        Visual* visual =
            tree_->ResolveHandle(buttons_[index].handle);
        if (visual == &button) return index;
    }
    return UINT32_MAX;
}

ButtonBase* ControlInteractionManager::ResolveButton(
    std::uint32_t index) noexcept {
    Visual* visual = tree_->ResolveHandle(buttons_[index].handle);
    return visual != nullptr
        ? static_cast<ButtonBase*>(visual->AsUIElement())
        : nullptr;
}

void ControlInteractionManager::UnsubscribeCommand(
    ButtonRecord& record) noexcept {
    if (record.command) {
        static_cast<void>(
            record.command->RemoveCanExecuteChanged(
                requeryHandler_));
        record.command.Reset();
    }
}

Base::Result<void> ControlInteractionManager::SubscribeCommand(
    ButtonBase& button,
    ButtonRecord& record) noexcept {
    UnsubscribeCommand(record);
    ICommand* command = button.Command();
    if (command == nullptr) return {};
    record.command =
        Base::Ref<ICommand>::FromBorrowed(*command);
    Base::Result<void> subscribed =
        record.command->TryAddCanExecuteChanged(requeryHandler_);
    if (!subscribed) {
        record.command.Reset();
        return subscribed.GetStatus();
    }
    return {};
}

Base::Result<void> ControlInteractionManager::Attach(
    ButtonBase& button) noexcept {
    Base::Result<void> ready = Initialize();
    if (!ready) return ready.GetStatus();
    if (FindButton(button) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Button is already attached to interaction services");
    }
    if (!button.IsLoaded() || button.OwningTree() != tree_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Button must be loaded in the interaction tree");
    }
    Base::Result<VisualHandle> handle =
        tree_->GetHandle(button);
    if (!handle) return handle.GetStatus();
    ButtonRecord record;
    record.handle = handle.Value();
    record.wasMouseOver = button.IsMouseOver();
    if (button.RuntimeType() ==
            ToggleButton::StaticTypeId() ||
        button.RuntimeType() ==
            CheckBox::StaticTypeId() ||
        button.RuntimeType() ==
            RadioButton::StaticTypeId()) {
        record.toggleState =
            static_cast<ToggleButton&>(button).GetToggleState();
    }
    Base::Result<void> appended =
        buttons_.TryPushBack(std::move(record));
    if (!appended) return appended.GetStatus();

    Base::Result<void> result = button.TryAddHandler(
        UIElement::MouseDownEvent, mouseDownHandler_);
    if (result) result = button.TryAddHandler(
        UIElement::MouseUpEvent, mouseUpHandler_);
    if (result) result = button.TryAddHandler(
        UIElement::KeyDownEvent, keyDownHandler_);
    if (result) result = button.TryAddHandler(
        UIElement::KeyUpEvent, keyUpHandler_);
    if (result) result = button.TryAddHandler(
        UIElement::GotKeyboardFocusEvent,
        focusChangedHandler_);
    if (result) result = button.TryAddHandler(
        UIElement::LostKeyboardFocusEvent,
        focusChangedHandler_);
    if (result) result = button.TryAddValueChangedHandler(
        ButtonBase::CommandProperty,
        propertyChangedHandler_);
    if (result) result = button.TryAddValueChangedHandler(
        UIElement::IsEnabledProperty,
        propertyChangedHandler_);
    const bool isToggle =
        button.RuntimeType() == ToggleButton::StaticTypeId() ||
        button.RuntimeType() == CheckBox::StaticTypeId() ||
        button.RuntimeType() == RadioButton::StaticTypeId();
    if (result && isToggle) {
        result = button.TryAddValueChangedHandler(
            ToggleButton::IsCheckedProperty,
            propertyChangedHandler_);
    }
    if (result && isToggle) {
        result = button.TryAddValueChangedHandler(
            ToggleButton::IsThreeStateProperty,
            propertyChangedHandler_);
    }
    if (result && isToggle) {
        result = button.TryAddValueChangedHandler(
            ToggleButton::IsIndeterminateProperty,
            propertyChangedHandler_);
    }
    if (result &&
        button.RuntimeType() == RadioButton::StaticTypeId()) {
        result = button.TryAddValueChangedHandler(
            RadioButton::GroupNameProperty,
            propertyChangedHandler_);
    }
    if (result) result =
        SubscribeCommand(button, buttons_.Back());
    if (!result) {
        const Base::Status status = result.GetStatus();
        static_cast<void>(Detach(button));
        return status;
    }
    result = RefreshCanExecute(button);
    if (!result) {
        const Base::Status status = result.GetStatus();
        static_cast<void>(Detach(button));
        return status;
    }
    if (button.RuntimeType() == RadioButton::StaticTypeId()) {
        auto& radio = static_cast<RadioButton&>(button);
        if (radio.GetToggleState() == ToggleState::Checked) {
            UncheckRadioPeers(radio);
        }
    }
    SyncVisualState(button);
    return {};
}

void ControlInteractionManager::RemoveAt(
    std::uint32_t index) noexcept {
    if (index + 1U != buttons_.Size()) {
        buttons_[index] =
            std::move(buttons_[buttons_.Size() - 1U]);
    }
    buttons_.PopBack();
}

Base::Result<bool> ControlInteractionManager::Detach(
    ButtonBase& button) noexcept {
    const std::uint32_t index = FindButton(button);
    if (index == UINT32_MAX) return false;
    ButtonRecord& record = buttons_[index];
    if (record.pointerDown) {
        Base::Result<bool> released =
            pointer_->ReleasePointer(record.pointerId);
        if (!released) return released.GetStatus();
    }
    static_cast<void>(button.RemoveHandler(
        UIElement::MouseDownEvent, mouseDownHandler_));
    static_cast<void>(button.RemoveHandler(
        UIElement::MouseUpEvent, mouseUpHandler_));
    static_cast<void>(button.RemoveHandler(
        UIElement::KeyDownEvent, keyDownHandler_));
    static_cast<void>(button.RemoveHandler(
        UIElement::KeyUpEvent, keyUpHandler_));
    static_cast<void>(button.RemoveHandler(
        UIElement::GotKeyboardFocusEvent,
        focusChangedHandler_));
    static_cast<void>(button.RemoveHandler(
        UIElement::LostKeyboardFocusEvent,
        focusChangedHandler_));
    static_cast<void>(button.RemoveValueChangedHandler(
        ButtonBase::CommandProperty,
        propertyChangedHandler_));
    static_cast<void>(button.RemoveValueChangedHandler(
        UIElement::IsEnabledProperty,
        propertyChangedHandler_));
    const bool isToggle =
        button.RuntimeType() == ToggleButton::StaticTypeId() ||
        button.RuntimeType() == CheckBox::StaticTypeId() ||
        button.RuntimeType() == RadioButton::StaticTypeId();
    if (isToggle) {
        static_cast<void>(button.RemoveValueChangedHandler(
            ToggleButton::IsCheckedProperty,
            propertyChangedHandler_));
        static_cast<void>(button.RemoveValueChangedHandler(
            ToggleButton::IsThreeStateProperty,
            propertyChangedHandler_));
        static_cast<void>(button.RemoveValueChangedHandler(
            ToggleButton::IsIndeterminateProperty,
            propertyChangedHandler_));
    }
    if (button.RuntimeType() == RadioButton::StaticTypeId()) {
        static_cast<void>(button.RemoveValueChangedHandler(
            RadioButton::GroupNameProperty,
            propertyChangedHandler_));
    }
    UnsubscribeCommand(record);
    if (states_ != nullptr) {
        static_cast<void>(states_->Clear(button));
    }
    RemoveAt(index);
    return true;
}

Base::Result<void> ControlInteractionManager::RefreshCanExecute(
    ButtonBase& button) noexcept {
    const std::uint32_t index = FindButton(button);
    if (index == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Button is not attached to interaction services");
    }
    ICommand* command = button.Command();
    bool enabled = true;
    if (command != nullptr) {
        UIElement* target = button.CommandTarget();
        if (target == nullptr) target = &button;
        Base::Ref<Base::Object> parameter =
            button.CommandParameter();
        const Value value = Value::FromObject(
            TypeOf<Base::Object>(), std::move(parameter));
        Base::Result<bool> allowed =
            command->CanExecute(*commands_, value, *target);
        if (!allowed) return allowed.GetStatus();
        enabled = allowed.Value();
    }
    button.commandEnabled_ = enabled;
    Base::Result<void> coerced =
        button.CoerceValue(UIElement::IsEnabledProperty);
    if (!coerced) return coerced.GetStatus();
    if (!button.IsEnabled() &&
        focus_->FocusedNode() == &button) {
        Base::Result<bool> cleared = focus_->ClearFocus();
        if (!cleared) return cleared.GetStatus();
    }
    SyncVisualState(button);
    return {};
}

Base::Result<std::uint32_t>
ControlInteractionManager::AdvanceTime(
    std::uint32_t elapsedMilliseconds) noexcept {
    std::uint32_t emitted = 0U;
    for (std::uint32_t index = 0U;
        index < buttons_.Size(); ++index) {
        ButtonBase* button = ResolveButton(index);
        if (button == nullptr ||
            button->RuntimeType() != RepeatButton::StaticTypeId() ||
            !button->IsEnabled()) {
            continue;
        }
        ButtonRecord& record = buttons_[index];
        const bool active = record.keyboardDown ||
            (record.pointerDown && button->IsMouseOver());
        if (!active) continue;
        auto& repeat = static_cast<RepeatButton&>(*button);
        record.repeatElapsed += elapsedMilliseconds;
        if (record.nextRepeat == 0U) {
            record.nextRepeat = repeat.Delay();
        }
        const std::uint64_t interval = repeat.Interval();
        while (record.repeatElapsed >= record.nextRepeat &&
            emitted < 1024U) {
            Base::Result<void> clicked = InvokeClick(repeat);
            if (!clicked) return clicked.GetStatus();
            ++emitted;
            record.nextRepeat += interval;
        }
        if (emitted == 1024U &&
            record.repeatElapsed >= record.nextRepeat) {
            record.nextRepeat =
                record.repeatElapsed + interval;
        }
    }
    return emitted;
}

Base::Result<void> ControlInteractionManager::InvokeClick(
    ButtonBase& button) noexcept {
    if (!button.IsEnabled()) return {};
    const TypeId type = button.RuntimeType();
    if (type == ToggleButton::StaticTypeId() ||
        type == CheckBox::StaticTypeId()) {
        auto& toggle = static_cast<ToggleButton&>(button);
        ToggleState next = ToggleState::Unchecked;
        if (toggle.GetToggleState() == ToggleState::Unchecked) {
            next = ToggleState::Checked;
        } else if (toggle.GetToggleState() ==
                ToggleState::Checked &&
            toggle.IsThreeState()) {
            next = ToggleState::Indeterminate;
        }
        Base::Result<void> changed =
            ApplyToggleState(toggle, next);
        if (!changed) return changed.GetStatus();
    } else if (type == RadioButton::StaticTypeId()) {
        auto& radio = static_cast<RadioButton&>(button);
        Base::Result<void> changed = ApplyToggleState(
            radio, ToggleState::Checked);
        if (!changed) return changed.GetStatus();
    }
    RoutedEventArgs args;
    Base::Result<void> raised = events_->RaiseEvent(
        button, ButtonBase::ClickEvent, &args);
    if (!raised) return raised.GetStatus();
    ICommand* command = button.Command();
    if (command == nullptr) return {};
    UIElement* target = button.CommandTarget();
    if (target == nullptr) target = &button;
    Base::Ref<Base::Object> parameter =
        button.CommandParameter();
    const Value value = Value::FromObject(
        TypeOf<Base::Object>(), std::move(parameter));
    if (command->RuntimeType() == RoutedCommand::StaticTypeId()) {
        Base::Result<bool> executed = commands_->Execute(
            static_cast<RoutedCommand&>(*command), value, *target);
        if (!executed) return executed.GetStatus();
        return {};
    }
    Base::Result<bool> allowed =
        command->CanExecute(*commands_, value, *target);
    if (!allowed) return allowed.GetStatus();
    if (!allowed.Value()) return {};
    return command->Execute(*commands_, value, *target);
}

Base::Result<void> ControlInteractionManager::ApplyToggleState(
    ToggleButton& button,
    ToggleState state) noexcept {
    const std::uint32_t index = FindButton(button);
    if (index == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "ToggleButton is not attached to interaction services");
    }
    ButtonRecord& record = buttons_[index];
    record.updatingToggle = true;
    Base::Result<void> changed =
        button.SetToggleState(state);
    record.updatingToggle = false;
    if (!changed) return changed.GetStatus();
    PublishToggleState(button, record);
    return {};
}

void ControlInteractionManager::PublishToggleState(
    ToggleButton& button,
    ButtonRecord& record) noexcept {
    const ToggleState state = button.GetToggleState();
    if (record.toggleState == state) {
        SyncVisualState(button);
        return;
    }
    record.toggleState = state;
    RoutedEventArgs args;
    RoutedEventHandle event = ToggleButton::UncheckedEvent;
    if (state == ToggleState::Checked) {
        event = ToggleButton::CheckedEvent;
    } else if (state == ToggleState::Indeterminate) {
        event = ToggleButton::IndeterminateEvent;
    }
    static_cast<void>(events_->RaiseEvent(button, event, &args));
    if (state == ToggleState::Checked &&
        button.RuntimeType() == RadioButton::StaticTypeId()) {
        UncheckRadioPeers(static_cast<RadioButton&>(button));
    }
    SyncVisualState(button);
}

void ControlInteractionManager::UncheckRadioPeers(
    RadioButton& button) noexcept {
    Visual* parent = button.LogicalParent();
    const Base::StringView group = button.GroupName();
    for (std::uint32_t index = 0U;
        index < buttons_.Size(); ++index) {
        ButtonBase* candidate = ResolveButton(index);
        if (candidate == nullptr || candidate == &button ||
            candidate->RuntimeType() !=
                RadioButton::StaticTypeId()) {
            continue;
        }
        auto& radio = static_cast<RadioButton&>(*candidate);
        if (radio.LogicalParent() != parent ||
            radio.GroupName() != group ||
            radio.GetToggleState() != ToggleState::Checked) {
            continue;
        }
        static_cast<void>(ApplyToggleState(
            radio, ToggleState::Unchecked));
    }
}

void ControlInteractionManager::SyncVisualState(
    ButtonBase& button) noexcept {
    if (states_ == nullptr) return;
    Base::StringView common = "Normal";
    if (!button.IsEnabled()) common = "Disabled";
    else if (button.IsPressed()) common = "Pressed";
    else if (button.IsMouseOver()) common = "PointerOver";
    static_cast<void>(states_->GoToState(
        button, "CommonStates", common));
    static_cast<void>(states_->GoToState(
        button, "FocusStates",
        button.IsKeyboardFocused()
            ? Base::StringView("Focused")
            : Base::StringView("Unfocused")));
    const TypeId type = button.RuntimeType();
    if (type == ToggleButton::StaticTypeId() ||
        type == CheckBox::StaticTypeId() ||
        type == RadioButton::StaticTypeId()) {
        const ToggleState state =
            static_cast<ToggleButton&>(button).GetToggleState();
        Base::StringView name = "Unchecked";
        if (state == ToggleState::Checked) name = "Checked";
        else if (state == ToggleState::Indeterminate) {
            name = "Indeterminate";
        }
        static_cast<void>(states_->GoToState(
            button, "CheckStates", name));
    }
}

void ControlInteractionManager::OnMouseDown(
    Base::Object* sender,
    const MouseButtonEventArgs& args) noexcept {
    auto& button = *static_cast<ButtonBase*>(sender);
    if (args.changedButton != MouseButton::Left ||
        !button.IsEnabled()) return;
    const std::uint32_t index = FindButton(button);
    if (index == UINT32_MAX) return;
    ButtonRecord& record = buttons_[index];
    record.pointerId = args.pointerId;
    record.pointerDown = true;
    record.repeatElapsed = 0U;
    record.nextRepeat = 0U;
    static_cast<void>(
        pointer_->CapturePointer(args.pointerId, button));
    static_cast<void>(focus_->SetFocus(&button));
    args.handled = true;
    if (button.GetClickMode() == ClickMode::Press) {
        static_cast<void>(InvokeClick(button));
    }
    SyncVisualState(button);
}

void ControlInteractionManager::OnMouseUp(
    Base::Object* sender,
    const MouseButtonEventArgs& args) noexcept {
    auto& button = *static_cast<ButtonBase*>(sender);
    if (args.changedButton != MouseButton::Left) return;
    const std::uint32_t index = FindButton(button);
    if (index == UINT32_MAX) return;
    ButtonRecord& record = buttons_[index];
    if (!record.pointerDown ||
        record.pointerId != args.pointerId) return;
    record.pointerDown = false;
    record.repeatElapsed = 0U;
    record.nextRepeat = 0U;
    args.handled = true;
    if (button.GetClickMode() == ClickMode::Release &&
        button.IsEnabled() && button.IsMouseOver()) {
        static_cast<void>(InvokeClick(button));
    }
    SyncVisualState(button);
}

void ControlInteractionManager::OnKeyDown(
    Base::Object* sender,
    const KeyEventArgs& args) noexcept {
    auto& button = *static_cast<ButtonBase*>(sender);
    if (!button.IsEnabled() ||
        (args.key != KeyboardKeySpace &&
            args.key != KeyboardKeyEnter)) return;
    const std::uint32_t index = FindButton(button);
    if (index == UINT32_MAX) return;
    ButtonRecord& record = buttons_[index];
    if (!record.keyboardDown) {
        record.keyboardDown = true;
        record.repeatElapsed = 0U;
        record.nextRepeat = 0U;
        static_cast<void>(button.SetPressedState(true));
        if (button.GetClickMode() == ClickMode::Press) {
            static_cast<void>(InvokeClick(button));
        }
        SyncVisualState(button);
    }
    args.handled = true;
}

void ControlInteractionManager::OnKeyUp(
    Base::Object* sender,
    const KeyEventArgs& args) noexcept {
    auto& button = *static_cast<ButtonBase*>(sender);
    if (args.key != KeyboardKeySpace &&
        args.key != KeyboardKeyEnter) return;
    const std::uint32_t index = FindButton(button);
    if (index == UINT32_MAX ||
        !buttons_[index].keyboardDown) return;
    buttons_[index].keyboardDown = false;
    buttons_[index].repeatElapsed = 0U;
    buttons_[index].nextRepeat = 0U;
    static_cast<void>(button.SetPressedState(false));
    args.handled = true;
    if (button.IsEnabled() &&
        button.GetClickMode() == ClickMode::Release) {
        static_cast<void>(InvokeClick(button));
    }
    SyncVisualState(button);
}

void ControlInteractionManager::OnFocusChanged(
    Base::Object* sender,
    const KeyboardFocusChangedEventArgs& args) noexcept {
    auto& button = *static_cast<ButtonBase*>(sender);
    const std::uint32_t index = FindButton(button);
    if (index == UINT32_MAX) return;
    if (args.newFocus != &button &&
        buttons_[index].keyboardDown) {
        buttons_[index].keyboardDown = false;
        buttons_[index].repeatElapsed = 0U;
        buttons_[index].nextRepeat = 0U;
        static_cast<void>(button.SetPressedState(false));
    }
    SyncVisualState(button);
}

void ControlInteractionManager::OnPropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    auto& button = static_cast<ButtonBase&>(object);
    const std::uint32_t index = FindButton(button);
    if (index == UINT32_MAX) return;
    if (args.property == ButtonBase::CommandProperty) {
        if (!SubscribeCommand(button, buttons_[index])) return;
        static_cast<void>(RefreshCanExecute(button));
    } else if (args.property == UIElement::IsEnabledProperty) {
        SyncVisualState(button);
    } else {
        const TypeId type = button.RuntimeType();
        const bool isToggle =
            type == ToggleButton::StaticTypeId() ||
            type == CheckBox::StaticTypeId() ||
            type == RadioButton::StaticTypeId();
        if (!isToggle) return;
        auto& toggle = static_cast<ToggleButton&>(button);
        ButtonRecord& record = buttons_[index];
        if (!record.updatingToggle &&
            args.property == ToggleButton::IsCheckedProperty &&
            toggle.IsIndeterminate()) {
            record.updatingToggle = true;
            static_cast<void>(toggle.SetReadOnlyCurrentValue(
                ToggleButton::IsIndeterminateProperty,
                Value::FromBoolean(
                    BuiltinTypes::Boolean, false)));
            record.updatingToggle = false;
        } else if (!record.updatingToggle &&
            args.property ==
                ToggleButton::IsThreeStateProperty &&
            !toggle.IsThreeState() &&
            toggle.IsIndeterminate()) {
            record.updatingToggle = true;
            static_cast<void>(toggle.SetReadOnlyCurrentValue(
                ToggleButton::IsIndeterminateProperty,
                Value::FromBoolean(
                    BuiltinTypes::Boolean, false)));
            record.updatingToggle = false;
        }
        PublishToggleState(toggle, record);
        if (type == RadioButton::StaticTypeId() &&
            args.property == RadioButton::GroupNameProperty &&
            toggle.GetToggleState() == ToggleState::Checked) {
            UncheckRadioPeers(
                static_cast<RadioButton&>(toggle));
        }
    }
}

void ControlInteractionManager::OnPointerStateChanged(
    UIElement& element) noexcept {
    for (std::uint32_t index = 0U;
        index < buttons_.Size(); ++index) {
        ButtonBase* button = ResolveButton(index);
        if (button != &element) continue;
        const bool mouseOver = button->IsMouseOver();
        if (mouseOver && !buttons_[index].wasMouseOver &&
            button->IsEnabled() &&
            button->GetClickMode() == ClickMode::Hover) {
            static_cast<void>(InvokeClick(*button));
        }
        buttons_[index].wasMouseOver = mouseOver;
        SyncVisualState(*button);
        return;
    }
}

void ControlInteractionManager::OnCaptureChanged(
    std::uint32_t pointerId,
    UIElement* target,
    bool captured) noexcept {
    if (captured || target == nullptr) return;
    for (std::uint32_t index = 0U;
        index < buttons_.Size(); ++index) {
        ButtonBase* button = ResolveButton(index);
        if (button != target ||
            !buttons_[index].pointerDown ||
            buttons_[index].pointerId != pointerId) {
            continue;
        }
        buttons_[index].pointerDown = false;
        buttons_[index].repeatElapsed = 0U;
        buttons_[index].nextRepeat = 0U;
        SyncVisualState(*button);
        return;
    }
}

void ControlInteractionManager::OnRequerySuggested() noexcept {
    for (std::uint32_t index = 0U;
        index < buttons_.Size(); ++index) {
        ButtonBase* button = ResolveButton(index);
        if (button != nullptr) {
            static_cast<void>(RefreshCanExecute(*button));
        }
    }
}

} // namespace Aero::Controls
