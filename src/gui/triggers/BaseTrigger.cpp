#include <Aero/Style.hpp>
#include <Aero/Triggers/TriggerBase.hpp>
#include <Aero/Value.hpp>

#include "gui/data/BindingState.hpp"
#include "gui/triggers/TriggerDiagnostics.hpp"

namespace Aero {

Base::Result<void> InvalidStyle(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

bool IsDeferredBindingSetterValue(
    const PropertyValue& value) noexcept {
    return value.Kind() == ValueKind::Object &&
        !value.IsNullObject() &&
        value.Type() == Data::Binding::StaticTypeId();
}

Base::Result<void> TriggerBase::AddEnterAction(
    Base::Ref<Base::Object> action) noexcept {
    if (!action) {
        return InvalidStyle(
            "Trigger enter action is null");
    }
    return enterActions_.PushBack(
        std::move(action));
}

Base::Result<void> TriggerBase::AddExitAction(
    Base::Ref<Base::Object> action) noexcept {
    if (!action) {
        return InvalidStyle(
            "Trigger exit action is null");
    }
    return exitActions_.PushBack(
        std::move(action));
}

} // namespace Aero
