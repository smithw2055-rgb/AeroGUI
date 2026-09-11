#pragma once

#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Brush.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/Pen.hpp>

namespace Aero::Shapes {

using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::PropertyValue;
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

    void SetFill(Ref<Brush> value) noexcept;
    void SetStroke(Ref<Brush> value) noexcept;
    void SetPen(Ref<Media::Pen> value) noexcept;
    void SetStrokeThickness(double value) noexcept;
    void SetStretch(Stretch value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Ref<Brush>, Fill);
    AERO_DEPENDENCY_PROPERTY(Ref<Brush>, Stroke);
    AERO_DEPENDENCY_PROPERTY(Ref<Media::Pen>, Pen);
    AERO_DEPENDENCY_PROPERTY(double, StrokeThickness);
    AERO_DEPENDENCY_PROPERTY(Stretch, Stretch);

protected:
    explicit Shape(TypeId runtimeType) noexcept
        : FrameworkElement(runtimeType) {}
    ~Shape() override = default;
    // Replaces OnShapePenChanged (Fill/Stroke had a no-op and were dropped).
    void OnPropertyChanged(
        const DependencyPropertyChangedEventArgs& args) noexcept override;
};

} // namespace Aero::Shapes
