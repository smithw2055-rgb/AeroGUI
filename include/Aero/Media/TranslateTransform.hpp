#pragma once

#include <Aero/Media/Transform.hpp>

namespace Aero::Media {

class AERO_GUI_API TranslateTransform : public Transform {
    AERO_DECLARE_TYPE(TranslateTransform, Transform)
public:
    TranslateTransform() noexcept : Transform(StaticTypeId()) {}
    double GetX() const noexcept;
    double GetY() const noexcept;
    void SetX(double value) noexcept;
    void SetY(double value) noexcept;

    AERO_DEPENDENCY_PROPERTY(double, X);
    AERO_DEPENDENCY_PROPERTY(double, Y);

    Base::Transform2D GetMatrix() const noexcept override;
};
} // namespace Aero::Media
