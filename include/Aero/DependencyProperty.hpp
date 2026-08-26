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
#include <Aero/Threading.hpp>
#include <Aero/Value.hpp>
#include <Aero/Diagnostics/PropertyValueSource.hpp>

#include <cstdint>
#include <utility>

namespace Aero { class DependencyObject; }

namespace Aero::Meta { class Registry; class Registration; }

namespace Aero::Meta {
class DependencyProperty;
class EffectiveValueEngine;
class BehaviorTable;
class DependencyPropertyRegistry;

struct DependencyPropertyHandle {
    MemberId value = InvalidMemberId;

    constexpr bool IsValid() const noexcept {
        return value != InvalidMemberId;
    }
};

constexpr DependencyPropertyHandle MakeDependencyPropertyHandle(
    TypeId ownerType,
    StringView name) noexcept;

template<class TOwner, class TValue>
class DependencyPropertyRef {
public:
    using Owner = TOwner;
    using ValueType = TValue;

    constexpr explicit DependencyPropertyRef(
        StringView name) noexcept
        : name_(name),
          handle_(MakeDependencyPropertyHandle(
              TOwner::StaticTypeIdValue_, name)) {}

    constexpr StringView Name() const noexcept {
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
    StringView name_;
    DependencyPropertyHandle handle_;
};

template<class TOwner, class TValue>
class AttachedPropertyRef
    : public DependencyPropertyRef<TOwner, TValue> {
public:
    using DependencyPropertyRef<TOwner, TValue>::DependencyPropertyRef;
};

template<class TOwner, class TValue>
class ReadOnlyPropertyRef {
public:
    using Owner = TOwner;
    using ValueType = TValue;

    constexpr explicit ReadOnlyPropertyRef(
        StringView name) noexcept
        : name_(name),
          handle_(MakeDependencyPropertyHandle(
              TOwner::StaticTypeIdValue_, name)) {}

    constexpr StringView Name() const noexcept {
        return name_;
    }
    constexpr DependencyPropertyHandle Handle() const noexcept {
        return handle_;
    }
    constexpr MemberId Id() const noexcept {
        return handle_.value;
    }

private:
    StringView name_;
    DependencyPropertyHandle handle_;
};

template<class T>
using PropertyAccess = T;

constexpr DependencyPropertyHandle MakeDependencyPropertyHandle(
    TypeId ownerType,
    StringView name) noexcept {
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
    ReadOnly = 1U << 1U,
    AnyValue = 1U << 2U,
    Structural = 1U << 3U
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

enum class FrameworkPropertyMetadataOptions : std::uint32_t {
    None = 0U,
    Inherits = 1U << 0U,
    AffectsMeasure = 1U << 1U,
    AffectsArrange = 1U << 2U,
    AffectsRender = 1U << 3U,
    BindsTwoWayByDefault = 1U << 4U,
    AffectsParentMeasure = 1U << 5U,
    AffectsParentArrange = 1U << 6U
};

inline constexpr FrameworkPropertyMetadataOptions Inherits =
    FrameworkPropertyMetadataOptions::Inherits;
inline constexpr FrameworkPropertyMetadataOptions AffectsMeasure =
    FrameworkPropertyMetadataOptions::AffectsMeasure;
inline constexpr FrameworkPropertyMetadataOptions AffectsArrange =
    FrameworkPropertyMetadataOptions::AffectsArrange;
inline constexpr FrameworkPropertyMetadataOptions AffectsRender =
    FrameworkPropertyMetadataOptions::AffectsRender;
inline constexpr FrameworkPropertyMetadataOptions BindsTwoWayByDefault =
    FrameworkPropertyMetadataOptions::BindsTwoWayByDefault;
inline constexpr FrameworkPropertyMetadataOptions AffectsParentMeasure =
    FrameworkPropertyMetadataOptions::AffectsParentMeasure;
inline constexpr FrameworkPropertyMetadataOptions AffectsParentArrange =
    FrameworkPropertyMetadataOptions::AffectsParentArrange;

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

constexpr FrameworkPropertyMetadataOptions operator|(
    FrameworkPropertyMetadataOptions left,
    FrameworkPropertyMetadataOptions right) noexcept {
    return static_cast<FrameworkPropertyMetadataOptions>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

constexpr FrameworkPropertyMetadataOptions operator&(
    FrameworkPropertyMetadataOptions left,
    FrameworkPropertyMetadataOptions right) noexcept {
    return static_cast<FrameworkPropertyMetadataOptions>(
        static_cast<std::uint32_t>(left) &
        static_cast<std::uint32_t>(right));
}

constexpr bool HasFlag(
    FrameworkPropertyMetadataOptions value,
    FrameworkPropertyMetadataOptions flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

constexpr PropertyMetadataFlags ToPropertyMetadataFlags(
    FrameworkPropertyMetadataOptions options) noexcept {
    return static_cast<PropertyMetadataFlags>(
        static_cast<std::uint32_t>(options));
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

class DependencyPropertyChangedEventArgs {
public:
    DependencyPropertyChangedEventArgs(
        DependencyPropertyHandle property,
        const PropertyValue& oldValue,
        const PropertyValue& newValue,
        EffectiveValueSource oldSource = EffectiveValueSource::Default,
        EffectiveValueSource newSource = EffectiveValueSource::Default) noexcept
        : property_(property),
          oldValue_(oldValue),
          newValue_(newValue),
          oldSource_(oldSource),
          newSource_(newSource) {}

    DependencyPropertyHandle GetProperty() const noexcept { return property_; }
    const PropertyValue& GetOldValue() const noexcept { return oldValue_; }
    const PropertyValue& GetNewValue() const noexcept { return newValue_; }
    EffectiveValueSource GetOldSource() const noexcept { return oldSource_; }
    EffectiveValueSource GetNewSource() const noexcept { return newSource_; }

private:
    DependencyPropertyHandle property_;
    const PropertyValue& oldValue_;
    const PropertyValue& newValue_;
    EffectiveValueSource oldSource_ = EffectiveValueSource::Default;
    EffectiveValueSource newSource_ = EffectiveValueSource::Default;
};

using ValidateValueCallback = Base::Delegate<bool(
    const PropertyValue& value)>;
using CoerceValueCallback = Base::Delegate<Result<PropertyValue>(
    DependencyObject& object,
    const DependencyProperty& property,
    const PropertyValue& baseValue)>;
using PropertyChangedCallback = Base::Delegate<void(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args)>;
using DependencyPropertyChangedEventHandler = Base::Delegate<void(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args)>;

struct PropertyMetadata {
    PropertyValue defaultValue;
    PropertyMetadataFlags flags = PropertyMetadataFlags::None;
    UpdateSourceTrigger defaultUpdateSourceTrigger =
        UpdateSourceTrigger::Default;
    ValidateValueCallback validate = nullptr;
    CoerceValueCallback coerce = nullptr;
    PropertyChangedCallback changed = nullptr;
};

template<class TValue>
class FrameworkPropertyMetadata;

class AERO_GUI_API DependencyPropertyKey {
public:
    DependencyPropertyKey() noexcept = default;

    bool IsValid() const noexcept {
        return registry_ != nullptr && property_.IsValid() && secret_ != 0U;
    }

    DependencyPropertyHandle Property() const noexcept {
        return property_;
    }

private:
    friend class ::Aero::Meta::Registry;
    friend class DependencyPropertyRegistry;
    friend class ::Aero::DependencyObject;

    const DependencyPropertyRegistry* registry_ = nullptr;
    DependencyPropertyHandle property_;
    std::uint64_t secret_ = 0U;
};

struct DependencyPropertyRegistration {
    StringView name;
    TypeId ownerType = InvalidTypeId;
    TypeId valueType = InvalidTypeId;
    DependencyPropertyFlags flags = DependencyPropertyFlags::None;
    PropertyMetadata metadata;
};

struct DependencyPropertyRegistrationResult {
    DependencyPropertyHandle property;
    DependencyPropertyKey readOnlyKey;
};

class AERO_GUI_API DependencyProperty {
public:
    DependencyProperty(DependencyProperty&&) noexcept = default;
    DependencyProperty& operator=(DependencyProperty&&) noexcept = default;

    DependencyProperty(const DependencyProperty&) = delete;
    DependencyProperty& operator=(const DependencyProperty&) = delete;

    DependencyPropertyHandle Handle() const noexcept {
        return handle_;
    }
    StringView Name() const noexcept {
        return name_.View();
    }
    TypeId ValueType() const noexcept { return valueType_; }
    TypeId RegisteredOwnerType() const noexcept {
        return registeredOwnerType_;
    }
    DependencyPropertyFlags Flags() const noexcept {
        return flags_;
    }
    bool GetIsReadOnly() const noexcept {
        return HasFlag(flags_, DependencyPropertyFlags::ReadOnly);
    }
    bool IsAttached() const noexcept {
        return HasFlag(flags_, DependencyPropertyFlags::Attached);
    }
    bool AcceptsAnyValue() const noexcept {
        return HasFlag(flags_, DependencyPropertyFlags::AnyValue);
    }
    std::uint32_t MetadataCount() const noexcept {
        return metadata_.Size();
    }

    // Metadata addresses become stable when the owning registry is frozen.
    const PropertyMetadata* MetadataFor(
        TypeId forType) const noexcept;

private:
    friend class ::Aero::Meta::Registry;
    friend class DependencyPropertyRegistry;
    friend class ::Aero::DependencyObject;

    struct MetadataEntry {
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
    String name_;
    Base::Vector<MetadataEntry> metadata_;
};

#if defined(AERO_GUI_IMPLEMENTATION)
class AERO_GUI_API DependencyPropertyRegistry {
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
        const PropertyMetadata& metadata) noexcept;

    // Override metadata is a complete replacement in the first runtime slice.
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

    // Returned addresses are stable after Freeze().
    const DependencyProperty* Find(
        DependencyPropertyHandle property) const noexcept;
    const DependencyProperty* Find(
        TypeId ownerType,
        StringView name) const noexcept;
    // Validates a provider value without mutating an object. Style/template
    // sealing uses this to reject invalid setter plans before frame execution.
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
#endif

} // namespace Aero::Meta

namespace Aero {

using Meta::AttachedPropertyRef;
using Meta::DependencyProperty;
using Meta::DependencyPropertyFlags;
using Meta::DependencyPropertyHandle;
using Meta::DependencyPropertyKey;
using Meta::DependencyPropertyRef;
#if defined(AERO_GUI_IMPLEMENTATION)
using Meta::DependencyPropertyRegistry;
#endif
using Meta::DependencyPropertyChangedEventArgs;
using Meta::DependencyPropertyChangedEventHandler;
using Meta::EffectiveValueSource;
using Meta::FrameworkPropertyMetadata;
using Meta::FrameworkPropertyMetadataOptions;
using Meta::InvalidTypeId;
using Meta::InvalidMemberId;
using Meta::MemberId;
using Meta::PropertyAccess;
using Meta::PropertyExpression;
using Meta::PropertyFlags;
using Meta::PropertyInvalidationFlags;
using Meta::PropertyMetadata;
using Meta::PropertyMetadataFlags;
using Meta::PropertyProviderToken;
using Meta::PropertyProviderSet;
using Meta::PropertyValue;
using Meta::PropertyValueSourceInfo;
using Meta::ReadOnlyPropertyRef;
using Meta::TypeId;
using Meta::TypeOf;
using Meta::UpdateSourceTrigger;
using Meta::ValueCodec;
using ::Aero::Threading::DispatcherObject;
using ::Aero::Threading::DispatcherReentrancyGuard;

} // namespace Aero
