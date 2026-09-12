#pragma once

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
    explicit TriggerAction(Meta::TypeId runtimeType) noexcept
        : DependencyObject(runtimeType) {}
    ~TriggerAction() override = default;
};

} // namespace Aero::Interactivity

// Pulling the concrete Blend actions here keeps TriggerAction.hpp as the
// interactivity action aggregate.
#include <Aero/Interactivity/ChangePropertyAction.hpp>
#include <Aero/Interactivity/SetFocusAction.hpp>
#include <Aero/Interactivity/LaunchUriOrFileAction.hpp>
#include <Aero/Interactivity/RemoveElementAction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/Conditions.hpp>
