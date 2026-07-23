#pragma once

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/TypeRegistry.hpp>

namespace Aero::Core {

// Explicit registration-domain view over value semantics and text converters.
//
// This service exists only while metadata is being registered or sealed. It is
// not a runtime reflection surface: sealed consumers must use MetadataRuntime
// and the owned ValueSemanticsFacet/TextConverterFacet instances instead.
//
// The view is intentionally non-owning so MetadataDomain transactions continue
// to replace their complete candidate TypeRegistry atomically.
class MetadataRegistrationValues final {
public:
    explicit MetadataRegistrationValues(TypeRegistry& types) noexcept
        : types_(&types), mutableTypes_(&types) {}

    explicit MetadataRegistrationValues(const TypeRegistry& types) noexcept
        : types_(&types), mutableTypes_(nullptr) {}

    Base::Result<void> TryRegisterValueSemantics(
        TypeId type,
        const ValueTypeRegistration& registration) const noexcept {
        if (mutableTypes_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Metadata registration values are read-only");
        }
        return mutableTypes_->TryRegisterValueSemantics(type, registration);
    }

    Base::Result<void> TryRegisterTextConverter(
        const TextValueConverterRegistration& registration) const noexcept {
        if (mutableTypes_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Metadata registration values are read-only");
        }
        return mutableTypes_->TryRegisterTextConverter(registration);
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
        AERO_ASSERT(types_ != nullptr);
        return types_->FindValueSemantics(type);
    }

    const TextValueConverterRegistration* FindTextConverter(
        TypeId type) const noexcept {
        AERO_ASSERT(types_ != nullptr);
        return types_->FindTextConverter(type);
    }

    bool IsFrozen() const noexcept {
        return types_ != nullptr && types_->IsFrozen();
    }

    const TypeRegistry& Types() const noexcept {
        AERO_ASSERT(types_ != nullptr);
        return *types_;
    }

private:
    const TypeRegistry* types_ = nullptr;
    TypeRegistry* mutableTypes_ = nullptr;
};

} // namespace Aero::Core
