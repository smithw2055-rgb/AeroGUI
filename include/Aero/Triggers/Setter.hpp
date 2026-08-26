#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Triggers/SetterBase.hpp>
#include <Aero/Value.hpp>

namespace Aero {

using Meta::DependencyPropertyHandle;
using Meta::PropertyValue;
using Meta::TypeId;

class AERO_GUI_API Setter : public SetterBase {
    AERO_DECLARE_TYPE(Setter, SetterBase)
public:
    explicit Setter(TypeId runtimeType = StaticTypeId()) noexcept
        : SetterBase(runtimeType) {}

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
    Result<void> Set(
        const Meta::DependencyPropertyRef<TOwner, TValue>& property,
        const TValue& value) noexcept {
        Result<PropertyValue> encoded = Meta::ValueCodec<TValue>::Encode(value);
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
    void SetPropertyName(StringView value) noexcept;
    void SetTargetName(StringView value) noexcept;
    void SetAuthoredValue(const PropertyValue& value) noexcept;
    StringView GetPropertyName() const noexcept { return propertyName_.View(); }
    StringView GetTargetName() const noexcept { return targetName_.View(); }
    const PropertyValue& GetAuthoredValue() const noexcept { return authoredValue_; }
    bool GetIsAuthored() const noexcept {
        return !propertyName_.Empty() && !authoredValue_.IsUnset();
    }
    Result<void> Resolve(DependencyPropertyHandle property,
                               const PropertyValue& value) noexcept;

private:
    DependencyPropertyHandle property_;
    PropertyValue value_;
    String propertyName_;
    String targetName_;
    PropertyValue authoredValue_;
};

} // namespace Aero
