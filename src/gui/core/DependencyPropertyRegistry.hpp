#pragma once

#include <Aero/Base/HashMap.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Meta {
class TypeRegistry;

class DependencyPropertyRegistry {
public:
    DependencyPropertyRegistry(
        TypeRegistry& typeRegistry,
        BehaviorTable& behaviors) noexcept;

    DependencyPropertyRegistry(const DependencyPropertyRegistry&) = delete;
    DependencyPropertyRegistry& operator=(
        const DependencyPropertyRegistry&) = delete;
    DependencyPropertyRegistry(DependencyPropertyRegistry&&) = delete;
    DependencyPropertyRegistry& operator=(
        DependencyPropertyRegistry&&) = delete;

    Result<DependencyPropertyRegistrationResult>
    Register(
        const DependencyPropertyRegistration& registration) noexcept;

    Result<void> AddOwner(
        DependencyPropertyHandle property,
        TypeId ownerType,
        const PropertyMetadata& metadata,
        DependencyPropertyFlags flags = DependencyPropertyFlags::None) noexcept;

    Result<void> OverrideMetadata(
        DependencyPropertyHandle property,
        TypeId forType,
        const PropertyMetadata& metadata) noexcept;

    Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    std::uint32_t PropertyCount() const noexcept {
        return properties_.Size();
    }
    Span<const DependencyProperty>
    Properties() const noexcept {
        return {
            properties_.Data(),
            properties_.Size()};
    }
    const TypeRegistry& Types() const noexcept {
        return *typeRegistry_;
    }

    const DependencyProperty* Find(
        DependencyPropertyHandle property) const noexcept;
    const DependencyProperty* Find(
        TypeId ownerType,
        StringView name) const noexcept;
    Result<void> ValidateValueFor(
        DependencyPropertyHandle property,
        TypeId ownerType,
        const PropertyValue& value) const noexcept;

private:
    friend class ::Aero::Meta::Registry;
    friend class ::Aero::DependencyObject;

    TypeRegistry* typeRegistry_ = nullptr;
    BehaviorTable* behaviorRegistrations_ = nullptr;
    Base::Vector<DependencyProperty> properties_;
    Base::HashMap<MemberId, std::uint32_t> memberIndex_;
    std::uint64_t nextReadOnlySecret_ = 1U;
    bool frozen_ = false;

    Result<void> ValidateMetadata(
        TypeId valueType,
        DependencyPropertyFlags propertyFlags,
        const PropertyMetadata& metadata) const noexcept;
    Result<void> ValidateValue(
        const DependencyProperty& property,
        const PropertyMetadata& metadata,
        const PropertyValue& value) const noexcept;
    Result<PropertyValue> EvaluateValue(
        DependencyObject& object,
        const DependencyProperty& property,
        const PropertyMetadata& metadata,
        const PropertyValue& baseValue) const noexcept;
    bool ValidateKey(
        DependencyPropertyHandle property,
        const DependencyPropertyKey* key) const noexcept;
    std::uint32_t FindIndex(MemberId member) const noexcept;
    static PropertyFlags ToTypeRegistryFlags(
        DependencyPropertyFlags propertyFlags,
        PropertyMetadataFlags metadataFlags) noexcept;
};

} // namespace Aero::Meta
