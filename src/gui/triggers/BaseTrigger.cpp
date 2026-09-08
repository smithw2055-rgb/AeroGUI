#include <Aero/Style.hpp>
#include <Aero/Triggers/TriggerBase.hpp>
#include <Aero/Value.hpp>

#include "gui/data/BindingEngine.hpp"
#include "gui/triggers/TriggerDiagnostics.hpp"

namespace Aero {

Base::Result<void> InvalidStyle(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

bool IsDeferredBindingSetterValue(
    const PropertyValue& value) noexcept {
    if (value.Kind() != ValueKind::Object ||
        value.IsNullObject()) {
        return false;
    }
    if (value.Type() == Data::Binding::StaticTypeId()) {
        return true;
    }
    return value.AsObject() &&
        value.AsObject()->RuntimeType() ==
            Data::Binding::StaticTypeId();
}

void TriggerBase::AddEnterAction(
    Base::Ref<Base::Object> action) noexcept {
    if (!action) { AERO_ASSERT(false); return; }
    enterActions_.PushBack(
        std::move(action));
}

void TriggerBase::AddExitAction(
    Base::Ref<Base::Object> action) noexcept {
    if (!action) { AERO_ASSERT(false); return; }
    exitActions_.PushBack(
        std::move(action));
}

} // namespace Aero
