#pragma once

#include <Aero/Data/Binding.hpp>
#include <Aero/Triggers/TriggerBase.hpp>

namespace Aero {

class AERO_GUI_API DataTrigger : public TriggerBase {
    AERO_DECLARE_TYPE(DataTrigger, TriggerBase)
public:
    DataTrigger() noexcept : TriggerBase(StaticTypeId()) {
        static_cast<void>(comparison_.Assign("Equal"));
    }
    Ref<Data::Binding> GetBinding() const noexcept { return binding_; }
    void SetBinding(Ref<Data::Binding> value) noexcept { binding_ = std::move(value); }
    StringView GetPropertyName() const noexcept { return propertyName_.View(); }
    void SetPropertyName(StringView value) noexcept;
    StringView GetSourceName() const noexcept { return sourceName_.View(); }
    void SetSourceName(StringView value) noexcept;
    const PropertyValue& GetAuthoredValue() const noexcept { return authoredValue_; }
    void SetAuthoredValue(const PropertyValue& value) noexcept {
        if (!value.IsUnset()) authoredValue_ = value;
    }
    StringView GetComparison() const noexcept { return comparison_.View(); }
    void SetComparison(StringView value) noexcept { (void)comparison_.Assign(value); }
    Result<void> AddAuthoredSetter(Ref<Setter> setter) noexcept;
    void ClearAuthoredSetters() noexcept { authoredSetters_.Clear(); }
    Span<const Ref<Setter>> GetAuthoredSetters() const noexcept {
        return {authoredSetters_.Data(), authoredSetters_.Size()};
    }

private:
    Ref<Data::Binding> binding_;
    String propertyName_;
    String sourceName_;
    PropertyValue authoredValue_;
    String comparison_;
    Base::Vector<Ref<Setter>> authoredSetters_;
};

} // namespace Aero
