#pragma once

#include <Aero/Controls/Panel.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API StackPanel : public Panel {
    AERO_DECLARE_TYPE(StackPanel, Panel)
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
#endif
public:
    StackPanel() noexcept;
    explicit StackPanel(Orientation orientation) noexcept;
    Orientation GetOrientation() const noexcept;
    void SetOrientation(Orientation value) noexcept;
    inline static constexpr DependencyProperty<Orientation> OrientationProperty{"Orientation"};
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

} // namespace Aero::Controls
