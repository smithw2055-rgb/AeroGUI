#pragma once

#include <Aero/Media/Brush.hpp>

namespace Aero::Media {

class AERO_GUI_API GradientStop : public Freezable {
    AERO_DECLARE_TYPE(GradientStop, Freezable)
public:
    GradientStop() noexcept
        : Freezable(StaticTypeId()) {}
    ~GradientStop() override = default;

    double GetOffset() const noexcept;
    Color GetColor() const noexcept;
    void SetOffset(double value) noexcept;
    void SetColor(Color value) noexcept;

    AERO_DEPENDENCY_PROPERTY(double, Offset);
    AERO_DEPENDENCY_PROPERTY(Color, Color);

};
} // namespace Aero::Media
