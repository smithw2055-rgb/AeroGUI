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

#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/EventTrigger.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>

static_assert(sizeof(Aero::EventArgs) != 0U);
static_assert(sizeof(Aero::Trigger) != 0U);
static_assert(sizeof(Aero::Interactivity::TriggerAction) != 0U);
