#pragma once

// Compiled trigger plan produced when a Style/ControlTemplate/DataTemplate is
// sealed. Mirrors the trigger-type organization under triggers/ (see
// C:\Projects\AeroGUI NsGui/Resources/). Internal engine type, not a public
// Aero::Triggers authoring type.

#include <Aero/Base/Object.hpp>
#include <Aero/Value.hpp>
#include <Aero/DependencyProperty.hpp>
#include "gui/data/BindingState.hpp"

namespace Aero {

struct StyleTriggerSetter {
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct TriggerPlan {
    DependencyPropertyHandle property;
    Base::Ref<Data::Binding> binding;
    PropertyValue value;
    bool IsBindingTrigger() const noexcept { return static_cast<bool>(binding); }
    Base::Vector<StyleTriggerSetter> setters;
    Base::Vector<Base::Ref<Base::Object>> enterActions;
    Base::Vector<Base::Ref<Base::Object>> exitActions;
};

} // namespace Aero
