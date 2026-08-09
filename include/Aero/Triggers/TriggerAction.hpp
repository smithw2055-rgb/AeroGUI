#pragma once

#include <Aero/Media/Animation.hpp>
#include <Aero/DependencyObject.hpp>

namespace Aero::Media::Animation {

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

} // namespace Aero::Media::Animation

// Pulling the concrete actions here keeps Animation.hpp as a core timeline
// header while preserving the convenient animation trigger aggregate.
#include <Aero/Triggers/ChangePropertyAction.hpp>
#include <Aero/Triggers/SetFocusAction.hpp>
#include <Aero/Triggers/LaunchUriOrFileAction.hpp>
#include <Aero/Triggers/RemoveElementAction.hpp>
#include <Aero/Triggers/StoryboardActions.hpp>
#include <Aero/Triggers/EventTrigger.hpp>
#include <Aero/Triggers/TimerTrigger.hpp>
#include <Aero/Triggers/Conditions.hpp>
#include <Aero/Triggers/StoryboardCompletedTrigger.hpp>
#include <Aero/Triggers/InteractionTriggers.hpp>
