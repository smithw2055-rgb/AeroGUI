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
        AERO_ASSERT(types_ != nullptr);
        return types_->TryCreateValue(type, source);
    }

    Base::Result<Value> TryConvertText(
        TypeId type,
        Base::StringView text) const noexcept {
        AERO_ASSERT(types_ != nullptr);
        return types_->TryConvertText(type, text);
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
