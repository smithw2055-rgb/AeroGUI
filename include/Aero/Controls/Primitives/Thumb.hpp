#pragma once

#include <Aero/Controls/Control.hpp>

namespace Aero::Controls {

struct ThumbDragDelta {
    double horizontalChange = 0.0;
    double verticalChange = 0.0;
};

namespace Primitives {

class AERO_GUI_API Thumb : public Control {
    AERO_DECLARE_TYPE(Thumb, Control)
public:
    Thumb() noexcept : Control(StaticTypeId()) {}
    ~Thumb() override = default;

    bool GetIsDragging() const noexcept {
        return GetValue(IsDraggingProperty);
    }
    Result<void> BeginDrag(
        std::uint32_t pointerId,
        Point position) noexcept;
    Result<ThumbDragDelta> DragTo(
        std::uint32_t pointerId,
        Point position) noexcept;
    Result<bool> EndDrag(
        std::uint32_t pointerId) noexcept;

    inline static constexpr ReadOnlyDependencyProperty<bool> IsDraggingProperty{"IsDragging"};

private:
    std::uint32_t pointerId_ = 0U;
    Point lastPosition_;
    bool dragging_ = false;
};

} // namespace Primitives
} // namespace Aero::Controls
