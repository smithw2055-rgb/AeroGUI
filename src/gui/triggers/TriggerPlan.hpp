#pragma once

// Compiled trigger plan produced when a Style/ControlTemplate/DataTemplate is
// sealed. Mirrors the trigger-type organization under triggers/ (see
// C:\Projects\AeroGUI NsGui/Resources/). Internal engine type, not a public
// Aero authoring type.

#include <Aero/Base/Object.hpp>
#include <Aero/Value.hpp>
#include <Aero/DependencyProperty.hpp>
#include "gui/data/BindingEngine.hpp"

namespace Aero {

class DependencyObject;

struct StyleTriggerSetter {
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct TriggerBindingCondition {
    Base::Ref<Data::Binding> binding;
    PropertyValue value;
};

struct TriggerPlan {
    DependencyPropertyHandle property;
    Base::Ref<Data::Binding> binding;
    PropertyValue value;
    Base::Vector<TriggerBindingCondition> extraBindings;
    bool IsBindingTrigger() const noexcept {
        return static_cast<bool>(binding) || !extraBindings.Empty();
    }
    // Property-trigger condition eval lives on the plan; binding triggers use
    // recorded StyleApplication binding state instead.
    Base::Result<bool> IsConditionMet(
        const DependencyObject& object) const noexcept;
    Base::Vector<StyleTriggerSetter> setters;
    Base::Vector<Base::Ref<Base::Object>> enterActions;
    Base::Vector<Base::Ref<Base::Object>> exitActions;
};

} // namespace Aero
