#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/input/InputManager.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include <Aero/VisualStateManager.hpp>
#include <Aero/Controls.hpp>
#include <Aero/LogicalTreeHelper.hpp>
#include <Aero/VisualTreeHelper.hpp>

#include <utility>

namespace Aero::Controls {

using namespace Primitives;

// ---------------------------------------------------------------------------
// ButtonBase
// ---------------------------------------------------------------------------

ButtonBase::ButtonBase(TypeId runtimeType) noexcept
    : ContentControl(runtimeType),
      canExecuteChangedHandler_(this, &ButtonBase::OnCanExecuteChanged) {
    HookCommand(GetCommand());
}

ButtonBase::~ButtonBase() {
    UnhookCommand();
    if (pointerDown_) {
        static_cast<void>(ReleasePointer(pointerId_));
        pointerDown_ = false;
    }
}

ClickMode ButtonBase::GetClickMode() const noexcept {
    return GetValue(ClickModeProperty);
}

ICommand* ButtonBase::GetCommand() const noexcept {
    return GetValue(CommandProperty).Get();
}

Value ButtonBase::GetCommandParameter() const noexcept {
    return GetValue(CommandParameterProperty);
}

UIElement* ButtonBase::GetCommandTarget() const noexcept {
    return GetValue(CommandTargetProperty).Get();
}

bool ButtonBase::GetIsCommandEnabled() const noexcept {
    return commandEnabled_;
}

void ButtonBase::SetClickMode(ClickMode value) noexcept {
    SetValue(ClickModeProperty, value);
}

void ButtonBase::SetCommand(Base::Ref<ICommand> command) noexcept {
    SetValue(CommandProperty, std::move(command));
}

void ButtonBase::SetCommandParameter(Value parameter) noexcept {
    SetValue(CommandParameterProperty, std::move(parameter));
}

void ButtonBase::SetCommandTarget(Base::Ref<UIElement> target) noexcept {
    SetValue(CommandTargetProperty, std::move(target));
}

void ButtonBase::OnApplyTemplate() noexcept {
    ContentControl::OnApplyTemplate();
    UpdateVisualState(false);
}

void ButtonBase::OnClick() {
    RoutedEventArgs args;
    RaiseEvent(ClickEvent, &args);

    ICommand* command = GetCommand();
    if (command != nullptr) {
        UIElement* target = GetCommandTarget();
        if (target == nullptr) target = this;
        const Value parameter = GetCommandParameter();
        Aero::InputRouter* input = AeroGuiInternal::InputRouterOf(*this);
        if (input != nullptr) {
            static_cast<void>(input->Execute(*command, parameter, *target));
        } else {
            command->Execute(parameter, target);
        }
    }
}

void ButtonBase::UpdateVisualState(bool useTransitions) noexcept {
    Base::StringView common = "Normal";
    if (!GetIsEnabled()) {
        common = "Disabled";
    } else if (GetIsPressed()) {
        common = "Pressed";
    } else if (GetIsMouseOver()) {
        common = "MouseOver";
    }
    VisualStateManager::GoToState(*this, common, useTransitions);

    Base::StringView focus = GetIsKeyboardFocused()
        ? Base::StringView("Focused")
        : Base::StringView("Unfocused");
    VisualStateManager::GoToState(*this, focus, useTransitions);
}

void ButtonBase::OnGotKeyboardFocus(KeyboardFocusChangedEventArgs& args) {
    ContentControl::OnGotKeyboardFocus(args);
    UpdateVisualState();
}

void ButtonBase::OnLostKeyboardFocus(KeyboardFocusChangedEventArgs& args) {
    ContentControl::OnLostKeyboardFocus(args);
    if (keyboardDown_) {
        keyboardDown_ = false;
        static_cast<void>(AeroGuiInternal::SetPressed(*this, false));
    }
    UpdateVisualState();
}

void ButtonBase::OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept {
    ContentControl::OnPropertyChanged(args);
    if (args.GetProperty() == CommandProperty) {
        HookCommand(GetCommand());
        RefreshCanExecute();
    } else if (args.GetProperty() == IsMouseOverProperty) {
        const bool mouseOver = GetIsMouseOver();
        if (mouseOver && !wasMouseOver_ && GetIsEnabled() && GetClickMode() == ClickMode::Hover) {
            OnClick();
        }
        wasMouseOver_ = mouseOver;
        UpdateVisualState();
    } else if (args.GetProperty() == IsEnabledProperty ||
               args.GetProperty() == IsPressedProperty ||
               args.GetProperty() == IsKeyboardFocusedProperty) {
        UpdateVisualState();
    }
}

void ButtonBase::OnCanExecuteChanged() noexcept {
    RefreshCanExecute();
}

void ButtonBase::HookCommand(ICommand* command) noexcept {
    UnhookCommand();
    if (command != nullptr) {
        hookedCommand_ = Base::Ref<ICommand>::FromBorrowed(*command);
        hookedCommand_->AddCanExecuteChanged(canExecuteChangedHandler_);
    }
}

void ButtonBase::UnhookCommand() noexcept {
    if (hookedCommand_) {
        hookedCommand_->RemoveCanExecuteChanged(canExecuteChangedHandler_);
        hookedCommand_.Reset();
    }
}

void ButtonBase::RefreshCanExecute() noexcept {
    ICommand* command = GetCommand();
    bool enabled = true;
    if (command != nullptr) {
        UIElement* target = GetCommandTarget();
        if (target == nullptr) target = this;
        const Value parameter = GetCommandParameter();
        Aero::InputRouter* input = AeroGuiInternal::InputRouterOf(*this);
        if (input != nullptr) {
            Base::Result<bool> allowed = input->CanExecute(*command, parameter, *target);
            if (allowed) enabled = allowed.Value();
        } else {
            Base::Result<bool> allowed = command->CanExecute(parameter, target);
            if (allowed) enabled = allowed.Value();
        }
    }
    commandEnabled_ = enabled;
    if (!enabled && GetIsEnabled()) {
        SetIsEnabled(false);
    }
    UpdateVisualState();
}

PropertyValue ButtonBase::CoerceValueCore(
    DependencyPropertyHandle property,
    const PropertyValue& baseValue) noexcept {
    if (property != UIElement::IsEnabledProperty.Handle()) {
        return baseValue;
    }
    if (baseValue.Kind() != Meta::ValueKind::Boolean) {
        return baseValue;
    }
    const bool adjusted = baseValue.AsBoolean() && GetIsCommandEnabled();
    if (adjusted == baseValue.AsBoolean()) {
        return baseValue;
    }
    Base::Result<PropertyValue> encoded =
        Meta::ValueCodec<bool>::Encode(adjusted);
    return encoded ? std::move(encoded).Value() : baseValue;
}

void ButtonBase::OnMouseLeftButtonDown(MouseButtonEventArgs& args) {
    if (!GetIsEnabled()) return;
    pointerId_ = args.GetPointerId();
    pointerDown_ = true;
    static_cast<void>(CapturePointer(args.GetPointerId()));
    static_cast<void>(Focus());
    args.SetHandled(true);
    if (GetClickMode() == ClickMode::Press) {
        OnClick();
    }
    UpdateVisualState();
}

void ButtonBase::OnMouseLeftButtonUp(MouseButtonEventArgs& args) {
    if (!pointerDown_) return;
    if (pointerId_ != args.GetPointerId()) return;
    pointerDown_ = false;
    static_cast<void>(ReleasePointer(args.GetPointerId()));
    args.SetHandled(true);
    if (GetClickMode() == ClickMode::Release && GetIsEnabled() && GetIsMouseOver()) {
        OnClick();
    }
    UpdateVisualState();
}

void ButtonBase::OnKeyDown(KeyEventArgs& args) {
    if (!GetIsEnabled()) return;
    if (args.GetKey() != KeyboardKeySpace && args.GetKey() != KeyboardKeyEnter) return;
    if (!keyboardDown_) {
        keyboardDown_ = true;
        static_cast<void>(AeroGuiInternal::SetPressed(*this, true));
        if (GetClickMode() == ClickMode::Press) {
            OnClick();
        }
        UpdateVisualState();
    }
    args.SetHandled(true);
}

void ButtonBase::OnKeyUp(KeyEventArgs& args) {
    if (args.GetKey() != KeyboardKeySpace && args.GetKey() != KeyboardKeyEnter) return;
    if (!keyboardDown_) return;
    keyboardDown_ = false;
    static_cast<void>(AeroGuiInternal::SetPressed(*this, false));
    args.SetHandled(true);
    if (GetIsEnabled() && GetClickMode() == ClickMode::Release) {
        OnClick();
    }
    UpdateVisualState();
}

// ---------------------------------------------------------------------------
// ToggleButton
// ---------------------------------------------------------------------------

ToggleButton::ToggleButton(TypeId runtimeType) noexcept
    : ButtonBase(runtimeType) {
}

ToggleButton::~ToggleButton() = default;

Nullable<bool> ToggleButton::GetIsChecked() const noexcept {
    return GetValue(IsCheckedProperty);
}

bool ToggleButton::GetIsThreeState() const noexcept {
    return GetValue(IsThreeStateProperty);
}

void ToggleButton::SetIsChecked(Nullable<bool> value) noexcept {
    SetValue(IsCheckedProperty, value);
}

void ToggleButton::SetIsThreeState(bool value) noexcept {
    SetValue(IsThreeStateProperty, value);
}

void ToggleButton::OnClick() {
    OnToggle();
    ButtonBase::OnClick();
}

void ToggleButton::OnToggle() noexcept {
    const Nullable<bool> current = GetIsChecked();
    if (!current.GetHasValue()) {
        SetIsChecked(false);
    } else if (current.GetValue()) {
        if (GetIsThreeState()) {
            SetIsChecked(Nullable<bool>{});
        } else {
            SetIsChecked(false);
        }
    } else {
        SetIsChecked(true);
    }
}

void ToggleButton::UpdateVisualState(bool useTransitions) noexcept {
    ButtonBase::UpdateVisualState(useTransitions);

    Base::StringView check = "Unchecked";
    const Nullable<bool> isChecked = GetIsChecked();
    if (isChecked.GetHasValue()) {
        check = isChecked.GetValue() ? Base::StringView("Checked") : Base::StringView("Unchecked");
    } else {
        check = "Indeterminate";
    }
    VisualStateManager::GoToState(*this, check, useTransitions);
}

void ToggleButton::OnChecked(RoutedEventArgs& e) {
    RaiseEvent(CheckedEvent, &e);
}

void ToggleButton::OnUnchecked(RoutedEventArgs& e) {
    RaiseEvent(UncheckedEvent, &e);
}

void ToggleButton::OnIndeterminate(RoutedEventArgs& e) {
    RaiseEvent(IndeterminateEvent, &e);
}

void ToggleButton::OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept {
    ButtonBase::OnPropertyChanged(args);
    if (args.GetProperty() == IsCheckedProperty) {
        const Nullable<bool> current = GetIsChecked();
        RoutedEventArgs eventArgs;
        if (!current.GetHasValue()) {
            OnIndeterminate(eventArgs);
        } else if (current.GetValue()) {
            OnChecked(eventArgs);
        } else {
            OnUnchecked(eventArgs);
        }
        UpdateVisualState();
    }
}

// ---------------------------------------------------------------------------
// RadioButton helpers
// ---------------------------------------------------------------------------

static void UncheckRadioSiblings(DependencyObject& container, RadioButton& current, Base::StringView group) noexcept {
    const std::uint32_t logicalCount = LogicalTreeHelper::GetChildrenCount(container);
    if (logicalCount > 0) {
        for (std::uint32_t i = 0; i < logicalCount; ++i) {
            DependencyObject* child = LogicalTreeHelper::GetChild(container, i);
            if (child == nullptr) continue;
            if (child != &current && child->RuntimeType() == RadioButton::StaticTypeId()) {
                auto* radio = static_cast<RadioButton*>(child);
                if (radio->GetGroupName() == group) {
                    const Nullable<bool> isChecked = radio->GetIsChecked();
                    if (isChecked.GetHasValue() && isChecked.GetValue()) {
                        radio->SetIsChecked(false);
                    }
                }
            } else if (child->RuntimeType() != RadioButton::StaticTypeId()) {
                UncheckRadioSiblings(*child, current, group);
            }
        }
    } else {
        auto* visual = ::Aero::TryCast<::Aero::Media::Visual>(&container);
        if (visual != nullptr) {
            const std::uint32_t visualCount = ::Aero::Media::VisualTreeHelper::GetChildrenCount(*visual);
            for (std::uint32_t i = 0; i < visualCount; ++i) {
                ::Aero::Media::Visual* childVisual = ::Aero::Media::VisualTreeHelper::GetChild(*visual, i);
                if (childVisual == nullptr) continue;
                if (childVisual != &current && childVisual->RuntimeType() == RadioButton::StaticTypeId()) {
                    auto* radio = static_cast<RadioButton*>(childVisual);
                    if (radio->GetGroupName() == group) {
                        const Nullable<bool> isChecked = radio->GetIsChecked();
                        if (isChecked.GetHasValue() && isChecked.GetValue()) {
                            radio->SetIsChecked(false);
                        }
                    }
                } else if (childVisual->RuntimeType() != RadioButton::StaticTypeId()) {
                    UncheckRadioSiblings(*childVisual, current, group);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// RadioButton
// ---------------------------------------------------------------------------

RadioButton::RadioButton(TypeId runtimeType) noexcept
    : Primitives::ToggleButton(runtimeType) {
}

RadioButton::~RadioButton() = default;

Base::StringView RadioButton::GetGroupName() const noexcept {
    return GetValue(GroupNameProperty);
}

void RadioButton::SetGroupName(Base::StringView value) noexcept {
    SetValue(GroupNameProperty, value);
}

void RadioButton::OnClick() {
    OnToggle();
    ButtonBase::OnClick();
}

void RadioButton::OnToggle() noexcept {
    SetIsChecked(true);
}

void RadioButton::OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept {
    ToggleButton::OnPropertyChanged(args);
    if (args.GetProperty() == IsCheckedProperty ||
        args.GetProperty() == GroupNameProperty) {
        const Nullable<bool> current = GetIsChecked();
        if (current.GetHasValue() && current.GetValue()) {
            UncheckRadioPeers();
        }
    }
}

void RadioButton::UncheckRadioPeers() noexcept {
    ::Aero::Media::Visual* parent = ::Aero::TryCast<::Aero::Media::Visual>(GetLogicalParent());
    if (parent == nullptr) parent = GetVisualParent();
    if (parent == nullptr) return;
    const Base::StringView group = GetGroupName();
    UncheckRadioSiblings(*parent, *this, group);
}

// ---------------------------------------------------------------------------
// RepeatButton
// ---------------------------------------------------------------------------

RepeatButton::RepeatButton(TypeId runtimeType) noexcept
    : ButtonBase(runtimeType) {
    SetClickMode(ClickMode::Press);
}

RepeatButton::~RepeatButton() {
    AeroGuiInternal::SetActiveRepeatButton(*this, nullptr);
}

std::uint32_t RepeatButton::GetDelay() const noexcept {
    return GetValue(DelayProperty);
}

std::uint32_t RepeatButton::GetInterval() const noexcept {
    return GetValue(IntervalProperty);
}

void RepeatButton::SetDelay(std::uint32_t value) noexcept {
    SetValue(DelayProperty, value);
}

void RepeatButton::SetInterval(std::uint32_t value) noexcept {
    SetValue(IntervalProperty, value);
}

void RepeatButton::OnMouseLeftButtonDown(MouseButtonEventArgs& args) {
    ButtonBase::OnMouseLeftButtonDown(args);
    if (GetIsEnabled()) {
        AeroGuiInternal::SetActiveRepeatButton(*this, this);
    }
}

void RepeatButton::OnMouseLeftButtonUp(MouseButtonEventArgs& args) {
    ButtonBase::OnMouseLeftButtonUp(args);
    AeroGuiInternal::SetActiveRepeatButton(*this, nullptr);
}

void RepeatButton::OnKeyDown(KeyEventArgs& args) {
    ButtonBase::OnKeyDown(args);
    if (GetIsEnabled() && (args.GetKey() == KeyboardKeySpace || args.GetKey() == KeyboardKeyEnter)) {
        AeroGuiInternal::SetActiveRepeatButton(*this, this);
    }
}

void RepeatButton::OnKeyUp(KeyEventArgs& args) {
    ButtonBase::OnKeyUp(args);
    if (args.GetKey() == KeyboardKeySpace || args.GetKey() == KeyboardKeyEnter) {
        AeroGuiInternal::SetActiveRepeatButton(*this, nullptr);
    }
}

} // namespace Aero::Controls
