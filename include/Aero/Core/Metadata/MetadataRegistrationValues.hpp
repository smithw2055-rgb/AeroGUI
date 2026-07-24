#pragma once

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/Metadata/MetadataValueRegistrationStore.hpp>

namespace Aero::Core {

// Explicit registration-domain view over value semantics and text converters.
//
// This service exists only while metadata is being registered or sealed. It is
// not a runtime reflection surface: sealed consumers must use MetadataRuntime
// and the owned ValueSemanticsFacet/TextConverterFacet instances instead.
//
// The view is intentionally non-owning so MetadataDomain transactions continue
// to replace their complete candidate storage atomically.
class MetadataRegistrationValues final {
public:
    explicit MetadataRegistrationValues(
        MetadataValueRegistrationStore& registrations) noexcept
        : registrations_(&registrations),
          mutableRegistrations_(&registrations) {}

    explicit MetadataRegistrationValues(
        const MetadataValueRegistrationStore& registrations) noexcept
        : registrations_(&registrations),
          mutableRegistrations_(nullptr) {}

    Base::Result<void> TryRegisterValueSemantics(
        TypeId type,
        const ValueTypeRegistration& registration) const noexcept {
        if (mutableRegistrations_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Metadata registration values are read-only");
        }
        return mutableRegistrations_->TryRegisterValueSemantics(
            type, registration);
    }

    Base::Result<void> TryRegisterTextConverter(
        const TextValueConverterRegistration& registration) const noexcept {
        if (mutableRegistrations_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Metadata registration values are read-only");
        }
        return mutableRegistrations_->TryRegisterTextConverter(registration);
    }

    Base::Result<Value> TryCreateValue(
        TypeId type,
        const void* source) const noexcept {
        const Base::Ref<ValueTypeSemantics>* semantics =
            FindValueSemantics(type);
        if (semantics == nullptr || !*semantics) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Value type semantics are not registered");
        }
        return Value::TryFromCustom(type, source, *semantics);
    }

    Base::Result<Value> TryConvertText(
        TypeId type,
        Base::StringView text) const noexcept {
        const TextValueConverterRegistration* converter =
            FindTextConverter(type);
        if (converter == nullptr || converter->convert == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Text value converter is not registered");
        }
        Base::Result<Value> converted = converter->convert(
            type, text, converter->context);
        if (!converted) return converted.GetStatus();
        if (converted.Value().IsUnset() || converted.Value().Type() != type) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Text converter returned an incompatible value");
        }
        return converted;
    }

    const Base::Ref<ValueTypeSemantics>* FindValueSemantics(
        TypeId type) const noexcept {
        AERO_ASSERT(registrations_ != nullptr);
        return registrations_->FindValueSemantics(type);
    }

    const TextValueConverterRegistration* FindTextConverter(
        TypeId type) const noexcept {
        AERO_ASSERT(registrations_ != nullptr);
        return registrations_->FindTextConverter(type);
    }

    bool IsFrozen() const noexcept {
        return registrations_ != nullptr && registrations_->IsFrozen();
    }

    const TypeRegistry& Types() const noexcept {
        AERO_ASSERT(registrations_ != nullptr);
        return registrations_->Types();
    }

private:
    const MetadataValueRegistrationStore* registrations_ = nullptr;
    MetadataValueRegistrationStore* mutableRegistrations_ = nullptr;
};

} // namespace Aero::Core
