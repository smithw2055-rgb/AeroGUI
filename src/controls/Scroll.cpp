#include <Aero/Controls/Scroll.hpp>

#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>

#include <algorithm>
#include <cmath>

namespace Aero::Controls {
namespace {

constexpr double LayoutInfinity = 1.0e12;

bool Same(double left, double right) noexcept {
    return std::fabs(left - right) <= 0.000001;
}

bool SameData(
    const ScrollData& left,
    const ScrollData& right) noexcept {
    return Same(left.horizontalOffset, right.horizontalOffset) &&
        Same(left.verticalOffset, right.verticalOffset) &&
        Same(left.extentWidth, right.extentWidth) &&
        Same(left.extentHeight, right.extentHeight) &&
        Same(left.viewportWidth, right.viewportWidth) &&
        Same(left.viewportHeight, right.viewportHeight);
}

bool ValidNonnegative(double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

bool ValidData(const ScrollData& value) noexcept {
    return ValidNonnegative(value.horizontalOffset) &&
        ValidNonnegative(value.verticalOffset) &&
        ValidNonnegative(value.extentWidth) &&
        ValidNonnegative(value.extentHeight) &&
        ValidNonnegative(value.viewportWidth) &&
        ValidNonnegative(value.viewportHeight);
}

double ClampOffset(
    double value,
    double extent,
    double viewport,
    bool enabled) noexcept {
    if (!enabled) return 0.0;
    return std::clamp(
        value, 0.0, std::max(0.0, extent - viewport));
}

double ReadDouble(
    const DependencyObject& object,
    DependencyPropertyHandle property,
    double fallback = 0.0) noexcept {
    Base::Result<Value> value = object.GetValue(property);
    return value ? value.Value().AsDouble() : fallback;
}

bool ReadBool(
    const DependencyObject& object,
    DependencyPropertyHandle property,
    bool fallback) noexcept {
    Base::Result<Value> value = object.GetValue(property);
    return value ? value.Value().AsBoolean() : fallback;
}

Orientation ReadOrientation(
    const DependencyObject& object,
    DependencyPropertyHandle property) noexcept {
    Base::Result<Value> value = object.GetValue(property);
    return value
        ? static_cast<Orientation>(
            value.Value().AsUnsignedInteger())
        : Orientation::Vertical;
}

Base::Result<void> StoreDouble(
    DependencyObject& object,
    DependencyPropertyHandle property,
    double value) noexcept {
    return object.SetValue(
        property,
        Value::FromDouble(BuiltinTypes::Double, value));
}

Base::Result<void> StoreOrientation(
    DependencyObject& object,
    DependencyPropertyHandle property,
    Orientation value) noexcept {
    if (value > Orientation::Vertical) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Scroll orientation is invalid");
    }
    return object.SetValue(
        property,
        Value::FromUnsignedInteger(
            BuiltinTypes::Orientation,
            static_cast<std::uint64_t>(value)));
}

} // namespace

ScrollContentPresenter::ScrollContentPresenter() noexcept
    : ScrollContentPresenter(StaticTypeId()) {}

ScrollContentPresenter::ScrollContentPresenter(
    TypeId runtimeType) noexcept
    : Decorator(runtimeType) {
    static_cast<void>(SetClipToBounds(true));
}

IScrollInfo*
ScrollContentPresenter::ActiveContentScrollInfo() const noexcept {
    return UsesContentScrolling()
        ? contentScrollInfo_
        : nullptr;
}

ScrollData ScrollContentPresenter::Data() const noexcept {
    IScrollInfo* logical = ActiveContentScrollInfo();
    return logical != nullptr ? logical->Data() : data_;
}

Base::Result<void>
ScrollContentPresenter::SetContentScrollInfo(
    IScrollInfo* value) noexcept {
    if (contentScrollInfo_ == value) return {};
    contentScrollInfo_ = value;
    Base::Result<void> invalidated = InvalidateMeasure();
    if (!invalidated) return invalidated.GetStatus();
    Base::Result<bool> synced =
        SyncLogicalData(ScrollInputKind::Line);
    return synced ? Base::Result<void>{}
                  : synced.GetStatus();
}

bool ScrollContentPresenter::CanHorizontallyScroll() const noexcept {
    return AllowsHorizontalScroll();
}

bool ScrollContentPresenter::CanVerticallyScroll() const noexcept {
    return AllowsVerticalScroll();
}

bool ScrollContentPresenter::CanContentScroll() const noexcept {
    return UsesContentScrolling();
}

Base::Result<void>
ScrollContentPresenter::SetCanHorizontallyScroll(
    bool value) noexcept {
    if (canHorizontallyScroll_ == value) return {};
    canHorizontallyScroll_ = value;
    return InvalidateMeasure();
}

Base::Result<void>
ScrollContentPresenter::SetCanVerticallyScroll(
    bool value) noexcept {
    if (canVerticallyScroll_ == value) return {};
    canVerticallyScroll_ = value;
    return InvalidateMeasure();
}

Base::Result<void>
ScrollContentPresenter::SetCanContentScroll(
    bool value) noexcept {
    if (canContentScroll_ == value) return {};
    canContentScroll_ = value;
    return InvalidateMeasure();
}

bool ScrollContentPresenter::AllowsHorizontalScroll() const noexcept {
    return canHorizontallyScroll_;
}

bool ScrollContentPresenter::AllowsVerticalScroll() const noexcept {
    return canVerticallyScroll_;
}

bool ScrollContentPresenter::UsesContentScrolling() const noexcept {
    return canContentScroll_;
}

Base::Result<void>
ScrollContentPresenter::SetLineScrollAmount(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Line scroll amount must be positive and finite");
    }
    lineScrollAmount_ = value;
    return {};
}

Base::Result<bool> ScrollContentPresenter::UpdateData(
    ScrollData value,
    ScrollInputKind kind,
    bool invalidateArrange) noexcept {
    if (!ValidData(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Scroll data must be finite and nonnegative");
    }
    value.horizontalOffset = ClampOffset(
        value.horizontalOffset,
        value.extentWidth,
        value.viewportWidth,
        AllowsHorizontalScroll());
    value.verticalOffset = ClampOffset(
        value.verticalOffset,
        value.extentHeight,
        value.viewportHeight,
        AllowsVerticalScroll());
    if (SameData(data_, value)) return false;
    const ScrollData oldData = data_;
    data_ = value;
    pendingInputKind_ = kind;
    if (invalidateArrange) {
        Base::Result<void> invalidated =
            InvalidateArrange();
        if (!invalidated) {
            data_ = oldData;
            return invalidated.GetStatus();
        }
    }
    OnScrollDataChanged(oldData, data_, kind);
    return true;
}

Base::Result<bool>
ScrollContentPresenter::SyncLogicalData(
    ScrollInputKind kind) noexcept {
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical == nullptr) return false;
    ScrollData value = logical->Data();
    if (!ValidData(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Logical scrolling provider returned invalid data");
    }
    if (SameData(data_, value)) return false;
    const ScrollData oldData = data_;
    data_ = value;
    pendingInputKind_ = kind;
    OnScrollDataChanged(oldData, data_, kind);
    return true;
}

Base::Result<bool>
ScrollContentPresenter::SetViewport(
    Size viewport) noexcept {
    if (!IsFinite(viewport) ||
        viewport.width < 0.0 ||
        viewport.height < 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Scroll viewport must be finite and nonnegative");
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        Base::Result<bool> changed =
            logical->SetViewport(viewport);
        if (!changed) return changed.GetStatus();
        Base::Result<bool> synced =
            SyncLogicalData(pendingInputKind_);
        if (!synced) return synced.GetStatus();
        return changed.Value() || synced.Value();
    }
    ScrollData value = data_;
    value.viewportWidth = viewport.width;
    value.viewportHeight = viewport.height;
    return UpdateData(value, pendingInputKind_, true);
}

Base::Result<bool>
ScrollContentPresenter::SetHorizontalOffset(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Horizontal offset must be finite and nonnegative");
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        Base::Result<bool> changed =
            logical->SetHorizontalOffset(value);
        if (!changed) return changed.GetStatus();
        Base::Result<bool> synced =
            SyncLogicalData(pendingInputKind_);
        if (!synced) return synced.GetStatus();
        return changed.Value() || synced.Value();
    }
    ScrollData data = data_;
    data.horizontalOffset = value;
    return UpdateData(
        data, pendingInputKind_, true);
}

Base::Result<bool>
ScrollContentPresenter::SetVerticalOffset(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Vertical offset must be finite and nonnegative");
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        Base::Result<bool> changed =
            logical->SetVerticalOffset(value);
        if (!changed) return changed.GetStatus();
        Base::Result<bool> synced =
            SyncLogicalData(pendingInputKind_);
        if (!synced) return synced.GetStatus();
        return changed.Value() || synced.Value();
    }
    ScrollData data = data_;
    data.verticalOffset = value;
    return UpdateData(
        data, pendingInputKind_, true);
}

Base::Result<bool>
ScrollContentPresenter::ApplyScrollDelta(
    double deltaX,
    double deltaY,
    ScrollInputKind kind) noexcept {
    if (!std::isfinite(deltaX) ||
        !std::isfinite(deltaY)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Scroll delta must be finite");
    }
    pendingInputKind_ = kind;
    const ScrollData current = Data();
    Base::Result<bool> horizontal =
        SetHorizontalOffset(std::max(
            0.0, current.horizontalOffset + deltaX));
    if (!horizontal) {
        pendingInputKind_ = ScrollInputKind::Line;
        return horizontal.GetStatus();
    }
    pendingInputKind_ = kind;
    const ScrollData afterHorizontal = Data();
    Base::Result<bool> vertical =
        SetVerticalOffset(std::max(
            0.0, afterHorizontal.verticalOffset + deltaY));
    pendingInputKind_ = ScrollInputKind::Line;
    if (!vertical) return vertical.GetStatus();
    return horizontal.Value() || vertical.Value();
}

Base::Result<bool>
ScrollContentPresenter::LineHorizontal(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Line scroll direction must be finite");
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        pendingInputKind_ = ScrollInputKind::Line;
        Base::Result<bool> changed =
            logical->LineHorizontal(direction);
        if (!changed) return changed.GetStatus();
        Base::Result<bool> synced =
            SyncLogicalData(ScrollInputKind::Line);
        pendingInputKind_ = ScrollInputKind::Line;
        if (!synced) return synced.GetStatus();
        return changed.Value() || synced.Value();
    }
    return ApplyScrollDelta(
        direction * lineScrollAmount_,
        0.0,
        ScrollInputKind::Line);
}

Base::Result<bool>
ScrollContentPresenter::LineVertical(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Line scroll direction must be finite");
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        pendingInputKind_ = ScrollInputKind::Line;
        Base::Result<bool> changed =
            logical->LineVertical(direction);
        if (!changed) return changed.GetStatus();
        Base::Result<bool> synced =
            SyncLogicalData(ScrollInputKind::Line);
        pendingInputKind_ = ScrollInputKind::Line;
        if (!synced) return synced.GetStatus();
        return changed.Value() || synced.Value();
    }
    return ApplyScrollDelta(
        0.0,
        direction * lineScrollAmount_,
        ScrollInputKind::Line);
}

Base::Result<bool>
ScrollContentPresenter::PageHorizontal(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Page scroll direction must be finite");
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        pendingInputKind_ = ScrollInputKind::Page;
        Base::Result<bool> changed =
            logical->PageHorizontal(direction);
        if (!changed) return changed.GetStatus();
        Base::Result<bool> synced =
            SyncLogicalData(ScrollInputKind::Page);
        pendingInputKind_ = ScrollInputKind::Line;
        if (!synced) return synced.GetStatus();
        return changed.Value() || synced.Value();
    }
    return ApplyScrollDelta(
        direction * Data().viewportWidth,
        0.0,
        ScrollInputKind::Page);
}

Base::Result<bool>
ScrollContentPresenter::PageVertical(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Page scroll direction must be finite");
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        pendingInputKind_ = ScrollInputKind::Page;
        Base::Result<bool> changed =
            logical->PageVertical(direction);
        if (!changed) return changed.GetStatus();
        Base::Result<bool> synced =
            SyncLogicalData(ScrollInputKind::Page);
        pendingInputKind_ = ScrollInputKind::Line;
        if (!synced) return synced.GetStatus();
        return changed.Value() || synced.Value();
    }
    return ApplyScrollDelta(
        0.0,
        direction * Data().viewportHeight,
        ScrollInputKind::Page);
}

Base::Result<Size>
ScrollContentPresenter::MeasureOverride(
    Size availableSize) noexcept {
    UIElement* child = Child();
    if (child == nullptr) {
        ScrollData empty = data_;
        empty.extentWidth = 0.0;
        empty.extentHeight = 0.0;
        empty.viewportWidth = availableSize.width;
        empty.viewportHeight = availableSize.height;
        Base::Result<bool> updated = UpdateData(
            empty, pendingInputKind_, false);
        if (!updated) return updated.GetStatus();
        return Size{};
    }

    IScrollInfo* logical = ActiveContentScrollInfo();
    Size childAvailable = availableSize;
    if (logical != nullptr) {
        Base::Result<bool> viewport =
            logical->SetViewport(availableSize);
        if (!viewport) return viewport.GetStatus();
    } else {
        if (AllowsHorizontalScroll()) {
            childAvailable.width = LayoutInfinity;
        }
        if (AllowsVerticalScroll()) {
            childAvailable.height = LayoutInfinity;
        }
    }
    Base::Result<void> measured =
        MeasureChild(*child, childAvailable);
    if (!measured) return measured.GetStatus();

    if (logical != nullptr) {
        Base::Result<bool> synced =
            SyncLogicalData(pendingInputKind_);
        if (!synced) return synced.GetStatus();
    } else {
        ScrollData value = data_;
        value.extentWidth = child->DesiredSize().width;
        value.extentHeight = child->DesiredSize().height;
        value.viewportWidth = availableSize.width;
        value.viewportHeight = availableSize.height;
        Base::Result<bool> updated = UpdateData(
            value, pendingInputKind_, false);
        if (!updated) return updated.GetStatus();
    }
    const ScrollData value = Data();
    return Size{
        std::min(value.extentWidth, availableSize.width),
        std::min(value.extentHeight, availableSize.height)};
}

Base::Result<Size>
ScrollContentPresenter::ArrangeOverride(
    Size finalSize) noexcept {
    UIElement* child = Child();
    if (child == nullptr) {
        Base::Result<bool> viewport =
            SetViewport(finalSize);
        if (!viewport) return viewport.GetStatus();
        return finalSize;
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    Base::Result<bool> viewport =
        SetViewport(finalSize);
    if (!viewport) return viewport.GetStatus();
    const ScrollData value = Data();
    const Rect slot = logical != nullptr
        ? Rect{0.0, 0.0, finalSize.width, finalSize.height}
        : Rect{
            -value.horizontalOffset,
            -value.verticalOffset,
            std::max(value.extentWidth, finalSize.width),
            std::max(value.extentHeight, finalSize.height)};
    Base::Result<void> arranged =
        ArrangeChild(*child, slot);
    if (!arranged) return arranged.GetStatus();
    return finalSize;
}

void ScrollContentPresenter::OnScrollDataChanged(
    const ScrollData&,
    const ScrollData&,
    ScrollInputKind) noexcept {}

ScrollViewer::ScrollViewer() noexcept
    : ScrollContentPresenter(StaticTypeId()) {}

ScrollViewer::~ScrollViewer() {
    if (interactions_ != nullptr) {
        static_cast<void>(
            interactions_->Detach(*this));
    }
}

double ScrollViewer::HorizontalOffset() const noexcept {
    return ReadDouble(*this, HorizontalOffsetProperty);
}

double ScrollViewer::VerticalOffset() const noexcept {
    return ReadDouble(*this, VerticalOffsetProperty);
}

double ScrollViewer::ExtentWidth() const noexcept {
    return ReadDouble(*this, ExtentWidthProperty);
}

double ScrollViewer::ExtentHeight() const noexcept {
    return ReadDouble(*this, ExtentHeightProperty);
}

double ScrollViewer::ViewportWidth() const noexcept {
    return ReadDouble(*this, ViewportWidthProperty);
}

double ScrollViewer::ViewportHeight() const noexcept {
    return ReadDouble(*this, ViewportHeightProperty);
}

Base::Result<void>
ScrollViewer::SetCanHorizontallyScroll(
    bool value) noexcept {
    return SetValue(
        CanHorizontallyScrollProperty,
        Value::FromBoolean(BuiltinTypes::Boolean, value));
}

Base::Result<void>
ScrollViewer::SetCanVerticallyScroll(
    bool value) noexcept {
    return SetValue(
        CanVerticallyScrollProperty,
        Value::FromBoolean(BuiltinTypes::Boolean, value));
}

Base::Result<void>
ScrollViewer::SetCanContentScroll(
    bool value) noexcept {
    return SetValue(
        CanContentScrollProperty,
        Value::FromBoolean(BuiltinTypes::Boolean, value));
}

bool ScrollViewer::AllowsHorizontalScroll() const noexcept {
    return ReadBool(
        *this, CanHorizontallyScrollProperty, true);
}

bool ScrollViewer::AllowsVerticalScroll() const noexcept {
    return ReadBool(
        *this, CanVerticallyScrollProperty, true);
}

bool ScrollViewer::UsesContentScrolling() const noexcept {
    return ReadBool(
        *this, CanContentScrollProperty, false);
}

void ScrollViewer::OnScrollDataChanged(
    const ScrollData& oldData,
    const ScrollData& newData,
    ScrollInputKind kind) noexcept {
    static_cast<void>(SetReadOnlyCurrentValue(
        HorizontalOffsetProperty,
        Value::FromDouble(
            BuiltinTypes::Double,
            newData.horizontalOffset)));
    static_cast<void>(SetReadOnlyCurrentValue(
        VerticalOffsetProperty,
        Value::FromDouble(
            BuiltinTypes::Double,
            newData.verticalOffset)));
    static_cast<void>(SetReadOnlyCurrentValue(
        ExtentWidthProperty,
        Value::FromDouble(
            BuiltinTypes::Double,
            newData.extentWidth)));
    static_cast<void>(SetReadOnlyCurrentValue(
        ExtentHeightProperty,
        Value::FromDouble(
            BuiltinTypes::Double,
            newData.extentHeight)));
    static_cast<void>(SetReadOnlyCurrentValue(
        ViewportWidthProperty,
        Value::FromDouble(
            BuiltinTypes::Double,
            newData.viewportWidth)));
    static_cast<void>(SetReadOnlyCurrentValue(
        ViewportHeightProperty,
        Value::FromDouble(
            BuiltinTypes::Double,
            newData.viewportHeight)));
    if (events_ != nullptr) {
        ScrollChangedEventArgs args;
        args.oldData = oldData;
        args.newData = newData;
        args.inputKind = kind;
        static_cast<void>(events_->RaiseEvent(
            *this, ScrollChangedEvent, &args));
    }
}

Base::Result<void> Thumb::BeginDrag(
    std::uint32_t pointerId,
    Point position) noexcept {
    if (!IsFinite(position) || dragging_) {
        return Base::Status::Failure(
            dragging_
                ? Base::ErrorCode::InvalidState
                : Base::ErrorCode::InvalidArgument,
            dragging_
                ? "Thumb is already dragging"
                : "Thumb drag position must be finite");
    }
    pointerId_ = pointerId;
    lastPosition_ = position;
    dragging_ = true;
    return {};
}

Base::Result<ThumbDragDelta> Thumb::DragTo(
    std::uint32_t pointerId,
    Point position) noexcept {
    if (!dragging_ || pointerId != pointerId_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Thumb drag pointer is not active");
    }
    if (!IsFinite(position)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Thumb drag position must be finite");
    }
    const ThumbDragDelta delta{
        position.x - lastPosition_.x,
        position.y - lastPosition_.y};
    lastPosition_ = position;
    return delta;
}

Base::Result<bool> Thumb::EndDrag(
    std::uint32_t pointerId) noexcept {
    if (!dragging_) return false;
    if (pointerId != pointerId_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Thumb drag pointer does not match");
    }
    pointerId_ = 0U;
    dragging_ = false;
    return true;
}

Orientation Track::GetOrientation() const noexcept {
    return ReadOrientation(*this, OrientationProperty);
}

double Track::Minimum() const noexcept {
    return ReadDouble(*this, MinimumProperty);
}

double Track::Maximum() const noexcept {
    return ReadDouble(*this, MaximumProperty);
}

double Track::Value() const noexcept {
    return ReadDouble(*this, ValueProperty);
}

double Track::ViewportSize() const noexcept {
    return ReadDouble(*this, ViewportSizeProperty);
}

Base::Result<void> Track::SetOrientation(
    Orientation value) noexcept {
    return StoreOrientation(
        *this, OrientationProperty, value);
}

Base::Result<void> Track::SetRange(
    double minimum,
    double maximum) noexcept {
    if (!std::isfinite(minimum) ||
        !std::isfinite(maximum) ||
        maximum < minimum) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Track range is invalid");
    }
    Base::Result<void> low =
        StoreDouble(*this, MinimumProperty, minimum);
    if (!low) return low.GetStatus();
    Base::Result<void> high =
        StoreDouble(*this, MaximumProperty, maximum);
    if (!high) return high.GetStatus();
    return SetValue(Value());
}

Base::Result<void> Track::SetValue(
    double value) noexcept {
    if (!std::isfinite(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Track value must be finite");
    }
    if (Maximum() < Minimum()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Track range is invalid");
    }
    return StoreDouble(
        *this, ValueProperty,
        std::clamp(value, Minimum(), Maximum()));
}

Base::Result<void> Track::SetViewportSize(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Track viewport must be finite and nonnegative");
    }
    return StoreDouble(
        *this, ViewportSizeProperty, value);
}

double Track::ThumbLength(
    double trackLength,
    double minimumThumbLength) const noexcept {
    if (!ValidNonnegative(trackLength) ||
        !ValidNonnegative(minimumThumbLength) ||
        trackLength == 0.0) {
        return 0.0;
    }
    const double range =
        std::max(0.0, Maximum() - Minimum());
    const double viewport = ViewportSize();
    if (range == 0.0) return trackLength;
    const double proportional =
        viewport > 0.0
        ? trackLength * viewport / (viewport + range)
        : minimumThumbLength;
    return std::clamp(
        proportional,
        std::min(minimumThumbLength, trackLength),
        trackLength);
}

double Track::ThumbOffset(
    double trackLength,
    double minimumThumbLength) const noexcept {
    const double travel =
        trackLength - ThumbLength(
            trackLength, minimumThumbLength);
    const double range =
        Maximum() - Minimum();
    if (travel <= 0.0 || range <= 0.0) return 0.0;
    return travel *
        (std::clamp(Value(), Minimum(), Maximum()) -
            Minimum()) /
        range;
}

Base::Result<double> Track::ValueFromThumbOffset(
    double offset,
    double trackLength,
    double minimumThumbLength) const noexcept {
    if (!std::isfinite(offset) ||
        !ValidNonnegative(trackLength) ||
        !ValidNonnegative(minimumThumbLength)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Track thumb geometry is invalid");
    }
    const double travel =
        trackLength - ThumbLength(
            trackLength, minimumThumbLength);
    const double range =
        Maximum() - Minimum();
    if (travel <= 0.0 || range <= 0.0) {
        return Minimum();
    }
    return Minimum() +
        std::clamp(offset, 0.0, travel) / travel * range;
}

Orientation ScrollBar::GetOrientation() const noexcept {
    return ReadOrientation(*this, OrientationProperty);
}

double ScrollBar::Minimum() const noexcept {
    return ReadDouble(*this, MinimumProperty);
}

double ScrollBar::Maximum() const noexcept {
    return ReadDouble(*this, MaximumProperty);
}

double ScrollBar::Value() const noexcept {
    return ReadDouble(*this, ValueProperty);
}

double ScrollBar::ViewportSize() const noexcept {
    return ReadDouble(*this, ViewportSizeProperty);
}

double ScrollBar::SmallChange() const noexcept {
    return ReadDouble(*this, SmallChangeProperty, 16.0);
}

double ScrollBar::LargeChange() const noexcept {
    const double configured =
        ReadDouble(*this, LargeChangeProperty);
    return configured > 0.0
        ? configured
        : std::max(ViewportSize(), SmallChange());
}

Base::Result<void> ScrollBar::SetOrientation(
    Orientation value) noexcept {
    return StoreOrientation(
        *this, OrientationProperty, value);
}

Base::Result<void> ScrollBar::SetRange(
    double minimum,
    double maximum) noexcept {
    if (!std::isfinite(minimum) ||
        !std::isfinite(maximum) ||
        maximum < minimum) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ScrollBar range is invalid");
    }
    Base::Result<void> low =
        StoreDouble(*this, MinimumProperty, minimum);
    if (!low) return low.GetStatus();
    Base::Result<void> high =
        StoreDouble(*this, MaximumProperty, maximum);
    if (!high) return high.GetStatus();
    Base::Result<bool> clamped = SetValue(Value());
    return clamped ? Base::Result<void>{}
                   : clamped.GetStatus();
}

Base::Result<bool> ScrollBar::SetValue(
    double value) noexcept {
    if (!std::isfinite(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ScrollBar value must be finite");
    }
    if (Maximum() < Minimum()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ScrollBar range is invalid");
    }
    const double clamped =
        std::clamp(value, Minimum(), Maximum());
    if (Same(clamped, Value())) return false;
    Base::Result<void> stored =
        StoreDouble(*this, ValueProperty, clamped);
    if (!stored) return stored.GetStatus();
    return true;
}

Base::Result<void> ScrollBar::SetViewportSize(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ScrollBar viewport must be finite and nonnegative");
    }
    return StoreDouble(
        *this, ViewportSizeProperty, value);
}

Base::Result<void> ScrollBar::SetSmallChange(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ScrollBar SmallChange must be positive and finite");
    }
    return StoreDouble(
        *this, SmallChangeProperty, value);
}

Base::Result<void> ScrollBar::SetLargeChange(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ScrollBar LargeChange must be finite and nonnegative");
    }
    return StoreDouble(
        *this, LargeChangeProperty, value);
}

Base::Result<bool> ScrollBar::LineDecrement() noexcept {
    return SetValue(Value() - SmallChange());
}

Base::Result<bool> ScrollBar::LineIncrement() noexcept {
    return SetValue(Value() + SmallChange());
}

Base::Result<bool> ScrollBar::PageDecrement() noexcept {
    return SetValue(Value() - LargeChange());
}

Base::Result<bool> ScrollBar::PageIncrement() noexcept {
    return SetValue(Value() + LargeChange());
}

Base::Result<bool> ScrollBar::DragThumb(
    double thumbOffset,
    double trackLength,
    double minimumThumbLength) noexcept {
    Track track;
    Base::Result<void> status =
        track.SetOrientation(GetOrientation());
    if (status) {
        status = track.SetRange(
            Minimum(), Maximum());
    }
    if (status) {
        status = track.SetViewportSize(
            ViewportSize());
    }
    if (!status) return status.GetStatus();
    Base::Result<double> value =
        track.ValueFromThumbOffset(
            thumbOffset,
            trackLength,
            minimumThumbLength);
    if (!value) return value.GetStatus();
    return SetValue(value.Value());
}

ScrollInteractionManager::ScrollInteractionManager(
    ObjectTree& tree,
    RoutedEventManager& events) noexcept
    : tree_(&tree),
      events_(&events),
      wheelHandler_(
          this,
          &ScrollInteractionManager::OnMouseWheel) {}

ScrollInteractionManager::~ScrollInteractionManager() noexcept {
    while (!viewers_.Empty()) {
        ScrollViewer* viewer =
            viewers_.Back().viewer;
        if (viewer != nullptr) {
            static_cast<void>(Detach(*viewer));
        } else {
            viewers_.PopBack();
        }
    }
}

std::uint32_t ScrollInteractionManager::FindViewer(
    const ScrollViewer& viewer) const noexcept {
    const VisualHandle handle = viewer.Handle();
    for (std::uint32_t index = 0U;
        index < viewers_.Size(); ++index) {
        if (viewers_[index].viewer == &viewer ||
            (viewers_[index].handle.index == handle.index &&
                viewers_[index].handle.generation ==
                    handle.generation)) {
            return index;
        }
    }
    return UINT32_MAX;
}

Base::Result<void> ScrollInteractionManager::Attach(
    ScrollViewer& viewer) noexcept {
    if (FindViewer(viewer) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "ScrollViewer is already attached");
    }
    if (viewer.interactions_ != nullptr &&
        viewer.interactions_ != this) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ScrollViewer belongs to another interaction manager");
    }
    if (viewer.OwningTree() != tree_ ||
        !viewer.Handle().IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ScrollViewer must be loaded in the interaction tree");
    }
    Base::Result<void> handler =
        viewer.TryAddHandler(
            UIElement::MouseWheelEvent,
            wheelHandler_);
    if (!handler) return handler.GetStatus();
    Base::Result<void> added =
        viewers_.TryPushBack(
            {&viewer, viewer.Handle()});
    if (!added) {
        static_cast<void>(viewer.RemoveHandler(
            UIElement::MouseWheelEvent,
            wheelHandler_));
        return added.GetStatus();
    }
    viewer.events_ = events_;
    viewer.interactions_ = this;
    return {};
}

Base::Result<bool> ScrollInteractionManager::Detach(
    ScrollViewer& viewer) noexcept {
    const std::uint32_t index = FindViewer(viewer);
    if (index == UINT32_MAX) return false;
    static_cast<void>(viewer.RemoveHandler(
        UIElement::MouseWheelEvent,
        wheelHandler_));
    viewer.events_ = nullptr;
    viewer.interactions_ = nullptr;
    if (index + 1U != viewers_.Size()) {
        viewers_[index] = viewers_.Back();
    }
    viewers_.PopBack();
    return true;
}

void ScrollInteractionManager::OnMouseWheel(
    Base::Object* sender,
    const MouseWheelEventArgs& args) noexcept {
    auto* viewer = static_cast<ScrollViewer*>(sender);
    if (viewer == nullptr ||
        FindViewer(*viewer) == UINT32_MAX) {
        return;
    }
    const double horizontal =
        -args.deltaX * viewer->LineScrollAmount();
    const double vertical =
        -args.deltaY * viewer->LineScrollAmount();
    Base::Result<bool> changed =
        viewer->ApplyScrollDelta(
            horizontal,
            vertical,
            ScrollInputKind::Wheel);
    if (changed && changed.Value()) {
        args.handled = true;
    }
}

} // namespace Aero::Controls
