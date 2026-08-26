#pragma once

#include <Aero/Controls/VirtualizingStackPanel.hpp>
#include <Aero/Controls/WrapPanel.hpp>

namespace Aero::Controls {

class AERO_GUI_API VirtualizingWrapPanel : public VirtualizingStackPanel {
    AERO_DECLARE_TYPE(VirtualizingWrapPanel, VirtualizingStackPanel)
public:
    VirtualizingWrapPanel() noexcept;

    double GetItemWidth() const noexcept;
    double GetItemHeight() const noexcept;
    void SetItemWidth(double value) noexcept;
    void SetItemHeight(double value) noexcept;

    inline static constexpr DependencyProperty<double> ItemWidthProperty{"ItemWidth"};
    inline static constexpr DependencyProperty<double> ItemHeightProperty{"ItemHeight"};

protected:
    void CalculateRealizationRange() noexcept override;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

} // namespace Aero::Controls
