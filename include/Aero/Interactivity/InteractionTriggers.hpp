#pragma once

#include <Aero/Data/Binding.hpp>
#include <Aero/Input.hpp>
#include <Aero/Triggers/TriggerBase.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>

namespace Aero::Interactivity {

// Fires its actions whenever the authored Binding source publishes a changed
// value. The Binding is retained as an authoring plan and is materialized per
// associated element, matching Blend's PropertyChangedTrigger lifetime.
class AERO_GUI_API PropertyChangedTrigger : public ::Aero::TriggerBase {
    AERO_DECLARE_TYPE(PropertyChangedTrigger, ::Aero::TriggerBase)
public:
    PropertyChangedTrigger() noexcept : TriggerBase(StaticTypeId()) {}

    Ref<Aero::Data::Binding> GetBinding() const noexcept {
        return binding_;
    }
    void SetBinding(Ref<Aero::Data::Binding> value) noexcept {
        binding_ = std::move(value);
    }
    Result<void> AddAction(Ref<TriggerAction> value) noexcept {
        return value
            ? actions_.PushBack(std::move(value))
            : Result<void>(Base::Status::Failure(
                  Base::ErrorCode::InvalidArgument,
                  "PropertyChangedTrigger action cannot be null"));
    }
    void ClearActions() noexcept { actions_.Clear(); }
    Span<const Ref<TriggerAction>> GetActions() const noexcept {
        return actions_.AsSpan();
    }

private:
    Ref<Aero::Data::Binding> binding_;
    Base::Vector<Ref<TriggerAction>> actions_;
};

// Executes actions for a matching KeyDown routed event. Key remains a string
// authoring value so WPF spellings such as Enter, Space and Escape remain
// portable without adding a second public keyboard enum beside KeyboardInput.
class AERO_GUI_API KeyTrigger : public ::Aero::TriggerBase {
    AERO_DECLARE_TYPE(KeyTrigger, ::Aero::TriggerBase)
public:
    KeyTrigger() noexcept : TriggerBase(StaticTypeId()) {}

    StringView GetKey() const noexcept { return key_.View(); }
    void SetKey(StringView value) noexcept {
        static_cast<void>(key_.Assign(value));
    }
    bool GetActiveOnFocus() const noexcept { return activeOnFocus_; }
    void SetActiveOnFocus(bool value) noexcept { activeOnFocus_ = value; }
    Result<void> AddAction(Ref<TriggerAction> value) noexcept {
        return value
            ? actions_.PushBack(std::move(value))
            : Result<void>(Base::Status::Failure(
                  Base::ErrorCode::InvalidArgument,
                  "KeyTrigger action cannot be null"));
    }
    void ClearActions() noexcept { actions_.Clear(); }
    Span<const Ref<TriggerAction>> GetActions() const noexcept {
        return actions_.AsSpan();
    }

private:
    String key_;
    bool activeOnFocus_ = false;
    Base::Vector<Ref<TriggerAction>> actions_;
};

// Blend-compatible command action. Bindings are authoring plans because an
// action can be retained in a Style and must resolve against each styled
// element's DataContext and NameScope independently.
class AERO_GUI_API InvokeCommandAction : public TriggerAction {
    AERO_DECLARE_TYPE(InvokeCommandAction, TriggerAction)
public:
    InvokeCommandAction() noexcept : TriggerAction(StaticTypeId()) {}

    Ref<Aero::Input::ICommand> GetCommand() const noexcept {
        return command_;
    }
    void SetCommand(Ref<Aero::Input::ICommand> value) noexcept {
        command_ = std::move(value);
    }
    const Meta::PropertyValue& GetCommandParameter() const noexcept {
        return commandParameter_;
    }
    void SetCommandParameter(const Meta::PropertyValue& value) noexcept {
        commandParameter_ = value;
    }
    Ref<Aero::Data::Binding> GetCommandBinding() const noexcept {
        return commandBinding_;
    }
    void SetCommandBinding(Ref<Aero::Data::Binding> value) noexcept {
        commandBinding_ = std::move(value);
    }
    Ref<Aero::Data::Binding> GetCommandParameterBinding() const noexcept {
        return commandParameterBinding_;
    }
    void SetCommandParameterBinding(
        Ref<Aero::Data::Binding> value) noexcept {
        commandParameterBinding_ = std::move(value);
    }

private:
    Ref<Aero::Input::ICommand> command_;
    Meta::PropertyValue commandParameter_;
    Ref<Aero::Data::Binding> commandBinding_;
    Ref<Aero::Data::Binding> commandParameterBinding_;
};

// Selects the associated item container. The runtime maps this to the native
// selector contract instead of synthesizing a mouse click.
class AERO_GUI_API SelectAction : public TriggerAction {
    AERO_DECLARE_TYPE(SelectAction, TriggerAction)
public:
    SelectAction() noexcept : TriggerAction(StaticTypeId()) {}
};

// Selects all text in the associated TextBox or PasswordBox.
class AERO_GUI_API SelectAllAction : public TriggerAction {
    AERO_DECLARE_TYPE(SelectAllAction, TriggerAction)
public:
    SelectAllAction() noexcept : TriggerAction(StaticTypeId()) {}
};

// Blend-compatible one-shot sound action. IsEnabled is a dependency property
// because authored XAML commonly targets it from ChangePropertyAction.
class AERO_GUI_API PlaySoundAction : public TriggerAction {
    AERO_DECLARE_TYPE(PlaySoundAction, TriggerAction)
public:
    PlaySoundAction() noexcept : TriggerAction(StaticTypeId()) {}

    StringView GetSource() const noexcept {
        return source_.View();
    }
    void SetSource(StringView value) noexcept {
        static_cast<void>(source_.Assign(value));
    }
    double GetVolume() const noexcept { return volume_; }
    void SetVolume(double value) noexcept { volume_ = value; }
    bool GetIsEnabled() const noexcept {
        return GetValueOr(IsEnabledProperty, true);
    }
    void SetIsEnabled(bool value) noexcept {
        SetValue(IsEnabledProperty, value);
    }

    inline static constexpr DependencyProperty<bool> IsEnabledProperty{"IsEnabled"};

private:
    String source_;
    double volume_ = 1.0;
};

} // namespace Aero::Interactivity
