#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Data.hpp>
#include <Aero/Resources.hpp>
#include <utility>

namespace Aero::Internal {
class StylePrivate;
}

namespace Aero {

using Meta::DependencyPropertyHandle;
using Meta::PropertyValue;
using Meta::TypeId;

struct StyleSetter final {
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct StyleTriggerSetter final {
    DependencyPropertyHandle property;
    PropertyValue value;
};

// Internal compiled representation of a WPF Trigger.  The public authoring
// type is Aero::Trigger; this plan is kept only for the style engine.
struct TriggerPlan final {
    DependencyPropertyHandle property;
    PropertyValue value;
    Base::Vector<StyleTriggerSetter> setters;
    Base::Vector<Base::Ref<Base::Object>> enterActions;
    Base::Vector<Base::Ref<Base::Object>> exitActions;
};

class AERO_API SetterBase : public Base::Object {
    AERO_DECLARE_TYPE(SetterBase, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return runtimeType_; }

protected:
    explicit SetterBase(Meta::TypeId runtimeType) noexcept : runtimeType_(runtimeType) {}
    ~SetterBase() override = default;

private:
    Meta::TypeId runtimeType_ = StaticTypeId();
};

class AERO_API Setter final : public SetterBase {
    AERO_DECLARE_TYPE(Setter, SetterBase)
public:
    explicit Setter(
        TypeId runtimeType = StaticTypeId()) noexcept
        : SetterBase(runtimeType) {}

    DependencyPropertyHandle GetProperty() const noexcept { return property_; }
    const PropertyValue& GetValue() const noexcept { return value_; }
    void SetProperty(
        DependencyPropertyHandle value) noexcept {
        if (!value.IsValid()) {
            return;
        }
        property_ = value;
        return;
    }
    void SetValue(
        const PropertyValue& value) noexcept {
        if (value.IsUnset()) {
            return;
        }
        value_ = value;
        return;
    }
    template<class TOwner, class TValue>
    Base::Result<void> TrySet(
        const Meta::DependencyPropertyRef<TOwner, TValue>& property,
        const TValue& value) noexcept {
        Base::Result<PropertyValue> encoded =
            Meta::ValueCodec<TValue>::Encode(value);
        if (!encoded) return encoded.GetStatus();
        if (!property.Handle().IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Setter property is invalid");
        }
        SetProperty(property.Handle());
        SetValue(encoded.Value());
        return {};
    }
    void SetPropertyName(
        Base::StringView value) noexcept;
    void SetTargetName(
        Base::StringView value) noexcept;
    void SetAuthoredValue(
        const PropertyValue& value) noexcept;
    Base::StringView GetPropertyName() const noexcept { return propertyName_.View(); }
    Base::StringView GetTargetName() const noexcept { return targetName_.View(); }
    const PropertyValue& GetAuthoredValue() const noexcept {
        return authoredValue_;
    }
    bool GetIsAuthored() const noexcept {
        return !propertyName_.Empty() &&
            !authoredValue_.IsUnset();
    }
    Base::Result<void> Resolve(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;

private:
    DependencyPropertyHandle property_;
    PropertyValue value_;
    Base::String propertyName_;
    Base::String targetName_;
    PropertyValue authoredValue_;
};

class AERO_API TriggerBase : public Base::Object {
    AERO_DECLARE_TYPE(TriggerBase, Base::Object)
public:
    TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    Base::Result<void> TryAddEnterAction(
        Base::Ref<Base::Object> action) noexcept;
    Base::Result<void> TryAddExitAction(
        Base::Ref<Base::Object> action) noexcept;
    void ClearEnterActions() noexcept {
        enterActions_.Clear();
    }
    void ClearExitActions() noexcept {
        exitActions_.Clear();
    }
    Base::Span<const Base::Ref<Base::Object>>
    GetEnterActions() const noexcept {
        return {
            enterActions_.Data(),
            enterActions_.Size()};
    }
    Base::Span<const Base::Ref<Base::Object>>
    GetExitActions() const noexcept {
        return {
            exitActions_.Data(),
            exitActions_.Size()};
    }

protected:
    explicit TriggerBase(TypeId runtimeType) noexcept
        : runtimeType_(runtimeType) {}
    ~TriggerBase() override = default;

private:
    TypeId runtimeType_ = StaticTypeId();
    Base::Vector<Base::Ref<Base::Object>>
        enterActions_;
    Base::Vector<Base::Ref<Base::Object>>
        exitActions_;
};

class AERO_API Trigger final
    : public TriggerBase {
    AERO_DECLARE_TYPE_NAMED(
        Trigger,
        TriggerBase,
        "urn:aero",
        "Trigger")
public:
    explicit Trigger(
        TypeId runtimeType = StaticTypeId()) noexcept
        : TriggerBase(runtimeType) {}
    DependencyPropertyHandle GetProperty() const noexcept { return property_; }
    const PropertyValue& GetValue() const noexcept {
        return value_;
    }
    void SetProperty(
        DependencyPropertyHandle value) noexcept;
    void SetValue(
        const PropertyValue& value) noexcept;
    Base::Result<void> TryAddSetter(
        const Setter& setter) noexcept;
    void SetPropertyName(
        Base::StringView value) noexcept;
    Base::StringView GetSourceName() const noexcept {
        return sourceName_.View();
    }
    void SetSourceName(
        Base::StringView value) noexcept;
    void SetAuthoredValue(
        const PropertyValue& value) noexcept;
    Base::Result<void> TryAddAuthoredSetter(
        Base::Ref<Setter> setter) noexcept;
    void ClearAuthoredSetters() noexcept;
    Base::StringView GetPropertyName() const noexcept {
        return propertyName_.View();
    }
    const PropertyValue& GetAuthoredValue() const noexcept {
        return authoredValue_;
    }
    Base::Span<const Base::Ref<Setter>>
    GetAuthoredSetters() const noexcept {
        return {
            authoredSetters_.Data(),
            authoredSetters_.Size()};
    }
    bool GetIsAuthored() const noexcept {
        return !propertyName_.Empty() &&
            !authoredValue_.IsUnset() &&
            !authoredSetters_.Empty();
    }
    Base::Result<TriggerPlan>
    BuildPlan() const noexcept;

private:
    DependencyPropertyHandle property_;
    PropertyValue value_;
    Base::Vector<StyleTriggerSetter> setters_;
    Base::String propertyName_;
    Base::String sourceName_;
    PropertyValue authoredValue_;
    Base::Vector<Base::Ref<Setter>> authoredSetters_;
};

class AERO_API DataTrigger final
    : public TriggerBase {
    AERO_DECLARE_TYPE(DataTrigger, TriggerBase)
public:
    DataTrigger() noexcept
        : TriggerBase(StaticTypeId()) {
        static_cast<void>(
            comparison_.TryAssign("Equal"));
    }
    Base::Ref<Data::Binding> GetBinding() const noexcept {
        return binding_;
    }
    void SetBinding(
        Base::Ref<Data::Binding> value) noexcept {
        binding_ = std::move(value);
        return;
    }
    Base::StringView GetPropertyName() const noexcept {
        return propertyName_.View();
    }
    void SetPropertyName(
        Base::StringView value) noexcept;
    Base::StringView GetSourceName() const noexcept {
        return sourceName_.View();
    }
    void SetSourceName(
        Base::StringView value) noexcept;
    const PropertyValue& GetAuthoredValue() const noexcept {
        return authoredValue_;
    }
    void SetAuthoredValue(
        const PropertyValue& value) noexcept {
        if (value.IsUnset()) {
            return;
        }
        authoredValue_ = value;
        return;
    }
    Base::StringView GetComparison() const noexcept { return comparison_.View(); }
    void SetComparison(Base::StringView value) noexcept {
        (void)comparison_.TryAssign(value);
    }
    Base::Result<void> TryAddAuthoredSetter(
        Base::Ref<Setter> setter) noexcept;
    void ClearAuthoredSetters() noexcept {
        authoredSetters_.Clear();
    }
    Base::Span<const Base::Ref<Setter>>
    GetAuthoredSetters() const noexcept {
        return {
            authoredSetters_.Data(),
            authoredSetters_.Size()};
    }

private:
    Base::Ref<Data::Binding> binding_;
    Base::String propertyName_;
    Base::String sourceName_;
    PropertyValue authoredValue_;
    Base::String comparison_;
    Base::Vector<Base::Ref<Setter>>
        authoredSetters_;
};

class AERO_API Condition final : public Base::Object {
    AERO_DECLARE_TYPE(Condition, Base::Object)
public:
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Ref<Data::Binding> GetBinding() const noexcept {
        return binding_;
    }
    void SetBinding(
        Base::Ref<Data::Binding> value) noexcept {
        binding_ = std::move(value);
        return;
    }
    Base::StringView GetPropertyName() const noexcept {
        return propertyName_.View();
    }
    void SetPropertyName(
        Base::StringView value) noexcept;
    Base::StringView GetSourceName() const noexcept {
        return sourceName_.View();
    }
    void SetSourceName(
        Base::StringView value) noexcept;
    const PropertyValue& GetAuthoredValue() const noexcept {
        return authoredValue_;
    }
    void SetAuthoredValue(
        const PropertyValue& value) noexcept {
        if (value.IsUnset()) {
            return;
        }
        authoredValue_ = value;
        return;
    }

private:
    Base::Ref<Data::Binding> binding_;
    Base::String propertyName_;
    Base::String sourceName_;
    PropertyValue authoredValue_;
};

class AERO_API MultiTrigger final : public TriggerBase {
    AERO_DECLARE_TYPE(MultiTrigger, TriggerBase)
public:
    MultiTrigger() noexcept : TriggerBase(StaticTypeId()) {}
    Base::Result<void> TryAddCondition(Base::Ref<Condition> condition) noexcept;
    void ClearConditions() noexcept { conditions_.Clear(); }
    Base::Span<const Base::Ref<Condition>> GetConditions() const noexcept {
        return {conditions_.Data(), conditions_.Size()};
    }
    Base::Result<void> TryAddAuthoredSetter(Base::Ref<Setter> setter) noexcept;
    void ClearAuthoredSetters() noexcept { authoredSetters_.Clear(); }
    Base::Span<const Base::Ref<Setter>> GetAuthoredSetters() const noexcept {
        return {authoredSetters_.Data(), authoredSetters_.Size()};
    }
private:
    Base::Vector<Base::Ref<Condition>> conditions_;
    Base::Vector<Base::Ref<Setter>> authoredSetters_;
};

class AERO_API MultiDataTrigger final
    : public TriggerBase {
    AERO_DECLARE_TYPE(MultiDataTrigger, TriggerBase)
public:
    MultiDataTrigger() noexcept
        : TriggerBase(StaticTypeId()) {}
    Base::Result<void> TryAddCondition(
        Base::Ref<Condition> condition) noexcept;
    void ClearConditions() noexcept {
        conditions_.Clear();
    }
    Base::Span<const Base::Ref<Condition>>
    GetConditions() const noexcept {
        return {
            conditions_.Data(),
            conditions_.Size()};
    }
    Base::Result<void> TryAddAuthoredSetter(
        Base::Ref<Setter> setter) noexcept;
    void ClearAuthoredSetters() noexcept {
        authoredSetters_.Clear();
    }
    Base::Span<const Base::Ref<Setter>>
    GetAuthoredSetters() const noexcept {
        return {
            authoredSetters_.Data(),
            authoredSetters_.Size()};
    }

private:
    Base::Vector<Base::Ref<Condition>> conditions_;
    Base::Vector<Base::Ref<Setter>>
        authoredSetters_;
};

class Style;

class AERO_API SetterBaseCollection final {
public:
    std::uint32_t Count() const noexcept;
    SetterBase* At(std::uint32_t index) const noexcept;
    void Add(Base::Ref<Setter> setter) noexcept;
    void Clear() noexcept;

private:
    friend class Style;
    explicit SetterBaseCollection(Style& owner) noexcept : owner_(&owner) {}
    Style* owner_ = nullptr;
};

class AERO_API TriggerCollection final {
public:
    std::uint32_t Count() const noexcept;
    TriggerBase* At(std::uint32_t index) const noexcept;
    void Add(Base::Ref<Trigger> trigger) noexcept;
    void Clear() noexcept;

private:
    friend class Style;
    explicit TriggerCollection(Style& owner) noexcept : owner_(&owner) {}
    Style* owner_ = nullptr;
};

// WPF-shaped Style authoring surface. Runtime plans and provider precedence are
// compiled privately when the style is sealed.
class AERO_API Style final : public Base::Object {
    AERO_DECLARE_TYPE(Style, Base::Object)
public:
    Style() noexcept;
    explicit Style(
        TypeId targetType,
        const Style* basedOn = nullptr) noexcept;
    Style(
        TypeId targetType,
        const Style* basedOn,
        TypeId runtimeType) noexcept;

    Style(const Style&) = delete;
    Style& operator=(const Style&) = delete;

    TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    Base::Result<void> TryAddSetter(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    Base::Result<void> TryAddSetter(
        const Setter& setter) noexcept;
    Base::Result<void> TryAddTrigger(
        TriggerPlan trigger) noexcept;
    Base::Result<void> TryAddTrigger(
        const Trigger& trigger) noexcept;

    class TriggerBuilder final {
    public:
        template<class TOwner, class TValue>
        Base::Result<void> TrySet(
            const Meta::DependencyPropertyRef<TOwner, TValue>& property,
            const TValue& value) noexcept {
            if (!status_.IsOk()) return status_;
            Base::Result<PropertyValue> encoded =
                Meta::ValueCodec<TValue>::Encode(value);
            if (!encoded) return encoded.GetStatus();
            TriggerPlan trigger;
            trigger.property = condition_;
            trigger.value = std::move(conditionValue_);
            Base::Result<void> added =
                trigger.setters.TryPushBack({
                    property.Handle(),
                    std::move(encoded).Value()});
            if (!added) return added.GetStatus();
            return owner_->TryAddTrigger(
                std::move(trigger));
        }

    private:
        friend class Style;

        TriggerBuilder(
            Style& owner,
            DependencyPropertyHandle condition,
            PropertyValue&& value) noexcept
            : owner_(&owner),
              condition_(condition),
              conditionValue_(std::move(value)) {}
        explicit TriggerBuilder(
            Base::Status status) noexcept
            : status_(status) {}

        Style* owner_ = nullptr;
        DependencyPropertyHandle condition_;
        PropertyValue conditionValue_;
        Base::Status status_;
    };

    template<class TOwner, class TValue>
    Base::Result<void> TrySet(
        const Meta::DependencyPropertyRef<TOwner, TValue>& property,
        const TValue& value) noexcept {
        Base::Result<PropertyValue> encoded =
            Meta::ValueCodec<TValue>::Encode(value);
        if (!encoded) return encoded.GetStatus();
        return TryAddSetter(
            property.Handle(), encoded.Value());
    }
    template<class TOwner, class TValue>
    TriggerBuilder When(
        const Meta::DependencyPropertyRef<TOwner, TValue>& property,
        const TValue& value) noexcept {
        Base::Result<PropertyValue> encoded =
            Meta::ValueCodec<TValue>::Encode(value);
        if (!encoded) {
            return TriggerBuilder(encoded.GetStatus());
        }
        return TriggerBuilder(
            *this,
            property.Handle(),
            std::move(encoded).Value());
    }
    template<class TOwner, class TValue>
    TriggerBuilder When(
        const Meta::ReadOnlyPropertyRef<TOwner, TValue>& property,
        const TValue& value) noexcept {
        Base::Result<PropertyValue> encoded =
            Meta::ValueCodec<TValue>::Encode(value);
        if (!encoded) {
            return TriggerBuilder(encoded.GetStatus());
        }
        return TriggerBuilder(
            *this,
            property.Handle(),
            std::move(encoded).Value());
    }
    // Builder configuration is intentionally available only before Seal().
    // XAML object construction supplies TargetType and BasedOn as members,
    // whereas native callers commonly provide both to the constructor.
    Base::Result<void> TrySetTargetType(
        TypeId targetType) noexcept;
    Base::Result<void> TrySetBasedOn(
        const Style* basedOn) noexcept;
    Base::Result<void> TrySetBasedOn(
        Base::Ref<Base::Object> basedOn) noexcept;
    Base::Result<void> TryAddAuthoredSetter(
        Base::Ref<Setter> setter) noexcept;
    Base::Result<void> TryAddAuthoredTrigger(
        Base::Ref<Trigger> trigger) noexcept;
    void ClearAuthoredSetters() noexcept;
    void ClearAuthoredTriggers() noexcept;
    Base::Span<const Base::Ref<Setter>>
    GetAuthoredSetters() const noexcept {
        return {
            authoredSetterObjects_.Data(),
            authoredSetterObjects_.Size()};
    }
    Base::Span<const Base::Ref<Trigger>>
    GetAuthoredTriggers() const noexcept {
        return {
            authoredTriggerObjects_.Data(),
            authoredTriggerObjects_.Size()};
    }
    TypeId GetTargetType() const noexcept { return sealed_ ? program_.TargetType() : targetType_; }
    const Style* GetBasedOn() const noexcept { return basedOn_; }
    SetterBaseCollection GetSetters() noexcept { return SetterBaseCollection(*this); }
    TriggerCollection GetTriggers() noexcept { return TriggerCollection(*this); }
    bool GetIsSealed() const noexcept { return sealed_; }
    Base::Span<const StyleSetter> GetRuntimeSetters() const noexcept {
        return program_.Setters();
    }
    Base::Span<const TriggerPlan> GetRuntimeTriggers() const noexcept {
        return program_.Triggers();
    }
    ResourceDictionary& GetResources() noexcept { return resources_; }
    const ResourceDictionary& GetResources() const noexcept { return resources_; }
    void SetResources(
        Base::Ref<ResourceDictionary> value) noexcept;

private:
    friend class ::Aero::Internal::StylePrivate;

    Base::Result<void> SealRuntime(
        const void* properties) noexcept;

    struct Impl final {
        Impl() noexcept = default;
        Impl(Impl&&) noexcept = default;
        Impl& operator=(Impl&&) noexcept = default;

        Impl(const Impl&) = delete;
        Impl& operator=(const Impl&) = delete;

        TypeId TargetType() const noexcept { return targetType; }
        Base::Span<const StyleSetter> Setters() const noexcept {
            return {setters.Data(), setters.Size()};
        }
        Base::Span<const TriggerPlan> Triggers() const noexcept {
            return {triggers.Data(), triggers.Size()};
        }
        Base::Result<void> Freeze(
            TypeId valueTargetType,
            Base::Vector<StyleSetter>&& valueSetters,
            Base::Vector<TriggerPlan>&& valueTriggers) noexcept;
        void Reset() noexcept;

        TypeId targetType = InvalidTypeId;
        Base::Vector<StyleSetter> setters;
        Base::Vector<TriggerPlan> triggers;
        bool frozen = false;
    };

    TypeId runtimeType_ = StaticTypeId();
    TypeId targetType_ = InvalidTypeId;
    const Style* basedOn_ = nullptr;
    Base::Ref<Base::Object> basedOnOwner_;
    Base::Vector<Base::Ref<Setter>>
        authoredSetterObjects_;
    Base::Vector<Base::Ref<Trigger>>
        authoredTriggerObjects_;
    Base::Vector<StyleSetter> authored_;
    Base::Vector<TriggerPlan> authoredTriggers_;
    Impl program_;
    ResourceDictionary resources_;
    bool sealed_ = false;
};

// Styles are applied by the private view runtime. Provider precedence and
// type-keyed theme-style lookup are not part of the public authoring surface.


} // namespace Aero
