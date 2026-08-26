#pragma once

#include <Aero/Controls/Panel.hpp>

namespace Aero::Controls {

// Wraps tab headers according to the nearest templated TabControl's strip
// placement, matching the WPF TabPanel layout contract.
class AERO_GUI_API TabPanel : public Panel {
    AERO_DECLARE_TYPE(TabPanel, Panel)
public:
    TabPanel() noexcept : Panel(StaticTypeId()) {}
    ~TabPanel() override = default;

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;

private:
    bool GetIsVertical() const noexcept;
};

} // namespace Aero::Controls
