#include <Aero/Core/Controls.hpp>

#include <algorithm>
#include <cmath>

namespace Aero::Core {

namespace {
bool IsValidColor(Color value) noexcept {
    return IsFinite(value) && value.red >= 0.0F && value.red <= 1.0F &&
        value.green >= 0.0F && value.green <= 1.0F &&
        value.blue >= 0.0F && value.blue <= 1.0F &&
        value.alpha >= 0.0F && value.alpha <= 1.0F;
}
} // namespace

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

Canvas::Canvas(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
    TypeId runtimeType, Base::IAllocator* allocator) noexcept
    : LayoutElement(dispatcher, registry, runtimeType, allocator),
      positions_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()) {}

Base::Result<void> Canvas::SetChildPosition(
    LayoutElement& child, Point position) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (!IsFinite(position)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Canvas child position must be finite");
    }
    bool attached = false;
    for (LayoutElement* current : LayoutChildren()) attached = attached || current == &child;
    if (!attached) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Canvas child must be attached before positioning");
    }
    for (Position& entry : positions_) {
        if (entry.child == &child) {
            entry.point = position;
            return InvalidateArrange();
        }
    }
    Base::Result<void> added = positions_.TryPushBack({&child, position});
    if (!added) return added.GetStatus();
    return InvalidateArrange();
}

Point Canvas::ChildPosition(const LayoutElement& child) const noexcept {
    for (const Position& entry : positions_) {
        if (entry.child == &child) return entry.point;
    }
    return {};
}

Base::Result<Size> Canvas::MeasureOverride(Size) noexcept {
    Size desired;
    for (LayoutElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Base::Result<void> measured = MeasureChild(*child, {1.0e12, 1.0e12});
        if (!measured) return measured.GetStatus();
        const Point position = ChildPosition(*child);
        desired.width = std::max(desired.width, position.x + child->DesiredSize().width);
        desired.height = std::max(desired.height, position.y + child->DesiredSize().height);
    }
    return desired;
}

Base::Result<Size> Canvas::ArrangeOverride(Size finalSize) noexcept {
    for (LayoutElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Point position = ChildPosition(*child);
        const Size desired = child->DesiredSize();
        Base::Result<void> arranged = ArrangeChild(*child,
            {position.x, position.y, desired.width, desired.height});
        if (!arranged) return arranged.GetStatus();
    }
    return finalSize;
}

Border::Border(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
    TypeId runtimeType, Base::IAllocator* allocator) noexcept
    : RenderElement(dispatcher, registry, runtimeType, allocator) {}

Base::Result<void> Border::SetBackground(Color value) noexcept {
    if (!IsValidColor(value)) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Border color is invalid");
    background_ = value;
    return InvalidateRender();
}

Base::Result<void> Border::SetStroke(Color value, double thickness) noexcept {
    if (!IsValidColor(value) || !std::isfinite(thickness) || thickness < 0.0) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Border stroke is invalid");
    }
    stroke_ = value;
    strokeThickness_ = thickness;
    return InvalidateRender();
}

Base::Result<Size> Border::MeasureOverride(Size availableSize) noexcept {
    Size desired;
    for (LayoutElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Base::Result<void> measured = MeasureChild(*child, availableSize);
        if (!measured) return measured.GetStatus();
        desired.width = std::max(desired.width, child->DesiredSize().width);
        desired.height = std::max(desired.height, child->DesiredSize().height);
    }
    return desired;
}

Base::Result<Size> Border::ArrangeOverride(Size finalSize) noexcept {
    for (LayoutElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Base::Result<void> arranged = ArrangeChild(*child, {0.0, 0.0, finalSize.width, finalSize.height});
        if (!arranged) return arranged.GetStatus();
    }
    return finalSize;
}

Base::Result<void> Border::BuildDisplayList(DisplayListBuilder& builder) noexcept {
    const Rect bounds{0.0, 0.0, RenderSize().width, RenderSize().height};
    Base::Result<void> fill = builder.FillRect(bounds, background_);
    if (!fill) return fill.GetStatus();
    if (strokeThickness_ > 0.0 && stroke_.alpha > 0.0F) {
        return builder.StrokeRect(bounds, stroke_, strokeThickness_);
    }
    return {};
}

} // namespace Aero::Core
