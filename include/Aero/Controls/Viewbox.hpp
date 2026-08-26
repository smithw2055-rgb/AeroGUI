#pragma once

#include <Aero/Controls/Decorator.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/Transforms.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;
using ::Aero::Media::MatrixTransform;

// WPF-shaped single-child scaling decorator. The child participates in
// layout at its natural size and is then fitted into the Viewbox slot.
class AERO_GUI_API Viewbox : public Decorator {
    AERO_DECLARE_TYPE(Viewbox, Decorator)
public:
    Viewbox() noexcept : Decorator(StaticTypeId()) {}

    Stretch GetStretch() const noexcept;
    StretchDirection GetStretchDirection() const noexcept;
    void SetStretch(Stretch value) noexcept;
    void SetStretchDirection(
        StretchDirection value) noexcept;

    inline static constexpr DependencyProperty<Stretch> StretchProperty{"Stretch"};
    inline static constexpr DependencyProperty<StretchDirection> StretchDirectionProperty{"StretchDirection"};

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;

private:
    Ref<MatrixTransform> viewTransform_;
    Ref<FrameworkElement> projectedChild_;
    Result<void> ApplyViewTransform(
        double scaleX,
        double scaleY,
        double offsetX,
        double offsetY) noexcept;
};

} // namespace Aero::Controls
