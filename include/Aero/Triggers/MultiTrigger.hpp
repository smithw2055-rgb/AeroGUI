#pragma once

#include <Aero/Triggers/Conditions.hpp>

namespace Aero {

class AERO_GUI_API MultiTrigger : public TriggerBase {
    AERO_DECLARE_TYPE(MultiTrigger, TriggerBase)
public:
    MultiTrigger() noexcept : TriggerBase(StaticTypeId()) {}
    Base::Result<void> AddCondition(Base::Ref<Condition> condition) noexcept;
    void ClearConditions() noexcept { conditions_.Clear(); }
    Base::Span<const Base::Ref<Condition>> GetConditions() const noexcept {
        return {conditions_.Data(), conditions_.Size()};
    }
    Base::Result<void> AddAuthoredSetter(Base::Ref<Setter> setter) noexcept;
    void ClearAuthoredSetters() noexcept { authoredSetters_.Clear(); }
    Base::Span<const Base::Ref<Setter>> GetAuthoredSetters() const noexcept {
        return {authoredSetters_.Data(), authoredSetters_.Size()};
    }

private:
    Base::Vector<Base::Ref<Condition>> conditions_;
    Base::Vector<Base::Ref<Setter>> authoredSetters_;
};

} // namespace Aero
