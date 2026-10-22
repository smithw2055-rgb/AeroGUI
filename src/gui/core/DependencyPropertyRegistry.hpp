#pragma once

#include <Aero/Base/HashMap.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero {
class DependencyObject;
}

namespace Aero::Meta {
class TypeRegistry;

class IInheritanceChangeSink {
public:
    virtual ~IInheritanceChangeSink() = default;
    virtual void OnInheritancePropertyChanged(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept = 0;
    virtual void OnObjectDestroyed(
        DependencyObject&) noexcept {}
};

class RegistryLifetime final : public Base::Object {
public:
    bool isAlive = true;
};

class DependencyPropertyRegistry {
public:
    DependencyPropertyRegistry(
        TypeRegistry& typeRegistry,
        BehaviorTable& behaviors) noexcept;
    ~DependencyPropertyRegistry() noexcept {
        if (lifetime_) {
            lifetime_->isAlive = false;
        }
    }

    Base::Ref<RegistryLifetime> Lifetime() const noexcept { return lifetime_; }

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

    Span<const DependencyPropertyHandle> InheritableProperties() const noexcept {
        return {inheritableProperties_.Data(), inheritableProperties_.Size()};
    }

    void SetInheritanceChangeSink(IInheritanceChangeSink* sink) noexcept {
        inheritanceSink_ = sink;
    }

    void NotifyInheritancePropertyChanged(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        if (inheritanceSink_ != nullptr) {
            inheritanceSink_->OnInheritancePropertyChanged(object, property);
        }
    }

    void NotifyObjectDestroyed(
        DependencyObject& object) noexcept {
        if (inheritanceSink_ != nullptr) {
            inheritanceSink_->OnObjectDestroyed(object);
        }
    }

    const DependencyProperty* Find(
        DependencyPropertyHandle property) const noexcept;
    const DependencyProperty* Find(
        TypeId ownerType,
        StringView name) const noexcept;
    // P2.3: registration-time canonicalization fast path. Aliases inserted
    // by Register/AddOwner resolve to the registered-owner (canonical)
    // handle with a single memberIndex_ hash lookup, without loading the
    // full descriptor. Unknown handles fall back to the incoming value,
    // matching the legacy CanonicalPropertyKey contract.
    MemberId CanonicalHandle(
        DependencyPropertyHandle property) const noexcept;
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
    Base::Vector<DependencyPropertyHandle> inheritableProperties_;
    IInheritanceChangeSink* inheritanceSink_ = nullptr;
    static Base::Ref<RegistryLifetime> CreateLifetime() noexcept {
        auto res = Base::MakeRef<RegistryLifetime>();
        return res ? std::move(res).Value() : Base::Ref<RegistryLifetime>{};
    }
    Base::Ref<RegistryLifetime> lifetime_{CreateLifetime()};
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
        FrameworkPropertyMetadataOptions metadataFlags) noexcept;
};

} // namespace Aero::Meta
