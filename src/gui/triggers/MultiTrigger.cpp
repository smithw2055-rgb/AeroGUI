#include <Aero/Style.hpp>
#include <Aero/Triggers/MultiTrigger.hpp>

#include "gui/triggers/TriggerDiagnostics.hpp"

namespace Aero {

void MultiTrigger::AddCondition(
    Base::Ref<Condition> condition) noexcept {
    if (!condition) { AERO_ASSERT(false); return; }
    conditions_.PushBack(std::move(condition));
}

void MultiTrigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) { AERO_ASSERT(false); return; }
    authoredSetters_.PushBack(std::move(setter));
}

} // namespace Aero
