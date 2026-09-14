#pragma once

#include <Aero/Media/GradientBrush.hpp>

namespace Aero::Media {

class AERO_GUI_API RadialGradientBrush
    : public GradientBrush {
    AERO_DECLARE_TYPE(RadialGradientBrush, GradientBrush)
public:
    RadialGradientBrush() noexcept
        : GradientBrush(StaticTypeId()) {}
    ~RadialGradientBrush() override = default;

    Point GetCenter() const noexcept;
    Point GetGradientOrigin() const noexcept;
    double GetRadiusX() const noexcept;
    double GetRadiusY() const noexcept;
    void SetCenter(Point value) noexcept;
    void SetGradientOrigin(Point value) noexcept;
    void SetRadiusX(double value) noexcept;
    void SetRadiusY(double value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Point, Center);
    AERO_DEPENDENCY_PROPERTY(Point, GradientOrigin);
    AERO_DEPENDENCY_PROPERTY(double, RadiusX);
    AERO_DEPENDENCY_PROPERTY(double, RadiusY);
};
} // namespace Aero::Media
