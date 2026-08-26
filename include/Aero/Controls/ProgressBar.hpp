#pragma once

#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/Primitives/RangeBase.hpp>

namespace Aero::Controls {

class AERO_GUI_API ProgressBar : public Primitives::RangeBase {
    AERO_DECLARE_TYPE(ProgressBar, Primitives::RangeBase)
public:
    ProgressBar() noexcept : Primitives::RangeBase(StaticTypeId()) {}
    ~ProgressBar() override = default;

    bool GetIsIndeterminate() const noexcept;
    Orientation GetOrientation() const noexcept;
    void SetIsIndeterminate(
        bool value) noexcept;
    void SetOrientation(
        Orientation value) noexcept;
    double GetNormalizedValue() const noexcept;

    inline static constexpr DependencyProperty<bool> IsIndeterminateProperty{"IsIndeterminate"};
    inline static constexpr DependencyProperty<Orientation> OrientationProperty{"Orientation"};
};

} // namespace Aero::Controls
