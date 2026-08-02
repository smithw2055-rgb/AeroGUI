#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero {

using Meta::DependencyPropertyHandle;
using Meta::PropertyValue;
using Meta::TypeId;

struct StyleSetter {
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct StyleTriggerSetter {
    DependencyPropertyHandle property;
    PropertyValue value;
};

// Compact representation consumed by the private style engine after a
// Trigger has been sealed.  It is intentionally separate from authoring
// objects so a style can be compiled without retaining the source graph.
struct TriggerPlan {
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

class AERO_API Setter : public SetterBase {
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
    bool Set(const Meta::DependencyPropertyRef<TOwner, TValue>& property,
             const TValue& value) noexcept {
        Base::Result<PropertyValue> encoded = Meta::ValueCodec<TValue>::Encode(value);
        if (!encoded || !property.Handle().IsValid()) return false;
        SetProperty(property.Handle());
        SetValue(encoded.Value());
        return true;
    }
    void SetPropertyName(Base::StringView value) noexcept;
    void SetTargetName(Base::StringView value) noexcept;
    void SetAuthoredValue(const PropertyValue& value) noexcept;
    Base::StringView GetPropertyName() const noexcept { return propertyName_.View(); }
    Base::StringView GetTargetName() const noexcept { return targetName_.View(); }
    const PropertyValue& GetAuthoredValue() const noexcept { return authoredValue_; }
    bool GetIsAuthored() const noexcept {
        return !propertyName_.Empty() && !authoredValue_.IsUnset();
    }
    Base::Result<void> Resolve(DependencyPropertyHandle property,
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
    TypeId RuntimeType() const noexcept override { return runtimeType_; }
    Base::Result<void> AddEnterAction(Base::Ref<Base::Object> action) noexcept;
    Base::Result<void> AddExitAction(Base::Ref<Base::Object> action) noexcept;
    void ClearEnterActions() noexcept { enterActions_.Clear(); }
    void ClearExitActions() noexcept { exitActions_.Clear(); }
    Base::Span<const Base::Ref<Base::Object>> GetEnterActions() const noexcept {
        return {enterActions_.Data(), enterActions_.Size()};
    }
    Base::Span<const Base::Ref<Base::Object>> GetExitActions() const noexcept {
        return {exitActions_.Data(), exitActions_.Size()};
    }

protected:
    explicit TriggerBase(TypeId runtimeType) noexcept : runtimeType_(runtimeType) {}
    ~TriggerBase() override = default;

private:
    TypeId runtimeType_ = StaticTypeId();
    Base::Vector<Base::Ref<Base::Object>> enterActions_;
    Base::Vector<Base::Ref<Base::Object>> exitActions_;
};

} // namespace Aero
