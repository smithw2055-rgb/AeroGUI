#pragma once

#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ElasticEase : public EasingFunctionBase {
    AERO_DECLARE_TYPE(ElasticEase, EasingFunctionBase)
public:
    ElasticEase() noexcept
        : EasingFunctionBase(StaticTypeId(), Kind::Elastic) {}
    double GetOscillations() const noexcept {
        return GetValue(OscillationsProperty);
    }
    double GetSpringiness() const noexcept {
        return GetValue(SpringinessProperty);
    }
    void SetOscillations(double value) noexcept;
    void SetSpringiness(double value) noexcept;
    AERO_DEPENDENCY_PROPERTY(double, Oscillations);
    AERO_DEPENDENCY_PROPERTY(double, Springiness);
};

} // namespace Aero::Media::Animation
