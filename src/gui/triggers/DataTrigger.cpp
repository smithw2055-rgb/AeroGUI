#include <Aero/Style.hpp>
#include <Aero/Triggers/DataTrigger.hpp>

#include "gui/triggers/TriggerDiagnostics.hpp"

namespace Aero {

Base::Result<void> DataTrigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) {
        return InvalidStyle(
            "DataTrigger authored setter is null");
    }
    return authoredSetters_.PushBack(
        std::move(setter));
}

void DataTrigger::SetPropertyName(StringView value) noexcept {
    static_cast<void>(propertyName_.Assign(value));
}

void DataTrigger::SetSourceName(StringView value) noexcept {
    static_cast<void>(sourceName_.Assign(value));
}

} // namespace Aero
