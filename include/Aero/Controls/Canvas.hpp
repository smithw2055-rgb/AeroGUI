#pragma once

#include <Aero/Controls/Panel.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API Canvas : public Panel {
    AERO_DECLARE_TYPE(Canvas, Panel)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    Canvas() noexcept;
    void SetChildPosition(UIElement& child, Point position) noexcept;
    Point GetChildPosition(const UIElement& child) const noexcept;
    AERO_ATTACHED_PROPERTY(double, Left);
    AERO_ATTACHED_PROPERTY(double, Top);
    AERO_ATTACHED_PROPERTY(double, Right);
    AERO_ATTACHED_PROPERTY(double, Bottom);
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

} // namespace Aero::Controls
