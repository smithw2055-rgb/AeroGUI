#pragma once

#include <Aero/Controls/Panel.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API DockPanel : public Panel {
    AERO_DECLARE_TYPE(DockPanel, Panel)
public:
    DockPanel() noexcept : Panel(StaticTypeId()) {}
    bool GetLastChildFill() const noexcept;
    void SetLastChildFill(bool value) noexcept;
    void SetChildDock(
        UIElement& child, Dock value) noexcept;
    Dock GetChildDock(const UIElement& child) const noexcept;
    inline static constexpr DependencyProperty<bool> LastChildFillProperty{"LastChildFill"};
    inline static constexpr AttachedProperty<Dock> DockProperty{"Dock"};
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

} // namespace Aero::Controls
