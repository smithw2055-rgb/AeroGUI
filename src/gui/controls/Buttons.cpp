#include "gui/core/State.hpp" 
#include "gui/input/InputState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/controls/State.hpp"
#include "gui/templates/TemplateState.hpp"
#include <Aero/Controls.hpp>

#include <utility>
#include "ControlBehavior.hpp"

namespace Aero::Controls {
using Aero::Controls::ButtonBehavior;

using namespace Primitives;

ButtonBase::~ButtonBase() {
    auto* behaviors = static_cast<ControlBehavior*>(
        AeroGuiInternal::ControlBehaviorRuntime(*this));
    if (behaviors != nullptr) {
        static_cast<void>(behaviors->Detach(*this));
    }
}

ClickMode ButtonBase::GetClickMode() const noexcept {
    return GetValueOr(ClickModeProperty, ClickMode::Release);
}

ICommand* ButtonBase::GetCommand() const noexcept {
    return GetValueOr(
        CommandProperty, Base::Ref<ICommand>{}).Get();
}

Value ButtonBase::GetCommandParameter() const noexcept {
    return GetValueOr(
        CommandParameterProperty,
        Value::NullObject(TypeOf<Base::Object>()));
}

UIElement* ButtonBase::GetCommandTarget() const noexcept {
    return GetValueOr(
        CommandTargetProperty,
        Base::Ref<UIElement>{}).Get();
}

void ButtonBase::SetClickMode(
    ClickMode value) noexcept {
    SetValue(ClickModeProperty, value);
}

void ButtonBase::SetCommand(
    Base::Ref<ICommand> command) noexcept {
    SetValue(CommandProperty, std::move(command));
}

void ButtonBase::SetCommandParameter(
    Value parameter) noexcept {
    SetValue(
        CommandParameterProperty, std::move(parameter));
}

void ButtonBase::SetCommandTarget(
    Base::Ref<UIElement> target) noexcept {
    SetValue(CommandTargetProperty, std::move(target));
}

std::uint32_t RepeatButton::GetDelay() const noexcept {
    return GetValueOr(DelayProperty, 400U);
}

std::uint32_t RepeatButton::GetInterval() const noexcept {
    return GetValueOr(IntervalProperty, 100U);
}

void RepeatButton::SetDelay(
    std::uint32_t value) noexcept {
    SetValue(DelayProperty, value);
}

void RepeatButton::SetInterval(
    std::uint32_t value) noexcept {
    SetValue(IntervalProperty, value);
}

Nullable<bool> ToggleButton::GetIsChecked() const noexcept {
    return GetValueOr(IsCheckedProperty, Nullable<bool>{false});
}

bool ToggleButton::GetIsThreeState() const noexcept {
    return GetValueOr(IsThreeStateProperty, false);
}

void ToggleButton::SetIsChecked(
    Nullable<bool> value) noexcept {
    SetValue(IsCheckedProperty, value);
}

void ToggleButton::SetIsThreeState(
    bool value) noexcept {
    SetValue(IsThreeStateProperty, value);
}

void ToggleButton::SetToggleState(
    std::uint8_t rawValue) noexcept {
    const ToggleState value =
        static_cast<ToggleState>(rawValue);
    if (value > ToggleState::Indeterminate ||
        (value == ToggleState::Indeterminate &&
            !GetIsThreeState())) {
        return;
    }
    switch (value) {
    case ToggleState::Checked:
        SetValue(IsCheckedProperty, Nullable<bool>{true});
        return;
    case ToggleState::Unchecked:
        SetValue(IsCheckedProperty, Nullable<bool>{false});
        return;
    case ToggleState::Indeterminate:
        SetValue(IsCheckedProperty, Nullable<bool>{});
        return;
    }
    return;
}

Base::StringView RadioButton::GetGroupName() const noexcept {
    return GetValueOr(GroupNameProperty, Base::StringView());
}

void RadioButton::SetGroupName(
    Base::StringView value) noexcept {
    SetValue(GroupNameProperty, value);
}

void ButtonBase::OnApplyTemplate() noexcept {
    ContentControl::OnApplyTemplate();
    auto* behaviors = static_cast<ControlBehavior*>(
        AeroGuiInternal::ControlBehaviorRuntime(*this));
    if (behaviors != nullptr) {
        static_cast<void>(behaviors->RefreshButtonVisualState(
            *this, false));
    }
}

} // namespace Aero::Controls

namespace Aero::Controls {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Controls::Primitives;
using namespace ::Aero;

namespace {

ToggleState ReadToggleState(
    const ToggleButton& button) noexcept {
    const Nullable<bool> value = button.GetIsChecked();
    if (!value.GetHasValue()) {
        return ToggleState::Indeterminate;
    }
    return value.GetValue()
        ? ToggleState::Checked
        : ToggleState::Unchecked;
}

} // namespace

ButtonBehavior::ButtonBehavior(
    ElementTree& tree,
    EventRouter& events,
    InputRouter& input,
    VisualStateManager* states) noexcept
    : tree_(&tree),
      events_(&events),
      input_(&input),
      states_(states),
      buttons_(&Base::GetDefaultAllocator()),
      mouseDownHandler_(
          this, &ButtonBehavior::OnMouseDown),
      mouseUpHandler_(
          this, &ButtonBehavior::OnMouseUp),
      keyDownHandler_(
          this, &ButtonBehavior::OnKeyDown),
      keyUpHandler_(
          this, &ButtonBehavior::OnKeyUp),
      focusChangedHandler_(
          this, &ButtonBehavior::OnFocusChanged),
      propertyChangedHandler_(
          this, &ButtonBehavior::OnPropertyChanged),
      pointerStateChangedHandler_(
          this, &ButtonBehavior::OnPointerStateChanged),
      captureChangedHandler_(
          this, &ButtonBehavior::OnCaptureChanged),
      requeryHandler_(
          this, &ButtonBehavior::OnRequerySuggested) {}

ButtonBehavior::~ButtonBehavior() noexcept {
    if (initialized_) {
        static_cast<void>(
            input_->RemovePointerStateChanged(
                pointerStateChangedHandler_));
        static_cast<void>(
            input_->RemovePointerCaptureChanged(
                captureChangedHandler_));
        static_cast<void>(
            input_->RemoveRequerySuggested(requeryHandler_));
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

Base::Result<void> ButtonBehavior::Initialize() noexcept {
    if (initialized_) return {};
    input_->AddPointerStateChanged(pointerStateChangedHandler_);
    input_->AddPointerCaptureChanged(captureChangedHandler_);
    input_->AddRequerySuggested(requeryHandler_);
    initialized_ = true;
    return {};
}

std::uint32_t ButtonBehavior::FindButton(
    const ButtonBase& button) const noexcept {
    for (std::uint32_t index = 0U;
        index < buttons_.Size(); ++index) {
        ::Aero::Media::Visual* visual =
            tree_->ResolveHandle(buttons_[index].handle);
        if (visual == &button) return index;
    }
    return UINT32_MAX;
}

ButtonBase* ButtonBehavior::ResolveButton(
    std::uint32_t index) noexcept {
    ::Aero::Media::Visual* visual = tree_->ResolveHandle(buttons_[index].handle);
    return visual != nullptr
        ? static_cast<ButtonBase*>(visual->AsUIElement())
        : nullptr;
}

void ButtonBehavior::UnsubscribeCommand(
    ButtonRecord& record) noexcept {
    if (record.command) {
        static_cast<void>(
            record.command->RemoveCanExecuteChanged(
                requeryHandler_));
        record.command.Reset();
    }
}

Base::Result<void> ButtonBehavior::SubscribeCommand(
    ButtonBase& button,
    ButtonRecord& record) noexcept {
    UnsubscribeCommand(record);
    ICommand* command = button.GetCommand();
    if (command == nullptr) return {};
    record.command =
        Base::Ref<ICommand>::FromBorrowed(*command);
    record.command->AddCanExecuteChanged(requeryHandler_);
    return {};
}

Base::Result<void> ButtonBehavior::Attach(
    ButtonBase& button) noexcept {
    Base::Result<void> ready = Initialize();
    if (!ready) return ready.GetStatus();
    if (FindButton(button) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Button is already attached to interaction services");
    }
    if (!button.GetIsLoaded() || button.GetTree() != tree_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Button must be loaded in the interaction tree");
    }
    Base::Result<VisualHandle> handle =
        tree_->GetHandle(button);
    if (!handle) return handle.GetStatus();
    ButtonRecord record;
    record.handle = handle.Value();
    record.wasMouseOver = button.GetIsMouseOver();
    if (button.RuntimeType() ==
            ToggleButton::StaticTypeId() ||
        button.RuntimeType() ==
            CheckBox::StaticTypeId() ||
        button.RuntimeType() ==
            RadioButton::StaticTypeId()) {
        record.toggleState = ReadToggleState(
            static_cast<ToggleButton&>(button));
    }
    Base::Result<void> appended =
        buttons_.PushBack(std::move(record));
    if (!appended) return appended.GetStatus();

    button.AddHandlerChecked(UIElement::MouseDownEvent, mouseDownHandler_);
    button.AddHandlerChecked(UIElement::MouseUpEvent, mouseUpHandler_);
    button.AddHandlerChecked(UIElement::KeyDownEvent, keyDownHandler_);
    button.AddHandlerChecked(UIElement::KeyUpEvent, keyUpHandler_);
    button.AddHandlerChecked(UIElement::GotKeyboardFocusEvent, focusChangedHandler_);
    button.AddHandlerChecked(UIElement::LostKeyboardFocusEvent, focusChangedHandler_);
    Base::Result<void> result = button.AddValueChangedHandlerChecked(
        ButtonBase::CommandProperty,
        propertyChangedHandler_);
    if (result) result = button.AddValueChangedHandlerChecked(
        UIElement::IsEnabledProperty,
        propertyChangedHandler_);
    const bool isToggle =
        button.RuntimeType() == ToggleButton::StaticTypeId() ||
        button.RuntimeType() == CheckBox::StaticTypeId() ||
        button.RuntimeType() == RadioButton::StaticTypeId();
    if (result && isToggle) {
        result = button.AddValueChangedHandlerChecked(
            ToggleButton::IsCheckedProperty,
            propertyChangedHandler_);
    }
    if (result && isToggle) {
        result = button.AddValueChangedHandlerChecked(
            ToggleButton::IsThreeStateProperty,
            propertyChangedHandler_);
    }
    if (result &&
        button.RuntimeType() == RadioButton::StaticTypeId()) {
        result = button.AddValueChangedHandlerChecked(
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
        if (ReadToggleState(radio) == ToggleState::Checked) {
            UncheckRadioPeers(radio);
        }
    }
    result = SyncVisualState(button, false);
    if (!result &&
        result.GetStatus().code !=
            Base::ErrorCode::InvalidState) {
        const Base::Status status =
            result.GetStatus();
        static_cast<void>(Detach(button));
        return status;
    }
    return {};
}

void ButtonBehavior::RemoveAt(
    std::uint32_t index) noexcept {
    if (index + 1U != buttons_.Size()) {
        buttons_[index] =
            std::move(buttons_[buttons_.Size() - 1U]);
    }
    buttons_.PopBack();
}

Base::Result<bool> ButtonBehavior::Detach(
    ButtonBase& button) noexcept {
    const std::uint32_t index = FindButton(button);
    if (index == UINT32_MAX) return false;
    ButtonRecord& record = buttons_[index];
    if (record.pointerDown) {
        Base::Result<bool> released =
            input_->ReleasePointer(record.pointerId);
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
    }
    if (button.RuntimeType() == RadioButton::StaticTypeId()) {
        static_cast<void>(button.RemoveValueChangedHandler(
            RadioButton::GroupNameProperty,
            propertyChangedHandler_));
    }
    UnsubscribeCommand(record);
    if (states_ != nullptr) {
        static_cast<void>(Aero::VisualStateManagerRuntime::Clear(*states_, button));
    }
    RemoveAt(index);
    return true;
}

Base::Result<void> ButtonBehavior::RefreshCanExecute(
    ButtonBase& button) noexcept {
    const std::uint32_t index = FindButton(button);
    if (index == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Button is not attached to interaction services");
    }
    ICommand* command = button.GetCommand();
    bool enabled = true;
    if (command != nullptr) {
        UIElement* target = button.GetCommandTarget();
        if (target == nullptr) target = &button;
        const Value parameter = button.GetCommandParameter();
        Base::Result<bool> allowed =
            input_->CanExecute(*command, parameter, *target);
        if (!allowed) return allowed.GetStatus();
        enabled = allowed.Value();
    }
    button.commandEnabled_ = enabled;
    button.CoerceValue(UIElement::IsEnabledProperty);
    if (!button.GetIsEnabled() &&
        input_->GetFocusedElement() == &button) {
        Base::Result<bool> cleared = input_->ClearFocus();
        if (!cleared) return cleared.GetStatus();
    }
    Base::Result<void> synchronized =
        SyncVisualState(button);
    if (!synchronized &&
        synchronized.GetStatus().code !=
            Base::ErrorCode::InvalidState) {
        return synchronized.GetStatus();
    }
    return {};
}

Base::Result<std::uint32_t>
ButtonBehavior::AdvanceTime(
    std::uint32_t elapsedMilliseconds) noexcept {
    std::uint32_t emitted = 0U;
    for (std::uint32_t index = 0U;
        index < buttons_.Size(); ++index) {
        ButtonBase* button = ResolveButton(index);
        if (button == nullptr ||
            button->RuntimeType() != RepeatButton::StaticTypeId() ||
            !button->GetIsEnabled()) {
            continue;
        }
        ButtonRecord& record = buttons_[index];
        const bool active = record.keyboardDown ||
            (record.pointerDown && button->GetIsMouseOver());
        if (!active) continue;
        auto& repeat = static_cast<RepeatButton&>(*button);
        record.repeatElapsed += elapsedMilliseconds;
        if (record.nextRepeat == 0U) {
            record.nextRepeat = repeat.GetDelay();
        }
        const std::uint64_t interval = repeat.GetInterval();
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

Base::Result<void> ButtonBehavior::InvokeClick(
    ButtonBase& button) noexcept {
    if (!button.GetIsEnabled()) return {};
    const TypeId type = button.RuntimeType();
    if (type == ToggleButton::StaticTypeId() ||
        type == CheckBox::StaticTypeId()) {
        auto& toggle = static_cast<ToggleButton&>(button);
        ToggleState next = ToggleState::Unchecked;
        if (ReadToggleState(toggle) == ToggleState::Unchecked) {
            next = ToggleState::Checked;
        } else if (ReadToggleState(toggle) ==
                ToggleState::Checked &&
            toggle.GetIsThreeState()) {
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
    ICommand* command = button.GetCommand();
    if (command == nullptr) return {};
    UIElement* target = button.GetCommandTarget();
    if (target == nullptr) target = &button;
    const Value parameter = button.GetCommandParameter();
    Base::Result<bool> executed =
        input_->Execute(*command, parameter, *target);
    return executed
        ? Base::Result<void>()
        : Base::Result<void>(executed.GetStatus());
}

Base::Result<void> ButtonBehavior::ApplyToggleState(
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
    button.SetToggleState(static_cast<std::uint8_t>(state));
    record.updatingToggle = false;
    PublishToggleState(button, record);
    return {};
}

void ButtonBehavior::PublishToggleState(
    ToggleButton& button,
    ButtonRecord& record) noexcept {
    const ToggleState state = ReadToggleState(button);
    if (record.toggleState == state) {
        static_cast<void>(
            SyncVisualState(button));
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
    static_cast<void>(
        SyncVisualState(button));
}

void ButtonBehavior::UncheckRadioPeers(
    RadioButton& button) noexcept {
    ::Aero::Media::Visual* parent = button.GetLogicalParent();
    const Base::StringView group = button.GetGroupName();
    for (std::uint32_t index = 0U;
        index < buttons_.Size(); ++index) {
        ButtonBase* candidate = ResolveButton(index);
        if (candidate == nullptr || candidate == &button ||
            candidate->RuntimeType() !=
                RadioButton::StaticTypeId()) {
            continue;
        }
        auto& radio = static_cast<RadioButton&>(*candidate);
        if (radio.GetLogicalParent() != parent ||
            radio.GetGroupName() != group ||
            ReadToggleState(radio) != ToggleState::Checked) {
            continue;
        }
        static_cast<void>(ApplyToggleState(
            radio, ToggleState::Unchecked));
    }
}

Base::Result<void>
ButtonBehavior::SyncVisualState(
    ButtonBase& button,
    bool useTransitions) noexcept {
    if (states_ == nullptr) return {};
    const auto apply =
        [this, &button, useTransitions](
            Base::StringView group,
            Base::StringView state) noexcept
            -> Base::Result<void> {
        Base::Result<bool> changed =
            Aero::VisualStateManagerRuntime::GoToState(*states_,
                button, group, state,
                useTransitions);
        if (!changed &&
            changed.GetStatus().code !=
                Base::ErrorCode::NotFound) {
            return changed.GetStatus();
        }
        return {};
    };
    Base::StringView common = "Normal";
    if (!button.GetIsEnabled()) common = "Disabled";
    else if (button.GetIsPressed()) common = "Pressed";
    else if (button.GetIsMouseOver()) common = "MouseOver";
    Base::Result<void> synchronized =
        apply("CommonStates", common);
    if (!synchronized) {
        return synchronized.GetStatus();
    }
    synchronized = apply(
        "FocusStates",
        button.GetIsKeyboardFocused()
            ? Base::StringView("Focused")
            : Base::StringView("Unfocused"));
    if (!synchronized) {
        return synchronized.GetStatus();
    }
    const TypeId type = button.RuntimeType();
    if (type == ToggleButton::StaticTypeId() ||
        type == CheckBox::StaticTypeId() ||
        type == RadioButton::StaticTypeId()) {
        const ToggleState state =
            ReadToggleState(static_cast<ToggleButton&>(button));
        Base::StringView name = "Unchecked";
        if (state == ToggleState::Checked) name = "Checked";
        else if (state == ToggleState::Indeterminate) {
            name = "Indeterminate";
        }
        synchronized =
            apply("CheckStates", name);
        if (!synchronized) {
            return synchronized.GetStatus();
        }
    }
    return {};
}

void ButtonBehavior::OnMouseDown(
    Base::Object* sender,
    MouseButtonEventArgs& args) noexcept {
    auto& button = *static_cast<ButtonBase*>(sender);
    if (args.GetChangedButton() != MouseButton::Left ||
        !button.GetIsEnabled()) return;
    const std::uint32_t index = FindButton(button);
    if (index == UINT32_MAX) return;
    ButtonRecord& record = buttons_[index];
    record.pointerId = args.GetPointerId();
    record.pointerDown = true;
    record.repeatElapsed = 0U;
    record.nextRepeat = 0U;
    static_cast<void>(
        input_->CapturePointer(args.GetPointerId(), button));
    static_cast<void>(input_->SetFocus(&button));
    args.SetHandled(true);
    if (button.GetClickMode() == ClickMode::Press) {
        static_cast<void>(InvokeClick(button));
    }
    static_cast<void>(
        SyncVisualState(button));
}

void ButtonBehavior::OnMouseUp(
    Base::Object* sender,
    MouseButtonEventArgs& args) noexcept {
    auto& button = *static_cast<ButtonBase*>(sender);
    if (args.GetChangedButton() != MouseButton::Left) return;
    const std::uint32_t index = FindButton(button);
    if (index == UINT32_MAX) return;
    ButtonRecord& record = buttons_[index];
    if (!record.pointerDown ||
        record.pointerId != args.GetPointerId()) return;
    record.pointerDown = false;
    record.repeatElapsed = 0U;
    record.nextRepeat = 0U;
    args.SetHandled(true);
    if (button.GetClickMode() == ClickMode::Release &&
        button.GetIsEnabled() && button.GetIsMouseOver()) {
        static_cast<void>(InvokeClick(button));
    }
    static_cast<void>(
        SyncVisualState(button));
}

void ButtonBehavior::OnKeyDown(
    Base::Object* sender,
    KeyEventArgs& args) noexcept {
    auto& button = *static_cast<ButtonBase*>(sender);
    if (!button.GetIsEnabled() ||
        (args.GetKey() != KeyboardKeySpace &&
            args.GetKey() != KeyboardKeyEnter)) return;
    const std::uint32_t index = FindButton(button);
    if (index == UINT32_MAX) return;
    ButtonRecord& record = buttons_[index];
    if (!record.keyboardDown) {
        record.keyboardDown = true;
        record.repeatElapsed = 0U;
        record.nextRepeat = 0U;
        static_cast<void>(AeroGuiInternal::SetPressed(button, true));
        if (button.GetClickMode() == ClickMode::Press) {
            static_cast<void>(InvokeClick(button));
        }
        static_cast<void>(
            SyncVisualState(button));
    }
    args.SetHandled(true);
}

void ButtonBehavior::OnKeyUp(
    Base::Object* sender,
    KeyEventArgs& args) noexcept {
    auto& button = *static_cast<ButtonBase*>(sender);
    if (args.GetKey() != KeyboardKeySpace &&
        args.GetKey() != KeyboardKeyEnter) return;
    const std::uint32_t index = FindButton(button);
    if (index == UINT32_MAX ||
        !buttons_[index].keyboardDown) return;
    buttons_[index].keyboardDown = false;
    buttons_[index].repeatElapsed = 0U;
    buttons_[index].nextRepeat = 0U;
    static_cast<void>(AeroGuiInternal::SetPressed(button, false));
    args.SetHandled(true);
    if (button.GetIsEnabled() &&
        button.GetClickMode() == ClickMode::Release) {
        static_cast<void>(InvokeClick(button));
    }
    static_cast<void>(
        SyncVisualState(button));
}

void ButtonBehavior::OnFocusChanged(
    Base::Object* sender,
    KeyboardFocusChangedEventArgs& args) noexcept {
    auto& button = *static_cast<ButtonBase*>(sender);
    const std::uint32_t index = FindButton(button);
    if (index == UINT32_MAX) return;
    if (args.GetNewFocus() != &button &&
        buttons_[index].keyboardDown) {
        buttons_[index].keyboardDown = false;
        buttons_[index].repeatElapsed = 0U;
        buttons_[index].nextRepeat = 0U;
        static_cast<void>(AeroGuiInternal::SetPressed(button, false));
    }
    static_cast<void>(
        SyncVisualState(button));
}

void ButtonBehavior::OnPropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    auto& button = static_cast<ButtonBase&>(object);
    const std::uint32_t index = FindButton(button);
    if (index == UINT32_MAX) return;
    if (args.GetProperty() == ButtonBase::CommandProperty) {
        if (!SubscribeCommand(button, buttons_[index])) return;
        static_cast<void>(RefreshCanExecute(button));
    } else if (args.GetProperty() == UIElement::IsEnabledProperty ||
               args.GetProperty() == UIElement::IsMouseOverProperty ||
               args.GetProperty() == UIElement::IsPressedProperty ||
               args.GetProperty() == UIElement::IsKeyboardFocusedProperty) {
        static_cast<void>(
            SyncVisualState(button));
    } else {
        const TypeId type = button.RuntimeType();
        const bool isToggle =
            type == ToggleButton::StaticTypeId() ||
            type == CheckBox::StaticTypeId() ||
            type == RadioButton::StaticTypeId();
        if (!isToggle) return;
        auto& toggle = static_cast<ToggleButton&>(button);
        ButtonRecord& record = buttons_[index];
        PublishToggleState(toggle, record);
        if (type == RadioButton::StaticTypeId() &&
            args.GetProperty() == RadioButton::GroupNameProperty &&
            ReadToggleState(toggle) == ToggleState::Checked) {
            UncheckRadioPeers(
                static_cast<RadioButton&>(toggle));
        }
    }
}

void ButtonBehavior::OnPointerStateChanged(
    UIElement& element) noexcept {
    for (std::uint32_t index = 0U;
        index < buttons_.Size(); ++index) {
        ButtonBase* button = ResolveButton(index);
        if (button != &element) continue;
        const bool mouseOver = button->GetIsMouseOver();
        if (mouseOver && !buttons_[index].wasMouseOver &&
            button->GetIsEnabled() &&
            button->GetClickMode() == ClickMode::Hover) {
            static_cast<void>(InvokeClick(*button));
        }
        buttons_[index].wasMouseOver = mouseOver;
        static_cast<void>(
            SyncVisualState(*button));
        return;
    }
}

void ButtonBehavior::OnCaptureChanged(
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
        static_cast<void>(
            SyncVisualState(*button));
        return;
    }
}

void ButtonBehavior::OnRequerySuggested() noexcept {
    for (std::uint32_t index = 0U;
        index < buttons_.Size(); ++index) {
        ButtonBase* button = ResolveButton(index);
        if (button != nullptr) {
            static_cast<void>(RefreshCanExecute(*button));
        }
    }
}

} // namespace Aero::Controls
