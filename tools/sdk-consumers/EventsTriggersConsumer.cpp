#include <Aero/Events/ApplicationEventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/Events/ControlEventArgs.hpp>
#include <Aero/Events/Event.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events.hpp>
#include <Aero/Events/NavigationEventArgs.hpp>
#include <Aero/Events/PropertyEventArgs.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Events/WindowEventArgs.hpp>

#include <Aero/Interactivity/ChangePropertyAction.hpp>
#include <Aero/Triggers/Conditions.hpp>
#include <Aero/Triggers/DataTrigger.hpp>
#include <Aero/Media/Animation/EventTrigger.hpp>
#include <Aero/Interactivity/LaunchUriOrFileAction.hpp>
#include <Aero/Triggers/MultiDataTrigger.hpp>
#include <Aero/Triggers/MultiTrigger.hpp>
#include <Aero/Interactivity/RemoveElementAction.hpp>
#include <Aero/Interactivity/SetFocusAction.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Triggers/Trigger.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Triggers/TriggerBase.hpp>
#include <Aero/Triggers/Triggers.hpp>

static_assert(sizeof(Aero::EventArgs) != 0U);
static_assert(sizeof(Aero::Trigger) != 0U);
static_assert(sizeof(Aero::Interactivity::TriggerAction) != 0U);
