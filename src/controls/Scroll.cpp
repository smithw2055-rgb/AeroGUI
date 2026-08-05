#include "gui/GuiPrivate.hpp"
#include "../render/DisplayList.hpp"
#include <Aero/Controls/Primitives.hpp>
#include "../render/RenderPrivate.hpp"
#include "../media/MediaPrivate.hpp"
#include <Aero/Value.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include "ControlBehavior.hpp"

namespace Aero::Controls {
using Aero::Controls::Detail::ScrollBehavior;
using Aero::Controls::Detail::SliderBehavior;

using namespace Primitives;
using namespace ::Aero::Render;
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

Visibility ComputeScrollBarVisibility(
    ScrollBarVisibility mode,
    double extent,
    double viewport) noexcept {
    switch (mode) {
    case ScrollBarVisibility::Visible:
        return Visibility::Visible;
    case ScrollBarVisibility::Auto:
        return extent > viewport + 0.000001
            ? Visibility::Visible
            : Visibility::Collapsed;
    case ScrollBarVisibility::Disabled:
    case ScrollBarVisibility::Hidden:
    default:
        return Visibility::Collapsed;
    }
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

template <typename TProperty>
double ReadDouble(
    const DependencyObject& object,
    const TProperty& property,
    double fallback = 0.0) noexcept {
    return object.GetValueOr(property, fallback);
}

template <typename TProperty>
bool ReadBool(
    const DependencyObject& object,
    const TProperty& property,
    bool fallback) noexcept {
    return object.GetValueOr(property, fallback);
}

template <typename TProperty>
Orientation ReadOrientation(
    const DependencyObject& object,
    const TProperty& property) noexcept {
    return object.GetValueOr(
        property, Orientation::Vertical);
}

template <typename TProperty>
void StoreDouble(
    DependencyObject& object,
    const TProperty& property,
    double value) noexcept {
    object.SetValue(property, value);
}

template <typename TProperty>
void StoreOrientation(
    DependencyObject& object,
    const TProperty& property,
    Orientation value) noexcept {
    object.SetValue(property, value);
}

} // namespace

ScrollContentPresenter::ScrollContentPresenter() noexcept
    : ScrollContentPresenter(StaticTypeId()) {}

ScrollContentPresenter::ScrollContentPresenter(
    TypeId runtimeType) noexcept
    : ContentControl(runtimeType) {
    static_cast<void>(SetClipToBounds(true));
}

IScrollInfo*
ScrollContentPresenter::ActiveContentScrollInfo() const noexcept {
    return GetUsesContentScrolling()
        ? contentScrollInfo_
        : nullptr;
}

ScrollData ScrollContentPresenter::GetData() const noexcept {
    IScrollInfo* logical = ActiveContentScrollInfo();
    return logical != nullptr ? logical->GetData() : data_;
}

void ScrollContentPresenter::SetContentScrollInfo(
    IScrollInfo* value) noexcept {
    if (contentScrollInfo_ == value) return;
    contentScrollInfo_ = value;
    (void)InvalidateMeasure();
    (void)SyncLogicalData(ScrollInputKind::Line);
}

bool ScrollContentPresenter::GetCanHorizontallyScroll() const noexcept {
    return GetAllowsHorizontalScroll();
}

bool ScrollContentPresenter::GetCanVerticallyScroll() const noexcept {
    return GetAllowsVerticalScroll();
}

bool ScrollContentPresenter::GetCanContentScroll() const noexcept {
    return GetUsesContentScrolling();
}

void ScrollContentPresenter::SetCanHorizontallyScroll(
    bool value) noexcept {
    if (canHorizontallyScroll_ == value) return;
    canHorizontallyScroll_ = value;
    (void)InvalidateMeasure();
}

void ScrollContentPresenter::SetCanVerticallyScroll(
    bool value) noexcept {
    if (canVerticallyScroll_ == value) return;
    canVerticallyScroll_ = value;
    (void)InvalidateMeasure();
}

void ScrollContentPresenter::SetCanContentScroll(
    bool value) noexcept {
    DependencyObject::SetValue(CanContentScrollProperty, value);
}

bool ScrollContentPresenter::GetAllowsHorizontalScroll() const noexcept {
    return canHorizontallyScroll_;
}

bool ScrollContentPresenter::GetAllowsVerticalScroll() const noexcept {
    return canVerticallyScroll_;
}

bool ScrollContentPresenter::GetUsesContentScrolling() const noexcept {
    return GetValueOr(
        CanContentScrollProperty, false);
}

void ScrollContentPresenter::SetLineScrollAmount(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return;
    }
    lineScrollAmount_ = value;
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
        GetAllowsHorizontalScroll());
    value.verticalOffset = ClampOffset(
        value.verticalOffset,
        value.extentHeight,
        value.viewportHeight,
        GetAllowsVerticalScroll());
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
    ScrollData value = logical->GetData();
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

void ScrollContentPresenter::SetViewport(
    Size viewport) noexcept {
    if (!IsFinite(viewport) ||
        viewport.width < 0.0 ||
        viewport.height < 0.0) {
        return;
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        logical->SetViewport(viewport);
        (void)SyncLogicalData(pendingInputKind_);
        return;
    }
    ScrollData value = data_;
    value.viewportWidth = viewport.width;
    value.viewportHeight = viewport.height;
    (void)UpdateData(value, pendingInputKind_, true);
}

void ScrollContentPresenter::SetHorizontalOffset(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return;
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        logical->SetHorizontalOffset(value);
        (void)SyncLogicalData(pendingInputKind_);
        return;
    }
    ScrollData data = data_;
    data.horizontalOffset = value;
    (void)UpdateData(data, pendingInputKind_, true);
}

void ScrollContentPresenter::SetVerticalOffset(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return;
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        logical->SetVerticalOffset(value);
        (void)SyncLogicalData(pendingInputKind_);
        return;
    }
    ScrollData data = data_;
    data.verticalOffset = value;
    (void)UpdateData(data, pendingInputKind_, true);
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
    const ScrollData current = GetData();
    SetHorizontalOffset(std::max(
        0.0, current.horizontalOffset + deltaX));
    pendingInputKind_ = kind;
    const ScrollData afterHorizontal = GetData();
    SetVerticalOffset(std::max(
        0.0, afterHorizontal.verticalOffset + deltaY));
    pendingInputKind_ = ScrollInputKind::Line;
    const ScrollData afterVertical = GetData();
    return afterHorizontal.horizontalOffset != afterVertical.horizontalOffset ||
        afterHorizontal.verticalOffset != afterVertical.verticalOffset;
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
        direction * GetData().viewportWidth,
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
        direction * GetData().viewportHeight,
        ScrollInputKind::Page);
}

Size
ScrollContentPresenter::MeasureOverride(
    Size availableSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        return ContentControl::MeasureOverride(
            availableSize);
    }
    UIElement* child = ContentElement();
    if (child == nullptr) {
        ScrollData empty = data_;
        empty.extentWidth = 0.0;
        empty.extentHeight = 0.0;
        empty.viewportWidth = availableSize.width;
        empty.viewportHeight = availableSize.height;
        Base::Result<bool> updated = UpdateData(
            empty, pendingInputKind_, false);
        if (!updated) return Size{};
        return Size{};
    }

    IScrollInfo* logical = ActiveContentScrollInfo();
    Size childAvailable = availableSize;
    if (logical != nullptr) {
        logical->SetViewport(availableSize);
    } else {
        if (GetAllowsHorizontalScroll()) {
            childAvailable.width = LayoutInfinity;
        }
        if (GetAllowsVerticalScroll()) {
            childAvailable.height = LayoutInfinity;
        }
    }
    Base::Result<void> measured =
        MeasureChild(*child, childAvailable);
    if (!measured) return Size{};

    if (logical != nullptr) {
        Base::Result<bool> synced =
            SyncLogicalData(pendingInputKind_);
        if (!synced) return Size{};
    } else {
        ScrollData value = data_;
        value.extentWidth = child->GetDesiredSize().width;
        value.extentHeight = child->GetDesiredSize().height;
        value.viewportWidth = availableSize.width;
        value.viewportHeight = availableSize.height;
        Base::Result<bool> updated = UpdateData(
            value, pendingInputKind_, false);
        if (!updated) return Size{};
    }
    const ScrollData value = GetData();
    return Size{
        std::min(value.extentWidth, availableSize.width),
        std::min(value.extentHeight, availableSize.height)};
}

Size
ScrollContentPresenter::ArrangeOverride(
    Size finalSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        return ContentControl::ArrangeOverride(
            finalSize);
    }
    UIElement* child = ContentElement();
    if (child == nullptr) {
        SetViewport(finalSize);
        return finalSize;
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    SetViewport(finalSize);
    const ScrollData value = GetData();
    const Rect slot = logical != nullptr
        ? Rect{0.0, 0.0, finalSize.width, finalSize.height}
        : Rect{
            -value.horizontalOffset,
            -value.verticalOffset,
            std::max(value.extentWidth, finalSize.width),
            std::max(value.extentHeight, finalSize.height)};
    Base::Result<void> arranged =
        ArrangeChild(*child, slot);
    if (!arranged) return finalSize;
    return finalSize;
}

void ScrollContentPresenter::OnScrollDataChanged(
    const ScrollData&,
    const ScrollData& newData,
    ScrollInputKind kind) noexcept {
    DependencyObject* templatedParent =
        GetTemplatedParent();
    if (templatedParent == nullptr ||
        templatedParent == this ||
        !PropertyRegistry().Types().IsDerivedFrom(
            templatedParent->RuntimeType(),
            ScrollViewer::StaticTypeId())) {
        return;
    }
    static_cast<ScrollViewer*>(templatedParent)->
        AdoptPresenterData(*this, newData, kind);
}

ScrollViewer::ScrollViewer() noexcept
    : ScrollContentPresenter(StaticTypeId()) {
    UpdateComputedScrollBarVisibility(GetData());
}

ScrollViewer::~ScrollViewer() {
    auto* behaviors = static_cast<Detail::ControlBehavior*>(
        Visual::Impl::ControlBehaviorRuntime(*this));
    if (behaviors != nullptr) {
        static_cast<void>(behaviors->Detach(*this));
    }
}

double ScrollViewer::GetHorizontalOffset() const noexcept {
    return ReadDouble(*this, HorizontalOffsetProperty);
}

double ScrollViewer::GetVerticalOffset() const noexcept {
    return ReadDouble(*this, VerticalOffsetProperty);
}

double ScrollViewer::GetExtentWidth() const noexcept {
    return ReadDouble(*this, ExtentWidthProperty);
}

double ScrollViewer::GetExtentHeight() const noexcept {
    return ReadDouble(*this, ExtentHeightProperty);
}

double ScrollViewer::GetViewportWidth() const noexcept {
    return ReadDouble(*this, ViewportWidthProperty);
}

double ScrollViewer::GetViewportHeight() const noexcept {
    return ReadDouble(*this, ViewportHeightProperty);
}

double ScrollViewer::GetScrollableWidth() const noexcept {
    return ReadDouble(*this, ScrollableWidthProperty);
}

double ScrollViewer::GetScrollableHeight() const noexcept {
    return ReadDouble(*this, ScrollableHeightProperty);
}

ScrollBarVisibility
ScrollViewer::GetHorizontalScrollBarVisibility() const noexcept {
    return GetHorizontalScrollBarVisibility(*this);
}

ScrollBarVisibility
ScrollViewer::GetVerticalScrollBarVisibility() const noexcept {
    return GetVerticalScrollBarVisibility(*this);
}

PanningMode ScrollViewer::GetPanningMode() const noexcept {
    return GetValueOr(PanningModeProperty, PanningMode::None);
}

void ScrollViewer::SetPanningMode(
    PanningMode value) noexcept {
    SetValue(PanningModeProperty, value);
}

Visibility
ScrollViewer::GetComputedHorizontalScrollBarVisibility()
    const noexcept {
    return GetValueOr(
        ComputedHorizontalScrollBarVisibilityProperty,
        Visibility::Collapsed);
}

Visibility
ScrollViewer::GetComputedVerticalScrollBarVisibility()
    const noexcept {
    return GetValueOr(
        ComputedVerticalScrollBarVisibilityProperty,
        Visibility::Collapsed);
}

void ScrollViewer::SetCanHorizontallyScroll(
    bool value) noexcept {
    SetValue(CanHorizontallyScrollProperty, value);
}

void ScrollViewer::SetCanVerticallyScroll(
    bool value) noexcept {
    SetValue(CanVerticallyScrollProperty, value);
}

void ScrollViewer::SetCanContentScroll(
    bool value) noexcept {
    SetValue(CanContentScrollProperty, value);
}

void ScrollViewer::SetHorizontalScrollBarVisibility(
    ScrollBarVisibility value) noexcept {
    SetHorizontalScrollBarVisibility(*this, value);
    UpdateComputedScrollBarVisibility(GetData());
}

void ScrollViewer::SetVerticalScrollBarVisibility(
    ScrollBarVisibility value) noexcept {
    SetVerticalScrollBarVisibility(*this, value);
    UpdateComputedScrollBarVisibility(GetData());
}

void ScrollViewer::SetHorizontalOffset(
    double value) noexcept {
    if (contentPresenter_ != nullptr) {
        contentPresenter_->SetHorizontalOffset(value);
    } else {
        ScrollContentPresenter::SetHorizontalOffset(value);
    }
}

void ScrollViewer::SetVerticalOffset(
    double value) noexcept {
    if (contentPresenter_ != nullptr) {
        contentPresenter_->SetVerticalOffset(value);
    } else {
        ScrollContentPresenter::SetVerticalOffset(value);
    }
}

Base::Result<bool> ScrollViewer::LineHorizontal(
    double direction) noexcept {
    return contentPresenter_ != nullptr
        ? contentPresenter_->LineHorizontal(direction)
        : ScrollContentPresenter::
            LineHorizontal(direction);
}

Base::Result<bool> ScrollViewer::LineVertical(
    double direction) noexcept {
    return contentPresenter_ != nullptr
        ? contentPresenter_->LineVertical(direction)
        : ScrollContentPresenter::
            LineVertical(direction);
}

Base::Result<bool> ScrollViewer::PageHorizontal(
    double direction) noexcept {
    return contentPresenter_ != nullptr
        ? contentPresenter_->PageHorizontal(direction)
        : ScrollContentPresenter::
            PageHorizontal(direction);
}

Base::Result<bool> ScrollViewer::PageVertical(
    double direction) noexcept {
    return contentPresenter_ != nullptr
        ? contentPresenter_->PageVertical(direction)
        : ScrollContentPresenter::
            PageVertical(direction);
}

void ScrollViewer::AdoptPresenterData(
    ScrollContentPresenter& presenter,
    const ScrollData& data,
    ScrollInputKind kind) noexcept {
    contentPresenter_ = &presenter;
    static_cast<void>(
        UpdateData(data, kind, false));
}

void ScrollViewer::OnApplyTemplate()
    noexcept {
    Control::OnApplyTemplate();
    DependencyObject* part = GetTemplateChild(
        ScrollContentPresenter::StaticTypeId());
    contentPresenter_ =
        part != nullptr &&
        part != this
        ? static_cast<ScrollContentPresenter*>(part)
        : nullptr;
    return;
}

Size ScrollViewer::MeasureOverride(
    Size availableSize) noexcept {
    const Size measured =
        ScrollContentPresenter::MeasureOverride(
            availableSize);
    if (contentPresenter_ == nullptr) {
        return measured;
    }
    AdoptPresenterData(
        *contentPresenter_,
        contentPresenter_->GetData(),
        ScrollInputKind::Line);
    return measured;
}

void ScrollViewer::OnTemplateDetached() noexcept {
    contentPresenter_ = nullptr;
    ScrollContentPresenter::OnTemplateDetached();
}

ScrollBarVisibility
ScrollViewer::GetHorizontalScrollBarVisibility(
    const DependencyObject& element) noexcept {
    return element.GetValueOr(
        HorizontalScrollBarVisibilityProperty,
        ScrollBarVisibility::Disabled);
}

ScrollBarVisibility
ScrollViewer::GetVerticalScrollBarVisibility(
    const DependencyObject& element) noexcept {
    return element.GetValueOr(
        VerticalScrollBarVisibilityProperty,
        ScrollBarVisibility::Visible);
}

void
ScrollViewer::SetHorizontalScrollBarVisibility(
    DependencyObject& element,
    ScrollBarVisibility value) noexcept {
    element.SetValue(HorizontalScrollBarVisibilityProperty, value);
}

void
ScrollViewer::SetVerticalScrollBarVisibility(
    DependencyObject& element,
    ScrollBarVisibility value) noexcept {
    element.SetValue(VerticalScrollBarVisibilityProperty, value);
}

bool ScrollViewer::GetAllowsHorizontalScroll() const noexcept {
    return GetHorizontalScrollBarVisibility() !=
            ScrollBarVisibility::Disabled &&
        ReadBool(
            *this, CanHorizontallyScrollProperty, true);
}

bool ScrollViewer::GetAllowsVerticalScroll() const noexcept {
    return GetVerticalScrollBarVisibility() !=
            ScrollBarVisibility::Disabled &&
        ReadBool(
            *this, CanVerticallyScrollProperty, true);
}

bool ScrollViewer::GetUsesContentScrolling() const noexcept {
    return ReadBool(
        *this, CanContentScrollProperty, false);
}

void ScrollViewer::OnScrollDataChanged(
    const ScrollData& oldData,
    const ScrollData& newData,
    ScrollInputKind kind) noexcept {
    static_cast<void>(SetReadOnlyCurrentValue(
        HorizontalOffsetProperty, newData.horizontalOffset));
    static_cast<void>(SetReadOnlyCurrentValue(
        VerticalOffsetProperty, newData.verticalOffset));
    static_cast<void>(SetReadOnlyCurrentValue(
        ExtentWidthProperty, newData.extentWidth));
    static_cast<void>(SetReadOnlyCurrentValue(
        ExtentHeightProperty, newData.extentHeight));
    static_cast<void>(SetReadOnlyCurrentValue(
        ViewportWidthProperty, newData.viewportWidth));
    static_cast<void>(SetReadOnlyCurrentValue(
        ViewportHeightProperty, newData.viewportHeight));
    static_cast<void>(SetReadOnlyCurrentValue(
        ScrollableWidthProperty,
        std::max(
            0.0,
            newData.extentWidth -
                newData.viewportWidth)));
    static_cast<void>(SetReadOnlyCurrentValue(
        ScrollableHeightProperty,
        std::max(
            0.0,
            newData.extentHeight -
                newData.viewportHeight)));
    UpdateComputedScrollBarVisibility(newData);

    const auto synchronizeBar = [&](
        Base::StringView name,
        bool horizontal) noexcept {
        DependencyObject* part =
            GetTemplateChild(name);
        if (part == nullptr ||
            !PropertyRegistry().Types().IsDerivedFrom(
                part->RuntimeType(),
                ScrollBar::StaticTypeId())) {
            return;
        }
        auto& bar = *static_cast<ScrollBar*>(part);
        const double extent = horizontal
            ? newData.extentWidth
            : newData.extentHeight;
        const double viewport = horizontal
            ? newData.viewportWidth
            : newData.viewportHeight;
        const double offset = horizontal
            ? newData.horizontalOffset
            : newData.verticalOffset;
        const ScrollBarVisibility mode = horizontal
            ? GetHorizontalScrollBarVisibility()
            : GetVerticalScrollBarVisibility();
        static_cast<void>(bar.SetRange(
            0.0,
            std::max(0.0, extent - viewport)));
        static_cast<void>(
            bar.SetViewportSize(viewport));
        static_cast<void>(bar.SetValue(offset));
        static_cast<void>(bar.SetVisibility(
            ComputeScrollBarVisibility(
                mode, extent, viewport)));
    };
    synchronizeBar(
        Base::StringView(
            "PART_HorizontalScrollBar"),
        true);
    synchronizeBar(
        Base::StringView(
            "PART_VerticalScrollBar"),
        false);

    auto* events = Visual::Impl::EventRouterFor(*this);
    if (events != nullptr) {
        ScrollChangedEventArgs args(oldData, newData, kind);
        static_cast<void>(
            events->RaiseEvent(
            *this, ScrollChangedEvent, &args));
    }
}

void ScrollViewer::UpdateComputedScrollBarVisibility(
    const ScrollData& data) noexcept {
    static_cast<void>(SetReadOnlyCurrentValue(
        ComputedHorizontalScrollBarVisibilityProperty,
        ComputeScrollBarVisibility(
            GetHorizontalScrollBarVisibility(),
            data.extentWidth,
            data.viewportWidth)));
    static_cast<void>(SetReadOnlyCurrentValue(
        ComputedVerticalScrollBarVisibilityProperty,
        ComputeScrollBarVisibility(
            GetVerticalScrollBarVisibility(),
            data.extentHeight,
            data.viewportHeight)));
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
    SetReadOnlyCurrentValue(IsDraggingProperty, true);
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
    SetReadOnlyCurrentValue(IsDraggingProperty, false);
    pointerId_ = 0U;
    dragging_ = false;
    return true;
}

Orientation Track::GetOrientation() const noexcept {
    return ReadOrientation(*this, OrientationProperty);
}

double Track::GetMinimum() const noexcept {
    return ReadDouble(*this, MinimumProperty);
}

double Track::GetMaximum() const noexcept {
    return ReadDouble(*this, MaximumProperty);
}

double Track::GetValue() const noexcept {
    return ReadDouble(*this, ValueProperty);
}

double Track::GetViewportSize() const noexcept {
    return ReadDouble(*this, ViewportSizeProperty);
}

double GridSplitter::GetDragIncrement() const noexcept {
    return GetValueOr(DragIncrementProperty, 1.0);
}

double GridSplitter::GetKeyboardIncrement() const noexcept {
    return GetValueOr(KeyboardIncrementProperty, 10.0);
}

GridResizeDirection GridSplitter::GetResizeDirection() const noexcept {
    return GetValueOr(ResizeDirectionProperty, GridResizeDirection::Auto);
}

GridResizeBehavior GridSplitter::GetResizeBehavior() const noexcept {
    return GetValueOr(
        ResizeBehaviorProperty,
        GridResizeBehavior::BasedOnAlignment);
}

bool GridSplitter::GetShowsPreview() const noexcept {
    return GetValueOr(ShowsPreviewProperty, false);
}

Base::Ref<Aero::Style> GridSplitter::GetPreviewStyle() const noexcept {
    return GetValueOr(
        PreviewStyleProperty,
        Base::Ref<Aero::Style>{});
}

void GridSplitter::SetDragIncrement(double value) noexcept {
    SetValue(DragIncrementProperty, value);
}

void GridSplitter::SetKeyboardIncrement(double value) noexcept {
    SetValue(KeyboardIncrementProperty, value);
}

void GridSplitter::SetResizeDirection(
    GridResizeDirection value) noexcept {
    SetValue(ResizeDirectionProperty, value);
}

void GridSplitter::SetResizeBehavior(
    GridResizeBehavior value) noexcept {
    SetValue(ResizeBehaviorProperty, value);
}

void GridSplitter::SetShowsPreview(bool value) noexcept {
    SetValue(ShowsPreviewProperty, value);
}

void GridSplitter::SetPreviewStyle(
    Base::Ref<Aero::Style> value) noexcept {
    SetValue(PreviewStyleProperty, std::move(value));
}

bool Track::GetIsDirectionReversed() const noexcept {
    return GetValueOr(
        IsDirectionReversedProperty, false);
}

void Track::SetOrientation(
    Orientation value) noexcept {
    StoreOrientation(*this, OrientationProperty, value);
}

void Track::SetRange(
    double minimum,
    double maximum) noexcept {
    if (!std::isfinite(minimum) ||
        !std::isfinite(maximum) ||
        maximum < minimum) {
        return;
    }
    StoreDouble(*this, MinimumProperty, minimum);
    StoreDouble(*this, MaximumProperty, maximum);
    SetValue(GetValue());
}

void Track::SetValue(
    double value) noexcept {
    if (!std::isfinite(value)) {
        return;
    }
    if (GetMaximum() < GetMinimum()) {
        return;
    }
    StoreDouble(*this, ValueProperty,
        std::clamp(value, GetMinimum(), GetMaximum()));
}

void Track::SetViewportSize(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return;
    }
    StoreDouble(*this, ViewportSizeProperty, value);
}

void Track::SetIsDirectionReversed(
    bool value) noexcept {
    DependencyObject::SetValue(IsDirectionReversedProperty, value);
}

void Track::SetDecreaseRepeatButton(
    Base::Ref<RepeatButton> value) noexcept {
    decreaseRepeatButton_ = std::move(value);
    (void)InvalidateMeasure();
}

void Track::SetThumb(
    Base::Ref<Thumb> value) noexcept {
    thumb_ = std::move(value);
    (void)InvalidateMeasure();
}

void Track::SetIncreaseRepeatButton(
    Base::Ref<RepeatButton> value) noexcept {
    increaseRepeatButton_ = std::move(value);
    (void)InvalidateMeasure();
}

double Track::GetThumbLength(
    double trackLength,
    double minimumThumbLength) const noexcept {
    if (!ValidNonnegative(trackLength) ||
        !ValidNonnegative(minimumThumbLength) ||
        trackLength == 0.0) {
        return 0.0;
    }
    const double range =
        std::max(0.0, GetMaximum() - GetMinimum());
    const double viewport = GetViewportSize();
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

double Track::GetThumbOffset(
    double trackLength,
    double minimumThumbLength) const noexcept {
    const double travel =
        trackLength - GetThumbLength(
            trackLength, minimumThumbLength);
    const double range =
        GetMaximum() - GetMinimum();
    if (travel <= 0.0 || range <= 0.0) return 0.0;
    const double offset = travel *
        (std::clamp(GetValue(), GetMinimum(), GetMaximum()) -
            GetMinimum()) /
        range;
    const bool invert =
        GetOrientation() == Orientation::Vertical
        ? !GetIsDirectionReversed()
        : GetIsDirectionReversed();
    return invert
        ? travel - offset
        : offset;
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
        trackLength - GetThumbLength(
            trackLength, minimumThumbLength);
    const double range =
        GetMaximum() - GetMinimum();
    if (travel <= 0.0 || range <= 0.0) {
        return GetMinimum();
    }
    double normalized =
        std::clamp(offset, 0.0, travel) / travel;
    const bool invert =
        GetOrientation() == Orientation::Vertical
        ? !GetIsDirectionReversed()
        : GetIsDirectionReversed();
    if (invert) {
        normalized = 1.0 - normalized;
    }
    return GetMinimum() + normalized * range;
}

Size Track::MeasureOverride(
    Size availableSize) noexcept {
    Size desired{};
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Base::Result<void> measured =
            MeasureChild(*child, availableSize);
        if (!measured) return Size{};
        if (GetOrientation() == Orientation::Vertical) {
            desired.width = std::max(
                desired.width,
                child->GetDesiredSize().width);
            desired.height += child->GetDesiredSize().height;
        } else {
            desired.width += child->GetDesiredSize().width;
            desired.height = std::max(
                desired.height,
                child->GetDesiredSize().height);
        }
    }
    return desired;
}

Size Track::ArrangeOverride(
    Size finalSize) noexcept {
    const bool vertical =
        GetOrientation() == Orientation::Vertical;
    const double length =
        vertical ? finalSize.height : finalSize.width;
    const double desiredThumbLength = thumb_
        ? std::max(0.0, vertical
            ? thumb_->GetDesiredSize().height
            : thumb_->GetDesiredSize().width)
        : 8.0;
    const double thumbLength =
        GetThumbLength(length, desiredThumbLength);
    const double thumbOffset =
        GetThumbOffset(length, desiredThumbLength);
    const double before = thumbOffset;
    const double after = std::max(
        0.0, length - thumbOffset - thumbLength);
    const bool invert = vertical
        ? !GetIsDirectionReversed()
        : GetIsDirectionReversed();
    RepeatButton* first = invert
        ? increaseRepeatButton_.Get()
        : decreaseRepeatButton_.Get();
    RepeatButton* last = invert
        ? decreaseRepeatButton_.Get()
        : increaseRepeatButton_.Get();
    const auto arrange = [&](UIElement* child,
                             double offset,
                             double extent) noexcept
        -> Base::Result<void> {
        if (child == nullptr) return {};
        return ArrangeChild(
            *child,
            vertical
                ? Rect{0.0, offset,
                       finalSize.width, extent}
                : Rect{offset, 0.0,
                       extent, finalSize.height});
    };
    Base::Result<void> arranged =
        arrange(first, 0.0, before);
    if (arranged) {
        arranged = arrange(
            thumb_.Get(), thumbOffset, thumbLength);
    }
    if (arranged) {
        arranged = arrange(
            last,
            thumbOffset + thumbLength,
            after);
    }
    return finalSize;
}

Orientation ScrollBar::GetOrientation() const noexcept {
    return ReadOrientation(*this, OrientationProperty);
}

ScrollBar::ScrollBar() noexcept
    : RangeBase(StaticTypeId()),
      trackPropertyChangedHandler_(
          this,
          &ScrollBar::OnTrackPropertyChanged) {
    static_cast<void>(AddValueChangedHandlerChecked(
        OrientationProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        MinimumProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        MaximumProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        ValueProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        ViewportSizeProperty,
        trackPropertyChangedHandler_));
}

ScrollBar::~ScrollBar() {
    static_cast<void>(RemoveValueChangedHandler(
        OrientationProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        MinimumProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        MaximumProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        ValueProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        ViewportSizeProperty,
        trackPropertyChangedHandler_));
}

void ScrollBar::OnApplyTemplate()
    noexcept {
    Control::OnApplyTemplate();
    DependencyObject* part =
        GetTemplateChild("PART_Track");
    track_ =
        part != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            part->RuntimeType(),
            Track::StaticTypeId())
        ? static_cast<Track*>(part)
        : nullptr;
    SynchronizeTrack();
    return;
}

void ScrollBar::OnTemplateDetached() noexcept {
    track_ = nullptr;
    Control::OnTemplateDetached();
}

void ScrollBar::OnTrackPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&)
        noexcept {
    SynchronizeTrack();
}

void ScrollBar::SynchronizeTrack() noexcept {
    if (track_ == nullptr) return;
    static_cast<void>(
        track_->SetOrientation(GetOrientation()));
    static_cast<void>(
        track_->SetRange(GetMinimum(), GetMaximum()));
    static_cast<void>(
        track_->SetViewportSize(GetViewportSize()));
    static_cast<void>(
        track_->SetValue(GetValue()));
}

RangeBase::RangeBase(TypeId runtimeType) noexcept
    : Control(runtimeType),
      rangeChangedHandler_(
          this,
          &RangeBase::OnRangePropertyChanged) {
    static_cast<void>(AddValueChangedHandlerChecked(
        MinimumProperty, rangeChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        MaximumProperty, rangeChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        ValueProperty, rangeChangedHandler_));
}

RangeBase::~RangeBase() {
    static_cast<void>(RemoveValueChangedHandler(
        MinimumProperty, rangeChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        MaximumProperty, rangeChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        ValueProperty, rangeChangedHandler_));
}

double RangeBase::GetMinimum() const noexcept {
    return ReadDouble(*this, MinimumProperty);
}

double RangeBase::GetMaximum() const noexcept {
    return ReadDouble(*this, MaximumProperty);
}

double RangeBase::GetValue() const noexcept {
    return ReadDouble(*this, ValueProperty);
}

void RangeBase::SetMinimum(
    double value) noexcept {
    SetRange(value, GetMaximum());
}

void RangeBase::SetMaximum(
    double value) noexcept {
    SetRange(GetMinimum(), value);
}

void RangeBase::SetRange(
    double minimum,
    double maximum) noexcept {
    if (!std::isfinite(minimum) ||
        !std::isfinite(maximum) ||
        maximum < minimum) {
        return;
    }
    StoreDouble(*this, MinimumProperty, minimum);
    StoreDouble(*this, MaximumProperty, maximum);
    SetValue(GetValue());
}

void RangeBase::SetValue(
    double value) noexcept {
    if (!std::isfinite(value)) {
        return;
    }
    if (GetMaximum() < GetMinimum()) {
        return;
    }
    const double clamped =
        std::clamp(value, GetMinimum(), GetMaximum());
    const double oldValue = GetValue();
    if (Same(clamped, oldValue)) return;
    StoreDouble(*this, ValueProperty, clamped);
}

void RangeBase::OnRangePropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&
        args) noexcept {
    if (args.GetProperty() == ValueProperty) {
        OnValueChanged(
            args.GetOldValue().AsDouble(),
            args.GetNewValue().AsDouble());
    } else if (
        args.GetProperty() == MinimumProperty ||
        args.GetProperty() == MaximumProperty) {
        SetValue(GetValue());
    }
}

void RangeBase::OnValueChanged(
    double oldValue,
    double newValue) noexcept {
    RangeValueChangedEventArgs args(oldValue, newValue);
    RaiseEvent(ValueChangedEvent, &args);
}

double ScrollBar::GetViewportSize() const noexcept {
    return ReadDouble(*this, ViewportSizeProperty);
}

double ScrollBar::GetSmallChange() const noexcept {
    return ReadDouble(*this, SmallChangeProperty, 16.0);
}

double ScrollBar::GetLargeChange() const noexcept {
    const double configured =
        ReadDouble(*this, LargeChangeProperty);
    return configured > 0.0
        ? configured
        : std::max(GetViewportSize(), GetSmallChange());
}

void ScrollBar::SetOrientation(
    Orientation value) noexcept {
    StoreOrientation(*this, OrientationProperty, value);
}

void ScrollBar::SetViewportSize(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return;
    }
    StoreDouble(*this, ViewportSizeProperty, value);
}

void ScrollBar::SetSmallChange(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return;
    }
    StoreDouble(*this, SmallChangeProperty, value);
}

void ScrollBar::SetLargeChange(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return;
    }
    StoreDouble(*this, LargeChangeProperty, value);
}

Base::Result<bool> ScrollBar::LineDecrement() noexcept {
    const double oldValue = GetValue();
    SetValue(oldValue - GetSmallChange());
    return !Same(oldValue, GetValue());
}

Base::Result<bool> ScrollBar::LineIncrement() noexcept {
    const double oldValue = GetValue();
    SetValue(oldValue + GetSmallChange());
    return !Same(oldValue, GetValue());
}

Base::Result<bool> ScrollBar::PageDecrement() noexcept {
    const double oldValue = GetValue();
    SetValue(oldValue - GetLargeChange());
    return !Same(oldValue, GetValue());
}

Base::Result<bool> ScrollBar::PageIncrement() noexcept {
    const double oldValue = GetValue();
    SetValue(oldValue + GetLargeChange());
    return !Same(oldValue, GetValue());
}

Base::Result<bool> ScrollBar::DragThumb(
    double thumbOffset,
    double trackLength,
    double minimumThumbLength) noexcept {
    Track track;
    track.SetOrientation(GetOrientation());
    track.SetRange(GetMinimum(), GetMaximum());
    track.SetViewportSize(GetViewportSize());
    Base::Result<double> value =
        track.ValueFromThumbOffset(
            thumbOffset,
            trackLength,
            minimumThumbLength);
    if (!value) return value.GetStatus();
    const double oldValue = GetValue();
    SetValue(value.Value());
    return !Same(oldValue, GetValue());
}

Slider::Slider() noexcept
    : Primitives::RangeBase(StaticTypeId()),
      trackPropertyChangedHandler_(
          this, &Slider::OnTrackPropertyChanged) {
    static_cast<void>(AddValueChangedHandlerChecked(
        OrientationProperty, trackPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        MinimumProperty, trackPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        MaximumProperty, trackPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        ValueProperty, trackPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        IsDirectionReversedProperty, trackPropertyChangedHandler_));
}

Slider::~Slider() {
    static_cast<void>(RemoveValueChangedHandler(
        OrientationProperty, trackPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        MinimumProperty, trackPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        MaximumProperty, trackPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        ValueProperty, trackPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        IsDirectionReversedProperty, trackPropertyChangedHandler_));
}

void Slider::OnApplyTemplate() noexcept {
    Control::OnApplyTemplate();
    DependencyObject* part = GetTemplateChild("PART_Track");
    track_ = part != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            part->RuntimeType(), Track::StaticTypeId())
        ? static_cast<Track*>(part)
        : nullptr;
    SynchronizeTrack();
}

void Slider::OnTemplateDetached() noexcept {
    track_ = nullptr;
    Control::OnTemplateDetached();
}

void Slider::OnTrackPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&) noexcept {
    SynchronizeTrack();
}

void Slider::SynchronizeTrack() noexcept {
    if (track_ == nullptr) return;
    track_->SetOrientation(GetOrientation());
    track_->SetRange(GetMinimum(), GetMaximum());
    track_->SetValue(GetValue());
    track_->SetIsDirectionReversed(GetIsDirectionReversed());
}

Orientation Slider::GetOrientation() const noexcept {
    return ReadOrientation(*this, OrientationProperty);
}

double Slider::GetSmallChange() const noexcept {
    return ReadDouble(*this, SmallChangeProperty, 1.0);
}

double Slider::GetLargeChange() const noexcept {
    return ReadDouble(*this, LargeChangeProperty, 10.0);
}

TickPlacement Slider::GetTickPlacement() const noexcept {
    return GetValueOr(
        TickPlacementProperty,
        TickPlacement::None);
}

double Slider::GetTickFrequency() const noexcept {
    return ReadDouble(*this, TickFrequencyProperty, 1.0);
}

Base::StringView Slider::GetTicks() const noexcept {
    return GetValueOr(
        TicksProperty, Base::StringView());
}

bool Slider::GetIsSnapToTickEnabled() const noexcept {
    return ReadBool(
        *this, IsSnapToTickEnabledProperty, false);
}

bool Slider::GetIsDirectionReversed() const noexcept {
    return ReadBool(
        *this, IsDirectionReversedProperty, false);
}

bool Slider::GetIsMoveToPointEnabled() const noexcept {
    return ReadBool(
        *this, IsMoveToPointEnabledProperty, false);
}

void Slider::SetOrientation(
    Orientation value) noexcept {
    StoreOrientation(*this, OrientationProperty, value);
}

void Slider::SetSmallChange(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return;
    }
    StoreDouble(*this, SmallChangeProperty, value);
}

void Slider::SetLargeChange(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return;
    }
    StoreDouble(*this, LargeChangeProperty, value);
}

void Slider::SetTickPlacement(
    TickPlacement value) noexcept {
    DependencyObject::SetValue(TickPlacementProperty, value);
}

void Slider::SetTickFrequency(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return;
    }
    StoreDouble(*this, TickFrequencyProperty, value);
}

void Slider::SetTicks(
    Base::StringView value) noexcept {
    std::uint32_t start = 0U;
    while (start < value.SizeBytes()) {
        while (start < value.SizeBytes() &&
            (value[start] == ' ' ||
             value[start] == '\t' ||
             value[start] == ',' ||
             value[start] == ';')) {
            ++start;
        }
        if (start >= value.SizeBytes()) break;
        std::uint32_t end = start;
        while (end < value.SizeBytes() &&
            value[end] != ' ' &&
            value[end] != '\t' &&
            value[end] != ',' &&
            value[end] != ';') {
            ++end;
        }
        Base::Result<double> parsed =
            ::Aero::Base::Detail::ValueConversion::ParseDouble(
                value.Substr(start, end - start));
        if (!parsed || !std::isfinite(parsed.Value())) {
            return;
        }
        start = end;
    }
    DependencyObject::SetValue(TicksProperty, value);
}

void Slider::SetIsSnapToTickEnabled(
    bool value) noexcept {
    DependencyObject::SetValue(IsSnapToTickEnabledProperty, value);
}

void Slider::SetIsDirectionReversed(
    bool value) noexcept {
    DependencyObject::SetValue(IsDirectionReversedProperty, value);
}

void Slider::SetIsMoveToPointEnabled(
    bool value) noexcept {
    DependencyObject::SetValue(IsMoveToPointEnabledProperty, value);
}

double Slider::GetSnapValue(double value) const noexcept {
    value = std::clamp(
        value, GetMinimum(), GetMaximum());
    if (!GetIsSnapToTickEnabled()) return value;

    const Base::StringView ticks = GetTicks();
    bool found = false;
    double nearest = value;
    double distance =
        std::numeric_limits<double>::infinity();
    std::uint32_t start = 0U;
    while (start < ticks.SizeBytes()) {
        while (start < ticks.SizeBytes() &&
            (ticks[start] == ' ' ||
             ticks[start] == '\t' ||
             ticks[start] == ',' ||
             ticks[start] == ';')) {
            ++start;
        }
        if (start >= ticks.SizeBytes()) break;
        std::uint32_t end = start;
        while (end < ticks.SizeBytes() &&
            ticks[end] != ' ' &&
            ticks[end] != '\t' &&
            ticks[end] != ',' &&
            ticks[end] != ';') {
            ++end;
        }
        Base::Result<double> parsed =
            ::Aero::Base::Detail::ValueConversion::ParseDouble(
                ticks.Substr(start, end - start));
        if (parsed &&
            parsed.Value() >= GetMinimum() &&
            parsed.Value() <= GetMaximum()) {
            const double candidateDistance =
                std::fabs(parsed.Value() - value);
            if (!found ||
                candidateDistance < distance) {
                found = true;
                nearest = parsed.Value();
                distance = candidateDistance;
            }
        }
        start = end;
    }
    if (found) return nearest;

    const double frequency = GetTickFrequency();
    const double step = std::round(
        (value - GetMinimum()) / frequency);
    return std::clamp(
        GetMinimum() + step * frequency,
        GetMinimum(), GetMaximum());
}

Base::Result<bool> Slider::DecreaseSmall() noexcept {
    const double oldValue = GetValue();
    SetValue(GetSnapValue(oldValue - GetSmallChange()));
    return !Same(oldValue, GetValue());
}

Base::Result<bool> Slider::IncreaseSmall() noexcept {
    const double oldValue = GetValue();
    SetValue(GetSnapValue(oldValue + GetSmallChange()));
    return !Same(oldValue, GetValue());
}

Base::Result<bool> Slider::DecreaseLarge() noexcept {
    const double oldValue = GetValue();
    SetValue(GetSnapValue(oldValue - GetLargeChange()));
    return !Same(oldValue, GetValue());
}

Base::Result<bool> Slider::IncreaseLarge() noexcept {
    const double oldValue = GetValue();
    SetValue(GetSnapValue(oldValue + GetLargeChange()));
    return !Same(oldValue, GetValue());
}

void Slider::SetValueFromPosition(
    double position,
    double trackLength) noexcept {
    if (!std::isfinite(position) ||
        !std::isfinite(trackLength) ||
        trackLength <= 0.0) {
        return;
    }
    double normalized =
        std::clamp(position / trackLength, 0.0, 1.0);
    if (GetIsDirectionReversed()) {
        normalized = 1.0 - normalized;
    }
    SetValue(GetSnapValue(
        GetMinimum() +
        normalized * (GetMaximum() - GetMinimum())));
}

double Slider::GetNormalizedValueForLayout() const noexcept {
    const double range = GetMaximum() - GetMinimum();
    double normalized = range > 0.0
        ? std::clamp(
            (GetValue() - GetMinimum()) / range,
            0.0, 1.0)
        : 0.0;
    if (GetIsDirectionReversed()) {
        normalized = 1.0 - normalized;
    }
    return normalized;
}

Size Slider::ArrangeOverride(
    Size finalSize) noexcept {
    return Control::ArrangeOverride(finalSize);
}

void Slider::OnRender(
    DrawingContext& context) noexcept {
    auto& builder = Aero::Render::Detail::DrawingPrivate::Builder(context);
    const TickPlacement placement =
        GetTickPlacement();
    const Size size = GetRenderSize();
    if (size.width <= 0.0 ||
        size.height <= 0.0) {
        return;
    }
    const bool first =
        placement == TickPlacement::TopLeft ||
        placement == TickPlacement::Both;
    const bool second =
        placement == TickPlacement::BottomRight ||
        placement == TickPlacement::Both;
    const bool horizontal =
        GetOrientation() ==
        Orientation::Horizontal;
    const double primary =
        horizontal ? size.width : size.height;
    constexpr double thumbLength = 14.0;
    const double startPixel =
        std::min(primary * 0.5, thumbLength * 0.5);
    const double travel =
        std::max(0.0,
            primary - thumbLength);
    const Color color = ::Aero::Media::Detail::SampleBrush(GetForeground());
    Color trackColor = color;
    trackColor.alpha *= 0.35F;
    const double normalized =
        GetNormalizedValueForLayout();
    Base::Result<void> chrome;
    if (horizontal) {
        chrome = builder.FillRoundedRect(
            {
                startPixel,
                std::max(0.0,
                    (size.height - 4.0) * 0.5),
                travel,
                std::min(4.0, size.height)},
            trackColor,
            2.0);
        if (chrome) {
            chrome = builder.FillRoundedRect(
                {
                    std::clamp(normalized, 0.0, 1.0) *
                        travel,
                    std::max(0.0,
                        (size.height -
                            thumbLength) * 0.5),
                    std::min(thumbLength, size.width),
                    std::min(thumbLength, size.height)},
                color,
                thumbLength * 0.5);
        }
    } else {
        chrome = builder.FillRoundedRect(
            {
                std::max(0.0,
                    (size.width - 4.0) * 0.5),
                startPixel,
                std::min(4.0, size.width),
                travel},
            trackColor,
            2.0);
        if (chrome) {
            chrome = builder.FillRoundedRect(
                {
                    std::max(0.0,
                        (size.width -
                            thumbLength) * 0.5),
                    std::clamp(normalized, 0.0, 1.0) *
                        travel,
                    std::min(thumbLength, size.width),
                    std::min(thumbLength, size.height)},
                color,
                thumbLength * 0.5);
        }
    }
    if (!chrome) return;
    if (placement == TickPlacement::None) {
        return;
    }
    auto drawTick = [&](double value)
        noexcept -> Base::Result<void> {
        if (value < GetMinimum() ||
            value > GetMaximum()) {
            return {};
        }
        const double range =
            GetMaximum() - GetMinimum();
        double normalized = range > 0.0
            ? (value - GetMinimum()) / range
            : 0.0;
        if (GetIsDirectionReversed()) {
            normalized = 1.0 - normalized;
        }
        const double pixel =
            startPixel +
            std::clamp(normalized, 0.0, 1.0) *
                travel;
        Base::Result<void> drawn;
        if (horizontal) {
            const double center =
                size.height * 0.5;
            if (first) {
                drawn = builder.FillRect(
                    {pixel, std::max(0.0, center - 9.0),
                     1.0, std::min(5.0, size.height)},
                    color);
                if (!drawn) return {};
            }
            if (second) {
                drawn = builder.FillRect(
                    {pixel, std::min(size.height, center + 4.0),
                     1.0, std::min(5.0, size.height)},
                    color);
            }
        } else {
            const double center =
                size.width * 0.5;
            if (first) {
                drawn = builder.FillRect(
                    {std::max(0.0, center - 9.0), pixel,
                     std::min(5.0, size.width), 1.0},
                    color);
                if (!drawn) return {};
            }
            if (second) {
                drawn = builder.FillRect(
                    {std::min(size.width, center + 4.0), pixel,
                     std::min(5.0, size.width), 1.0},
                    color);
            }
        }
        return drawn;
    };

    const Base::StringView ticks = GetTicks();
    if (!ticks.Empty()) {
        std::uint32_t start = 0U;
        std::uint32_t count = 0U;
        while (start < ticks.SizeBytes() &&
            count < 1024U) {
            while (start < ticks.SizeBytes() &&
                (ticks[start] == ' ' ||
                 ticks[start] == '\t' ||
                 ticks[start] == ',' ||
                 ticks[start] == ';')) {
                ++start;
            }
            if (start >= ticks.SizeBytes()) break;
            std::uint32_t end = start;
            while (end < ticks.SizeBytes() &&
                ticks[end] != ' ' &&
                ticks[end] != '\t' &&
                ticks[end] != ',' &&
                ticks[end] != ';') {
                ++end;
            }
            Base::Result<double> parsed =
                ::Aero::Base::Detail::ValueConversion::ParseDouble(
                    ticks.Substr(start, end - start));
            if (parsed) {
                Base::Result<void> drawn =
                    drawTick(parsed.Value());
                if (!drawn) {
                    return;
                }
            }
            start = end;
            ++count;
        }
        return;
    }

    const double frequency = GetTickFrequency();
    std::uint32_t count = 0U;
    for (double value = GetMinimum();
         value <= GetMaximum() +
            frequency * 0.000001 &&
         count < 1024U;
         value += frequency, ++count) {
        Base::Result<void> drawn =
            drawTick(std::min(value, GetMaximum()));
        if (!drawn) return;
    }
    return;
}

Base::Ref<Media::Brush> TickBar::GetFill() const noexcept {
    return GetValueOr(
        FillProperty,
        Base::Ref<Media::Brush>{});
}

TickBarPlacement TickBar::GetPlacement() const noexcept {
    return GetValueOr(
        PlacementProperty,
        TickBarPlacement::Top);
}

void TickBar::SetFill(
    Base::Ref<Media::Brush> value) noexcept {
    SetValue(FillProperty, std::move(value));
}

void TickBar::SetPlacement(
    TickBarPlacement value) noexcept {
    SetValue(PlacementProperty, value);
}

void TickBar::OnRender(
    DrawingContext& context) noexcept {
    auto& builder = Aero::Render::Detail::DrawingPrivate::Builder(context);
    DependencyObject* parent = GetTemplatedParent();
    if (parent == nullptr ||
        !PropertyRegistry().Types().IsDerivedFrom(
            parent->RuntimeType(), Slider::StaticTypeId())) {
        return;
    }

    const Slider& slider = static_cast<const Slider&>(*parent);
    const Size size = GetRenderSize();
    const bool horizontal =
        GetPlacement() == TickBarPlacement::Top ||
        GetPlacement() == TickBarPlacement::Bottom;
    const double primary = horizontal ? size.width : size.height;
    const double range = slider.GetMaximum() - slider.GetMinimum();
    if (primary <= 0.0 || range < 0.0) return;

    const Color color = ::Aero::Media::Detail::SampleBrush(
        GetFill(), 0.5, ::Aero::Media::Detail::SampleBrush(GetForeground()));
    constexpr double thumbLength = 14.0;
    const double start = std::min(
        primary * 0.5, thumbLength * 0.5);
    const double travel = std::max(0.0, primary - thumbLength);
    auto drawTick = [&](double value)
        noexcept -> Base::Result<void> {
        if (value < slider.GetMinimum() ||
            value > slider.GetMaximum()) {
            return {};
        }
        double normalized = range > 0.0
            ? (value - slider.GetMinimum()) / range
            : 0.0;
        if (slider.GetIsDirectionReversed()) {
            normalized = 1.0 - normalized;
        }
        const double position = start +
            std::clamp(normalized, 0.0, 1.0) * travel;
        if (horizontal) {
            return builder.FillRect({
                position,
                GetPlacement() == TickBarPlacement::Top
                    ? 0.0
                    : std::max(0.0, size.height - 4.0),
                1.0,
                std::min(4.0, size.height)}, color);
        }
        return builder.FillRect({
            GetPlacement() == TickBarPlacement::Left
                ? 0.0
                : std::max(0.0, size.width - 4.0),
            position,
            std::min(4.0, size.width),
            1.0}, color);
    };

    const Base::StringView ticks = slider.GetTicks();
    if (!ticks.Empty()) {
        std::uint32_t begin = 0U;
        std::uint32_t count = 0U;
        while (begin < ticks.SizeBytes() && count < 1024U) {
            while (begin < ticks.SizeBytes() &&
                (ticks[begin] == ' ' || ticks[begin] == '\t' ||
                 ticks[begin] == ',' || ticks[begin] == ';')) {
                ++begin;
            }
            if (begin >= ticks.SizeBytes()) break;
            std::uint32_t end = begin;
            while (end < ticks.SizeBytes() &&
                ticks[end] != ' ' && ticks[end] != '\t' &&
                ticks[end] != ',' && ticks[end] != ';') {
                ++end;
            }
            Base::Result<double> parsed =
                ::Aero::Base::Detail::ValueConversion::ParseDouble(
                    ticks.Substr(begin, end - begin));
            if (parsed) {
                Base::Result<void> rendered = drawTick(parsed.Value());
                if (!rendered) return;
            }
            begin = end;
            ++count;
        }
        return;
    }

    const double frequency = slider.GetTickFrequency();
    for (double value = slider.GetMinimum(), count = 0.0;
         value <= slider.GetMaximum() + frequency * 0.000001 &&
         count < 1024.0;
         value += frequency, count += 1.0) {
        Base::Result<void> rendered = drawTick(
            std::min(value, slider.GetMaximum()));
        if (!rendered) return;
    }
    return;
}

bool ProgressBar::GetIsIndeterminate() const noexcept {
    return ReadBool(
        *this, IsIndeterminateProperty, false);
}

Orientation ProgressBar::GetOrientation() const noexcept {
    return ReadOrientation(*this, OrientationProperty);
}

void ProgressBar::SetIsIndeterminate(
    bool value) noexcept {
    DependencyObject::SetValue(IsIndeterminateProperty, value);
}

void ProgressBar::SetOrientation(
    Orientation value) noexcept {
    StoreOrientation(*this, OrientationProperty, value);
}

double ProgressBar::GetNormalizedValue() const noexcept {
    const double range = GetMaximum() - GetMinimum();
    return range > 0.0
        ? std::clamp(
            (GetValue() - GetMinimum()) / range,
            0.0,
            1.0)
        : 0.0;
}

} // namespace Aero::Controls

namespace Aero::Controls {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Controls;
using namespace ::Aero::Controls::Detail;
using namespace ::Aero::GuiPrivate::Detail;

ScrollViewer::Impl::Impl(
    ElementTree& tree,
    EventRouter& events) noexcept
    : tree_(&tree),
      events_(&events),
      wheelHandler_(
          this,
          &ScrollViewer::Impl::OnMouseWheel) {}

ScrollViewer::Impl::~Impl() noexcept {
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

std::uint32_t ScrollViewer::Impl::FindViewer(
    const ScrollViewer& viewer) const noexcept {
    const VisualHandle handle = Aero::GuiPrivate::Detail::ElementPrivate::Handle(viewer);
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

Base::Result<void> ScrollViewer::Impl::Attach(
    ScrollViewer& viewer) noexcept {
    if (FindViewer(viewer) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "ScrollViewer is already attached");
    }
    if (Aero::GuiPrivate::Detail::ElementPrivate::Tree(viewer) != tree_ ||
        !Aero::GuiPrivate::Detail::ElementPrivate::Handle(viewer).IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ScrollViewer must be loaded in the interaction tree");
    }
    Base::Result<void> handler =
        viewer.AddHandlerChecked(
            UIElement::MouseWheelEvent,
            wheelHandler_);
    if (!handler) return handler.GetStatus();
    Base::Result<void> added =
        viewers_.PushBack(
            {&viewer, Aero::GuiPrivate::Detail::ElementPrivate::Handle(viewer)});
    if (!added) {
        static_cast<void>(viewer.RemoveHandler(
            UIElement::MouseWheelEvent,
            wheelHandler_));
        return added.GetStatus();
    }
    return {};
}

Base::Result<bool> ScrollViewer::Impl::Detach(
    ScrollViewer& viewer) noexcept {
    const std::uint32_t index = FindViewer(viewer);
    if (index == UINT32_MAX) return false;
    static_cast<void>(viewer.RemoveHandler(
        UIElement::MouseWheelEvent,
        wheelHandler_));
    if (index + 1U != viewers_.Size()) {
        viewers_[index] = viewers_.Back();
    }
    viewers_.PopBack();
    return true;
}

void ScrollViewer::Impl::OnMouseWheel(
    Base::Object* sender,
    MouseWheelEventArgs& args) noexcept {
    auto* viewer = static_cast<ScrollViewer*>(sender);
    if (viewer == nullptr ||
        FindViewer(*viewer) == UINT32_MAX) {
        return;
    }
    const double horizontal =
        -args.GetDeltaX() * viewer->GetLineScrollAmount();
    const double vertical =
        -args.GetDeltaY() * viewer->GetLineScrollAmount();
    Base::Result<bool> changed =
        viewer->ApplyScrollDelta(
            horizontal,
            vertical,
            ScrollInputKind::Wheel);
    if (changed && changed.Value()) {
        args.SetHandled(true);
    }
}

Slider::Impl::Impl(
    ElementTree& tree,
    EventRouter& events,
    InputRouter& input) noexcept
    : tree_(&tree),
      events_(&events),
      input_(&input),
      mouseDownHandler_(
          this,
          &Slider::Impl::OnMouseDown),
      mouseMoveHandler_(
          this,
          &Slider::Impl::OnMouseMove),
      mouseUpHandler_(
          this,
          &Slider::Impl::OnMouseUp),
      keyDownHandler_(
          this,
          &Slider::Impl::OnKeyDown),
      captureChangedHandler_(
          this,
          &Slider::Impl::OnCaptureChanged),
      decreaseSmallHandler_(
          this,
          &Slider::Impl::OnDecreaseSmallCommand),
      increaseSmallHandler_(
          this,
          &Slider::Impl::OnIncreaseSmallCommand),
      decreaseLargeHandler_(
          this,
          &Slider::Impl::OnDecreaseLargeCommand),
      increaseLargeHandler_(
          this,
          &Slider::Impl::OnIncreaseLargeCommand) {}

Slider::Impl::~Impl()
    noexcept {
    while (!sliders_.Empty()) {
        Slider* slider =
            Resolve(sliders_.Size() - 1U);
        if (slider == nullptr) {
            sliders_.PopBack();
            continue;
        }
        static_cast<void>(Detach(*slider));
    }
    static_cast<void>(
        input_->RemovePointerCaptureChanged(
            captureChangedHandler_));
}

std::uint32_t Slider::Impl::Find(
    const Slider& slider) const noexcept {
    for (std::uint32_t index = 0U;
         index < sliders_.Size(); ++index) {
        const VisualHandle current =
            Aero::GuiPrivate::Detail::ElementPrivate::Handle(slider);
        if (sliders_[index].handle.index ==
                current.index &&
            sliders_[index].handle.generation ==
                current.generation) {
            return index;
        }
    }
    return UINT32_MAX;
}

Slider* Slider::Impl::Resolve(
    std::uint32_t index) noexcept {
    if (index >= sliders_.Size()) return nullptr;
    Visual* node =
        tree_->ResolveHandle(
            sliders_[index].handle);
    if (node == nullptr ||
        !node->PropertyRegistry().Types().
            IsDerivedFrom(
                node->RuntimeType(),
                Slider::StaticTypeId())) {
        return nullptr;
    }
    return static_cast<Slider*>(node);
}

void Slider::Impl::RemoveAt(
    std::uint32_t index) noexcept {
    if (index >= sliders_.Size()) return;
    if (index + 1U != sliders_.Size()) {
        sliders_[index] =
            sliders_.Back();
    }
    sliders_.PopBack();
}

Base::Result<void> Slider::Impl::Attach(
    Slider& slider) noexcept {
    if (Find(slider) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Slider is already attached");
    }
    if (Aero::GuiPrivate::Detail::ElementPrivate::Tree(slider) != tree_ ||
        !Aero::GuiPrivate::Detail::ElementPrivate::Handle(slider).IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Slider must be loaded in the interaction tree");
    }
    if (sliders_.Empty()) {
        input_->AddPointerCaptureChanged(captureChangedHandler_);
    }
    Base::Result<void> status =
        slider.AddHandlerChecked(
            UIElement::MouseDownEvent,
            mouseDownHandler_);
    if (status) {
        status = slider.AddHandlerChecked(
            UIElement::MouseMoveEvent,
            mouseMoveHandler_);
    }
    if (status) {
        status = slider.AddHandlerChecked(
            UIElement::MouseUpEvent,
            mouseUpHandler_);
    }
    if (status) {
        status = slider.AddHandlerChecked(
            UIElement::KeyDownEvent,
            keyDownHandler_);
    }
    if (!status) {
        static_cast<void>(slider.RemoveHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_));
        static_cast<void>(slider.RemoveHandler(
            UIElement::MouseMoveEvent,
            mouseMoveHandler_));
        static_cast<void>(slider.RemoveHandler(
            UIElement::MouseUpEvent,
            mouseUpHandler_));
        static_cast<void>(slider.RemoveHandler(
            UIElement::KeyDownEvent,
            keyDownHandler_));
        if (sliders_.Empty()) {
            static_cast<void>(
                input_->RemovePointerCaptureChanged(
                    captureChangedHandler_));
        }
        return status.GetStatus();
    }
    SliderRecord record;
    record.handle =
        Aero::GuiPrivate::Detail::ElementPrivate::Handle(slider);
    const auto addCommand =
        [this, &slider](
            Base::StringView name,
            const ExecutedRoutedEventHandler& handler,
            Input::CommandBindingHandle& output) noexcept
            -> Base::Result<void> {
        Base::Result<Base::Ref<Input::RoutedCommand>> command =
            Input::RoutedCommand::ResolveStatic(
                Slider::StaticTypeId(), name);
        if (!command) return command.GetStatus();
        Base::Result<Input::CommandBindingHandle> added =
            input_->AddCommandBinding(
                slider,
                Input::CommandBinding(
                    std::move(command).Value(), handler));
        if (!added) return added.GetStatus();
        output = added.Value();
        return {};
    };
    status = addCommand(
        "DecreaseSmall", decreaseSmallHandler_,
        record.decreaseSmallCommand);
    if (status) {
        status = addCommand(
            "IncreaseSmall", increaseSmallHandler_,
            record.increaseSmallCommand);
    }
    if (status) {
        status = addCommand(
            "DecreaseLarge", decreaseLargeHandler_,
            record.decreaseLargeCommand);
    }
    if (status) {
        status = addCommand(
            "IncreaseLarge", increaseLargeHandler_,
            record.increaseLargeCommand);
    }
    if (!status) {
        if (record.decreaseSmallCommand.IsValid()) {
            static_cast<void>(input_->RemoveCommandBinding(
                record.decreaseSmallCommand));
        }
        if (record.increaseSmallCommand.IsValid()) {
            static_cast<void>(input_->RemoveCommandBinding(
                record.increaseSmallCommand));
        }
        if (record.decreaseLargeCommand.IsValid()) {
            static_cast<void>(input_->RemoveCommandBinding(
                record.decreaseLargeCommand));
        }
        static_cast<void>(slider.RemoveHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_));
        static_cast<void>(slider.RemoveHandler(
            UIElement::MouseMoveEvent,
            mouseMoveHandler_));
        static_cast<void>(slider.RemoveHandler(
            UIElement::MouseUpEvent,
            mouseUpHandler_));
        static_cast<void>(slider.RemoveHandler(
            UIElement::KeyDownEvent,
            keyDownHandler_));
        if (sliders_.Empty()) {
            static_cast<void>(
                input_->RemovePointerCaptureChanged(
                    captureChangedHandler_));
        }
        return status.GetStatus();
    }
    Base::Result<void> appended =
        sliders_.PushBack(record);
    if (!appended) {
        static_cast<void>(input_->RemoveCommandBinding(
            record.decreaseSmallCommand));
        static_cast<void>(input_->RemoveCommandBinding(
            record.increaseSmallCommand));
        static_cast<void>(input_->RemoveCommandBinding(
            record.decreaseLargeCommand));
        static_cast<void>(input_->RemoveCommandBinding(
            record.increaseLargeCommand));
        static_cast<void>(slider.RemoveHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_));
        static_cast<void>(slider.RemoveHandler(
            UIElement::MouseMoveEvent,
            mouseMoveHandler_));
        static_cast<void>(slider.RemoveHandler(
            UIElement::MouseUpEvent,
            mouseUpHandler_));
        static_cast<void>(slider.RemoveHandler(
            UIElement::KeyDownEvent,
            keyDownHandler_));
        if (sliders_.Empty()) {
            static_cast<void>(
                input_->RemovePointerCaptureChanged(
                    captureChangedHandler_));
        }
        return appended.GetStatus();
    }
    return {};
}

Base::Result<bool> Slider::Impl::Detach(
    Slider& slider) noexcept {
    const std::uint32_t index = Find(slider);
    if (index == UINT32_MAX) return false;
    if (sliders_[index].dragging) {
        static_cast<void>(
            input_->ReleasePointer(
                sliders_[index].pointerId));
    }
    static_cast<void>(slider.RemoveHandler(
        UIElement::MouseDownEvent,
        mouseDownHandler_));
    static_cast<void>(slider.RemoveHandler(
        UIElement::MouseMoveEvent,
        mouseMoveHandler_));
    static_cast<void>(slider.RemoveHandler(
        UIElement::MouseUpEvent,
        mouseUpHandler_));
    static_cast<void>(slider.RemoveHandler(
        UIElement::KeyDownEvent,
        keyDownHandler_));
    static_cast<void>(input_->RemoveCommandBinding(
        sliders_[index].decreaseSmallCommand));
    static_cast<void>(input_->RemoveCommandBinding(
        sliders_[index].increaseSmallCommand));
    static_cast<void>(input_->RemoveCommandBinding(
        sliders_[index].decreaseLargeCommand));
    static_cast<void>(input_->RemoveCommandBinding(
        sliders_[index].increaseLargeCommand));
    RemoveAt(index);
    if (sliders_.Empty()) {
        static_cast<void>(
            input_->RemovePointerCaptureChanged(
                captureChangedHandler_));
    }
    return true;
}

void Slider::Impl::OnDecreaseSmallCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* slider = static_cast<Slider*>(sender);
    if (slider != nullptr) {
        static_cast<void>(slider->DecreaseSmall());
        args.SetHandled(true);
    }
}

void Slider::Impl::OnIncreaseSmallCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* slider = static_cast<Slider*>(sender);
    if (slider != nullptr) {
        static_cast<void>(slider->IncreaseSmall());
        args.SetHandled(true);
    }
}

void Slider::Impl::OnDecreaseLargeCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* slider = static_cast<Slider*>(sender);
    if (slider != nullptr) {
        static_cast<void>(slider->DecreaseLarge());
        args.SetHandled(true);
    }
}

void Slider::Impl::OnIncreaseLargeCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* slider = static_cast<Slider*>(sender);
    if (slider != nullptr) {
        static_cast<void>(slider->IncreaseLarge());
        args.SetHandled(true);
    }
}

Base::Result<void>
Slider::Impl::SetFromPoint(
    Slider& slider,
    Point point) noexcept {
    const bool horizontal =
        slider.GetOrientation() ==
        Orientation::Horizontal;
    const double position =
        horizontal ? point.x : point.y;
    const double length =
        horizontal
        ? slider.GetRenderSize().width
        : slider.GetRenderSize().height;
    slider.SetValueFromPosition(position, length);
    return {};
}

void Slider::Impl::OnMouseDown(
    Base::Object* sender,
    MouseButtonEventArgs& args) noexcept {
    auto& slider =
        *static_cast<Slider*>(sender);
    const std::uint32_t index =
        Find(slider);
    if (index == UINT32_MAX ||
        !slider.GetIsEnabled() ||
        args.GetChangedButton() !=
            MouseButton::Left) {
        return;
    }
    SliderRecord& record =
        sliders_[index];
    record.pointerId = args.GetPointerId();
    static_cast<void>(
        input_->SetFocus(&slider));
    Point local = args.GetPosition();
    const bool horizontal =
        slider.GetOrientation() ==
        Orientation::Horizontal;
    const double position =
        horizontal ? local.x : local.y;
    const double length =
        horizontal
        ? slider.GetRenderSize().width
        : slider.GetRenderSize().height;
    const double range =
        slider.GetMaximum() - slider.GetMinimum();
    double normalized = range > 0.0
        ? (slider.GetValue() - slider.GetMinimum()) /
            range
        : 0.0;
    if (slider.GetIsDirectionReversed()) {
        normalized = 1.0 - normalized;
    }
    const double thumbPosition =
        7.0 +
        std::clamp(normalized, 0.0, 1.0) *
            std::max(0.0, length - 14.0);
    record.dragging =
        slider.GetIsMoveToPointEnabled() ||
        std::fabs(position - thumbPosition) <=
            10.0;
    if (record.dragging) {
        static_cast<void>(
            input_->CapturePointer(
                args.GetPointerId(), slider));
    }
    if (slider.GetIsMoveToPointEnabled()) {
        static_cast<void>(
            SetFromPoint(slider, args.GetPosition()));
    } else if (!record.dragging) {
        const bool after =
            position >= length * 0.5;
        const bool increase =
            slider.GetIsDirectionReversed()
            ? !after : after;
        static_cast<void>(
            increase
            ? slider.IncreaseLarge()
            : slider.DecreaseLarge());
    }
    args.SetHandled(true);
}

void Slider::Impl::OnMouseMove(
    Base::Object* sender,
    MouseEventArgs& args) noexcept {
    auto& slider =
        *static_cast<Slider*>(sender);
    const std::uint32_t index =
        Find(slider);
    if (index == UINT32_MAX ||
        !sliders_[index].dragging ||
        sliders_[index].pointerId !=
            args.GetPointerId()) {
        return;
    }
    static_cast<void>(
        SetFromPoint(slider, args.GetPosition()));
    args.SetHandled(true);
}

void Slider::Impl::OnMouseUp(
    Base::Object* sender,
    MouseButtonEventArgs& args) noexcept {
    auto& slider =
        *static_cast<Slider*>(sender);
    const std::uint32_t index =
        Find(slider);
    if (index == UINT32_MAX ||
        args.GetChangedButton() !=
            MouseButton::Left ||
        !sliders_[index].dragging ||
        sliders_[index].pointerId !=
            args.GetPointerId()) {
        return;
    }
    static_cast<void>(
        SetFromPoint(slider, args.GetPosition()));
    sliders_[index].dragging = false;
    static_cast<void>(
        input_->ReleasePointer(
            args.GetPointerId()));
    args.SetHandled(true);
}

void Slider::Impl::OnKeyDown(
    Base::Object* sender,
    KeyEventArgs& args) noexcept {
    auto& slider =
        *static_cast<Slider*>(sender);
    if (Find(slider) == UINT32_MAX ||
        !slider.GetIsEnabled()) {
        return;
    }
    bool changed = false;
    bool handled = true;
    const bool reversed =
        slider.GetIsDirectionReversed();
    if (args.GetKey() == KeyboardKeyHome) {
        const double oldValue = slider.GetValue();
        slider.SetValue(
            reversed
            ? slider.GetMaximum()
            : slider.GetMinimum());
        changed = !Same(oldValue, slider.GetValue());
    } else if (args.GetKey() == KeyboardKeyEnd) {
        const double oldValue = slider.GetValue();
        slider.SetValue(
            reversed
            ? slider.GetMinimum()
            : slider.GetMaximum());
        changed = !Same(oldValue, slider.GetValue());
    } else if (
        args.GetKey() == KeyboardKeyLeft ||
        args.GetKey() == KeyboardKeyDown) {
        Base::Result<bool> result = reversed
            ? slider.IncreaseSmall()
            : slider.DecreaseSmall();
        changed = result && result.Value();
    } else if (
        args.GetKey() == KeyboardKeyRight ||
        args.GetKey() == KeyboardKeyUp) {
        Base::Result<bool> result = reversed
            ? slider.DecreaseSmall()
            : slider.IncreaseSmall();
        changed = result && result.Value();
    } else {
        handled = false;
    }
    if (handled && changed) {
        args.SetHandled(true);
    }
}

void Slider::Impl::OnCaptureChanged(
    std::uint32_t pointerId,
    UIElement* target,
    bool captured) noexcept {
    if (captured) return;
    for (SliderRecord& record :
         sliders_) {
        if (!record.dragging ||
            record.pointerId != pointerId) {
            continue;
        }
        Slider* slider =
            static_cast<Slider*>(
                tree_->ResolveHandle(record.handle));
        if (target == nullptr ||
            target == slider) {
            record.dragging = false;
        }
    }
}

} // namespace Aero::Controls
