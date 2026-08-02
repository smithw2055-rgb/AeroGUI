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
#include <Aero/Triggers/TriggerBase.hpp>
#include <Aero/Triggers/Trigger.hpp>
#include <Aero/Triggers/DataTrigger.hpp>
#include <Aero/Triggers/Conditions.hpp>
#include <Aero/Triggers/MultiTrigger.hpp>
#include <Aero/Triggers/MultiDataTrigger.hpp>
#include <utility>

namespace Aero::Internal {
class StylePrivate;
}

namespace Aero {

class Style;

class AERO_API SetterBaseCollection {
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

class AERO_API TriggerCollection {
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
class AERO_API Style : public Base::Object {
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
    Base::Result<void> AddSetter(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    Base::Result<void> AddSetter(
        const Setter& setter) noexcept;
    Base::Result<void> AddTrigger(
        TriggerPlan trigger) noexcept;
    Base::Result<void> AddTrigger(
        const Trigger& trigger) noexcept;

    class TriggerBuilder {
    public:
        template<class TOwner, class TValue>
        bool Set(
            const Meta::DependencyPropertyRef<TOwner, TValue>& property,
            const TValue& value) noexcept {
            if (!status_.IsOk()) return status_;
            Base::Result<PropertyValue> encoded =
                Meta::ValueCodec<TValue>::Encode(value);
            if (!encoded) return false;
            TriggerPlan trigger;
            trigger.property = condition_;
            trigger.value = std::move(conditionValue_);
            Base::Result<void> added =
                trigger.setters.PushBack({
                    property.Handle(),
                    std::move(encoded).Value()});
            if (!added) return false;
            return static_cast<bool>(owner_->AddTrigger(std::move(trigger)));
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
        explicit TriggerBuilder(Base::Status status) noexcept
            : status_(status) {}

        Style* owner_ = nullptr;
        DependencyPropertyHandle condition_;
        PropertyValue conditionValue_;
        Base::Status status_;
    };

    template<class TOwner, class TValue>
    bool Set(
        const Meta::DependencyPropertyRef<TOwner, TValue>& property,
        const TValue& value) noexcept {
        Base::Result<PropertyValue> encoded =
            Meta::ValueCodec<TValue>::Encode(value);
        if (!encoded) return false;
        return static_cast<bool>(AddSetter(property.Handle(), encoded.Value()));
    }
    template<class TOwner, class TValue>
    TriggerBuilder When(
        const Meta::DependencyPropertyRef<TOwner, TValue>& property,
        const TValue& value) noexcept {
        Base::Result<PropertyValue> encoded =
            Meta::ValueCodec<TValue>::Encode(value);
        if (!encoded) return TriggerBuilder(encoded.GetStatus());
        return TriggerBuilder(*this, property.Handle(), std::move(encoded).Value());
    }
    template<class TOwner, class TValue>
    TriggerBuilder When(
        const Meta::ReadOnlyPropertyRef<TOwner, TValue>& property,
        const TValue& value) noexcept {
        Base::Result<PropertyValue> encoded =
            Meta::ValueCodec<TValue>::Encode(value);
        if (!encoded) return TriggerBuilder(encoded.GetStatus());
        return TriggerBuilder(*this, property.Handle(), std::move(encoded).Value());
    }
    // Builder configuration is intentionally available only before Seal().
    bool SetTargetType(TypeId targetType) noexcept;
    bool SetBasedOn(const Style* basedOn) noexcept;
    bool SetBasedOn(Base::Ref<Base::Object> basedOn) noexcept;
    Base::Result<void> AddAuthoredSetter(Base::Ref<Setter> setter) noexcept;
    Base::Result<void> AddAuthoredTrigger(Base::Ref<Trigger> trigger) noexcept;
    void ClearAuthoredSetters() noexcept;
    void ClearAuthoredTriggers() noexcept;
    Base::Span<const Base::Ref<Setter>> GetAuthoredSetters() const noexcept {
        return {authoredSetterObjects_.Data(), authoredSetterObjects_.Size()};
    }
    Base::Span<const Base::Ref<Trigger>> GetAuthoredTriggers() const noexcept {
        return {authoredTriggerObjects_.Data(), authoredTriggerObjects_.Size()};
    }
    TypeId GetTargetType() const noexcept {
        return sealed_ ? program_.TargetType() : targetType_;
    }
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
    void SetResources(Base::Ref<ResourceDictionary> value) noexcept;

private:
    friend class ::Aero::Internal::StylePrivate;

    Base::Result<void> SealRuntime(const void* properties) noexcept;

    struct Impl {
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
    Base::Vector<Base::Ref<Setter>> authoredSetterObjects_;
    Base::Vector<Base::Ref<Trigger>> authoredTriggerObjects_;
    Base::Vector<StyleSetter> authored_;
    Base::Vector<TriggerPlan> authoredTriggers_;
    Impl program_;
    ResourceDictionary resources_;
    bool sealed_ = false;
};

} // namespace Aero
