#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Presentation/Resources.hpp>

#include <utility>

namespace Aero::Presentation {

using namespace Aero::Core;

struct StyleSetter final {
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct StyleTriggerSetter final {
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct StylePropertyTrigger final {
    DependencyPropertyHandle property;
    PropertyValue value;
    Base::Vector<StyleTriggerSetter> setters;
};

class AERO_API Setter final : public Base::Object {
    AERO_DECLARE_TYPE(Setter, Base::Object)
public:
    explicit Setter(
        TypeId runtimeType = StaticTypeId()) noexcept
        : runtimeType_(runtimeType) {}

    TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    DependencyPropertyHandle Property() const noexcept {
        return property_;
    }
    const PropertyValue& Value() const noexcept {
        return value_;
    }
    Base::Result<void> SetProperty(
        DependencyPropertyHandle value) noexcept {
        if (!value.IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Setter property is invalid");
        }
        property_ = value;
        return {};
    }
    Base::Result<void> SetValue(
        const PropertyValue& value) noexcept {
        if (value.IsUnset()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Setter value is unset");
        }
        value_ = value;
        return {};
    }
    template<class TOwner, class TValue>
    Base::Result<void> Set(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const TValue& value) noexcept {
        Base::Result<PropertyValue> encoded =
            ValueCodec<TValue>::Encode(value);
        if (!encoded) return encoded.GetStatus();
        Base::Result<void> selected =
            SetProperty(property.Handle());
        return selected
            ? SetValue(encoded.Value())
            : selected;
    }
    Base::Result<void> SetPropertyName(
        Base::StringView value) noexcept;
    Base::Result<void> SetTargetName(
        Base::StringView value) noexcept;
    Base::Result<void> SetAuthoredValue(
        const PropertyValue& value) noexcept;
    Base::StringView PropertyName() const noexcept {
        return propertyName_.View();
    }
    Base::StringView TargetName() const noexcept {
        return targetName_.View();
    }
    const PropertyValue& AuthoredValue() const noexcept {
        return authoredValue_;
    }
    bool IsAuthored() const noexcept {
        return !propertyName_.Empty() &&
            !authoredValue_.IsUnset();
    }
    Base::Result<void> Resolve(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;

private:
    TypeId runtimeType_ = StaticTypeId();
    DependencyPropertyHandle property_;
    PropertyValue value_;
    Base::String propertyName_;
    Base::String targetName_;
    PropertyValue authoredValue_;
};

class AERO_API PropertyTrigger final
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        PropertyTrigger,
        Base::Object,
        "urn:aero",
        "Trigger")
public:
    explicit PropertyTrigger(
        TypeId runtimeType = StaticTypeId()) noexcept
        : runtimeType_(runtimeType) {}

    TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    DependencyPropertyHandle Property() const noexcept {
        return property_;
    }
    const PropertyValue& Value() const noexcept {
        return value_;
    }
    Base::Result<void> SetProperty(
        DependencyPropertyHandle value) noexcept;
    Base::Result<void> SetValue(
        const PropertyValue& value) noexcept;
    Base::Result<void> TryAddSetter(
        const Setter& setter) noexcept;
    Base::Result<void> SetPropertyName(
        Base::StringView value) noexcept;
    Base::Result<void> SetAuthoredValue(
        const PropertyValue& value) noexcept;
    Base::Result<void> TryAddAuthoredSetter(
        Base::Ref<Setter> setter) noexcept;
    Base::Result<void> ClearAuthoredSetters() noexcept;
    Base::StringView PropertyName() const noexcept {
        return propertyName_.View();
    }
    const PropertyValue& AuthoredValue() const noexcept {
        return authoredValue_;
    }
    Base::Span<const Base::Ref<Setter>>
    AuthoredSetters() const noexcept {
        return {
            authoredSetters_.Data(),
            authoredSetters_.Size()};
    }
    bool IsAuthored() const noexcept {
        return !propertyName_.Empty() &&
            !authoredValue_.IsUnset() &&
            !authoredSetters_.Empty();
    }
    Base::Result<StylePropertyTrigger>
    BuildPlan() const noexcept;

private:
    TypeId runtimeType_ = StaticTypeId();
    DependencyPropertyHandle property_;
    PropertyValue value_;
    Base::Vector<StyleTriggerSetter> setters_;
    Base::String propertyName_;
    PropertyValue authoredValue_;
    Base::Vector<Base::Ref<Setter>> authoredSetters_;
};

// Frozen execution plan produced from Style authoring objects. Runtime
// managers consume only this snapshot; Setter/Trigger authoring objects are
// not consulted after Style::Seal().
class AERO_API StyleProgram final {
public:
    StyleProgram() noexcept = default;
    StyleProgram(StyleProgram&&) noexcept = default;
    StyleProgram& operator=(StyleProgram&&) noexcept = default;

    StyleProgram(const StyleProgram&) = delete;
    StyleProgram& operator=(const StyleProgram&) = delete;

    TypeId TargetType() const noexcept { return targetType_; }
    Base::Span<const StyleSetter> Setters() const noexcept {
        return {setters_.Data(), setters_.Size()};
    }
    Base::Span<const StylePropertyTrigger> Triggers() const noexcept {
        return {triggers_.Data(), triggers_.Size()};
    }
    bool IsFrozen() const noexcept { return frozen_; }

private:
    friend class Style;
    Base::Result<void> Freeze(
        TypeId targetType,
        Base::Vector<StyleSetter>&& setters,
        Base::Vector<StylePropertyTrigger>&& triggers) noexcept;
    void Reset() noexcept;

    TypeId targetType_ = InvalidTypeId;
    Base::Vector<StyleSetter> setters_;
    Base::Vector<StylePropertyTrigger> triggers_;
    bool frozen_ = false;
};

// Host-owned immutable style plan. Styles are authored through setters and
// sealed only after DependencyProperty metadata is frozen. BasedOn setters are
// flattened deterministically; a derived style replaces a base setter for the
// same property.
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
    Base::Result<void> TryAddPropertyTrigger(
        StylePropertyTrigger trigger) noexcept;
    Base::Result<void> TryAddPropertyTrigger(
        const PropertyTrigger& trigger) noexcept;

    class TriggerBuilder final {
    public:
        template<class TOwner, class TValue>
        Base::Result<void> Set(
            const DependencyPropertyRef<TOwner, TValue>& property,
            const TValue& value) noexcept {
            if (!status_.IsOk()) return status_;
            Base::Result<PropertyValue> encoded =
                ValueCodec<TValue>::Encode(value);
            if (!encoded) return encoded.GetStatus();
            StylePropertyTrigger trigger;
            trigger.property = condition_;
            trigger.value = std::move(conditionValue_);
            Base::Result<void> added =
                trigger.setters.TryPushBack({
                    property.Handle(),
                    std::move(encoded).Value()});
            if (!added) return added.GetStatus();
            return owner_->TryAddPropertyTrigger(
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
    Base::Result<void> Set(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const TValue& value) noexcept {
        Base::Result<PropertyValue> encoded =
            ValueCodec<TValue>::Encode(value);
        if (!encoded) return encoded.GetStatus();
        return TryAddSetter(
            property.Handle(), encoded.Value());
    }
    template<class TOwner, class TValue>
    TriggerBuilder When(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const TValue& value) noexcept {
        Base::Result<PropertyValue> encoded =
            ValueCodec<TValue>::Encode(value);
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
        const ReadOnlyPropertyRef<TOwner, TValue>& property,
        const TValue& value) noexcept {
        Base::Result<PropertyValue> encoded =
            ValueCodec<TValue>::Encode(value);
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
        Base::Ref<PropertyTrigger> trigger) noexcept;
    Base::Result<void> ClearAuthoredSetters() noexcept;
    Base::Result<void> ClearAuthoredTriggers() noexcept;
    Base::Span<const Base::Ref<Setter>>
    AuthoredSetters() const noexcept {
        return {
            authoredSetterObjects_.Data(),
            authoredSetterObjects_.Size()};
    }
    Base::Span<const Base::Ref<PropertyTrigger>>
    AuthoredTriggers() const noexcept {
        return {
            authoredTriggerObjects_.Data(),
            authoredTriggerObjects_.Size()};
    }
    Base::Result<void> Seal(
        const DependencyPropertyRegistry& properties) noexcept;

    TypeId TargetType() const noexcept {
        return sealed_ ? program_.TargetType() : targetType_;
    }
    const Style* BasedOn() const noexcept { return basedOn_; }
    bool IsSealed() const noexcept { return sealed_; }
    const StyleProgram& Program() const noexcept { return program_; }
    Base::Span<const StyleSetter> Setters() const noexcept {
        return program_.Setters();
    }
    Base::Span<const StylePropertyTrigger> Triggers() const noexcept {
        return program_.Triggers();
    }
    ResourceDictionary& Resources() noexcept {
        return resources_;
    }
    const ResourceDictionary& Resources() const noexcept {
        return resources_;
    }
    Base::Result<void> SetResources(
        Base::Ref<ResourceDictionary> value) noexcept;

private:
    TypeId runtimeType_ = StaticTypeId();
    TypeId targetType_ = InvalidTypeId;
    const Style* basedOn_ = nullptr;
    Base::Ref<Base::Object> basedOnOwner_;
    Base::Vector<Base::Ref<Setter>>
        authoredSetterObjects_;
    Base::Vector<Base::Ref<PropertyTrigger>>
        authoredTriggerObjects_;
    Base::Vector<StyleSetter> authored_;
    Base::Vector<StylePropertyTrigger> authoredTriggers_;
    StyleProgram program_;
    ResourceDictionary resources_;
    bool sealed_ = false;
};

// Applies sealed style setters through EffectiveValueEngine, thereby retaining
// the existing precedence contract: local values and local expressions remain
// above Style values, and template/animation layers remain independent.
class AERO_API StyleManager final {
public:
    explicit StyleManager(
        EffectiveValueEngine& values,
        DependencyPropertyRegistry& properties) noexcept
        : values_(&values), properties_(&properties),
          applications_(),
          propertyChangedHandler_(
              this, &StyleManager::OnPropertyChanged) {}

    Base::Result<void> Apply(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> Clear(
        DependencyObject& object,
        const Style& style) noexcept;
    // Tree/object ownership code calls this before destroying an object.
    Base::Result<bool> DetachObject(
        DependencyObject& object) noexcept;
    const Style* AppliedStyle(
        const DependencyObject& object)
        const noexcept;

private:
    EffectiveValueEngine* values_ = nullptr;
    DependencyPropertyRegistry* properties_ = nullptr;
    struct Application final {
        DependencyObject* object = nullptr;
        const Style* style = nullptr;
    };
    Base::Vector<Application> applications_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;

    Base::Result<void> VerifyTarget(
        const DependencyObject& object,
        const Style& style) const noexcept;
    std::uint32_t FindApplication(
        const DependencyObject& object) const noexcept;
    Base::Result<void> ClearSetters(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> SubscribeTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
    void UnsubscribeTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> EvaluateTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> ClearTriggerSetters(
        DependencyObject& object,
        const Style& style) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
};

// Type-keyed default styles are resolved through the registered base-type
// chain and occupy the ThemeStyle provider below explicit Style values.
class AERO_API ThemeStyleRegistry final {
public:
    explicit ThemeStyleRegistry(
        const DependencyPropertyRegistry& properties) noexcept
        : properties_(&properties) {}

    Base::Result<void> TryRegister(
        TypeId controlType,
        const Style& style) noexcept;
    const Style* Find(TypeId controlType) const noexcept;

private:
    struct Entry final {
        TypeId controlType = InvalidTypeId;
        const Style* style = nullptr;
    };
    const DependencyPropertyRegistry* properties_ = nullptr;
    Base::Vector<Entry> entries_;
};

class AERO_API ThemeStyleManager final {
public:
    ThemeStyleManager(
        EffectiveValueEngine& values,
        const ThemeStyleRegistry& registry) noexcept
        : values_(&values), registry_(&registry) {}

    Base::Result<bool> ApplyDefault(
        DependencyObject& object) noexcept;
    Base::Result<bool> Clear(
        DependencyObject& object) noexcept;

private:
    struct Application final {
        DependencyObject* object = nullptr;
        const Style* style = nullptr;
    };
    EffectiveValueEngine* values_ = nullptr;
    const ThemeStyleRegistry* registry_ = nullptr;
    Base::Vector<Application> applications_;

    std::uint32_t FindApplication(
        const DependencyObject& object) const noexcept;
    Base::Result<void> ClearSetters(
        DependencyObject& object,
        const Style& style) noexcept;
};

} // namespace Aero::Presentation
