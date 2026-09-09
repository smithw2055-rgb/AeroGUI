#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Triggers/TriggerBase.hpp>
#include <Aero/Triggers/Trigger.hpp>

#include <Aero/Triggers/DataTrigger.hpp>
#include <Aero/Triggers/Conditions.hpp>
#include <Aero/Triggers/MultiTrigger.hpp>
#include <Aero/Triggers/MultiDataTrigger.hpp>
#include <utility>

namespace Aero {

using Meta::DependencyPropertyHandle;
using Meta::InvalidTypeId;
using Meta::PropertyValue;
using Meta::TypeId;

class Style;

class AERO_GUI_API SetterBaseCollection {
public:
    std::uint32_t GetCount() const noexcept;
    SetterBase* GetItem(std::uint32_t index) const noexcept;
    bool GetIsEmpty() const noexcept { return GetCount() == 0U; }
    void Add(Ref<SetterBase> setter) noexcept;
    void Add(Ref<Setter> setter) noexcept;
    void Clear() noexcept;

private:
    friend class Style;
    explicit SetterBaseCollection(Style& owner) noexcept : owner_(&owner) {}
    Style* owner_ = nullptr;
};

class AERO_GUI_API TriggerCollection {
public:
    std::uint32_t GetCount() const noexcept;
    TriggerBase* GetItem(std::uint32_t index) const noexcept;
    bool GetIsEmpty() const noexcept { return GetCount() == 0U; }
    void Add(Ref<TriggerBase> trigger) noexcept;
    void Clear() noexcept;

private:
    friend class Style;
    explicit TriggerCollection(Style& owner) noexcept : owner_(&owner) {}
    Style* owner_ = nullptr;
};

// WPF-shaped Style authoring surface. Runtime plans and provider precedence are
// compiled privately when the style is sealed.
class AERO_GUI_API Style : public Base::Object {
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
    ~Style() override;

    Style(const Style&) = delete;
    Style& operator=(const Style&) = delete;

    TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    void AddSetter(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    void AddSetter(
        const Setter& setter) noexcept;
    void AddTrigger(
        const Trigger& trigger) noexcept;
    void AddTrigger(
        const DataTrigger& trigger) noexcept;
    void AddTrigger(
        const MultiDataTrigger& trigger) noexcept;

    class TriggerBuilder {
    public:
        template<class TOwner, class TValue>
        void Set(const Meta::DependencyPropertyRef<TOwner, TValue>& property, const TValue& value) noexcept {
            if (!status_.IsOk()) { AERO_ASSERT(false); return; }
            Result<PropertyValue> encoded =
                Meta::ValueCodec<TValue>::Encode(value);
            if (!encoded) { AERO_ASSERT(false); return; }
            owner_->AddPropertyTrigger(
                condition_, conditionValue_, property.Handle(),
                std::move(encoded).Value());
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
    void Set(const Meta::DependencyPropertyRef<TOwner, TValue>& property, const TValue& value) noexcept {
        Result<PropertyValue> encoded =
            Meta::ValueCodec<TValue>::Encode(value);
        if (!encoded) { AERO_ASSERT(false); return; }
        AddSetter(property.Handle(), encoded.Value());
    }
    template<class TOwner, class TValue>
    TriggerBuilder When(
        const Meta::DependencyPropertyRef<TOwner, TValue>& property,
        const TValue& value) noexcept {
        Result<PropertyValue> encoded =
            Meta::ValueCodec<TValue>::Encode(value);
        if (!encoded) return TriggerBuilder(encoded.GetStatus());
        return TriggerBuilder(*this, property.Handle(), std::move(encoded).Value());
    }
    template<class TOwner, class TValue>
    TriggerBuilder When(
        const Meta::ReadOnlyPropertyRef<TOwner, TValue>& property,
        const TValue& value) noexcept {
        Result<PropertyValue> encoded =
            Meta::ValueCodec<TValue>::Encode(value);
        if (!encoded) return TriggerBuilder(encoded.GetStatus());
        return TriggerBuilder(*this, property.Handle(), std::move(encoded).Value());
    }
    // Builder configuration is intentionally available only before Seal().
    bool SetTargetType(TypeId targetType) noexcept;
    bool SetBasedOn(const Style* basedOn) noexcept;
    bool SetBasedOn(Ref<Base::Object> basedOn) noexcept;
    void AddAuthoredSetter(Ref<SetterBase> setter) noexcept;
    void AddAuthoredSetter(Ref<Setter> setter) noexcept;
    void AddAuthoredTrigger(Ref<TriggerBase> trigger) noexcept;
    void ClearAuthoredSetters() noexcept;
    void ClearAuthoredTriggers() noexcept;
    Span<const Ref<SetterBase>> GetAuthoredSetters() const noexcept {
        return {authoredSetterObjects_.Data(), authoredSetterObjects_.Size()};
    }
    Span<const Ref<TriggerBase>> GetAuthoredTriggers() const noexcept {
        return {authoredTriggerObjects_.Data(), authoredTriggerObjects_.Size()};
    }
    TypeId GetTargetType() const noexcept;
    const Style* GetBasedOn() const noexcept { return basedOn_; }
    SetterBaseCollection GetSetters() noexcept { return SetterBaseCollection(*this); }
    TriggerCollection GetTriggers() noexcept { return TriggerCollection(*this); }
    bool GetIsSealed() const noexcept { return sealed_; }
    ResourceDictionary& GetResources() noexcept { return resources_; }
    const ResourceDictionary& GetResources() const noexcept { return resources_; }
    void SetResources(Ref<ResourceDictionary> value) noexcept;

private:
    void AddPropertyTrigger(
        DependencyPropertyHandle condition,
        const PropertyValue& conditionValue,
        DependencyPropertyHandle property,
        PropertyValue value) noexcept;
    // Compiled by SealStyle / markup finalize; not a public authoring API.
    Result<void> Seal(const Meta::DependencyPropertyRegistry& properties) noexcept;

    struct Program;
    friend struct Program;
    friend class StyleEngine;
    friend Result<void> SealStyle(
        Style& style,
        const Meta::DependencyPropertyRegistry& properties) noexcept;
    friend Span<const struct StyleSetter> StyleRuntimeSetters(
        const Style& style) noexcept;
    friend Span<const struct TriggerPlan> StyleRuntimeTriggers(
        const Style& style) noexcept;
    friend Result<void> ApplyStyleSetters(
        const Style& style,
        DependencyObject& object,
        class StyleProviderSession& values) noexcept;
    friend Result<void> ClearStyleSetters(
        const Style& style,
        DependencyObject& object,
        class StyleProviderSession& values) noexcept;

    TypeId runtimeType_ = StaticTypeId();
    TypeId targetType_ = InvalidTypeId;
    const Style* basedOn_ = nullptr;
    Ref<Base::Object> basedOnOwner_;
    Base::Vector<Ref<SetterBase>> authoredSetterObjects_;
    Base::Vector<Ref<TriggerBase>> authoredTriggerObjects_;
    Base::IAllocator* implAllocator_ = nullptr;
    Program* program_ = nullptr;
    ResourceDictionary resources_;
    bool sealed_ = false;
};

} // namespace Aero
