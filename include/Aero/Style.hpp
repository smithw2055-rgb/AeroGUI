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
#include <Aero/Triggers.hpp>


namespace Aero {

namespace Markup { class XamlStyleSchemaFacet; }
class StoryboardHost;

using Meta::DependencyPropertyHandle;
using Meta::InvalidTypeId;
using Meta::PropertyValue;
using Meta::TypeId;

class Style;

class AERO_GUI_API SetterBaseCollection {
public:
    void Add(Ref<SetterBase> setter) noexcept;
    void Add(Ref<Setter> setter) noexcept;

    std::uint32_t GetCount() const noexcept;
    SetterBase* GetItem(std::uint32_t index) const noexcept;
    bool GetIsEmpty() const noexcept { return GetCount() == 0U; }
    void Clear() noexcept;

private:
    friend class Style;

    explicit SetterBaseCollection(Style& owner) noexcept : owner_(&owner) {}

    Style* owner_ = nullptr;
};

class AERO_GUI_API TriggerCollection {
public:
    void Add(Ref<TriggerBase> trigger) noexcept;

    std::uint32_t GetCount() const noexcept;
    TriggerBase* GetItem(std::uint32_t index) const noexcept;
    bool GetIsEmpty() const noexcept { return GetCount() == 0U; }
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
    explicit Style(TypeId targetType, const Style* basedOn = nullptr) noexcept;
    Style(TypeId targetType, const Style* basedOn, TypeId runtimeType) noexcept;
    ~Style() override;
    Style(const Style&) = delete;
    Style& operator=(const Style&) = delete;

    TypeId RuntimeType() const noexcept override { return runtimeType_; }
    void AddSetter(DependencyPropertyHandle property, const PropertyValue& value) noexcept;
    void AddSetter(const Setter& setter) noexcept;
    void AddTrigger(const Trigger& trigger) noexcept;
    void AddTrigger(const DataTrigger& trigger) noexcept;
    void AddTrigger(const MultiDataTrigger& trigger) noexcept;

    TypeId GetTargetType() const noexcept;
    // Builder configuration is intentionally available only before Seal().
    bool SetTargetType(TypeId targetType) noexcept;
    const Style* GetBasedOn() const noexcept { return basedOn_; }
    bool SetBasedOn(const Style* basedOn) noexcept;
    bool SetBasedOn(Ref<Base::Object> basedOn) noexcept;
    SetterBaseCollection GetSetters() noexcept { return SetterBaseCollection(*this); }
    TriggerCollection GetTriggers() noexcept { return TriggerCollection(*this); }
    bool GetIsSealed() const noexcept { return sealed_; }
    ResourceDictionary& GetResources() noexcept { return resources_; }
    const ResourceDictionary& GetResources() const noexcept { return resources_; }
    void SetResources(Ref<ResourceDictionary> value) noexcept;

private:
    friend struct Program;
    friend class StyleEngine;
    friend class SetterBaseCollection;
    friend class TriggerCollection;
    friend class Markup::XamlStyleSchemaFacet;
    friend class StoryboardHost;

    struct Program;

    void AddPropertyTrigger(DependencyPropertyHandle condition, const PropertyValue& conditionValue,
        DependencyPropertyHandle property,
        PropertyValue value) noexcept;
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
    // Compiled by Style::Program::Seal / markup finalize; not a public authoring API.
    Result<void> Seal(const Meta::DependencyPropertyRegistry& properties) noexcept;
    // WPF-parity no-arg hook. Called at the end of Seal(); override to
    // validate without touching DependencyPropertyRegistry internals.
    virtual void OnSeal() noexcept {}

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
