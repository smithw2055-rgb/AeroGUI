#pragma once

// Trigger family: setters, TriggerBase, conditions, and the trigger types.
#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Value.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Data/Binding.hpp>

namespace Aero {
namespace Markup { class XamlStyleSchemaFacet; }
namespace Controls { class TemplateBuilder; class TemplateEngine; }
namespace Interactivity { class TriggerAction; }


using Meta::TypeId;

class AERO_GUI_API SetterBase : public Base::Object {
    AERO_DECLARE_TYPE(SetterBase, Base::Object)

public:
    Meta::TypeId RuntimeType() const noexcept override { return runtimeType_; }

protected:
    explicit SetterBase(Meta::TypeId runtimeType) noexcept : runtimeType_(runtimeType) {}
    ~SetterBase() override = default;

private:
    Meta::TypeId runtimeType_ = StaticTypeId();
};

using Meta::DependencyPropertyHandle;
using Meta::PropertyValue;

class AERO_GUI_API Setter : public SetterBase {
    AERO_DECLARE_TYPE(Setter, SetterBase)

public:
    explicit Setter(TypeId runtimeType = StaticTypeId()) noexcept : SetterBase(runtimeType) {}
    ~Setter() override;

    DependencyPropertyHandle GetProperty() const noexcept { return property_; }
    const PropertyValue& GetValue() const noexcept { return value_; }
    void SetProperty(DependencyPropertyHandle value) noexcept {
        if (!value.IsValid()) return;
        property_ = value;
    }
    void SetValue(const PropertyValue& value) noexcept {
        if (value.IsUnset()) return;
        value_ = value;
    }
    template<class TOwner, class TValue>
    void Set(const Meta::DependencyPropertyRef<TOwner, TValue>& property, const TValue& value) noexcept {
        if (!property.Handle().IsValid()) { AERO_ASSERT(false); return; }
        auto encoded = Meta::ValueCodec<TValue>::Encode(value);
        if constexpr (std::is_same_v<decltype(encoded), Result<PropertyValue>> ||
                      std::is_same_v<decltype(encoded), Result<Value>>) {
            if (!encoded) { AERO_ASSERT(false); return; }
            SetProperty(property.Handle());
            SetValue(encoded.Value());
        } else {
            if (encoded.IsUnset()) { AERO_ASSERT(false); return; }
            SetProperty(property.Handle());
            SetValue(encoded);
        }
    }
    void SetPropertyName(StringView value) noexcept;
    void SetTargetName(StringView value) noexcept;
    StringView GetPropertyName() const noexcept;
    StringView GetTargetName() const noexcept;
    const PropertyValue& GetAuthoredValue() const noexcept;
    bool GetIsAuthored() const noexcept;
    void ReleaseAuthored() noexcept;

private:
    friend class Style;
    friend class Markup::XamlStyleSchemaFacet;
    friend class Controls::TemplateBuilder;
    friend class Controls::TemplateEngine;
    void SetAuthoredValue(const PropertyValue& value) noexcept;
    bool Resolve(DependencyPropertyHandle property, const PropertyValue& value) noexcept;

    struct AuthoredData {
        AuthoredData() noexcept {}
        String propertyName;
        String targetName;
        PropertyValue authoredValue;
    };
    AuthoredData* EnsureAuthored() noexcept;

    DependencyPropertyHandle property_;
    PropertyValue value_;
    AuthoredData* authored_ = nullptr;
};

class AERO_GUI_API TriggerBase : public Base::Object {
    AERO_DECLARE_TYPE(TriggerBase, Base::Object)

public:
    TypeId RuntimeType() const noexcept override { return runtimeType_; }
    void AddEnterAction(Ref<Base::Object> action) noexcept;
    void AddExitAction(Ref<Base::Object> action) noexcept;
    void ClearEnterActions() noexcept { enterActions_.Clear(); }
    void ClearExitActions() noexcept { exitActions_.Clear(); }
    Span<const Ref<Base::Object>> GetEnterActions() const noexcept {
        return {enterActions_.Data(), enterActions_.Size()};
    }
    Span<const Ref<Base::Object>> GetExitActions() const noexcept { return {exitActions_.Data(), exitActions_.Size()}; }
    void AddBehavior(Ref<Base::Object> behavior) noexcept {
        if (!behavior) { AERO_ASSERT(false); return; }
        behaviors_.PushBack(std::move(behavior));
    }
    void ClearBehaviors() noexcept { behaviors_.Clear(); }
    Span<const Ref<Base::Object>> GetBehaviors() const noexcept { return {behaviors_.Data(), behaviors_.Size()}; }

protected:
    explicit TriggerBase(TypeId runtimeType) noexcept : runtimeType_(runtimeType) {}
    ~TriggerBase() override = default;

private:
    TypeId runtimeType_ = StaticTypeId();
    Base::Vector<Ref<Base::Object>> enterActions_;
    Base::Vector<Ref<Base::Object>> exitActions_;
    Base::Vector<Ref<Base::Object>> behaviors_;
};

class AERO_GUI_API SetterTriggerBase : public TriggerBase {
    AERO_DECLARE_TYPE(SetterTriggerBase, TriggerBase)

public:
    void AddAuthoredSetter(Ref<Setter> setter) noexcept;
    void ClearAuthoredSetters() noexcept { authoredSetters_.Clear(); }
    Span<const Ref<Setter>> GetAuthoredSetters() const noexcept {
        return {authoredSetters_.Data(), authoredSetters_.Size()};
    }

protected:
    explicit SetterTriggerBase(TypeId runtimeType) noexcept : TriggerBase(runtimeType) {}
    ~SetterTriggerBase() override = default;

private:
    Base::Vector<Ref<Setter>> authoredSetters_;
};

struct AuthoredTriggerData {
    AuthoredTriggerData() noexcept {}
    String propertyName;
    String sourceName;
    PropertyValue authoredValue;
};

class AERO_GUI_API Condition : public Base::Object {
    AERO_DECLARE_TYPE(Condition, Base::Object)

public:
    Condition() noexcept = default;
    ~Condition() override;

    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Ref<Data::Binding> GetBinding() const noexcept { return binding_; }
    void SetBinding(Ref<Data::Binding> value) noexcept { binding_ = std::move(value); }
    StringView GetPropertyName() const noexcept;
    void SetPropertyName(StringView value) noexcept;
    StringView GetSourceName() const noexcept;
    void SetSourceName(StringView value) noexcept;
    const PropertyValue& GetAuthoredValue() const noexcept;
    void ReleaseAuthored() noexcept;

private:
    friend class Style;
    friend class Markup::XamlStyleSchemaFacet;
    friend class Controls::TemplateBuilder;
    friend class Controls::TemplateEngine;
    void SetAuthoredValue(const PropertyValue& value) noexcept;

    using AuthoredData = AuthoredTriggerData;
    AuthoredData* EnsureAuthored() noexcept;

    Ref<Data::Binding> binding_;
    AuthoredData* authored_ = nullptr;
};

class Style;

class AERO_GUI_API Trigger : public SetterTriggerBase {
    AERO_DECLARE_TYPE_NAMED(Trigger, SetterTriggerBase, "urn:aero", "Trigger")

public:
    explicit Trigger(TypeId runtimeType = StaticTypeId()) noexcept : SetterTriggerBase(runtimeType) {}
    ~Trigger() override;

    DependencyPropertyHandle GetProperty() const noexcept { return property_; }
    const PropertyValue& GetValue() const noexcept { return value_; }
    void SetProperty(DependencyPropertyHandle value) noexcept;
    void SetValue(const PropertyValue& value) noexcept;
    void AddSetter(const Setter& setter) noexcept;
    void SetPropertyName(StringView value) noexcept;
    StringView GetSourceName() const noexcept;
    void SetSourceName(StringView value) noexcept;
    StringView GetPropertyName() const noexcept;
    const PropertyValue& GetAuthoredValue() const noexcept;
    bool GetIsAuthored() const noexcept;
    void ReleaseAuthored() noexcept;

private:
    friend class Style;
    friend class Markup::XamlStyleSchemaFacet;
    friend class Controls::TemplateBuilder;
    friend class Controls::TemplateEngine;
    void SetAuthoredValue(const PropertyValue& value) noexcept;

    using AuthoredData = AuthoredTriggerData;
    AuthoredData* EnsureAuthored() noexcept;

    DependencyPropertyHandle property_;
    PropertyValue value_;
    AuthoredData* authored_ = nullptr;
};

class AERO_GUI_API DataTrigger : public SetterTriggerBase {
    AERO_DECLARE_TYPE(DataTrigger, SetterTriggerBase)

public:
    DataTrigger() noexcept : SetterTriggerBase(StaticTypeId()) { static_cast<void>(comparison_.Assign("Equal")); }
    ~DataTrigger() override;

    Ref<Data::Binding> GetBinding() const noexcept { return binding_; }
    void SetBinding(Ref<Data::Binding> value) noexcept { binding_ = std::move(value); }
    StringView GetPropertyName() const noexcept;
    void SetPropertyName(StringView value) noexcept;
    StringView GetSourceName() const noexcept;
    void SetSourceName(StringView value) noexcept;
    const PropertyValue& GetAuthoredValue() const noexcept;
    StringView GetComparison() const noexcept { return comparison_.View(); }
    void SetComparison(StringView value) noexcept { (void)comparison_.Assign(value); }
    void ReleaseAuthored() noexcept;

private:
    friend class Style;
    friend class Markup::XamlStyleSchemaFacet;
    friend class Controls::TemplateBuilder;
    friend class Controls::TemplateEngine;
    void SetAuthoredValue(const PropertyValue& value) noexcept;

    using AuthoredData = AuthoredTriggerData;
    AuthoredData* EnsureAuthored() noexcept;

    Ref<Data::Binding> binding_;
    String comparison_;
    AuthoredData* authored_ = nullptr;
};

class AERO_GUI_API MultiTrigger : public SetterTriggerBase {
    AERO_DECLARE_TYPE(MultiTrigger, SetterTriggerBase)

public:
    MultiTrigger() noexcept : SetterTriggerBase(StaticTypeId()) {}
    void AddCondition(Ref<Condition> condition) noexcept;
    void ClearConditions() noexcept { conditions_.Clear(); }
    Span<const Ref<Condition>> GetConditions() const noexcept { return {conditions_.Data(), conditions_.Size()}; }

private:
    Base::Vector<Ref<Condition>> conditions_;
};

class AERO_GUI_API MultiDataTrigger : public SetterTriggerBase {
    AERO_DECLARE_TYPE(MultiDataTrigger, SetterTriggerBase)

public:
    MultiDataTrigger() noexcept : SetterTriggerBase(StaticTypeId()) {}
    void AddCondition(Ref<Condition> condition) noexcept;
    void ClearConditions() noexcept { conditions_.Clear(); }
    Span<const Ref<Condition>> GetConditions() const noexcept { return {conditions_.Data(), conditions_.Size()}; }

private:
    Base::Vector<Ref<Condition>> conditions_;
};


class AERO_GUI_API EventTrigger : public TriggerBase {
    AERO_DECLARE_TYPE(EventTrigger, TriggerBase)

public:
    EventTrigger() noexcept : EventTrigger(StaticTypeId()) {}
    StringView GetRoutedEvent() const noexcept { return routedEvent_.View(); }
    StringView GetEventName() const noexcept { return routedEvent_.View(); }
    StringView GetSourceName() const noexcept { return sourceName_.View(); }
    void SetRoutedEvent(StringView value) noexcept;
    void SetEventName(StringView value) noexcept { SetRoutedEvent(value); }
    void SetSourceName(StringView value) noexcept;
    void AddAction(Ref<Interactivity::TriggerAction> value) noexcept;
    void ClearActions() noexcept;
    Span<const Ref<Interactivity::TriggerAction>> GetActions() const noexcept {
        return {actions_.Data(), actions_.Size()};
    }
    void AddConditionBehavior(Ref<Base::Object> value) noexcept { behaviors_.PushBack(std::move(value)); }
    void ClearConditionBehaviors() noexcept { behaviors_.Clear(); }
    Span<const Ref<Base::Object>> GetBehaviors() const noexcept {
        return {behaviors_.Data(), behaviors_.Size()};
    }

protected:
    explicit EventTrigger(Meta::TypeId runtimeType) noexcept : TriggerBase(runtimeType) {}

private:
    String routedEvent_;
    String sourceName_;
    Base::Vector<Ref<Interactivity::TriggerAction>> actions_;
    Base::Vector<Ref<Base::Object>> behaviors_;
};

} // namespace Aero

namespace Aero::Media::Animation {
using EventTrigger = ::Aero::EventTrigger;
} // namespace Aero::Media::Animation
