#pragma once

#include <Aero/Data/Binding.hpp>

#include <Aero/DependencyObject.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Value.hpp>
#include <Aero/Interactivity/Interaction.hpp>

namespace Aero::Interactivity {

// WPF TriggerAction is a DependencyObject so named actions can participate in
// bindings and ChangePropertyAction targeting.
class AERO_GUI_API TriggerAction : public ::Aero::DependencyObject {
    AERO_DECLARE_TYPE(TriggerAction, ::Aero::DependencyObject)
protected:
    explicit TriggerAction(Meta::TypeId runtimeType) noexcept : DependencyObject(runtimeType) {}
    ~TriggerAction() override = default;
};



class AERO_GUI_API ChangePropertyAction : public TriggerAction {
    AERO_DECLARE_TYPE(ChangePropertyAction, TriggerAction)
public:
    ChangePropertyAction() noexcept : TriggerAction(StaticTypeId()) {}
    StringView GetTargetName() const noexcept { return targetName_.View(); }
    StringView GetPropertyName() const noexcept { return propertyName_.View(); }
    const Meta::PropertyValue& GetValue() const noexcept { return value_; }
    Ref<Aero::Data::Binding> GetValueBinding() const noexcept { return valueBinding_; }
    Ref<Aero::Data::Binding> GetTargetObject() const noexcept { return targetObject_; }
    void SetTargetName(StringView value) noexcept;
    void SetPropertyName(StringView value) noexcept;
    void SetValue(const Meta::PropertyValue& value) noexcept;
    void SetValueBinding(Ref<Aero::Data::Binding> value) noexcept;
    void SetTargetObject(Ref<Aero::Data::Binding> value) noexcept { targetObject_ = std::move(value); }

private:
    String targetName_;
    String propertyName_;
    Meta::PropertyValue value_;
    Ref<Aero::Data::Binding> valueBinding_;
    Ref<Aero::Data::Binding> targetObject_;
};

class AERO_GUI_API SetFocusAction : public TriggerAction {
    AERO_DECLARE_TYPE(SetFocusAction, TriggerAction)
public:
    SetFocusAction() noexcept : TriggerAction(StaticTypeId()) {}
    StringView GetTargetName() const noexcept { return targetName_.View(); }
    void SetTargetName(StringView value) noexcept { (void)targetName_.Assign(value); }
    Ref<Aero::Data::Binding> GetTargetObject() const noexcept { return targetObject_; }
    void SetTargetObject(Ref<Aero::Data::Binding> value) noexcept { targetObject_ = std::move(value); }
    bool GetEngage() const noexcept { return engage_; }
    void SetEngage(bool value) noexcept { engage_ = value; }

private:
    String targetName_;
    Ref<Aero::Data::Binding> targetObject_;
    bool engage_ = true;
};

class AERO_GUI_API LaunchUriOrFileAction : public TriggerAction {
    AERO_DECLARE_TYPE(LaunchUriOrFileAction, TriggerAction)
public:
    LaunchUriOrFileAction() noexcept : TriggerAction(StaticTypeId()) {}
    StringView GetPath() const noexcept { return path_.View(); }
    void SetPath(StringView value) noexcept;
    Ref<Aero::Data::Binding> GetPathBinding() const noexcept { return pathBinding_; }
    void SetPathBinding(Ref<Aero::Data::Binding> value) noexcept { pathBinding_ = std::move(value); }

private:
    String path_;
    Ref<Aero::Data::Binding> pathBinding_;
};

class AERO_GUI_API RemoveElementAction : public TriggerAction {
    AERO_DECLARE_TYPE(RemoveElementAction, TriggerAction)
public:
    RemoveElementAction() noexcept : TriggerAction(StaticTypeId()) {}
    Ref<Aero::Data::Binding> GetTargetObject() const noexcept { return targetObject_; }
    void SetTargetObject(Ref<Aero::Data::Binding> value) noexcept { targetObject_ = std::move(value); }

private:
    Ref<Aero::Data::Binding> targetObject_;
};
} // namespace Aero::Interactivity

// Pulling the concrete Blend actions here keeps TriggerAction.hpp as the
// interactivity action aggregate.
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/Conditions.hpp>
