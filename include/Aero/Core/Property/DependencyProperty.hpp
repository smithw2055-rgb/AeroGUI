#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Core/Metadata/TypeRegistry.hpp>
#include <Aero/Core/Metadata/ValueCodec.hpp>
#include <Aero/Core/Metadata/Value.hpp>

#include <cstdint>

namespace Aero::Core {

class DependencyObject;
class DependencyProperty;
class MetadataBehaviorRegistrationStore;
class DependencyPropertyRegistry;
class MetadataContext;

struct DependencyPropertyHandle final {
    MemberId value = InvalidMemberId;

    constexpr bool IsValid() const noexcept {
        return value != InvalidMemberId;
    }
};

constexpr DependencyPropertyHandle MakeDependencyPropertyHandle(
    TypeId ownerType,
    Base::StringView name) noexcept;

template<class TOwner, class TValue>
class DependencyPropertyRef {
public:
    using Owner = TOwner;
    using ValueType = TValue;

    constexpr explicit DependencyPropertyRef(
        Base::StringView name) noexcept
        : name_(name),
          handle_(MakeDependencyPropertyHandle(
              TOwner::StaticTypeIdValue_, name)) {}

    constexpr Base::StringView Name() const noexcept {
        return name_;
    }
    constexpr DependencyPropertyHandle Handle() const noexcept {
        return handle_;
    }
    constexpr operator DependencyPropertyHandle() const noexcept {
        return handle_;
    }
    constexpr MemberId Id() const noexcept {
        return handle_.value;
    }

private:
    Base::StringView name_;
    DependencyPropertyHandle handle_;
};

template<class TOwner, class TValue>
class AttachedPropertyRef final
    : public DependencyPropertyRef<TOwner, TValue> {
public:
    using DependencyPropertyRef<TOwner, TValue>::DependencyPropertyRef;
};

template<class TOwner, class TValue>
class ReadOnlyPropertyRef final {
public:
    using Owner = TOwner;
    using ValueType = TValue;

    constexpr explicit ReadOnlyPropertyRef(
        Base::StringView name) noexcept
        : name_(name),
          handle_(MakeDependencyPropertyHandle(
              TOwner::StaticTypeIdValue_, name)) {}

    constexpr Base::StringView Name() const noexcept {
        return name_;
    }
    constexpr DependencyPropertyHandle Handle() const noexcept {
        return handle_;
    }
    constexpr MemberId Id() const noexcept {
        return handle_.value;
    }

private:
    Base::StringView name_;
    DependencyPropertyHandle handle_;
};

template<class T>
using PropertyAccess = T;

constexpr DependencyPropertyHandle MakeDependencyPropertyHandle(
    TypeId ownerType,
    Base::StringView name) noexcept {
    return {MakeMemberId(ownerType, MemberKind::Property, name)};
}

constexpr bool operator==(
    DependencyPropertyHandle left,
    DependencyPropertyHandle right) noexcept {
    return left.value == right.value;
}

template<class TOwner, class TValue>
constexpr bool operator==(
    DependencyPropertyHandle left,
    const ReadOnlyPropertyRef<TOwner, TValue>& right) noexcept {
    return left == right.Handle();
}

template<class TOwner, class TValue>
constexpr bool operator==(
    const ReadOnlyPropertyRef<TOwner, TValue>& left,
    DependencyPropertyHandle right) noexcept {
    return left.Handle() == right;
}

constexpr bool operator!=(
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

constexpr DependencyPropertyFlags operator|(
    DependencyPropertyFlags left,
    DependencyPropertyFlags right) noexcept {
    return static_cast<DependencyPropertyFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

constexpr PropertyMetadataFlags operator|(
    PropertyMetadataFlags left,
    PropertyMetadataFlags right) noexcept {
    return static_cast<PropertyMetadataFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

constexpr bool HasFlag(
    DependencyPropertyFlags value,
    DependencyPropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

constexpr bool HasFlag(
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

constexpr PropertyInvalidationFlags operator|(
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

constexpr bool HasFlag(
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

using ValidateValueCallback = Base::Delegate<bool(
    const PropertyValue& value)>;
using CoerceValueCallback = Base::Delegate<Base::Result<PropertyValue>(
    DependencyObject& object,
    const DependencyProperty& property,
    const PropertyValue& baseValue)>;
using PropertyChangedCallback = Base::Delegate<void(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args)>;
using DependencyPropertyChangedEventHandler = Base::Delegate<void(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args)>;

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

    bool IsValid() const noexcept {
        return registry_ != nullptr && property_.IsValid() && secret_ != 0U;
    }

    DependencyPropertyHandle Property() const noexcept {
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

    DependencyPropertyHandle Handle() const noexcept {
        return handle_;
    }
    Base::StringView Name() const noexcept {
        return name_.View();
    }
    TypeId ValueType() const noexcept { return valueType_; }
    TypeId RegisteredOwnerType() const noexcept {
        return registeredOwnerType_;
    }
    DependencyPropertyFlags Flags() const noexcept {
        return flags_;
    }
    bool IsReadOnly() const noexcept {
        return HasFlag(flags_, DependencyPropertyFlags::ReadOnly);
    }
    bool IsAttached() const noexcept {
        return HasFlag(flags_, DependencyPropertyFlags::Attached);
    }
    std::uint32_t MetadataCount() const noexcept {
        return metadata_.Size();
    }

    // Metadata addresses become stable when the owning registry is frozen.
    const PropertyMetadata* MetadataFor(
        TypeId forType) const noexcept;

private:
    friend class DependencyPropertyRegistry;
    friend class DependencyObject;

    struct MetadataEntry final {
        TypeId forType = InvalidTypeId;
        bool owner = false;
        PropertyMetadata metadata;
    };

    DependencyProperty() noexcept : name_(), metadata_() {}

    const MetadataEntry* FindMetadataExact(
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
    DependencyPropertyRegistry(
        TypeRegistry& typeRegistry,
        MetadataBehaviorRegistrationStore& behaviors) noexcept;

    DependencyPropertyRegistry(const DependencyPropertyRegistry&) = delete;
    DependencyPropertyRegistry& operator=(
        const DependencyPropertyRegistry&) = delete;
    DependencyPropertyRegistry(DependencyPropertyRegistry&&) = delete;
    DependencyPropertyRegistry& operator=(
        DependencyPropertyRegistry&&) = delete;

    Base::Result<DependencyPropertyRegistrationResult>
    TryRegister(
        const DependencyPropertyRegistration& registration) noexcept;

    Base::Result<void> TryAddOwner(
        DependencyPropertyHandle property,
        TypeId ownerType,
        const PropertyMetadata& metadata) noexcept;

    // Override metadata is a complete replacement in the first runtime slice.
    Base::Result<void> TryOverrideMetadata(
        DependencyPropertyHandle property,
        TypeId forType,
        const PropertyMetadata& metadata) noexcept;

    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    std::uint32_t PropertyCount() const noexcept {
        return properties_.Size();
    }
    Base::Span<const DependencyProperty>
    Properties() const noexcept {
        return {
            properties_.Data(),
            properties_.Size()};
    }
    const TypeRegistry& Types() const noexcept {
        return *typeRegistry_;
    }

    // Returned addresses are stable after Freeze().
    const DependencyProperty* Find(
        DependencyPropertyHandle property) const noexcept;
    const DependencyProperty* Find(
        TypeId ownerType,
        Base::StringView name) const noexcept;
    // Validates a provider value without mutating an object. Style/template
    // sealing uses this to reject invalid setter plans before frame execution.
    Base::Result<void> ValidateValueFor(
        DependencyPropertyHandle property,
        TypeId ownerType,
        const PropertyValue& value) const noexcept;

private:
    friend class DependencyObject;

    TypeRegistry* typeRegistry_ = nullptr;
    MetadataBehaviorRegistrationStore* behaviorRegistrations_ = nullptr;
    Base::Vector<DependencyProperty> properties_;
    Base::HashMap<MemberId, std::uint32_t> memberIndex_;
    std::uint64_t nextReadOnlySecret_ = 1U;
    bool frozen_ = false;

    Base::Result<void> ValidateMetadata(
        TypeId valueType,
        const PropertyMetadata& metadata) const noexcept;
    Base::Result<void> ValidateValue(
        const DependencyProperty& property,
        const PropertyMetadata& metadata,
        const PropertyValue& value) const noexcept;
    Base::Result<PropertyValue> EvaluateValue(
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

class AERO_API DependencyObject : public DispatcherObject {
    AERO_DECLARE_TYPE(DependencyObject, Base::Object)
public:
    TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    DependencyPropertyRegistry& PropertyRegistry() const noexcept {
        return *registry_;
    }

    Base::Result<PropertyValue> GetValue(
        DependencyPropertyHandle property) const noexcept;
    template<class TOwner, class TValue>
    Base::Result<PropertyAccess<TValue>> GetValue(
        const DependencyPropertyRef<TOwner, TValue>& property) const noexcept;
    template<class TOwner>
    Base::Result<Base::StringView> GetValue(
        const DependencyPropertyRef<TOwner, Base::String>&
            property) const noexcept;
    template<class TOwner, class TValue>
    Base::Result<PropertyAccess<TValue>> GetValue(
        const AttachedPropertyRef<TOwner, TValue>& property) const noexcept;
    template<class TOwner, class TValue>
    Base::Result<PropertyAccess<TValue>> GetValue(
        const ReadOnlyPropertyRef<TOwner, TValue>& property) const noexcept;
    template<class TOwner, class TValue>
    TValue GetValueOr(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const TValue& fallback) const noexcept;
    template<class TOwner>
    Base::StringView GetValueOr(
        const DependencyPropertyRef<TOwner, Base::String>& property,
        Base::StringView fallback) const noexcept;
    template<class TOwner, class TValue>
    TValue GetValueOr(
        const AttachedPropertyRef<TOwner, TValue>& property,
        const TValue& fallback) const noexcept;
    template<class TOwner, class TValue>
    TValue GetValueOr(
        const ReadOnlyPropertyRef<TOwner, TValue>& property,
        const TValue& fallback) const noexcept;
    Base::Result<PropertyValue> ReadLocalValue(
        DependencyPropertyHandle property) const noexcept;
    Base::Result<EffectiveValueSource> GetValueSource(
        DependencyPropertyHandle property) const noexcept;

    Base::Result<void> SetValue(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    template<class TOwner, class TValue>
    Base::Result<void> SetValue(
        const DependencyPropertyRef<TOwner, TValue>& property,
        PropertyAccess<TValue> value) noexcept;
    template<class TOwner>
    Base::Result<void> SetValue(
        const DependencyPropertyRef<TOwner, Base::String>& property,
        Base::StringView value) noexcept;
    template<class TOwner, class TValue>
    Base::Result<void> SetValue(
        const AttachedPropertyRef<TOwner, TValue>& property,
        PropertyAccess<TValue> value) noexcept;
    Base::Result<void> SetValue(
        const DependencyPropertyKey& key,
        const PropertyValue& value) noexcept;

    Base::Result<void> SetCurrentValue(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    template<class TOwner, class TValue>
    Base::Result<void> SetCurrentValue(
        const DependencyPropertyRef<TOwner, TValue>& property,
        PropertyAccess<TValue> value) noexcept;
    Base::Result<void> SetCurrentValue(
        const DependencyPropertyKey& key,
        const PropertyValue& value) noexcept;

    Base::Result<void> ClearValue(
        DependencyPropertyHandle property) noexcept;
    Base::Result<void> ClearValue(
        const DependencyPropertyKey& key) noexcept;

    Base::Result<void> CoerceValue(
        DependencyPropertyHandle property) noexcept;

    // Listeners execute after the effective value has committed and after the
    // property's metadata callback. They are intended to queue later work,
    // not to synchronously mutate the same property.
    Base::Result<void> TryAddValueChangedHandler(
        DependencyPropertyHandle property,
        const DependencyPropertyChangedEventHandler& handler) noexcept;
    template<class TOwner, class TValue>
    Base::Result<void> TryAddValueChangedHandler(
        const ReadOnlyPropertyRef<TOwner, TValue>& property,
        const DependencyPropertyChangedEventHandler& handler) noexcept {
        return TryAddValueChangedHandler(
            property.Handle(), handler);
    }
    Base::Result<bool> RemoveValueChangedHandler(
        DependencyPropertyHandle property,
        const DependencyPropertyChangedEventHandler& handler) noexcept;
    template<class TOwner, class TValue>
    Base::Result<bool> RemoveValueChangedHandler(
        const ReadOnlyPropertyRef<TOwner, TValue>& property,
        const DependencyPropertyChangedEventHandler& handler) noexcept {
        return RemoveValueChangedHandler(
            property.Handle(), handler);
    }

    PropertyInvalidationFlags PendingInvalidations() const noexcept {
        return invalidations_;
    }
    Base::Result<PropertyInvalidationFlags>
    TakeInvalidations() noexcept;
    std::uint32_t StoredValueCount() const noexcept {
        return values_.Size();
    }

protected:
    explicit DependencyObject(TypeId runtimeType) noexcept;
    ~DependencyObject() override = default;
    // Framework-owned state properties use this path so public SetValue calls
    // remain read-only while derived runtime types can publish state changes.
    Base::Result<void> SetReadOnlyCurrentValue(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    template<class TOwner, class TValue>
    Base::Result<void> SetReadOnlyCurrentValue(
        const ReadOnlyPropertyRef<TOwner, TValue>& property,
        PropertyAccess<TValue> value) noexcept;
    virtual Base::Result<void> OnPropertyInvalidated(
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
        DependencyPropertyHandle property;
        DependencyPropertyChangedEventHandler handler;
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
    bool objectServicesAvailable_ = false;
    Base::Vector<EffectiveValueEntry> values_;
    Base::Vector<MemberId> updateStack_;
    Base::Vector<ChangeHandlerRecord> changeHandlers_;
    PropertyInvalidationFlags invalidations_ = PropertyInvalidationFlags::None;
    std::uint32_t changeHandlerNotificationDepth_ = 0U;

    Base::Result<void> VerifyReady() const noexcept;
    std::uint32_t FindEntryIndex(
        DependencyPropertyHandle property) const noexcept;
    Base::Result<MutationScope> BeginMutation(
        DependencyPropertyHandle property) noexcept;
    void LeaveMutation() noexcept;

    Base::Result<void> ApplyChange(
        DependencyPropertyHandle property,
        const DependencyPropertyKey* key,
        ChangeKind kind,
        const PropertyValue* value) noexcept;

    void RemoveEntry(std::uint32_t index) noexcept;
    void RemoveChangeHandler(std::uint32_t index) noexcept;
    void NotifyValueChanged(
        const DependencyPropertyChangedEventArgs& args) noexcept;
    PropertyInvalidationFlags AccumulateInvalidations(
        PropertyMetadataFlags metadataFlags) noexcept;
};

template<class TOwner, class TValue>
Base::Result<PropertyAccess<TValue>> DependencyObject::GetValue(
    const DependencyPropertyRef<TOwner, TValue>& property) const noexcept {
    Base::Result<PropertyValue> stored = GetValue(property.Handle());
    if (!stored) return stored.GetStatus();
    return ValueCodec<TValue>::Decode(stored.Value());
}

template<class TOwner>
Base::Result<Base::StringView> DependencyObject::GetValue(
    const DependencyPropertyRef<TOwner, Base::String>&
        property) const noexcept {
    Base::Result<PropertyValue> stored = GetValue(property.Handle());
    if (!stored) return stored.GetStatus();
    if (stored.Value().Type() != TypeOf<Base::String>() ||
        stored.Value().Kind() != ValueKind::String) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "String dependency property value is incompatible");
    }
    return stored.Value().AsString();
}

template<class TOwner, class TValue>
Base::Result<PropertyAccess<TValue>> DependencyObject::GetValue(
    const AttachedPropertyRef<TOwner, TValue>& property) const noexcept {
    return GetValue(
        static_cast<const DependencyPropertyRef<TOwner, TValue>&>(
            property));
}

template<class TOwner, class TValue>
Base::Result<PropertyAccess<TValue>> DependencyObject::GetValue(
    const ReadOnlyPropertyRef<TOwner, TValue>& property) const noexcept {
    Base::Result<PropertyValue> stored =
        GetValue(property.Handle());
    if (!stored) return stored.GetStatus();
    return ValueCodec<TValue>::Decode(stored.Value());
}

template<class TOwner, class TValue>
TValue DependencyObject::GetValueOr(
    const DependencyPropertyRef<TOwner, TValue>& property,
    const TValue& fallback) const noexcept {
    Base::Result<PropertyAccess<TValue>> value = GetValue(property);
    return value ? std::move(value).Value() : fallback;
}

template<class TOwner>
Base::StringView DependencyObject::GetValueOr(
    const DependencyPropertyRef<TOwner, Base::String>& property,
    Base::StringView fallback) const noexcept {
    Base::Result<Base::StringView> value = GetValue(property);
    return value ? value.Value() : fallback;
}

template<class TOwner, class TValue>
TValue DependencyObject::GetValueOr(
    const AttachedPropertyRef<TOwner, TValue>& property,
    const TValue& fallback) const noexcept {
    return GetValueOr(
        static_cast<const DependencyPropertyRef<TOwner, TValue>&>(
            property),
        fallback);
}

template<class TOwner, class TValue>
TValue DependencyObject::GetValueOr(
    const ReadOnlyPropertyRef<TOwner, TValue>& property,
    const TValue& fallback) const noexcept {
    Base::Result<PropertyAccess<TValue>> value =
        GetValue(property);
    return value ? std::move(value).Value() : fallback;
}

template<class TOwner, class TValue>
Base::Result<void> DependencyObject::SetValue(
    const DependencyPropertyRef<TOwner, TValue>& property,
    PropertyAccess<TValue> value) noexcept {
    Base::Result<PropertyValue> stored =
        ValueCodec<TValue>::Encode(value);
    if (!stored) return stored.GetStatus();
    return SetValue(property.Handle(), stored.Value());
}

template<class TOwner, class TValue>
Base::Result<void> DependencyObject::SetCurrentValue(
    const DependencyPropertyRef<TOwner, TValue>& property,
    PropertyAccess<TValue> value) noexcept {
    Base::Result<PropertyValue> stored =
        ValueCodec<TValue>::Encode(value);
    if (!stored) return stored.GetStatus();
    return SetCurrentValue(property.Handle(), stored.Value());
}

template<class TOwner>
Base::Result<void> DependencyObject::SetValue(
    const DependencyPropertyRef<TOwner, Base::String>& property,
    Base::StringView value) noexcept {
    Base::Result<PropertyValue> stored =
        Value::TryFromString(TypeOf<Base::String>(), value);
    if (!stored) return stored.GetStatus();
    return SetValue(property.Handle(), stored.Value());
}

template<class TOwner, class TValue>
Base::Result<void> DependencyObject::SetValue(
    const AttachedPropertyRef<TOwner, TValue>& property,
    PropertyAccess<TValue> value) noexcept {
    return SetValue(
        static_cast<const DependencyPropertyRef<TOwner, TValue>&>(
            property),
        std::move(value));
}

template<class TOwner, class TValue>
Base::Result<void> DependencyObject::SetReadOnlyCurrentValue(
    const ReadOnlyPropertyRef<TOwner, TValue>& property,
    PropertyAccess<TValue> value) noexcept {
    Base::Result<PropertyValue> stored =
        ValueCodec<TValue>::Encode(value);
    if (!stored) return stored.GetStatus();
    return SetReadOnlyCurrentValue(
        property.Handle(), stored.Value());
}

} // namespace Aero::Core
