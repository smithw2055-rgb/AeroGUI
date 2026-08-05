#pragma once

#include <Aero/Data.hpp>
#include <Aero/Input.hpp>
#include <Aero/Triggers/TriggerBase.hpp>
#include <Aero/Triggers/TriggerAction.hpp>

namespace Aero::Media::Animation {

// Fires its actions whenever the authored Binding source publishes a changed
// value. The Binding is retained as an authoring plan and is materialized per
// associated element, matching Blend's PropertyChangedTrigger lifetime.
class AERO_API PropertyChangedTrigger : public ::Aero::TriggerBase {
    AERO_DECLARE_TYPE(PropertyChangedTrigger, ::Aero::TriggerBase)
public:
    PropertyChangedTrigger() noexcept : TriggerBase(StaticTypeId()) {}

    Base::Ref<Aero::Data::Binding> GetBinding() const noexcept {
        return binding_;
    }
    void SetBinding(Base::Ref<Aero::Data::Binding> value) noexcept {
        binding_ = std::move(value);
    }
    Base::Result<void> AddAction(Base::Ref<TriggerAction> value) noexcept {
        return value
            ? actions_.PushBack(std::move(value))
            : Base::Result<void>(Base::Status::Failure(
                  Base::ErrorCode::InvalidArgument,
                  "PropertyChangedTrigger action cannot be null"));
    }
    void ClearActions() noexcept { actions_.Clear(); }
    Base::Span<const Base::Ref<TriggerAction>> GetActions() const noexcept {
        return actions_.AsSpan();
    }

private:
    Base::Ref<Aero::Data::Binding> binding_;
    Base::Vector<Base::Ref<TriggerAction>> actions_;
};

// Executes actions for a matching KeyDown routed event. Key remains a string
// authoring value so WPF spellings such as Enter, Space and Escape remain
// portable without adding a second public keyboard enum beside KeyboardInput.
class AERO_API KeyTrigger : public ::Aero::TriggerBase {
    AERO_DECLARE_TYPE(KeyTrigger, ::Aero::TriggerBase)
public:
    KeyTrigger() noexcept : TriggerBase(StaticTypeId()) {}

    Base::StringView GetKey() const noexcept { return key_.View(); }
    void SetKey(Base::StringView value) noexcept {
        static_cast<void>(key_.Assign(value));
    }
    bool GetActiveOnFocus() const noexcept { return activeOnFocus_; }
    void SetActiveOnFocus(bool value) noexcept { activeOnFocus_ = value; }
    Base::Result<void> AddAction(Base::Ref<TriggerAction> value) noexcept {
        return value
            ? actions_.PushBack(std::move(value))
            : Base::Result<void>(Base::Status::Failure(
                  Base::ErrorCode::InvalidArgument,
                  "KeyTrigger action cannot be null"));
    }
    void ClearActions() noexcept { actions_.Clear(); }
    Base::Span<const Base::Ref<TriggerAction>> GetActions() const noexcept {
        return actions_.AsSpan();
    }

private:
    Base::String key_;
    bool activeOnFocus_ = false;
    Base::Vector<Base::Ref<TriggerAction>> actions_;
};

// Blend-compatible command action. Bindings are authoring plans because an
// action can be retained in a Style and must resolve against each styled
// element's DataContext and NameScope independently.
class AERO_API InvokeCommandAction : public TriggerAction {
    AERO_DECLARE_TYPE(InvokeCommandAction, TriggerAction)
public:
    InvokeCommandAction() noexcept : TriggerAction(StaticTypeId()) {}

    Base::Ref<Aero::Input::ICommand> GetCommand() const noexcept {
        return command_;
    }
    void SetCommand(Base::Ref<Aero::Input::ICommand> value) noexcept {
        command_ = std::move(value);
    }
    const Meta::PropertyValue& GetCommandParameter() const noexcept {
        return commandParameter_;
    }
    void SetCommandParameter(const Meta::PropertyValue& value) noexcept {
        commandParameter_ = value;
    }
    Base::Ref<Aero::Data::Binding> GetCommandBinding() const noexcept {
        return commandBinding_;
    }
    void SetCommandBinding(Base::Ref<Aero::Data::Binding> value) noexcept {
        commandBinding_ = std::move(value);
    }
    Base::Ref<Aero::Data::Binding> GetCommandParameterBinding() const noexcept {
        return commandParameterBinding_;
    }
    void SetCommandParameterBinding(
        Base::Ref<Aero::Data::Binding> value) noexcept {
        commandParameterBinding_ = std::move(value);
    }

private:
    Base::Ref<Aero::Input::ICommand> command_;
    Meta::PropertyValue commandParameter_;
    Base::Ref<Aero::Data::Binding> commandBinding_;
    Base::Ref<Aero::Data::Binding> commandParameterBinding_;
};

} // namespace Aero::Media::Animation
