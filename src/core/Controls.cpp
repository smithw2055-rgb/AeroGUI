#include <Aero/Core/Controls.hpp>

#include <algorithm>

namespace Aero::Core {

StackPanel::StackPanel(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
    TypeId runtimeType, Orientation orientation, Base::IAllocator* allocator) noexcept
    : LayoutElement(dispatcher, registry, runtimeType, allocator), orientation_(orientation) {}

Base::Result<void> StackPanel::SetOrientation(Orientation value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (orientation_ == value) return {};
    orientation_ = value;
    return InvalidateMeasure();
}

Base::Result<Size> StackPanel::MeasureOverride(Size availableSize) noexcept {
    Size desired;
    for (LayoutElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Size childAvailable = availableSize;
        if (orientation_ == Orientation::Vertical) childAvailable.height = 1.0e12;
        else childAvailable.width = 1.0e12;
        Base::Result<void> measured = MeasureChild(*child, childAvailable);
        if (!measured) return measured.GetStatus();
        const Size childDesired = child->DesiredSize();
        if (orientation_ == Orientation::Vertical) {
            desired.width = std::max(desired.width, childDesired.width);
            desired.height += childDesired.height;
        } else {
            desired.width += childDesired.width;
            desired.height = std::max(desired.height, childDesired.height);
        }
    }
    return desired;
}

Base::Result<Size> StackPanel::ArrangeOverride(Size finalSize) noexcept {
    double offset = 0.0;
    for (LayoutElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size desired = child->DesiredSize();
        const Rect slot = orientation_ == Orientation::Vertical
            ? Rect{0.0, offset, finalSize.width, desired.height}
            : Rect{offset, 0.0, desired.width, finalSize.height};
        Base::Result<void> arranged = ArrangeChild(*child, slot);
        if (!arranged) return arranged.GetStatus();
        offset += orientation_ == Orientation::Vertical ? desired.height : desired.width;
    }
    return finalSize;
}

} // namespace Aero::Core
