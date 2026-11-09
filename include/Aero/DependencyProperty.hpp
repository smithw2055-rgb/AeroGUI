#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Value.hpp>
#include <Aero/Diagnostics.hpp>

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
    std::uint16_t globalIndex = 0xFFFFU;

    constexpr bool IsValid() const noexcept { return value != InvalidMemberId; }
    constexpr bool HasGlobalIndex() const noexcept { return globalIndex != 0xFFFFU; }
};

AERO_GUI_API std::uint16_t ResolveRegisteredPropertyGlobalIndex(MemberId member) noexcept;

constexpr DependencyPropertyHandle MakeDependencyPropertyHandle(TypeId ownerType, StringView name) noexcept;
AERO_GUI_API DependencyPropertyHandle ResolveDependencyPropertyHandle(DependencyPropertyHandle handle) noexcept;
AERO_GUI_API const Value* ResolveRegisteredPropertyDefaultValue(
    DependencyPropertyHandle handle,
    const Base::TypeClass* ownerType,
    bool onlyIfOverridden = false) noexcept;

template<class T> using PropertyAccess = std::conditional_t<
    (std::is_arithmetic_v<T> || std::is_enum_v<T> || std::is_same_v<T, TypeReference>),
    T,
    const T&>;

template<class TValue, bool IsLiteral = (std::is_trivially_destructible_v<TValue> && std::is_default_constructible_v<TValue>)>
struct TypedPropertyDefaultStorage {
    mutable TValue defaultValue{};
    constexpr TypedPropertyDefaultStorage() noexcept = default;
    constexpr explicit TypedPropertyDefaultStorage(TValue value) noexcept
        : defaultValue(value) {}
    constexpr const TValue& Get() const noexcept { return defaultValue; }
    void Set(const TValue& value) const noexcept { defaultValue = value; }
};

template<class TValue>
struct TypedPropertyDefaultStorage<TValue, false> {
    mutable const TValue* defaultValuePtr = nullptr;
    constexpr TypedPropertyDefaultStorage() noexcept = default;
    const TValue& Get() const noexcept {
        if (defaultValuePtr != nullptr) {
            return *defaultValuePtr;
        }
        static const TValue kDefault{};
        return kDefault;
    }
    void Set(const TValue& value) const noexcept {
        static constexpr std::uint32_t kMaxSlots = 256U;
        static TValue s_slots[kMaxSlots];
        static std::uint32_t s_nextSlot = 0U;
        if (defaultValuePtr >= &s_slots[0] && defaultValuePtr < &s_slots[kMaxSlots]) {
            *const_cast<TValue*>(defaultValuePtr) = value;
            return;
        }
        if (s_nextSlot < kMaxSlots) {
            TValue& slot = s_slots[s_nextSlot++];
            slot = value;
            defaultValuePtr = &slot;
        }
    }
};

template<class TValue>
class TypedDependencyProperty {
public:
    using ValueType = TValue;

    constexpr explicit TypedDependencyProperty(MemberId id) noexcept
        : id_(id), canonicalId_(id), globalIndex_(0xFFFFU), hasExplicitDefault_(false), defaultStorage_{} {}

    constexpr TypedDependencyProperty(MemberId id, TValue defaultValue) noexcept
        : id_(id), canonicalId_(id), globalIndex_(0xFFFFU), hasExplicitDefault_(true), defaultStorage_(defaultValue) {}

    constexpr TypedDependencyProperty(TypeId ownerType, StringView name) noexcept
        : id_(MakeMemberId(ownerType, MemberKind::Property, name)),
          canonicalId_(id_),
          globalIndex_(0xFFFFU),
          hasExplicitDefault_(false),
          defaultStorage_{} {}

    constexpr TypedDependencyProperty(TypeId ownerType, StringView name, TValue defaultValue) noexcept
        : id_(MakeMemberId(ownerType, MemberKind::Property, name)),
          canonicalId_(id_),
          globalIndex_(0xFFFFU),
          hasExplicitDefault_(true),
          defaultStorage_(defaultValue) {}

    constexpr MemberId Id() const noexcept { return id_; }
    std::uint16_t GlobalIndex() const noexcept { return ResolvedHandle().globalIndex; }
    constexpr DependencyPropertyHandle Handle() const noexcept { return {id_, globalIndex_}; }

    DependencyPropertyHandle ResolvedHandle() const noexcept {
        if (globalIndex_ != 0xFFFFU) {
            return {canonicalId_, globalIndex_};
        }
        const DependencyPropertyHandle resolved =
            ResolveDependencyPropertyHandle({id_, 0xFFFFU});
        if (resolved.HasGlobalIndex()) {
            canonicalId_ = resolved.value;
            globalIndex_ = resolved.globalIndex;
        }
        return resolved;
    }

    operator DependencyPropertyHandle() const noexcept { return ResolvedHandle(); }

    PropertyAccess<TValue> DefaultValue(const Base::TypeClass* ownerType = nullptr) const noexcept {
        if (ownerType != nullptr || !hasExplicitDefault_) {
            if (const Value* metaDefault =
                    ResolveRegisteredPropertyDefaultValue(ResolvedHandle(), ownerType, hasExplicitDefault_)) {
                if constexpr (std::is_same_v<TValue, String>) {
                    if (metaDefault->Kind() == ValueKind::String) {
                        defaultStorage_.Set(ValueCodec<String>::DecodeOrDefault(*metaDefault));
                        if (!hasExplicitDefault_ && ownerType == nullptr) {
                            hasExplicitDefault_ = true;
                        }
                        return defaultStorage_.Get();
                    }
                } else {
                    return ValueCodec<TValue>::DecodeOrDefault(*metaDefault);
                }
            }
        }
        return defaultStorage_.Get();
    }

    void SetDefaultValue(const TValue& value) const noexcept {
        if constexpr (std::is_trivially_destructible_v<TValue> && std::is_default_constructible_v<TValue>) {
            defaultStorage_.Set(value);
            hasExplicitDefault_ = true;
        } else if constexpr (std::is_same_v<TValue, String>) {
            if (!value.Empty()) {
                defaultStorage_.Set(value);
                hasExplicitDefault_ = true;
            }
        }
    }

private:
    MemberId id_ = InvalidMemberId;
    mutable MemberId canonicalId_ = InvalidMemberId;
    mutable std::uint16_t globalIndex_ = 0xFFFFU;
    mutable bool hasExplicitDefault_ = false;
    TypedPropertyDefaultStorage<TValue> defaultStorage_{};
};

template<class TOwner, class TValue> class DependencyPropertyRef {
public:
    using Owner = TOwner;
    using ValueType = TValue;

    constexpr explicit DependencyPropertyRef(StringView name) noexcept : name_(name),
          typed_(TOwner::StaticTypeIdValue_, name),
          handle_(MakeDependencyPropertyHandle(TOwner::StaticTypeIdValue_, name)) {}

    constexpr StringView Name() const noexcept { return name_; }
    constexpr DependencyPropertyHandle Handle() const noexcept { return handle_; }
    DependencyPropertyHandle ResolvedHandle() const noexcept { return typed_.ResolvedHandle(); }
    constexpr const TypedDependencyProperty<TValue>& Typed() const noexcept { return typed_; }
    constexpr const TypedDependencyProperty<TValue>* Ptr() const noexcept { return &typed_; }
    constexpr const TypedDependencyProperty<TValue>* operator->() const noexcept { return &typed_; }
    std::uint16_t GlobalIndex() const noexcept { return typed_.GlobalIndex(); }
    PropertyAccess<TValue> DefaultValue(const Base::TypeClass* ownerType = nullptr) const noexcept {
        return typed_.DefaultValue(ownerType);
    }
    void SetDefaultValue(const TValue& value) const noexcept {
        typed_.SetDefaultValue(value);
    }
    operator DependencyPropertyHandle() const noexcept { return typed_.ResolvedHandle(); }
    constexpr MemberId Id() const noexcept { return handle_.value; }

    template<class TNewOwner> constexpr DependencyPropertyRef<TNewOwner, TValue> AddOwner() const noexcept {
        return DependencyPropertyRef<TNewOwner, TValue>(name_);
    }

private:
    StringView name_;
    TypedDependencyProperty<TValue> typed_;
    DependencyPropertyHandle handle_;
};

template<class TOwner, class TValue> class AttachedPropertyRef : public DependencyPropertyRef<TOwner, TValue> {
public:
    using DependencyPropertyRef<TOwner, TValue>::DependencyPropertyRef;
};

template<class TOwner, class TValue> class ReadOnlyPropertyRef {
public:
    using Owner = TOwner;
    using ValueType = TValue;

    constexpr explicit ReadOnlyPropertyRef(StringView name) noexcept : name_(name),
          typed_(TOwner::StaticTypeIdValue_, name),
          handle_(MakeDependencyPropertyHandle(TOwner::StaticTypeIdValue_, name)) {}

    constexpr StringView Name() const noexcept { return name_; }
    constexpr DependencyPropertyHandle Handle() const noexcept { return handle_; }
    DependencyPropertyHandle ResolvedHandle() const noexcept { return typed_.ResolvedHandle(); }
    constexpr const TypedDependencyProperty<TValue>& Typed() const noexcept { return typed_; }
    constexpr const TypedDependencyProperty<TValue>* Ptr() const noexcept { return &typed_; }
    constexpr const TypedDependencyProperty<TValue>* operator->() const noexcept { return &typed_; }
    std::uint16_t GlobalIndex() const noexcept { return typed_.GlobalIndex(); }
    PropertyAccess<TValue> DefaultValue(const Base::TypeClass* ownerType = nullptr) const noexcept {
        return typed_.DefaultValue(ownerType);
    }
    void SetDefaultValue(const TValue& value) const noexcept {
        typed_.SetDefaultValue(value);
    }
    constexpr MemberId Id() const noexcept { return handle_.value; }

private:
    StringView name_;
    TypedDependencyProperty<TValue> typed_;
    DependencyPropertyHandle handle_;
};

// PropertyAccess<T> is defined above TypedDependencyProperty.

constexpr DependencyPropertyHandle MakeDependencyPropertyHandle(TypeId ownerType,
    StringView name) noexcept { return {MakeMemberId(ownerType, MemberKind::Property, name)}; }

constexpr bool operator==(DependencyPropertyHandle left,
    DependencyPropertyHandle right) noexcept { return left.value == right.value; }

template<class TOwner, class TValue> constexpr bool operator==(
    DependencyPropertyHandle left, const ReadOnlyPropertyRef<TOwner, TValue>& right) noexcept {
    return left == right.Handle();
}

template<class TOwner, class TValue> constexpr bool operator==(const ReadOnlyPropertyRef<TOwner, TValue>& left,
    DependencyPropertyHandle right) noexcept { return left.Handle() == right; }

constexpr bool operator!=(DependencyPropertyHandle left,
    DependencyPropertyHandle right) noexcept { return !(left == right); }

using PropertyValueKind = ValueKind;
using PropertyValue = Value;

enum class DependencyPropertyFlags : std::uint32_t {
    None = 0U,
    Attached = 1U << 0U, ReadOnly = 1U << 1U, AnyValue = 1U << 2U, Structural = 1U << 3U
};

enum class FrameworkPropertyMetadataOptions : std::uint32_t {
    None = 0U,
    Inherits = 1U << 0U, AffectsMeasure = 1U << 1U, AffectsArrange = 1U << 2U, AffectsRender = 1U << 3U,
    BindsTwoWayByDefault = 1U << 4U, AffectsParentMeasure = 1U << 5U, AffectsParentArrange = 1U << 6U
};

inline constexpr FrameworkPropertyMetadataOptions Inherits = FrameworkPropertyMetadataOptions::Inherits;
inline constexpr FrameworkPropertyMetadataOptions AffectsMeasure = FrameworkPropertyMetadataOptions::AffectsMeasure;
inline constexpr FrameworkPropertyMetadataOptions AffectsArrange = FrameworkPropertyMetadataOptions::AffectsArrange;
inline constexpr FrameworkPropertyMetadataOptions AffectsRender = FrameworkPropertyMetadataOptions::AffectsRender;
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

constexpr DependencyPropertyFlags operator|(DependencyPropertyFlags left, DependencyPropertyFlags right) noexcept {
    return static_cast<DependencyPropertyFlags>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

constexpr FrameworkPropertyMetadataOptions operator|(FrameworkPropertyMetadataOptions left,
    FrameworkPropertyMetadataOptions right) noexcept {
    return static_cast<FrameworkPropertyMetadataOptions>(static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

constexpr FrameworkPropertyMetadataOptions operator&(FrameworkPropertyMetadataOptions left,
    FrameworkPropertyMetadataOptions right) noexcept {
    return static_cast<FrameworkPropertyMetadataOptions>(static_cast<std::uint32_t>(left) &
        static_cast<std::uint32_t>(right));
}

constexpr bool HasFlag(FrameworkPropertyMetadataOptions value, FrameworkPropertyMetadataOptions flag) noexcept {
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0U;
}

constexpr bool HasFlag(DependencyPropertyFlags value, DependencyPropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0U;
}

enum class PropertyInvalidationFlags : std::uint8_t {
    None = 0U,
    Measure = 1U << 0U, Arrange = 1U << 1U, Render = 1U << 2U, Inheritance = 1U << 3U,
    ParentMeasure = 1U << 4U, ParentArrange = 1U << 5U
};

constexpr PropertyInvalidationFlags operator|(PropertyInvalidationFlags left,
    PropertyInvalidationFlags right) noexcept {
    return static_cast<PropertyInvalidationFlags>(static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
}

inline PropertyInvalidationFlags& operator|=(PropertyInvalidationFlags& left,
    PropertyInvalidationFlags right) noexcept {
    left = left | right;
    return left;
}

constexpr bool HasFlag(PropertyInvalidationFlags value, PropertyInvalidationFlags flag) noexcept {
    return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0U;
}

class DependencyPropertyChangedEventArgs {
public:
    DependencyPropertyChangedEventArgs(DependencyPropertyHandle property,
        const PropertyValue& oldValue, const PropertyValue& newValue,
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

using ValidateValueCallback = Base::Delegate<bool(const PropertyValue& value)>;
using PropertyChangedCallback = Base::Delegate<void(DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args)>;
using DependencyPropertyChangedEventHandler = Base::Delegate<void(DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args)>;
using ComputedPropertyReadCallback = PropertyValue (*)(const DependencyObject& object) noexcept;

struct PropertyMetadata {
    PropertyValue defaultValue;
    FrameworkPropertyMetadataOptions flags = FrameworkPropertyMetadataOptions::None;
    UpdateSourceTrigger defaultUpdateSourceTrigger = UpdateSourceTrigger::Default;
    ValidateValueCallback validate = nullptr;
    PropertyChangedCallback changed = nullptr;
    ComputedPropertyReadCallback computedReader = nullptr;
};

template<class TValue> class FrameworkPropertyMetadata;

class AERO_GUI_API DependencyPropertyKey {
public:
    DependencyPropertyKey() noexcept = default;

    bool IsValid() const noexcept { return registry_ != nullptr && property_.IsValid() && secret_ != 0U; }

    DependencyPropertyHandle Property() const noexcept { return property_; }

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

    DependencyPropertyHandle Handle() const noexcept { return handle_; }
    StringView Name() const noexcept { return name_.View(); }
    TypeId ValueType() const noexcept { return valueType_; }
    TypeId RegisteredOwnerType() const noexcept { return registeredOwnerType_; }
    DependencyPropertyFlags Flags() const noexcept { return flags_; }
    bool GetIsReadOnly() const noexcept { return HasFlag(flags_, DependencyPropertyFlags::ReadOnly); }
    bool IsAttached() const noexcept { return HasFlag(flags_, DependencyPropertyFlags::Attached); }
    bool AcceptsAnyValue() const noexcept { return HasFlag(flags_, DependencyPropertyFlags::AnyValue); }
    std::uint32_t MetadataCount() const noexcept { return metadata_.Size(); }
    bool HasOverrides() const noexcept { return metadata_.Size() > 1U; }
    std::uint8_t InheritanceIndex() const noexcept { return inheritanceIndex_; }
    std::uint16_t GlobalIndex() const noexcept { return globalIndex_; }
    bool HasDerivedDefaultOverride() const noexcept { return hasDerivedDefaultOverride_; }
    ComputedPropertyReadCallback ComputedReader() const noexcept { return computedReader_; }

    // Metadata addresses become stable when the owning registry is frozen.
    const PropertyMetadata* MetadataFor(TypeId forType) const noexcept;
    const PropertyMetadata* MetadataFor(const Base::TypeClass* forClass) const noexcept;
    const Base::TypeClass* OwnerClass() const noexcept { return ownerClass_; }
    const TypeRegistry* Types() const noexcept { return typeRegistry_; }

private:
    friend class ::Aero::Meta::Registry;
    friend class DependencyPropertyRegistry;
    friend class ::Aero::DependencyObject;

    enum ValueTypeTraitBits : std::uint8_t {
        kValueTypeTraitCached     = 1U << 0U,
        kValueTypeTraitIsValue    = 1U << 1U,
        kValueTypeTraitIsEnum     = 1U << 2U,
        kValueTypeTraitSignedEnum = 1U << 3U,
    };

    struct MetadataEntry {
        TypeId forType = InvalidTypeId;
        bool owner = false;
        PropertyMetadata metadata;
        Base::String aliasName;
        DependencyPropertyFlags flags = DependencyPropertyFlags::None;
    };

    static constexpr std::uint32_t kMetadataCacheSlots = 32U;

    DependencyProperty() noexcept : name_(), metadata_() {}

    void ClearMetadataCache() const noexcept {
        for (std::uint32_t i = 0U; i < kMetadataCacheSlots; ++i) {
            metadataCache_[i] = {};
        }
    }

    const MetadataEntry* FindMetadataExact(TypeId forType) const noexcept;

    TypeRegistry* typeRegistry_ = nullptr;
    DependencyPropertyHandle handle_;
    TypeId valueType_ = InvalidTypeId;
    TypeId registeredOwnerType_ = InvalidTypeId;
    const Base::TypeClass* ownerClass_ = nullptr;
    DependencyPropertyFlags flags_ = DependencyPropertyFlags::None;
    std::uint8_t inheritanceIndex_ = 0xFFU;
    bool hasDerivedDefaultOverride_ = false;
    std::uint8_t valueTypeTraits_ = 0U;
    std::uint16_t globalIndex_ = 0xFFFFU;
    std::uint16_t ownerTreeIn_ = 0U;
    std::uint16_t ownerTreeOut_ = 0U;
    std::uint64_t readOnlySecret_ = 0U;
    ComputedPropertyReadCallback computedReader_ = nullptr;

    struct MetadataCacheEntry {
        TypeId forType = InvalidTypeId;
        const PropertyMetadata* metadata = nullptr;
    };

    String name_;
    Base::Vector<MetadataEntry> metadata_;
    mutable MetadataCacheEntry metadataCache_[kMetadataCacheSlots]{};
};

template<class T, class = void>
struct HasEqualityOperator : std::false_type {};

template<class T>
struct HasEqualityOperator<
    T,
    std::void_t<decltype(static_cast<bool>(std::declval<const T&>() == std::declval<const T&>()))>>
    : std::true_type {};

template<class TValue>
bool ArePropertyValuesEqual(PropertyAccess<TValue> lhs, PropertyAccess<TValue> rhs) noexcept {
    if constexpr (std::is_same_v<TValue, PropertyValue>) {
        if (rhs.IsDynamicResource()) return false;
        return lhs == rhs;
    } else if constexpr (HasEqualityOperator<TValue>::value) {
        return static_cast<bool>(lhs == rhs);
    } else {
        static_cast<void>(lhs);
        static_cast<void>(rhs);
        return false;
    }
}

} // namespace Aero::Meta

namespace Aero {

using Meta::AttachedPropertyRef;
using Meta::DependencyProperty;
using Meta::DependencyPropertyFlags;
using Meta::DependencyPropertyHandle;
using Meta::DependencyPropertyKey;
using Meta::DependencyPropertyRef;
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
using Meta::PropertyProviderToken;
using Meta::PropertyValue;
using Meta::PropertyValueSourceInfo;
using Meta::ReadOnlyPropertyRef;
using Meta::TypedDependencyProperty;
using Meta::TypeId;
namespace Meta {
template<typename T>
struct PropParamTraits {
    using GetReturn = T;
    using SetParam = std::conditional_t<
        (std::is_scalar_v<T> || sizeof(T) <= 8),
        T,
        const T&
    >;
};

template<>
struct PropParamTraits<Base::String> {
    using GetReturn = Base::StringView;
    using SetParam = Base::StringView;
};
} // namespace Meta

using Meta::TypeOf;
using Meta::UpdateSourceTrigger;
using Meta::ValueCodec;

} // namespace Aero

#define AERO_DEPENDENCY_PROPERTY(type, name) \
    inline static const ::Aero::Meta::DependencyPropertyRef<Self, type> name##Property{#name}

#define AERO_ATTACHED_PROPERTY(type, name) \
    inline static const ::Aero::Meta::AttachedPropertyRef<Self, type> name##Property{#name}

#define AERO_READONLY_PROPERTY(type, name) \
    inline static const ::Aero::Meta::ReadOnlyPropertyRef<Self, type> name##Property{#name}

#define AERO_DEPENDENCY_PROPERTY2(type, name) \
    inline static const ::Aero::Meta::DependencyPropertyRef<Self, type> name##Property{#name}; \
    typename ::Aero::Meta::PropParamTraits<type>::GetReturn Get##name() const noexcept { return this->GetValue(name##Property); } \
    void Set##name(typename ::Aero::Meta::PropParamTraits<type>::SetParam value) noexcept { \
        this->SetValue(name##Property, value); \
    }

#define AERO_READONLY_PROPERTY2(type, name) \
    inline static const ::Aero::Meta::ReadOnlyPropertyRef<Self, type> name##Property{#name}; \
    typename ::Aero::Meta::PropParamTraits<type>::GetReturn Get##name() const noexcept { return this->GetValue(name##Property); }

#define AERO_ATTACHED_PROPERTY2(type, name) \
    inline static const ::Aero::Meta::AttachedPropertyRef<Self, type> name##Property{#name}; \
    static typename ::Aero::Meta::PropParamTraits<type>::GetReturn Get##name(const ::Aero::DependencyObject& element) noexcept { \
        return element.GetValue(name##Property); \
    } \
    static void Set##name(::Aero::DependencyObject& element, typename ::Aero::Meta::PropParamTraits<type>::SetParam value) noexcept { \
        element.SetValue(name##Property, value); \
    }

#define AERO_DEPENDENCY_PROPERTY_INLINE(type, name) AERO_DEPENDENCY_PROPERTY2(type, name)
#define AERO_READONLY_PROPERTY_INLINE(type, name) AERO_READONLY_PROPERTY2(type, name)
#define AERO_ATTACHED_PROPERTY_INLINE(type, name) AERO_ATTACHED_PROPERTY2(type, name)
