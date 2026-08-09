#pragma once

#include <Aero/Triggers/Conditions.hpp>

namespace Aero {

class AERO_GUI_API MultiTrigger : public TriggerBase {
    AERO_DECLARE_TYPE(MultiTrigger, TriggerBase)
public:
    MultiTrigger() noexcept : TriggerBase(StaticTypeId()) {}
    Result<void> AddCondition(Ref<Condition> condition) noexcept;
    void ClearConditions() noexcept { conditions_.Clear(); }
    Span<const Ref<Condition>> GetConditions() const noexcept {
        return {conditions_.Data(), conditions_.Size()};
    }
    Result<void> AddAuthoredSetter(Ref<Setter> setter) noexcept;
    void ClearAuthoredSetters() noexcept { authoredSetters_.Clear(); }
    Span<const Ref<Setter>> GetAuthoredSetters() const noexcept {
        return {authoredSetters_.Data(), authoredSetters_.Size()};
    }

private:
    Base::Vector<Ref<Condition>> conditions_;
    Base::Vector<Ref<Setter>> authoredSetters_;
};

} // namespace Aero
