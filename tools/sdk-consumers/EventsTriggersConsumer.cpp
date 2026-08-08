#include <Aero/Events/ApplicationEventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/Events/ControlEventArgs.hpp>
#include <Aero/Events/Event.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events.hpp>
#include <Aero/Events/NavigationEventArgs.hpp>
#include <Aero/Events/PropertyEventArgs.hpp>
#include <Aero/Gui/RoutedEvent.hpp>
#include <Aero/Events/WindowEventArgs.hpp>

#include <Aero/Triggers/ChangePropertyAction.hpp>
#include <Aero/Triggers/Conditions.hpp>
#include <Aero/Triggers/DataTrigger.hpp>
#include <Aero/Triggers/EventTrigger.hpp>
#include <Aero/Triggers/LaunchUriOrFileAction.hpp>
#include <Aero/Triggers/MultiDataTrigger.hpp>
#include <Aero/Triggers/MultiTrigger.hpp>
#include <Aero/Triggers/RemoveElementAction.hpp>
#include <Aero/Triggers/SetFocusAction.hpp>
#include <Aero/Triggers/StoryboardActions.hpp>
#include <Aero/Triggers/StoryboardCompletedTrigger.hpp>
#include <Aero/Triggers/TimerTrigger.hpp>
#include <Aero/Triggers/Trigger.hpp>
#include <Aero/Triggers/TriggerAction.hpp>
#include <Aero/Triggers/TriggerBase.hpp>
#include <Aero/Triggers/Triggers.hpp>

static_assert(sizeof(Aero::EventArgs) != 0U);
static_assert(sizeof(Aero::Trigger) != 0U);
static_assert(sizeof(Aero::Media::Animation::TriggerAction) != 0U);
