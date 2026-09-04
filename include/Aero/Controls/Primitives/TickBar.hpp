#pragma once

#include <Aero/Controls/Control.hpp>
#include <Aero/Media/Brushes.hpp>

namespace Aero::Controls {

enum class TickBarPlacement : std::uint8_t {
    Top = 0U,
    Bottom,
    Left,
    Right
};

class AERO_GUI_API TickBar : public Control {
    AERO_DECLARE_TYPE(TickBar, Control)
public:
    TickBar() noexcept : Control(StaticTypeId()) {}
    ~TickBar() override = default;

    Ref<Aero::Media::Brush> GetFill() const noexcept;
    TickBarPlacement GetPlacement() const noexcept;
    void SetFill(
        Ref<Aero::Media::Brush> value) noexcept;
    void SetPlacement(
        TickBarPlacement value) noexcept;

    inline static constexpr DependencyProperty<Ref<Aero::Media::Brush>> FillProperty{"Fill"};
    inline static constexpr DependencyProperty<TickBarPlacement> PlacementProperty{"Placement"};

protected:
    void OnRender(
        Aero::Media::DrawingContext& context) noexcept override;
};

} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::TickBarPlacement)
