#pragma once

#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ElasticEase : public EasingFunctionBase {
    AERO_DECLARE_TYPE(ElasticEase, EasingFunctionBase)
public:
    ElasticEase() noexcept
        : EasingFunctionBase(StaticTypeId(), Kind::Elastic) {}
    double GetOscillations() const noexcept {
        return GetValueOr(OscillationsProperty, 3.0);
    }
    double GetSpringiness() const noexcept {
        return GetValueOr(SpringinessProperty, 3.0);
    }
    void SetOscillations(double value) noexcept;
    void SetSpringiness(double value) noexcept;
    inline static constexpr DependencyProperty<double> OscillationsProperty{"Oscillations"};
    inline static constexpr DependencyProperty<double> SpringinessProperty{"Springiness"};
};

} // namespace Aero::Media::Animation
