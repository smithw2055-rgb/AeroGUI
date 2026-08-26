#pragma once

#include <Aero/Controls/Panel.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API Canvas : public Panel {
    AERO_DECLARE_TYPE(Canvas, Panel)
public:
    Canvas() noexcept;
    void SetChildPosition(UIElement& child, Point position) noexcept;
    Point GetChildPosition(const UIElement& child) const noexcept;
    inline static constexpr AttachedProperty<double> LeftProperty{"Left"};
    inline static constexpr AttachedProperty<double> TopProperty{"Top"};
    inline static constexpr AttachedProperty<double> RightProperty{"Right"};
    inline static constexpr AttachedProperty<double> BottomProperty{"Bottom"};
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

} // namespace Aero::Controls
