#pragma once

#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Brush.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/Pen.hpp>

namespace Aero::Shapes {

using ::Aero::Meta::TypeId;
using ::Aero::Media::Brush;
using ::Aero::Media::Stretch;

class AERO_GUI_API Shape : public FrameworkElement {
    AERO_DECLARE_TYPE(Shape, FrameworkElement)
public:
    Ref<Brush> GetFill() const noexcept;
    Ref<Brush> GetStroke() const noexcept;
    Ref<Media::Pen> GetPen() const noexcept;
    double GetStrokeThickness() const noexcept;
    Stretch GetStretch() const noexcept;

    void SetFill(
        Ref<Brush> value) noexcept;
    void SetStroke(
        Ref<Brush> value) noexcept;
    void SetPen(Ref<Media::Pen> value) noexcept;
    void SetStrokeThickness(double value) noexcept;
    void SetStretch(Stretch value) noexcept;

    inline static constexpr DependencyProperty<Ref<Brush>> FillProperty{"Fill"};
    inline static constexpr DependencyProperty<Ref<Brush>> StrokeProperty{"Stroke"};
    inline static constexpr DependencyProperty<Ref<Media::Pen>> PenProperty{"Pen"};
    inline static constexpr DependencyProperty<double> StrokeThicknessProperty{"StrokeThickness"};
    inline static constexpr DependencyProperty<Stretch> StretchProperty{"Stretch"};

protected:
    explicit Shape(TypeId runtimeType) noexcept
        : FrameworkElement(runtimeType) {}
    ~Shape() override = default;
};

} // namespace Aero::Shapes
