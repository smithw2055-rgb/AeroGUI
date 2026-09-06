#pragma once

#include <Aero/Controls/Panel.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API WrapPanel : public Panel {
    AERO_DECLARE_TYPE(WrapPanel, Panel)
public:
    WrapPanel() noexcept : Panel(StaticTypeId()) {}
    Orientation GetOrientation() const noexcept;
    void SetOrientation(Orientation value) noexcept;
    double GetItemWidth() const noexcept;
    double GetItemHeight() const noexcept;
    void SetItemWidth(double value) noexcept;
    void SetItemHeight(double value) noexcept;
    AERO_DEPENDENCY_PROPERTY(Orientation, Orientation);
    // Zero selects the child's desired dimension.
    AERO_DEPENDENCY_PROPERTY(double, ItemWidth);
    AERO_DEPENDENCY_PROPERTY(double, ItemHeight);
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

} // namespace Aero::Controls
