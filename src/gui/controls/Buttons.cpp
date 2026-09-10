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
#include <new>
#include "ControlBehavior.hpp"

namespace Aero::Controls {

using namespace Primitives;

// ---------------------------------------------------------------------------
// ButtonBase::State
// ---------------------------------------------------------------------------

struct ButtonBase::State {
    explicit State(ButtonBase& owner) noexcept
        : owner_(&owner),
          mouseDownHandler_(this, &State::OnMouseDown),
          mouseUpHandler_(this, &State::OnMouseUp),
          keyDownHandler_(this, &State::OnKeyDown),
          keyUpHandler_(this, &State::OnKeyUp),
          focusChangedHandler_(this, &State::OnFocusChanged),
          propertyChangedHandler_(this, &State::OnPropertyChanged),
          canExecuteChangedHandler_(this, &State::OnCanExecuteChanged) {
        owner.AddHandler(UIElement::MouseDownEvent, mouseDownHandler_);
        owner.AddHandler(UIElement::MouseUpEvent, mouseUpHandler_);
        owner.AddHandler(UIElement::KeyDownEvent, keyDownHandler_);
        owner.AddHandler(UIElement::KeyUpEvent, keyUpHandler_);
        owner.AddHandler(UIElement::GotKeyboardFocusEvent, focusChangedHandler_);
        owner.AddHandler(UIElement::LostKeyboardFocusEvent, focusChangedHandler_);

        owner.AddValueChangedHandler(ButtonBase::CommandProperty, propertyChangedHandler_);
        owner.AddValueChangedHandler(UIElement::IsEnabledProperty, propertyChangedHandler_);
        owner.AddValueChangedHandler(UIElement::IsMouseOverProperty, propertyChangedHandler_);
        owner.AddValueChangedHandler(UIElement::IsPressedProperty, propertyChangedHandler_);
        owner.AddValueChangedHandler(UIElement::IsKeyboardFocusedProperty, propertyChangedHandler_);

        HookCommand(owner.GetCommand());
    }

    ~State() noexcept {
        UnhookCommand();
        if (pointerDown_) {
            static_cast<void>(owner_->ReleasePointer(pointerId_));
            pointerDown_ = false;
        }
        owner_->RemoveHandler(UIElement::MouseDownEvent, mouseDownHandler_);
        owner_->RemoveHandler(UIElement::MouseUpEvent, mouseUpHandler_);
        owner_->RemoveHandler(UIElement::KeyDownEvent, keyDownHandler_);
        owner_->RemoveHandler(UIElement::KeyUpEvent, keyUpHandler_);
        owner_->RemoveHandler(UIElement::GotKeyboardFocusEvent, focusChangedHandler_);
        owner_->RemoveHandler(UIElement::LostKeyboardFocusEvent, focusChangedHandler_);

        owner_->RemoveValueChangedHandler(ButtonBase::CommandProperty, propertyChangedHandler_);
        owner_->RemoveValueChangedHandler(UIElement::IsEnabledProperty, propertyChangedHandler_);
        owner_->RemoveValueChangedHandler(UIElement::IsMouseOverProperty, propertyChangedHandler_);
        owner_->RemoveValueChangedHandler(UIElement::IsPressedProperty, propertyChangedHandler_);
        owner_->RemoveValueChangedHandler(UIElement::IsKeyboardFocusedProperty, propertyChangedHandler_);
    }

    void OnMouseDown(Base::Object*, MouseButtonEventArgs& args) noexcept {
        if (args.GetChangedButton() == MouseButton::Left) {
            owner_->OnMouseLeftButtonDown(args);
        }
    }

    void OnMouseUp(Base::Object*, MouseButtonEventArgs& args) noexcept {
        if (args.GetChangedButton() == MouseButton::Left) {
            owner_->OnMouseLeftButtonUp(args);
        }
    }

    void OnKeyDown(Base::Object*, KeyEventArgs& args) noexcept {
        owner_->OnKeyDown(args);
    }

    void OnKeyUp(Base::Object*, KeyEventArgs& args) noexcept {
        owner_->OnKeyUp(args);
    }

    void OnFocusChanged(Base::Object*, KeyboardFocusChangedEventArgs& args) noexcept {
        if (args.GetNewFocus() != owner_ && keyboardDown_) {
            keyboardDown_ = false;
            static_cast<void>(AeroGuiInternal::SetPressed(*owner_, false));
        }
        owner_->UpdateVisualState();
    }

    void OnPropertyChanged(DependencyObject&, const DependencyPropertyChangedEventArgs& args) noexcept {
        if (args.GetProperty() == ButtonBase::CommandProperty) {
            HookCommand(owner_->GetCommand());
            RefreshCanExecute();
        } else if (args.GetProperty() == UIElement::IsMouseOverProperty) {
            const bool mouseOver = owner_->GetIsMouseOver();
            if (mouseOver && !wasMouseOver_ && owner_->GetIsEnabled() && owner_->GetClickMode() == ClickMode::Hover) {
                owner_->OnClick();
            }
            wasMouseOver_ = mouseOver;
            owner_->UpdateVisualState();
        } else if (args.GetProperty() == UIElement::IsEnabledProperty ||
                   args.GetProperty() == UIElement::IsPressedProperty ||
                   args.GetProperty() == UIElement::IsKeyboardFocusedProperty) {
            owner_->UpdateVisualState();
        }
    }

    void OnCanExecuteChanged() noexcept {
        RefreshCanExecute();
    }

    void HookCommand(ICommand* command) noexcept {
        UnhookCommand();
        if (command != nullptr) {
            hookedCommand_ = Base::Ref<ICommand>::FromBorrowed(*command);
            hookedCommand_->AddCanExecuteChanged(canExecuteChangedHandler_);
        }
    }

    void UnhookCommand() noexcept {
        if (hookedCommand_) {
            hookedCommand_->RemoveCanExecuteChanged(canExecuteChangedHandler_);
            hookedCommand_.Reset();
        }
    }

    void RefreshCanExecute() noexcept {
        ICommand* command = owner_->GetCommand();
        bool enabled = true;
        if (command != nullptr) {
            UIElement* target = owner_->GetCommandTarget();
            if (target == nullptr) target = owner_;
            const Value parameter = owner_->GetCommandParameter();
            Aero::InputRouter* input = AeroGuiInternal::InputRouterOf(*owner_);
            if (input != nullptr) {
                Base::Result<bool> allowed = input->CanExecute(*command, parameter, *target);
                if (allowed) enabled = allowed.Value();
            } else {
                Base::Result<bool> allowed = command->CanExecute(parameter, target);
                if (allowed) enabled = allowed.Value();
            }
        }
        commandEnabled = enabled;
        if (!enabled && owner_->GetIsEnabled()) {
            owner_->SetIsEnabled(false);
        }
        owner_->UpdateVisualState();
    }

    ButtonBase* owner_ = nullptr;
    MouseButtonEventHandler mouseDownHandler_;
    MouseButtonEventHandler mouseUpHandler_;
    KeyEventHandler keyDownHandler_;
    KeyEventHandler keyUpHandler_;
    KeyboardFocusChangedEventHandler focusChangedHandler_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    Base::Delegate<void()> canExecuteChangedHandler_;

    Base::Ref<ICommand> hookedCommand_;
    std::uint32_t pointerId_ = 0U;
    bool pointerDown_ = false;
    bool keyboardDown_ = false;
    bool wasMouseOver_ = false;
    bool commandEnabled = true;
};

// ---------------------------------------------------------------------------
// ButtonBase
// ---------------------------------------------------------------------------

ButtonBase::ButtonBase(TypeId runtimeType) noexcept
    : ContentControl(runtimeType),
      state_(new (std::nothrow) State(*this)) {
}

ButtonBase::~ButtonBase() {
    delete state_;
    state_ = nullptr;
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
    return state_ != nullptr ? state_->commandEnabled : true;
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

void ButtonBase::OnMouseLeftButtonDown(MouseButtonEventArgs& args) {
    if (!GetIsEnabled()) return;
    if (state_ != nullptr) {
        state_->pointerId_ = args.GetPointerId();
        state_->pointerDown_ = true;
    }
    static_cast<void>(CapturePointer(args.GetPointerId()));
    static_cast<void>(Focus());
    args.SetHandled(true);
    if (GetClickMode() == ClickMode::Press) {
        OnClick();
    }
    UpdateVisualState();
}

void ButtonBase::OnMouseLeftButtonUp(MouseButtonEventArgs& args) {
    if (state_ == nullptr || !state_->pointerDown_) return;
    if (state_->pointerId_ != args.GetPointerId()) return;
    state_->pointerDown_ = false;
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
    if (state_ != nullptr && !state_->keyboardDown_) {
        state_->keyboardDown_ = true;
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
    if (state_ == nullptr || !state_->keyboardDown_) return;
    state_->keyboardDown_ = false;
    static_cast<void>(AeroGuiInternal::SetPressed(*this, false));
    args.SetHandled(true);
    if (GetIsEnabled() && GetClickMode() == ClickMode::Release) {
        OnClick();
    }
    UpdateVisualState();
}

// ---------------------------------------------------------------------------
// ToggleButton::ToggleState
// ---------------------------------------------------------------------------

struct ToggleButton::ToggleState {
    explicit ToggleState(ToggleButton& owner) noexcept
        : owner_(&owner),
          propertyChangedHandler_(this, &ToggleState::OnPropertyChanged) {
        owner.AddValueChangedHandler(ToggleButton::IsCheckedProperty, propertyChangedHandler_);
        owner.AddValueChangedHandler(ToggleButton::IsThreeStateProperty, propertyChangedHandler_);
    }

    ~ToggleState() noexcept {
        owner_->RemoveValueChangedHandler(ToggleButton::IsCheckedProperty, propertyChangedHandler_);
        owner_->RemoveValueChangedHandler(ToggleButton::IsThreeStateProperty, propertyChangedHandler_);
    }

    void OnPropertyChanged(DependencyObject&, const DependencyPropertyChangedEventArgs& args) noexcept {
        if (args.GetProperty() == ToggleButton::IsCheckedProperty) {
            const Nullable<bool> current = owner_->GetIsChecked();
            RoutedEventArgs eventArgs;
            if (!current.GetHasValue()) {
                owner_->RaiseEvent(ToggleButton::IndeterminateEvent, &eventArgs);
            } else if (current.GetValue()) {
                owner_->RaiseEvent(ToggleButton::CheckedEvent, &eventArgs);
            } else {
                owner_->RaiseEvent(ToggleButton::UncheckedEvent, &eventArgs);
            }
            owner_->UpdateVisualState();
        }
    }

    ToggleButton* owner_ = nullptr;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
};

// ---------------------------------------------------------------------------
// ToggleButton
// ---------------------------------------------------------------------------

ToggleButton::ToggleButton(TypeId runtimeType) noexcept
    : ButtonBase(runtimeType),
      toggleState_(new (std::nothrow) ToggleState(*this)) {
}

ToggleButton::~ToggleButton() {
    delete toggleState_;
    toggleState_ = nullptr;
}

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

// ---------------------------------------------------------------------------
// RadioButton::RadioState & helpers
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

struct RadioButton::RadioState {
    explicit RadioState(RadioButton& owner) noexcept
        : owner_(&owner),
          propertyChangedHandler_(this, &RadioState::OnPropertyChanged) {
        owner.AddValueChangedHandler(ToggleButton::IsCheckedProperty, propertyChangedHandler_);
        owner.AddValueChangedHandler(RadioButton::GroupNameProperty, propertyChangedHandler_);
    }

    ~RadioState() noexcept {
        owner_->RemoveValueChangedHandler(ToggleButton::IsCheckedProperty, propertyChangedHandler_);
        owner_->RemoveValueChangedHandler(RadioButton::GroupNameProperty, propertyChangedHandler_);
    }

    void OnPropertyChanged(DependencyObject&, const DependencyPropertyChangedEventArgs& args) noexcept {
        if (args.GetProperty() == ToggleButton::IsCheckedProperty ||
            args.GetProperty() == RadioButton::GroupNameProperty) {
            const Nullable<bool> current = owner_->GetIsChecked();
            if (current.GetHasValue() && current.GetValue()) {
                owner_->UncheckRadioPeers();
            }
        }
    }

    RadioButton* owner_ = nullptr;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
};

// ---------------------------------------------------------------------------
// RadioButton
// ---------------------------------------------------------------------------

RadioButton::RadioButton(TypeId runtimeType) noexcept
    : Primitives::ToggleButton(runtimeType),
      radioState_(new (std::nothrow) RadioState(*this)) {
}

RadioButton::~RadioButton() {
    delete radioState_;
    radioState_ = nullptr;
}

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
    auto* behaviors = static_cast<ControlBehavior*>(
        AeroGuiInternal::ControlBehaviorRuntime(*this));
    if (behaviors != nullptr) {
        behaviors->SetActiveRepeatButton(nullptr);
    }
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
        auto* behaviors = static_cast<ControlBehavior*>(
            AeroGuiInternal::ControlBehaviorRuntime(*this));
        if (behaviors != nullptr) {
            behaviors->SetActiveRepeatButton(this);
        }
    }
}

void RepeatButton::OnMouseLeftButtonUp(MouseButtonEventArgs& args) {
    ButtonBase::OnMouseLeftButtonUp(args);
    auto* behaviors = static_cast<ControlBehavior*>(
        AeroGuiInternal::ControlBehaviorRuntime(*this));
    if (behaviors != nullptr) {
        behaviors->SetActiveRepeatButton(nullptr);
    }
}

void RepeatButton::OnKeyDown(KeyEventArgs& args) {
    ButtonBase::OnKeyDown(args);
    if (GetIsEnabled() && (args.GetKey() == KeyboardKeySpace || args.GetKey() == KeyboardKeyEnter)) {
        auto* behaviors = static_cast<ControlBehavior*>(
            AeroGuiInternal::ControlBehaviorRuntime(*this));
        if (behaviors != nullptr) {
            behaviors->SetActiveRepeatButton(this);
        }
    }
}

void RepeatButton::OnKeyUp(KeyEventArgs& args) {
    ButtonBase::OnKeyUp(args);
    if (args.GetKey() == KeyboardKeySpace || args.GetKey() == KeyboardKeyEnter) {
        auto* behaviors = static_cast<ControlBehavior*>(
            AeroGuiInternal::ControlBehaviorRuntime(*this));
        if (behaviors != nullptr) {
            behaviors->SetActiveRepeatButton(nullptr);
        }
    }
}

} // namespace Aero::Controls
