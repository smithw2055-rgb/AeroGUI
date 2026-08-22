#include <Aero/Style.hpp>
#include <Aero/Triggers/MultiDataTrigger.hpp>

#include "gui/triggers/TriggerDiagnostics.hpp"

namespace Aero {

Base::Result<void> MultiDataTrigger::AddCondition(
    Base::Ref<Condition> condition) noexcept {
    if (!condition) {
        return InvalidStyle(
            "MultiDataTrigger condition is null");
    }
    return conditions_.PushBack(
        std::move(condition));
}

Base::Result<void> MultiDataTrigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) {
        return InvalidStyle(
            "MultiDataTrigger authored setter is null");
    }
    return authoredSetters_.PushBack(
        std::move(setter));
}

} // namespace Aero
