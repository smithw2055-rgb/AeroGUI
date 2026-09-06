#include <Aero/Style.hpp>
#include <Aero/Triggers/MultiTrigger.hpp>

#include "gui/triggers/TriggerDiagnostics.hpp"

namespace Aero {

void MultiTrigger::AddCondition(
    Base::Ref<Condition> condition) noexcept {
    if (!condition) { AERO_ASSERT(false); return; }
    Base::Result<void> pushed = conditions_.PushBack(std::move(condition));
    if (!pushed) { AERO_ASSERT(false); return; }
}

void MultiTrigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) { AERO_ASSERT(false); return; }
    Base::Result<void> pushed =
        authoredSetters_.PushBack(std::move(setter));
    if (!pushed) { AERO_ASSERT(false); return; }
}

} // namespace Aero
