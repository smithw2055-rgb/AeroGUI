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
    inline static constexpr DependencyProperty<Orientation> OrientationProperty{"Orientation"};
    // Zero selects the child's desired dimension.
    inline static constexpr DependencyProperty<double> ItemWidthProperty{"ItemWidth"};
    inline static constexpr DependencyProperty<double> ItemHeightProperty{"ItemHeight"};
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

} // namespace Aero::Controls
