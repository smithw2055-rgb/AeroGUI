#pragma once

#include <Aero/Media/GradientBrush.hpp>

namespace Aero::Media {

class AERO_GUI_API LinearGradientBrush
    : public GradientBrush {
    AERO_DECLARE_TYPE(LinearGradientBrush, GradientBrush)
public:
    LinearGradientBrush() noexcept
        : GradientBrush(StaticTypeId()) {}
    ~LinearGradientBrush() override = default;

    Point GetStartPoint() const noexcept;
    Point GetEndPoint() const noexcept;
    void SetStartPoint(Point value) noexcept;
    void SetEndPoint(Point value) noexcept;

    inline static constexpr DependencyProperty<Point> StartPointProperty{"StartPoint"};
    inline static constexpr DependencyProperty<Point> EndPointProperty{"EndPoint"};
};
} // namespace Aero::Media
