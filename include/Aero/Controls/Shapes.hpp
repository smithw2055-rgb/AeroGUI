#pragma once

#include <Aero/Presentation/Layout.hpp>
#include <Aero/Presentation/Brushes.hpp>
#include <Aero/Presentation/Rendering.hpp>

namespace Aero::Controls {

using namespace Aero::Core;
using namespace Aero::Presentation;

class AERO_API Shape : public FrameworkElement {
    AERO_DECLARE_TYPE(Shape, FrameworkElement)
public:
    Color Fill() const noexcept;
    Base::Ref<Brush> FillBrush() const noexcept;
    Color Stroke() const noexcept;
    Base::Ref<Brush> StrokeBrush() const noexcept;
    double StrokeThickness() const noexcept;

    Base::Result<void> SetFill(Color value) noexcept;
    Base::Result<void> SetFillBrush(
        Base::Ref<Brush> value) noexcept;
    Base::Result<void> SetStroke(Color value) noexcept;
    Base::Result<void> SetStrokeBrush(
        Base::Ref<Brush> value) noexcept;
    Base::Result<void> SetStrokeThickness(double value) noexcept;

    inline static constexpr Members::Property<Base::Ref<Brush>>
        FillProperty{"Fill"};
    inline static constexpr Members::Property<Base::Ref<Brush>>
        StrokeProperty{"Stroke"};
    inline static constexpr Members::Property<double>
        StrokeThicknessProperty{"StrokeThickness"};

protected:
    explicit Shape(TypeId runtimeType) noexcept
        : FrameworkElement(runtimeType) {}
    ~Shape() override = default;
};

class AERO_API Rectangle final : public Shape {
    AERO_DECLARE_TYPE(Rectangle, Shape)
public:
    Rectangle() noexcept : Shape(StaticTypeId()) {}
    ~Rectangle() override = default;

    double RadiusX() const noexcept;
    double RadiusY() const noexcept;
    Base::Result<void> SetRadiusX(double value) noexcept;
    Base::Result<void> SetRadiusY(double value) noexcept;

    inline static constexpr Members::Property<double>
        RadiusXProperty{"RadiusX"};
    inline static constexpr Members::Property<double>
        RadiusYProperty{"RadiusY"};

protected:
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<void> BuildDisplayList(
        DisplayListBuilder& builder) noexcept override;
};

class AERO_API Ellipse final : public Shape {
    AERO_DECLARE_TYPE(Ellipse, Shape)
public:
    Ellipse() noexcept : Shape(StaticTypeId()) {}
    ~Ellipse() override = default;

protected:
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<void> BuildDisplayList(
        DisplayListBuilder& builder) noexcept override;
};

} // namespace Aero::Controls
