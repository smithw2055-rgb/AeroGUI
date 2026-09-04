#pragma once

#include <Aero/Controls/Control.hpp>
#include <Aero/Style.hpp>

namespace Aero::Controls {

enum class GridResizeDirection : std::uint8_t {
    Auto = 0U,
    Columns,
    Rows
};

enum class GridResizeBehavior : std::uint8_t {
    BasedOnAlignment = 0U,
    CurrentAndNext,
    PreviousAndCurrent,
    PreviousAndNext
};

// WPF-compatible GridSplitter surface. The splitter carries the full
// resize-policy state even when the hosting grid chooses to apply the delta
// through a custom interaction adapter.
class AERO_GUI_API GridSplitter : public Control {
    AERO_DECLARE_TYPE(GridSplitter, Control)
public:
    GridSplitter() noexcept : Control(StaticTypeId()) {}
    ~GridSplitter() override = default;

    double GetDragIncrement() const noexcept;
    double GetKeyboardIncrement() const noexcept;
    GridResizeDirection GetResizeDirection() const noexcept;
    GridResizeBehavior GetResizeBehavior() const noexcept;
    bool GetShowsPreview() const noexcept;
    Ref<Aero::Style> GetPreviewStyle() const noexcept;
    void SetDragIncrement(double value) noexcept;
    void SetKeyboardIncrement(double value) noexcept;
    void SetResizeDirection(
        GridResizeDirection value) noexcept;
    void SetResizeBehavior(
        GridResizeBehavior value) noexcept;
    void SetShowsPreview(bool value) noexcept;
    void SetPreviewStyle(
        Ref<Aero::Style> value) noexcept;

    inline static constexpr DependencyProperty<double> DragIncrementProperty{"DragIncrement"};
    inline static constexpr DependencyProperty<double> KeyboardIncrementProperty{"KeyboardIncrement"};
    inline static constexpr DependencyProperty<GridResizeDirection> ResizeDirectionProperty{"ResizeDirection"};
    inline static constexpr DependencyProperty<GridResizeBehavior> ResizeBehaviorProperty{"ResizeBehavior"};
    inline static constexpr DependencyProperty<bool> ShowsPreviewProperty{"ShowsPreview"};
    inline static constexpr DependencyProperty<Ref<Aero::Style>> PreviewStyleProperty{"PreviewStyle"};
};

} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::GridResizeDirection)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::GridResizeBehavior)
