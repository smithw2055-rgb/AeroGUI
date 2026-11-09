#include "gui/core/TypeRegistryCore.hpp"

#include <Aero/Base/Assert.hpp>

#include <utility>

namespace Aero::Meta {
namespace {

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

} // namespace

RoutedEventTable::RoutedEventTable(
    TypeRegistry& types,
    BehaviorTable& behaviors) noexcept
    : types_(&types),
      behaviorRegistrations_(&behaviors),
      definitions_() {}

Base::Result<RoutedEventHandle> RoutedEventTable::Register(
    const RoutedEventRegistration& registration) noexcept {
    if (frozen_ || types_->IsFrozen()) {
        return InvalidState(
            "Routed events must be registered before TypeRegistry freeze");
    }
    if (registration.name.Empty() ||
        registration.ownerType == InvalidTypeId ||
        registration.eventArgsType == InvalidTypeId ||
        types_->FindType(registration.ownerType) == nullptr ||
        types_->FindType(registration.eventArgsType) == nullptr) {
        return InvalidArgument("Routed event registration is incomplete");
    }

    Definition definition;
    definition.ownerType = registration.ownerType;
    definition.eventArgsType = registration.eventArgsType;
    definition.strategy = registration.strategy;
    Base::Result<void> nameResult =
        definition.name.Assign(registration.name);
    if (!nameResult) return nameResult.GetStatus();
    definitions_.Reserve(definitions_.Size() + 1U);

    Base::Result<MemberId> member = RegistrationTypes(
        *types_, *behaviorRegistrations_).RegisterEvent(
            registration.ownerType,
            {registration.name, registration.eventArgsType,
             EventFlags::Routed});
    if (!member) return member.GetStatus();

    definition.handle.value = member.Value();
    const std::uint32_t index = definitions_.Size();
    definitions_.PushBack(std::move(definition));
    index_.Set(definitions_[index].handle.value, index);
    return definitions_[index].handle;
}

Base::Result<void> RoutedEventTable::Freeze() noexcept {
    if (frozen_) return {};
    if (!types_->IsFrozen()) {
        return InvalidState(
            "TypeRegistry must be frozen before routed event catalog");
    }
    frozen_ = true;
    return {};
}

const RoutedEventTable::Definition* RoutedEventTable::Find(
    RoutedEventHandle event) const noexcept {
    if (!event.IsValid()) return nullptr;
    if (const std::uint32_t* index = index_.Find(event.value)) {
        if (*index < definitions_.Size()) {
            return &definitions_[*index];
        }
    }
    return nullptr;
}

} // namespace Aero::Meta
