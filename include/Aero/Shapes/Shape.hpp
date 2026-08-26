#pragma once

#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Images.hpp>

namespace Aero::Shapes {

using ::Aero::Meta::TypeId;
using ::Aero::Media::Brush;
using ::Aero::Media::Stretch;

class AERO_GUI_API Shape : public FrameworkElement {
    AERO_DECLARE_TYPE(Shape, FrameworkElement)
public:
    Ref<Brush> GetFill() const noexcept;
    Ref<Brush> GetStroke() const noexcept;
    double GetStrokeThickness() const noexcept;
    Stretch GetStretch() const noexcept;

    void SetFill(
        Ref<Brush> value) noexcept;
    void SetStroke(
        Ref<Brush> value) noexcept;
    void SetStrokeThickness(double value) noexcept;
    void SetStretch(Stretch value) noexcept;

    inline static constexpr DependencyProperty<Ref<Brush>> FillProperty{"Fill"};
    inline static constexpr DependencyProperty<Ref<Brush>> StrokeProperty{"Stroke"};
    inline static constexpr DependencyProperty<double> StrokeThicknessProperty{"StrokeThickness"};
    inline static constexpr DependencyProperty<Stretch> StretchProperty{"Stretch"};

protected:
    explicit Shape(TypeId runtimeType) noexcept
        : FrameworkElement(runtimeType) {}
    ~Shape() override = default;
};

} // namespace Aero::Shapes
