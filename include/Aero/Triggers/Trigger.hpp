#pragma once

#include <Aero/Triggers/TriggerBase.hpp>

namespace Aero {

class Style;

class AERO_GUI_API Trigger : public TriggerBase {
    AERO_DECLARE_TYPE_NAMED(Trigger, TriggerBase, "urn:aero", "Trigger")
public:
    explicit Trigger(TypeId runtimeType = StaticTypeId()) noexcept
        : TriggerBase(runtimeType) {}
    DependencyPropertyHandle GetProperty() const noexcept { return property_; }
    const PropertyValue& GetValue() const noexcept { return value_; }
    void SetProperty(DependencyPropertyHandle value) noexcept;
    void SetValue(const PropertyValue& value) noexcept;
    Result<void> AddSetter(const Setter& setter) noexcept;
    void SetPropertyName(StringView value) noexcept;
    StringView GetSourceName() const noexcept { return sourceName_.View(); }
    void SetSourceName(StringView value) noexcept;
    void SetAuthoredValue(const PropertyValue& value) noexcept;
    Result<void> AddAuthoredSetter(Ref<Setter> setter) noexcept;
    void ClearAuthoredSetters() noexcept;
    StringView GetPropertyName() const noexcept { return propertyName_.View(); }
    const PropertyValue& GetAuthoredValue() const noexcept { return authoredValue_; }
    Span<const Ref<Setter>> GetAuthoredSetters() const noexcept {
        return {authoredSetters_.Data(), authoredSetters_.Size()};
    }
    bool GetIsAuthored() const noexcept {
        return !propertyName_.Empty() && !authoredValue_.IsUnset() &&
               !authoredSetters_.Empty();
    }
private:
    friend class Style;

    DependencyPropertyHandle property_;
    PropertyValue value_;
    Base::Vector<DependencyPropertyHandle> setterProperties_;
    Base::Vector<PropertyValue> setterValues_;
    String propertyName_;
    String sourceName_;
    PropertyValue authoredValue_;
    Base::Vector<Ref<Setter>> authoredSetters_;
};

} // namespace Aero
