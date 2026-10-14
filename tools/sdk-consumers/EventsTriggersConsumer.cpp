#include <Aero/Events.hpp>
#include <Aero/RoutedEvent.hpp>

#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>

static_assert(sizeof(Aero::EventArgs) != 0U);
static_assert(sizeof(Aero::Trigger) != 0U);
static_assert(sizeof(Aero::Interactivity::TriggerAction) != 0U);
