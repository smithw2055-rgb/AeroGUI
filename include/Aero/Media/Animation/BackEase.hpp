#pragma once

#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API BackEase : public EasingFunctionBase {
    AERO_DECLARE_TYPE(BackEase, EasingFunctionBase)
public:
    BackEase() noexcept
        : EasingFunctionBase(StaticTypeId(), Kind::Back) {}
    double GetAmplitude() const noexcept {
        return GetValue(AmplitudeProperty);
    }
    void SetAmplitude(double value) noexcept;
    AERO_DEPENDENCY_PROPERTY(double, Amplitude);
};

} // namespace Aero::Media::Animation
