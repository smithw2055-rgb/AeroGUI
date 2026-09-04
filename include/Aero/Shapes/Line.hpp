#pragma once

#include <Aero/Shapes/Shape.hpp>

namespace Aero::Shapes {

class AERO_GUI_API Line : public Shape {
    AERO_DECLARE_TYPE(Line, Shape)
public:
    Line() noexcept : Shape(StaticTypeId()) {}
    ~Line() override = default;

    double GetX1() const noexcept;
    double GetY1() const noexcept;
    double GetX2() const noexcept;
    double GetY2() const noexcept;
    void SetX1(double value) noexcept;
    void SetY1(double value) noexcept;
    void SetX2(double value) noexcept;
    void SetY2(double value) noexcept;

    inline static constexpr DependencyProperty<double> X1Property{"X1"};
    inline static constexpr DependencyProperty<double> Y1Property{"Y1"};
    inline static constexpr DependencyProperty<double> X2Property{"X2"};
    inline static constexpr DependencyProperty<double> Y2Property{"Y2"};

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;
};

} // namespace Aero::Shapes
