#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Core/TypeRegistry.hpp>
#include <Aero/Core/Value.hpp>

#include <cstdint>

namespace Aero::Core {

class DependencyObject;
class DependencyProperty;
class DependencyPropertyRegistry;

struct DependencyPropertyHandle final {
    MemberId value = InvalidMemberId;

    AERO_NODISCARD constexpr bool IsValid() const noexcept {
        return value != InvalidMemberId;
    }
};

AERO_NODISCARD constexpr bool operator==(
    DependencyPropertyHandle left,
    DependencyPropertyHandle right) noexcept {
    return left.value == right.value;
}

AERO_NODISCARD constexpr bool operator!=(
    DependencyPropertyHandle left,
    DependencyPropertyHandle right) noexcept {
    return !(left == right);
}

using PropertyValueKind = ValueKind;
using PropertyValue = Value;

enum class DependencyPropertyFlags : std::uint32_t {
    None = 0U,
    Attached = 1U << 0U,
    ReadOnly = 1U << 1U
};

enum class PropertyMetadataFlags : std::uint32_t {
    None = 0U,
    Inherits = 1U << 0U,
    AffectsMeasure = 1U << 1U,
    AffectsArrange = 1U << 2U,
    AffectsRender = 1U << 3U,
    BindsTwoWayByDefault = 1U << 4U,
    AffectsParentMeasure = 1U << 5U,
    AffectsParentArrange = 1U << 6U
};

enum class UpdateSourceTrigger : std::uint8_t {
    Default = 0U,
    PropertyChanged,
    LostFocus,
    Explicit
};

AERO_NODISCARD constexpr DependencyPropertyFlags operator|(
    DependencyPropertyFlags left,
    DependencyPropertyFlags right) noexcept {
    return static_cast<DependencyPropertyFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

AERO_NODISCARD constexpr PropertyMetadataFlags operator|(
    PropertyMetadataFlags left,
    PropertyMetadataFlags right) noexcept {
    return static_cast<PropertyMetadataFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

AERO_NODISCARD constexpr bool HasFlag(
    DependencyPropertyFlags value,
    DependencyPropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

AERO_NODISCARD constexpr bool HasFlag(
    PropertyMetadataFlags value,
    PropertyMetadataFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

enum class EffectiveValueSource : std::uint8_t {
    Default = 0U,
    Local,
    Current
};

enum class PropertyInvalidationFlags : std::uint32_t {
    None = 0U,
    Measure = 1U << 0U,
    Arrange = 1U << 1U,
    Render = 1U << 2U,
    Inheritance = 1U << 3U,
    ParentMeasure = 1U << 4U,
    ParentArrange = 1U << 5U
};

AERO_NODISCARD constexpr PropertyInvalidationFlags operator|(
    PropertyInvalidationFlags left,
    PropertyInvalidationFlags right) noexcept {
    return static_cast<PropertyInvalidationFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

inline PropertyInvalidationFlags& operator|=(
    PropertyInvalidationFlags& left,
    PropertyInvalidationFlags right) noexcept {
    left = left | right;
    return left;
}

AERO_NODISCARD constexpr bool HasFlag(
    PropertyInvalidationFlags value,
    PropertyInvalidationFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

struct DependencyPropertyChangedEventArgs final {
    DependencyPropertyHandle property;
    const PropertyValue& oldValue;
    const PropertyValue& newValue;
    EffectiveValueSource oldSource = EffectiveValueSource::Default;
    EffectiveValueSource newSource = EffectiveValueSource::Default;
};

using ValidateValueCallback = bool (*)(
    const PropertyValue& value) noexcept;
using CoerceValueCallback = Base::Result<PropertyValue> (*)(
    DependencyObject& object,
    const DependencyProperty& property,
    const PropertyValue& baseValue) noexcept;
using PropertyChangedCallback = void (*)(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept;
using DependencyPropertyChangeHandler = void (*)(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args,
    void* context) noexcept;

struct DependencyPropertyChangeSubscription final {
    std::uint64_t value = 0U;

    AERO_NODISCARD constexpr bool IsValid() const noexcept {
        return value != 0U;
    }
};

struct PropertyMetadata final {
    PropertyValue defaultValue;
    PropertyMetadataFlags flags = PropertyMetadataFlags::None;
    UpdateSourceTrigger defaultUpdateSourceTrigger =
        UpdateSourceTrigger::Default;
    ValidateValueCallback validate = nullptr;
    CoerceValueCallback coerce = nullptr;
    PropertyChangedCallback changed = nullptr;
};

class AERO_API DependencyPropertyKey final {
public:
    DependencyPropertyKey() noexcept = default;

    AERO_NODISCARD bool IsValid() const noexcept {
        return registry_ != nullptr && property_.IsValid() && secret_ != 0U;
    }

    AERO_NODISCARD DependencyPropertyHandle Property() const noexcept {
        return property_;
    }

private:
    friend class DependencyPropertyRegistry;
    friend class DependencyObject;

    const DependencyPropertyRegistry* registry_ = nullptr;
    DependencyPropertyHandle property_;
    std::uint64_t secret_ = 0U;
};

struct DependencyPropertyRegistration final {
    Base::StringView name;
    TypeId ownerType = InvalidTypeId;
    TypeId valueType = InvalidTypeId;
    DependencyPropertyFlags flags = DependencyPropertyFlags::None;
    PropertyMetadata metadata;
};

struct DependencyPropertyRegistrationResult final {
    DependencyPropertyHandle property;
    DependencyPropertyKey readOnlyKey;
};

class AERO_API DependencyProperty final {
public:
    DependencyProperty(DependencyProperty&&) noexcept = default;
    DependencyProperty& operator=(DependencyProperty&&) noexcept = default;

    DependencyProperty(const DependencyProperty&) = delete;
    DependencyProperty& operator=(const DependencyProperty&) = delete;

    AERO_NODISCARD DependencyPropertyHandle Handle() const noexcept {
        return handle_;
    }
    AERO_NODISCARD Base::StringView Name() const noexcept {
        return name_.View();
    }
    AERO_NODISCARD TypeId ValueType() const noexcept { return valueType_; }
    AERO_NODISCARD TypeId RegisteredOwnerType() const noexcept {
        return registeredOwnerType_;
    }
    AERO_NODISCARD DependencyPropertyFlags Flags() const noexcept {
        return flags_;
    }
    AERO_NODISCARD bool IsReadOnly() const noexcept {
        return HasFlag(flags_, DependencyPropertyFlags::ReadOnly);
    }
    AERO_NODISCARD bool IsAttached() const noexcept {
        return HasFlag(flags_, DependencyPropertyFlags::Attached);
    }
    AERO_NODISCARD std::uint32_t MetadataCount() const noexcept {
        return metadata_.Size();
    }

    // Metadata addresses become stable when the owning registry is frozen.
    AERO_NODISCARD const PropertyMetadata* MetadataFor(
        TypeId forType) const noexcept;

private:
    friend class DependencyPropertyRegistry;
    friend class DependencyObject;

    struct MetadataEntry final {
        TypeId forType = InvalidTypeId;
        bool owner = false;
        PropertyMetadata metadata;
    };

    explicit DependencyProperty(Base::IAllocator* allocator) noexcept
        : name_(allocator), metadata_(allocator) {}

    AERO_NODISCARD const MetadataEntry* FindMetadataExact(
        TypeId forType) const noexcept;

    TypeRegistry* typeRegistry_ = nullptr;
    DependencyPropertyHandle handle_;
    TypeId valueType_ = InvalidTypeId;
    TypeId registeredOwnerType_ = InvalidTypeId;
    DependencyPropertyFlags flags_ = DependencyPropertyFlags::None;
    std::uint64_t readOnlySecret_ = 0U;
    Base::String name_;
    Base::Vector<MetadataEntry> metadata_;
};

class AERO_API DependencyPropertyRegistry final {
public:
    explicit DependencyPropertyRegistry(
        TypeRegistry& typeRegistry,
        Base::IAllocator* allocator = nullptr) noexcept;

    DependencyPropertyRegistry(const DependencyPropertyRegistry&) = delete;
    DependencyPropertyRegistry& operator=(
        const DependencyPropertyRegistry&) = delete;
    DependencyPropertyRegistry(DependencyPropertyRegistry&&) = delete;
    DependencyPropertyRegistry& operator=(
        DependencyPropertyRegistry&&) = delete;

    AERO_NODISCARD Base::Result<DependencyPropertyRegistrationResult>
    TryRegister(
        const DependencyPropertyRegistration& registration) noexcept;

    AERO_NODISCARD Base::Result<void> TryAddOwner(
        DependencyPropertyHandle property,
        TypeId ownerType,
        const PropertyMetadata& metadata) noexcept;

    // Override metadata is a complete replacement in the first runtime slice.
    AERO_NODISCARD Base::Result<void> TryOverrideMetadata(
        DependencyPropertyHandle property,
        TypeId forType,
        const PropertyMetadata& metadata) noexcept;

    AERO_NODISCARD Base::Result<void> Freeze() noexcept;

    AERO_NODISCARD bool IsFrozen() const noexcept { return frozen_; }
    AERO_NODISCARD std::uint32_t PropertyCount() const noexcept {
        return properties_.Size();
    }
    AERO_NODISCARD Base::IAllocator& Allocator() const noexcept {
        return *allocator_;
    }
    AERO_NODISCARD TypeRegistry& Types() const noexcept {
        return *typeRegistry_;
    }

    // Returned addresses are stable after Freeze().
    AERO_NODISCARD const DependencyProperty* Find(
        DependencyPropertyHandle property) const noexcept;
    AERO_NODISCARD const DependencyProperty* Find(
        TypeId ownerType,
        Base::StringView name) const noexcept;
    // Validates a provider value without mutating an object. Style/template
    // sealing uses this to reject invalid setter plans before frame execution.
    AERO_NODISCARD Base::Result<void> ValidateValueFor(
        DependencyPropertyHandle property,
        TypeId ownerType,
        const PropertyValue& value) const noexcept;

private:
    friend class DependencyObject;

    TypeRegistry* typeRegistry_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<DependencyProperty> properties_;
    Base::HashMap<MemberId, std::uint32_t> memberIndex_;
    std::uint64_t nextReadOnlySecret_ = 1U;
    bool frozen_ = false;

    AERO_NODISCARD Base::Result<void> ValidateMetadata(
        TypeId valueType,
        const PropertyMetadata& metadata) const noexcept;
    AERO_NODISCARD Base::Result<void> ValidateValue(
        const DependencyProperty& property,
        const PropertyMetadata& metadata,
        const PropertyValue& value) const noexcept;
    AERO_NODISCARD Base::Result<PropertyValue> EvaluateValue(
        DependencyObject& object,
        const DependencyProperty& property,
        const PropertyMetadata& metadata,
        const PropertyValue& baseValue) const noexcept;
    AERO_NODISCARD bool ValidateKey(
        DependencyPropertyHandle property,
        const DependencyPropertyKey* key) const noexcept;
    AERO_NODISCARD std::uint32_t FindIndex(MemberId member) const noexcept;
    AERO_NODISCARD static PropertyFlags ToTypeRegistryFlags(
        DependencyPropertyFlags propertyFlags,
        PropertyMetadataFlags metadataFlags) noexcept;
};

class AERO_API DependencyObject : public DispatcherObject {
public:
    AERO_NODISCARD TypeId RuntimeType() const noexcept { return runtimeType_; }
    AERO_NODISCARD DependencyPropertyRegistry& PropertyRegistry() const noexcept {
        return *registry_;
    }

    AERO_NODISCARD Base::Result<PropertyValue> GetValue(
        DependencyPropertyHandle property) const noexcept;
    AERO_NODISCARD Base::Result<PropertyValue> ReadLocalValue(
        DependencyPropertyHandle property) const noexcept;
    AERO_NODISCARD Base::Result<EffectiveValueSource> GetValueSource(
        DependencyPropertyHandle property) const noexcept;

    AERO_NODISCARD Base::Result<void> SetValue(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    AERO_NODISCARD Base::Result<void> SetValue(
        const DependencyPropertyKey& key,
        const PropertyValue& value) noexcept;

    AERO_NODISCARD Base::Result<void> SetCurrentValue(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    AERO_NODISCARD Base::Result<void> SetCurrentValue(
        const DependencyPropertyKey& key,
        const PropertyValue& value) noexcept;

    AERO_NODISCARD Base::Result<void> ClearValue(
        DependencyPropertyHandle property) noexcept;
    AERO_NODISCARD Base::Result<void> ClearValue(
        const DependencyPropertyKey& key) noexcept;

    AERO_NODISCARD Base::Result<void> CoerceValue(
        DependencyPropertyHandle property) noexcept;

    // Listeners execute after the effective value has committed and after the
    // property's metadata callback. They are intended to queue later work,
    // not to synchronously mutate the same property.
    AERO_NODISCARD Base::Result<DependencyPropertyChangeSubscription>
    AddValueChangedHandler(
        DependencyPropertyHandle property,
        DependencyPropertyChangeHandler handler,
        void* context = nullptr) noexcept;
    AERO_NODISCARD Base::Result<bool> RemoveValueChangedHandler(
        DependencyPropertyChangeSubscription subscription) noexcept;

    AERO_NODISCARD PropertyInvalidationFlags PendingInvalidations() const noexcept {
        return invalidations_;
    }
    AERO_NODISCARD Base::Result<PropertyInvalidationFlags>
    TakeInvalidations() noexcept;
    AERO_NODISCARD std::uint32_t StoredValueCount() const noexcept {
        return values_.Size();
    }

protected:
    DependencyObject(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& registry,
        TypeId runtimeType,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~DependencyObject() override = default;
    AERO_NODISCARD virtual Base::Result<void> OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept;

private:
    enum class ChangeKind : std::uint8_t {
        SetLocal,
        SetCurrent,
        Clear,
        ReCoerce
    };

    struct EffectiveValueEntry final {
        DependencyPropertyHandle property;
        PropertyValue localValue;
        PropertyValue currentValue;
        PropertyValue effectiveValue;
        EffectiveValueSource source = EffectiveValueSource::Default;
        bool hasLocal = false;
        bool hasCurrent = false;
    };

    struct ChangeHandlerRecord final {
        DependencyPropertyChangeSubscription subscription;
        DependencyPropertyHandle property;
        DependencyPropertyChangeHandler handler = nullptr;
        void* context = nullptr;
        bool active = false;
    };

    class MutationScope final {
    public:
        MutationScope() noexcept = default;
        MutationScope(MutationScope&& other) noexcept;
        MutationScope& operator=(MutationScope&& other) noexcept;
        ~MutationScope();

        MutationScope(const MutationScope&) = delete;
        MutationScope& operator=(const MutationScope&) = delete;

        void Release() noexcept;

    private:
        friend class DependencyObject;

        MutationScope(
            DependencyObject* owner,
            DispatcherReentrancyGuard&& guard) noexcept;

        DependencyObject* owner_ = nullptr;
        DispatcherReentrancyGuard dispatcherGuard_;
    };

    DependencyPropertyRegistry* registry_ = nullptr;
    TypeId runtimeType_ = InvalidTypeId;
    Base::Vector<EffectiveValueEntry> values_;
    Base::Vector<MemberId> updateStack_;
    Base::Vector<ChangeHandlerRecord> changeHandlers_;
    PropertyInvalidationFlags invalidations_ = PropertyInvalidationFlags::None;
    std::uint64_t nextChangeHandler_ = 1U;
    std::uint32_t changeHandlerNotificationDepth_ = 0U;

    AERO_NODISCARD Base::Result<void> VerifyReady() const noexcept;
    AERO_NODISCARD std::uint32_t FindEntryIndex(
        DependencyPropertyHandle property) const noexcept;
    AERO_NODISCARD Base::Result<MutationScope> BeginMutation(
        DependencyPropertyHandle property) noexcept;
    void LeaveMutation() noexcept;

    AERO_NODISCARD Base::Result<void> ApplyChange(
        DependencyPropertyHandle property,
        const DependencyPropertyKey* key,
        ChangeKind kind,
        const PropertyValue* value) noexcept;

    void RemoveEntry(std::uint32_t index) noexcept;
    void RemoveChangeHandler(std::uint32_t index) noexcept;
    void NotifyValueChanged(
        const DependencyPropertyChangedEventArgs& args) noexcept;
    AERO_NODISCARD PropertyInvalidationFlags AccumulateInvalidations(
        PropertyMetadataFlags metadataFlags) noexcept;
};

} // namespace Aero::Core
