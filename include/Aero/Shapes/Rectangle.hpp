#pragma once

#include <Aero/Shapes/Shape.hpp>

namespace Aero::Shapes {

class AERO_GUI_API Rectangle : public Shape {
    AERO_DECLARE_TYPE(Rectangle, Shape)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    Rectangle() noexcept : Shape(StaticTypeId()) {}
    ~Rectangle() override = default;

    double GetRadiusX() const noexcept;
    double GetRadiusY() const noexcept;
    void SetRadiusX(double value) noexcept;
    void SetRadiusY(double value) noexcept;

    AERO_DEPENDENCY_PROPERTY(double, RadiusX);
    AERO_DEPENDENCY_PROPERTY(double, RadiusY);

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    void OnRender(
        ::Aero::Media::DrawingContext& context) noexcept override;
};

} // namespace Aero::Shapes
