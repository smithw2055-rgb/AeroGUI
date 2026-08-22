#pragma once

#include <Aero/DependencyObject.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Value.hpp>

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

// Marker object used by the interactivity metadata layer. It remains a
// concrete public type so behavior XAML can register the same owner type as
// the original Gallery model.
class AERO_GUI_API Interaction : public Base::Object {
    AERO_DECLARE_TYPE(Interaction, Base::Object)
private:
    Interaction() noexcept = default;
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
