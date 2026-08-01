#include "ValueTable.hpp"

#include <Aero/Base/Allocator.hpp>

#include <cstddef>
#include <utility>

namespace Aero::Core {
namespace {

Base::Status FrozenStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Metadata value registration store is frozen");
}

bool IsValueType(const TypeInfo& type) noexcept {
    return (static_cast<std::uint32_t>(type.Flags()) &
        static_cast<std::uint32_t>(TypeFlags::ValueType)) != 0U;
}

} // namespace

Base::Result<void> ValueTable::TryRegisterValueSemantics(
    TypeId type,
    const ValueTypeRegistration& registration) noexcept {
    if (frozen_) return FrozenStatus();
    const TypeInfo* info = types_ != nullptr ? types_->FindType(type) : nullptr;
    if (info == nullptr || !IsValueType(*info) ||
        registration.size == 0U || registration.alignment == 0U ||
        !Base::IsValidAlignment(registration.alignment) ||
        registration.equals == nullptr ||
        (registration.inlineSafe &&
            (registration.size > Value::InlineCapacity ||
             registration.alignment > alignof(std::max_align_t))) ||
        (!registration.inlineSafe && registration.copy == nullptr)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Value type semantics are invalid");
    }
    if (FindValueSemantics(type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Value type semantics are already registered");
    }
    Base::Result<Base::Ref<ValueTypeSemantics>> created =
        Base::MakeRef<ValueTypeSemantics>(registration);
    if (!created) return created.GetStatus();
    return valueSemantics_.TryPushBack({type, std::move(created).Value()});
}

Base::Result<void> ValueTable::TryRegisterTextConverter(
    const TextValueConverterRegistration& registration) noexcept {
    if (frozen_) return FrozenStatus();
    if (registration.type == InvalidTypeId || registration.convert == nullptr ||
        types_ == nullptr || types_->FindType(registration.type) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Text value converter registration is invalid");
    }
    if (FindTextConverter(registration.type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Text value converter is already registered");
    }
    return textConverters_.TryPushBack(registration);
}

Base::Result<void> ValueTable::Freeze() noexcept {
    if (frozen_) return {};
    if (types_ == nullptr || !types_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TypeRegistry must be frozen before value registrations");
    }
    frozen_ = true;
    return {};
}

const Base::Ref<ValueTypeSemantics>*
ValueTable::FindValueSemantics(TypeId type) const noexcept {
    for (const ValueSemanticsEntry& entry : valueSemantics_) {
        if (entry.type == type) return &entry.semantics;
    }
    return nullptr;
}

const TextValueConverterRegistration*
ValueTable::FindTextConverter(TypeId type) const noexcept {
    for (const TextValueConverterRegistration& entry : textConverters_) {
        if (entry.type == type) return &entry;
    }
    return nullptr;
}

} // namespace Aero::Core
