#include <Aero/Style.hpp>
#include <Aero/Triggers/DataTrigger.hpp>

#include "gui/triggers/TriggerDiagnostics.hpp"

namespace Aero {

void DataTrigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) { AERO_ASSERT(false); return; }
    authoredSetters_.PushBack(
        std::move(setter));
}

void DataTrigger::SetPropertyName(StringView value) noexcept {
    static_cast<void>(propertyName_.Assign(value));
}

void DataTrigger::SetSourceName(StringView value) noexcept {
    static_cast<void>(sourceName_.Assign(value));
}

} // namespace Aero
