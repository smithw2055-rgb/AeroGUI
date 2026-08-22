#include <Aero/Style.hpp>
#include <Aero/Triggers/MultiTrigger.hpp>

#include "gui/triggers/TriggerDiagnostics.hpp"

namespace Aero {

Base::Result<void> MultiTrigger::AddCondition(
    Base::Ref<Condition> condition) noexcept {
    return condition ? conditions_.PushBack(std::move(condition))
        : Base::Result<void>(InvalidStyle("MultiTrigger condition is null"));
}

Base::Result<void> MultiTrigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    return setter ? authoredSetters_.PushBack(std::move(setter))
        : Base::Result<void>(InvalidStyle("MultiTrigger setter is null"));
}

} // namespace Aero
