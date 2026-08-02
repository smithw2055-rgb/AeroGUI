#pragma once

#include <Aero/Data.hpp>
#include <Aero/Triggers/TriggerBase.hpp>

namespace Aero {

class AERO_API DataTrigger : public TriggerBase {
    AERO_DECLARE_TYPE(DataTrigger, TriggerBase)
public:
    DataTrigger() noexcept : TriggerBase(StaticTypeId()) {
        static_cast<void>(comparison_.Assign("Equal"));
    }
    Base::Ref<Data::Binding> GetBinding() const noexcept { return binding_; }
    void SetBinding(Base::Ref<Data::Binding> value) noexcept { binding_ = std::move(value); }
    Base::StringView GetPropertyName() const noexcept { return propertyName_.View(); }
    void SetPropertyName(Base::StringView value) noexcept;
    Base::StringView GetSourceName() const noexcept { return sourceName_.View(); }
    void SetSourceName(Base::StringView value) noexcept;
    const PropertyValue& GetAuthoredValue() const noexcept { return authoredValue_; }
    void SetAuthoredValue(const PropertyValue& value) noexcept {
        if (!value.IsUnset()) authoredValue_ = value;
    }
    Base::StringView GetComparison() const noexcept { return comparison_.View(); }
    void SetComparison(Base::StringView value) noexcept { (void)comparison_.Assign(value); }
    Base::Result<void> AddAuthoredSetter(Base::Ref<Setter> setter) noexcept;
    void ClearAuthoredSetters() noexcept { authoredSetters_.Clear(); }
    Base::Span<const Base::Ref<Setter>> GetAuthoredSetters() const noexcept {
        return {authoredSetters_.Data(), authoredSetters_.Size()};
    }

private:
    Base::Ref<Data::Binding> binding_;
    Base::String propertyName_;
    Base::String sourceName_;
    PropertyValue authoredValue_;
    Base::String comparison_;
    Base::Vector<Base::Ref<Setter>> authoredSetters_;
};

} // namespace Aero
