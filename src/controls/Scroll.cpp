#include "../render/DisplayList.hpp"
#include <Aero/Controls/Primitives.hpp>
#include "../render/DrawingContextAccess.hpp"
#include <Aero/Meta/ValueConversion.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include "gui/RoutedEventInternal.hpp"
#include "RuntimeManagers.hpp"

namespace Aero::Controls {

using namespace Primitives;
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
Base::Result<void> StoreDouble(
    DependencyObject& object,
    const TProperty& property,
    double value) noexcept {
    return object.SetValue(property, value);
}

template <typename TProperty>
Base::Result<void> StoreOrientation(
    DependencyObject& object,
    const TProperty& property,
    Orientation value) noexcept {
    return object.SetValue(property, value);
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
    return DependencyObject::SetValue(
        CanContentScrollProperty, value);
}

bool ScrollContentPresenter::AllowsHorizontalScroll() const noexcept {
    return canHorizontallyScroll_;
}

bool ScrollContentPresenter::AllowsVerticalScroll() const noexcept {
    return canVerticallyScroll_;
}

bool ScrollContentPresenter::UsesContentScrolling() const noexcept {
    return GetValueOr(
        CanContentScrollProperty, false);
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
        value.extentWidth = child->GetDesiredSize().width;
        value.extentHeight = child->GetDesiredSize().height;
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
    if (GetTemplateRoot() != nullptr) {
        return ContentControl::ArrangeOverride(
            finalSize);
    }
    UIElement* child = ContentElement();
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
    UpdateComputedScrollBarVisibility(Data());
}

ScrollViewer::~ScrollViewer() {
    if (interactions_ != nullptr) {
        static_cast<void>(
            static_cast<ScrollInteractionManager*>(
                interactions_)->Detach(*this));
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

double ScrollViewer::ScrollableWidth() const noexcept {
    return ReadDouble(*this, ScrollableWidthProperty);
}

double ScrollViewer::ScrollableHeight() const noexcept {
    return ReadDouble(*this, ScrollableHeightProperty);
}

ScrollBarVisibility
ScrollViewer::HorizontalScrollBarVisibility() const noexcept {
    return GetHorizontalScrollBarVisibility(*this);
}

ScrollBarVisibility
ScrollViewer::VerticalScrollBarVisibility() const noexcept {
    return GetVerticalScrollBarVisibility(*this);
}

PanningMode ScrollViewer::GetPanningMode() const noexcept {
    return GetValueOr(PanningModeProperty, PanningMode::None);
}

Base::Result<void> ScrollViewer::SetPanningMode(
    PanningMode value) noexcept {
    return SetValue(PanningModeProperty, value);
}

Visibility
ScrollViewer::ComputedHorizontalScrollBarVisibility()
    const noexcept {
    return GetValueOr(
        ComputedHorizontalScrollBarVisibilityProperty,
        Visibility::Collapsed);
}

Visibility
ScrollViewer::ComputedVerticalScrollBarVisibility()
    const noexcept {
    return GetValueOr(
        ComputedVerticalScrollBarVisibilityProperty,
        Visibility::Collapsed);
}

Base::Result<void>
ScrollViewer::SetCanHorizontallyScroll(
    bool value) noexcept {
    return SetValue(CanHorizontallyScrollProperty, value);
}

Base::Result<void>
ScrollViewer::SetCanVerticallyScroll(
    bool value) noexcept {
    return SetValue(CanVerticallyScrollProperty, value);
}

Base::Result<void>
ScrollViewer::SetCanContentScroll(
    bool value) noexcept {
    return SetValue(CanContentScrollProperty, value);
}

Base::Result<void>
ScrollViewer::SetHorizontalScrollBarVisibility(
    ScrollBarVisibility value) noexcept {
    Base::Result<void> changed =
        SetHorizontalScrollBarVisibility(*this, value);
    if (changed) {
        UpdateComputedScrollBarVisibility(Data());
    }
    return changed;
}

Base::Result<void>
ScrollViewer::SetVerticalScrollBarVisibility(
    ScrollBarVisibility value) noexcept {
    Base::Result<void> changed =
        SetVerticalScrollBarVisibility(*this, value);
    if (changed) {
        UpdateComputedScrollBarVisibility(Data());
    }
    return changed;
}

Base::Result<bool> ScrollViewer::SetHorizontalOffset(
    double value) noexcept {
    return contentPresenter_ != nullptr
        ? contentPresenter_->SetHorizontalOffset(value)
        : ScrollContentPresenter::
            SetHorizontalOffset(value);
}

Base::Result<bool> ScrollViewer::SetVerticalOffset(
    double value) noexcept {
    return contentPresenter_ != nullptr
        ? contentPresenter_->SetVerticalOffset(value)
        : ScrollContentPresenter::
            SetVerticalOffset(value);
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

Base::Result<void> ScrollViewer::OnApplyTemplate()
    noexcept {
    Base::Result<void> applied =
        Control::OnApplyTemplate();
    if (!applied) return applied.GetStatus();
    DependencyObject* part = GetTemplateChild(
        ScrollContentPresenter::StaticTypeId());
    contentPresenter_ =
        part != nullptr &&
        part != this
        ? static_cast<ScrollContentPresenter*>(part)
        : nullptr;
    return {};
}

Base::Result<Size> ScrollViewer::MeasureOverride(
    Size availableSize) noexcept {
    Base::Result<Size> measured =
        ScrollContentPresenter::MeasureOverride(
            availableSize);
    if (!measured || contentPresenter_ == nullptr) {
        return measured;
    }
    AdoptPresenterData(
        *contentPresenter_,
        contentPresenter_->Data(),
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

Base::Result<void>
ScrollViewer::SetHorizontalScrollBarVisibility(
    DependencyObject& element,
    ScrollBarVisibility value) noexcept {
    return element.SetValue(
        HorizontalScrollBarVisibilityProperty, value);
}

Base::Result<void>
ScrollViewer::SetVerticalScrollBarVisibility(
    DependencyObject& element,
    ScrollBarVisibility value) noexcept {
    return element.SetValue(
        VerticalScrollBarVisibilityProperty, value);
}

bool ScrollViewer::AllowsHorizontalScroll() const noexcept {
    return HorizontalScrollBarVisibility() !=
            ScrollBarVisibility::Disabled &&
        ReadBool(
            *this, CanHorizontallyScrollProperty, true);
}

bool ScrollViewer::AllowsVerticalScroll() const noexcept {
    return VerticalScrollBarVisibility() !=
            ScrollBarVisibility::Disabled &&
        ReadBool(
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
            ? HorizontalScrollBarVisibility()
            : VerticalScrollBarVisibility();
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

    if (events_ != nullptr) {
        ScrollChangedEventArgs args;
        args.oldData = oldData;
        args.newData = newData;
        args.inputKind = kind;
        static_cast<void>(
            static_cast<Aero::Detail::EventRouter*>(events_)->RaiseEvent(
            *this, ScrollChangedEvent, &args));
    }
}

void ScrollViewer::UpdateComputedScrollBarVisibility(
    const ScrollData& data) noexcept {
    static_cast<void>(SetReadOnlyCurrentValue(
        ComputedHorizontalScrollBarVisibilityProperty,
        ComputeScrollBarVisibility(
            HorizontalScrollBarVisibility(),
            data.extentWidth,
            data.viewportWidth)));
    static_cast<void>(SetReadOnlyCurrentValue(
        ComputedVerticalScrollBarVisibilityProperty,
        ComputeScrollBarVisibility(
            VerticalScrollBarVisibility(),
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
    Base::Result<void> published =
        SetReadOnlyCurrentValue(
            IsDraggingProperty, true);
    if (!published) return published.GetStatus();
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
    Base::Result<void> published =
        SetReadOnlyCurrentValue(
            IsDraggingProperty, false);
    if (!published) return published.GetStatus();
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

double GridSplitter::DragIncrement() const noexcept {
    return GetValueOr(DragIncrementProperty, 1.0);
}

double GridSplitter::KeyboardIncrement() const noexcept {
    return GetValueOr(KeyboardIncrementProperty, 10.0);
}

GridResizeDirection GridSplitter::ResizeDirection() const noexcept {
    return GetValueOr(ResizeDirectionProperty, GridResizeDirection::Auto);
}

GridResizeBehavior GridSplitter::ResizeBehavior() const noexcept {
    return GetValueOr(
        ResizeBehaviorProperty,
        GridResizeBehavior::BasedOnAlignment);
}

bool GridSplitter::ShowsPreview() const noexcept {
    return GetValueOr(ShowsPreviewProperty, false);
}

Base::Ref<Aero::Style> GridSplitter::PreviewStyle() const noexcept {
    return GetValueOr(
        PreviewStyleProperty,
        Base::Ref<Aero::Style>{});
}

Base::Result<void> GridSplitter::SetDragIncrement(double value) noexcept {
    return SetValue(DragIncrementProperty, value);
}

Base::Result<void> GridSplitter::SetKeyboardIncrement(double value) noexcept {
    return SetValue(KeyboardIncrementProperty, value);
}

Base::Result<void> GridSplitter::SetResizeDirection(
    GridResizeDirection value) noexcept {
    return SetValue(ResizeDirectionProperty, value);
}

Base::Result<void> GridSplitter::SetResizeBehavior(
    GridResizeBehavior value) noexcept {
    return SetValue(ResizeBehaviorProperty, value);
}

Base::Result<void> GridSplitter::SetShowsPreview(bool value) noexcept {
    return SetValue(ShowsPreviewProperty, value);
}

Base::Result<void> GridSplitter::SetPreviewStyle(
    Base::Ref<Aero::Style> value) noexcept {
    return SetValue(PreviewStyleProperty, std::move(value));
}

bool Track::IsDirectionReversed() const noexcept {
    return GetValueOr(
        IsDirectionReversedProperty, false);
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

Base::Result<void> Track::SetIsDirectionReversed(
    bool value) noexcept {
    return DependencyObject::SetValue(
        IsDirectionReversedProperty, value);
}

Base::Result<void> Track::SetDecreaseRepeatButton(
    Base::Ref<RepeatButton> value) noexcept {
    decreaseRepeatButton_ = std::move(value);
    return InvalidateMeasure();
}

Base::Result<void> Track::SetThumb(
    Base::Ref<Thumb> value) noexcept {
    thumb_ = std::move(value);
    return InvalidateMeasure();
}

Base::Result<void> Track::SetIncreaseRepeatButton(
    Base::Ref<RepeatButton> value) noexcept {
    increaseRepeatButton_ = std::move(value);
    return InvalidateMeasure();
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
    const double offset = travel *
        (std::clamp(Value(), Minimum(), Maximum()) -
            Minimum()) /
        range;
    const bool invert =
        GetOrientation() == Orientation::Vertical
        ? !IsDirectionReversed()
        : IsDirectionReversed();
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
        trackLength - ThumbLength(
            trackLength, minimumThumbLength);
    const double range =
        Maximum() - Minimum();
    if (travel <= 0.0 || range <= 0.0) {
        return Minimum();
    }
    double normalized =
        std::clamp(offset, 0.0, travel) / travel;
    const bool invert =
        GetOrientation() == Orientation::Vertical
        ? !IsDirectionReversed()
        : IsDirectionReversed();
    if (invert) {
        normalized = 1.0 - normalized;
    }
    return Minimum() + normalized * range;
}

Base::Result<Size> Track::MeasureOverride(
    Size availableSize) noexcept {
    Size desired{};
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Base::Result<void> measured =
            MeasureChild(*child, availableSize);
        if (!measured) return measured.GetStatus();
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

Base::Result<Size> Track::ArrangeOverride(
    Size finalSize) noexcept {
    const bool vertical =
        GetOrientation() == Orientation::Vertical;
    const double length =
        vertical ? finalSize.height : finalSize.width;
    const double thumbLength =
        ThumbLength(length);
    const double thumbOffset =
        ThumbOffset(length);
    const double before = thumbOffset;
    const double after = std::max(
        0.0, length - thumbOffset - thumbLength);
    const bool invert = vertical
        ? !IsDirectionReversed()
        : IsDirectionReversed();
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
    return arranged
        ? Base::Result<Size>(finalSize)
        : Base::Result<Size>(
              arranged.GetStatus());
}

Orientation ScrollBar::GetOrientation() const noexcept {
    return ReadOrientation(*this, OrientationProperty);
}

ScrollBar::ScrollBar() noexcept
    : RangeBase(StaticTypeId()),
      trackPropertyChangedHandler_(
          this,
          &ScrollBar::OnTrackPropertyChanged) {
    static_cast<void>(TryAddValueChangedHandler(
        OrientationProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        MinimumProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        MaximumProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        ValueProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
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

Base::Result<void> ScrollBar::OnApplyTemplate()
    noexcept {
    Base::Result<void> applied =
        Control::OnApplyTemplate();
    if (!applied) return applied.GetStatus();
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
    return {};
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
        track_->SetRange(Minimum(), Maximum()));
    static_cast<void>(
        track_->SetViewportSize(ViewportSize()));
    static_cast<void>(
        track_->SetValue(Value()));
}

RangeBase::RangeBase(TypeId runtimeType) noexcept
    : Control(runtimeType),
      rangeChangedHandler_(
          this,
          &RangeBase::OnRangePropertyChanged) {
    static_cast<void>(TryAddValueChangedHandler(
        MinimumProperty, rangeChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        MaximumProperty, rangeChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
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

double RangeBase::Minimum() const noexcept {
    return ReadDouble(*this, MinimumProperty);
}

double RangeBase::Maximum() const noexcept {
    return ReadDouble(*this, MaximumProperty);
}

double RangeBase::Value() const noexcept {
    return ReadDouble(*this, ValueProperty);
}

Base::Result<void> RangeBase::SetMinimum(
    double value) noexcept {
    return SetRange(value, Maximum());
}

Base::Result<void> RangeBase::SetMaximum(
    double value) noexcept {
    return SetRange(Minimum(), value);
}

Base::Result<void> RangeBase::SetRange(
    double minimum,
    double maximum) noexcept {
    if (!std::isfinite(minimum) ||
        !std::isfinite(maximum) ||
        maximum < minimum) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "RangeBase range is invalid");
    }
    Base::Result<void> low =
        StoreDouble(*this, MinimumProperty, minimum);
    if (!low) return low.GetStatus();
    Base::Result<void> high =
        StoreDouble(*this, MaximumProperty, maximum);
    if (!high) return high.GetStatus();
    Base::Result<bool> clamped = SetValue(Value());
    return clamped
        ? Base::Result<void>{}
        : Base::Result<void>(clamped.GetStatus());
}

Base::Result<bool> RangeBase::SetValue(
    double value) noexcept {
    if (!std::isfinite(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "RangeBase value must be finite");
    }
    if (Maximum() < Minimum()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "RangeBase range is invalid");
    }
    const double clamped =
        std::clamp(value, Minimum(), Maximum());
    const double oldValue = Value();
    if (Same(clamped, oldValue)) return false;
    Base::Result<void> stored =
        StoreDouble(*this, ValueProperty, clamped);
    if (!stored) return stored.GetStatus();
    return true;
}

void RangeBase::OnRangePropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&
        args) noexcept {
    if (args.property == ValueProperty) {
        OnValueChanged(
            args.oldValue.AsDouble(),
            args.newValue.AsDouble());
    } else if (
        args.property == MinimumProperty ||
        args.property == MaximumProperty) {
        static_cast<void>(SetValue(Value()));
    }
}

void RangeBase::OnValueChanged(
    double oldValue,
    double newValue) noexcept {
    RangeValueChangedEventArgs args;
    args.oldValue = oldValue;
    args.newValue = newValue;
    const Base::Result<void> raised =
        RaiseEvent(ValueChangedEvent, &args);
    if (!raised &&
        raised.GetStatus().code !=
            Base::ErrorCode::NotInitialized) {
        static_cast<void>(raised);
    }
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

Orientation Slider::GetOrientation() const noexcept {
    return ReadOrientation(*this, OrientationProperty);
}

double Slider::SmallChange() const noexcept {
    return ReadDouble(*this, SmallChangeProperty, 1.0);
}

double Slider::LargeChange() const noexcept {
    return ReadDouble(*this, LargeChangeProperty, 10.0);
}

TickPlacement Slider::GetTickPlacement() const noexcept {
    return GetValueOr(
        TickPlacementProperty,
        TickPlacement::None);
}

double Slider::TickFrequency() const noexcept {
    return ReadDouble(*this, TickFrequencyProperty, 1.0);
}

Base::StringView Slider::Ticks() const noexcept {
    return GetValueOr(
        TicksProperty, Base::StringView());
}

bool Slider::IsSnapToTickEnabled() const noexcept {
    return ReadBool(
        *this, IsSnapToTickEnabledProperty, false);
}

bool Slider::IsDirectionReversed() const noexcept {
    return ReadBool(
        *this, IsDirectionReversedProperty, false);
}

bool Slider::IsMoveToPointEnabled() const noexcept {
    return ReadBool(
        *this, IsMoveToPointEnabledProperty, false);
}

Base::Result<void> Slider::SetOrientation(
    Orientation value) noexcept {
    return StoreOrientation(
        *this, OrientationProperty, value);
}

Base::Result<void> Slider::SetSmallChange(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Slider SmallChange must be positive and finite");
    }
    return StoreDouble(
        *this, SmallChangeProperty, value);
}

Base::Result<void> Slider::SetLargeChange(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Slider LargeChange must be positive and finite");
    }
    return StoreDouble(
        *this, LargeChangeProperty, value);
}

Base::Result<void> Slider::SetTickPlacement(
    TickPlacement value) noexcept {
    return DependencyObject::SetValue(
        TickPlacementProperty, value);
}

Base::Result<void> Slider::SetTickFrequency(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Slider TickFrequency must be positive and finite");
    }
    return StoreDouble(
        *this, TickFrequencyProperty, value);
}

Base::Result<void> Slider::SetTicks(
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
            Core::ValueConversion::ParseDouble(
                value.Substr(start, end - start));
        if (!parsed || !std::isfinite(parsed.Value())) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Slider Ticks must contain finite numeric values");
        }
        start = end;
    }
    return DependencyObject::SetValue(
        TicksProperty, value);
}

Base::Result<void> Slider::SetIsSnapToTickEnabled(
    bool value) noexcept {
    return DependencyObject::SetValue(
        IsSnapToTickEnabledProperty, value);
}

Base::Result<void> Slider::SetIsDirectionReversed(
    bool value) noexcept {
    return DependencyObject::SetValue(
        IsDirectionReversedProperty, value);
}

Base::Result<void> Slider::SetIsMoveToPointEnabled(
    bool value) noexcept {
    return DependencyObject::SetValue(
        IsMoveToPointEnabledProperty, value);
}

double Slider::SnapValue(double value) const noexcept {
    value = std::clamp(
        value, Minimum(), Maximum());
    if (!IsSnapToTickEnabled()) return value;

    const Base::StringView ticks = Ticks();
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
            Core::ValueConversion::ParseDouble(
                ticks.Substr(start, end - start));
        if (parsed &&
            parsed.Value() >= Minimum() &&
            parsed.Value() <= Maximum()) {
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

    const double frequency = TickFrequency();
    const double step = std::round(
        (value - Minimum()) / frequency);
    return std::clamp(
        Minimum() + step * frequency,
        Minimum(), Maximum());
}

Base::Result<bool> Slider::DecreaseSmall() noexcept {
    return SetValue(
        SnapValue(Value() - SmallChange()));
}

Base::Result<bool> Slider::IncreaseSmall() noexcept {
    return SetValue(
        SnapValue(Value() + SmallChange()));
}

Base::Result<bool> Slider::DecreaseLarge() noexcept {
    return SetValue(
        SnapValue(Value() - LargeChange()));
}

Base::Result<bool> Slider::IncreaseLarge() noexcept {
    return SetValue(
        SnapValue(Value() + LargeChange()));
}

Base::Result<bool> Slider::SetValueFromPosition(
    double position,
    double trackLength) noexcept {
    if (!std::isfinite(position) ||
        !std::isfinite(trackLength) ||
        trackLength <= 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Slider track geometry is invalid");
    }
    double normalized =
        std::clamp(position / trackLength, 0.0, 1.0);
    if (IsDirectionReversed()) {
        normalized = 1.0 - normalized;
    }
    return SetValue(SnapValue(
        Minimum() +
        normalized * (Maximum() - Minimum())));
}

double Slider::NormalizedValueForLayout() const noexcept {
    const double range = Maximum() - Minimum();
    double normalized = range > 0.0
        ? std::clamp(
            (Value() - Minimum()) / range,
            0.0, 1.0)
        : 0.0;
    if (IsDirectionReversed()) {
        normalized = 1.0 - normalized;
    }
    return normalized;
}

Base::Result<Size> Slider::ArrangeOverride(
    Size finalSize) noexcept {
    return Control::ArrangeOverride(finalSize);
}

Base::Result<void> Slider::OnRender(
    DrawingContext& context) noexcept {
    auto& builder = Aero::Detail::DrawingContextAccess::Builder(context);
    const TickPlacement placement =
        GetTickPlacement();
    const Size size = GetRenderSize();
    if (size.width <= 0.0 ||
        size.height <= 0.0) {
        return {};
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
    const Color color = Foreground();
    Color trackColor = color;
    trackColor.alpha *= 0.35F;
    const double normalized =
        NormalizedValueForLayout();
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
    if (!chrome) return chrome.GetStatus();
    if (placement == TickPlacement::None) {
        return {};
    }
    auto drawTick = [&](double value)
        noexcept -> Base::Result<void> {
        if (value < Minimum() ||
            value > Maximum()) {
            return {};
        }
        const double range =
            Maximum() - Minimum();
        double normalized = range > 0.0
            ? (value - Minimum()) / range
            : 0.0;
        if (IsDirectionReversed()) {
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
                if (!drawn) return drawn.GetStatus();
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
                if (!drawn) return drawn.GetStatus();
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

    const Base::StringView ticks = Ticks();
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
                Core::ValueConversion::ParseDouble(
                    ticks.Substr(start, end - start));
            if (parsed) {
                Base::Result<void> drawn =
                    drawTick(parsed.Value());
                if (!drawn) {
                    return drawn.GetStatus();
                }
            }
            start = end;
            ++count;
        }
        return {};
    }

    const double frequency = TickFrequency();
    std::uint32_t count = 0U;
    for (double value = Minimum();
         value <= Maximum() +
            frequency * 0.000001 &&
         count < 1024U;
         value += frequency, ++count) {
        Base::Result<void> drawn =
            drawTick(std::min(value, Maximum()));
        if (!drawn) return drawn.GetStatus();
    }
    return {};
}

Base::Ref<Media::Brush> TickBar::GetFill() const noexcept {
    return GetValueOr(
        FillProperty,
        Base::Ref<Media::Brush>{});
}

TickBarPlacement TickBar::Placement() const noexcept {
    return GetValueOr(
        PlacementProperty,
        TickBarPlacement::Top);
}

Base::Result<void> TickBar::SetFill(
    Base::Ref<Media::Brush> value) noexcept {
    return SetValue(FillProperty, std::move(value));
}

Base::Result<void> TickBar::SetPlacement(
    TickBarPlacement value) noexcept {
    return SetValue(PlacementProperty, value);
}

Base::Result<void> TickBar::OnRender(
    DrawingContext& context) noexcept {
    auto& builder = Aero::Detail::DrawingContextAccess::Builder(context);
    DependencyObject* parent = GetTemplatedParent();
    if (parent == nullptr ||
        !PropertyRegistry().Types().IsDerivedFrom(
            parent->RuntimeType(), Slider::StaticTypeId())) {
        return {};
    }

    const Slider& slider = static_cast<const Slider&>(*parent);
    const Size size = GetRenderSize();
    const bool horizontal =
        Placement() == TickBarPlacement::Top ||
        Placement() == TickBarPlacement::Bottom;
    const double primary = horizontal ? size.width : size.height;
    const double range = slider.Maximum() - slider.Minimum();
    if (primary <= 0.0 || range < 0.0) return {};

    const Color color = Media::SampleBrush(
        GetFill(), 0.5, Foreground());
    constexpr double thumbLength = 14.0;
    const double start = std::min(
        primary * 0.5, thumbLength * 0.5);
    const double travel = std::max(0.0, primary - thumbLength);
    auto drawTick = [&](double value)
        noexcept -> Base::Result<void> {
        if (value < slider.Minimum() ||
            value > slider.Maximum()) {
            return {};
        }
        double normalized = range > 0.0
            ? (value - slider.Minimum()) / range
            : 0.0;
        if (slider.IsDirectionReversed()) {
            normalized = 1.0 - normalized;
        }
        const double position = start +
            std::clamp(normalized, 0.0, 1.0) * travel;
        if (horizontal) {
            return builder.FillRect({
                position,
                Placement() == TickBarPlacement::Top
                    ? 0.0
                    : std::max(0.0, size.height - 4.0),
                1.0,
                std::min(4.0, size.height)}, color);
        }
        return builder.FillRect({
            Placement() == TickBarPlacement::Left
                ? 0.0
                : std::max(0.0, size.width - 4.0),
            position,
            std::min(4.0, size.width),
            1.0}, color);
    };

    const Base::StringView ticks = slider.Ticks();
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
                Core::ValueConversion::ParseDouble(
                    ticks.Substr(begin, end - begin));
            if (parsed) {
                Base::Result<void> rendered = drawTick(parsed.Value());
                if (!rendered) return rendered.GetStatus();
            }
            begin = end;
            ++count;
        }
        return {};
    }

    const double frequency = slider.TickFrequency();
    for (double value = slider.Minimum(), count = 0.0;
         value <= slider.Maximum() + frequency * 0.000001 &&
         count < 1024.0;
         value += frequency, count += 1.0) {
        Base::Result<void> rendered = drawTick(
            std::min(value, slider.Maximum()));
        if (!rendered) return rendered.GetStatus();
    }
    return {};
}

bool ProgressBar::IsIndeterminate() const noexcept {
    return ReadBool(
        *this, IsIndeterminateProperty, false);
}

Orientation ProgressBar::GetOrientation() const noexcept {
    return ReadOrientation(*this, OrientationProperty);
}

Base::Result<void> ProgressBar::SetIsIndeterminate(
    bool value) noexcept {
    return DependencyObject::SetValue(
        IsIndeterminateProperty, value);
}

Base::Result<void> ProgressBar::SetOrientation(
    Orientation value) noexcept {
    return StoreOrientation(
        *this, OrientationProperty, value);
}

double ProgressBar::NormalizedValue() const noexcept {
    const double range = Maximum() - Minimum();
    return range > 0.0
        ? std::clamp(
            (Value() - Minimum()) / range,
            0.0,
            1.0)
        : 0.0;
}

} // namespace Aero::Controls

namespace Aero::Detail {

using namespace Aero::Core;
using namespace Aero::Controls;

ScrollInteractionManager::ScrollInteractionManager(
    GuiContext& tree,
    EventRouter& events) noexcept
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
    const VisualHandle handle = Aero::Detail::VisualAccess::Handle(viewer);
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
    if (Aero::Detail::VisualAccess::Tree(viewer) != tree_ ||
        !Aero::Detail::VisualAccess::Handle(viewer).IsValid()) {
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
            {&viewer, Aero::Detail::VisualAccess::Handle(viewer)});
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
    MouseWheelEventArgs& args) noexcept {
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

SliderInteractionManager::SliderInteractionManager(
    GuiContext& tree,
    EventRouter& events,
    InputService& input) noexcept
    : tree_(&tree),
      events_(&events),
      input_(&input),
      mouseDownHandler_(
          this,
          &SliderInteractionManager::OnMouseDown),
      mouseMoveHandler_(
          this,
          &SliderInteractionManager::OnMouseMove),
      mouseUpHandler_(
          this,
          &SliderInteractionManager::OnMouseUp),
      keyDownHandler_(
          this,
          &SliderInteractionManager::OnKeyDown),
      captureChangedHandler_(
          this,
          &SliderInteractionManager::OnCaptureChanged) {}

SliderInteractionManager::~SliderInteractionManager()
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

std::uint32_t SliderInteractionManager::Find(
    const Slider& slider) const noexcept {
    for (std::uint32_t index = 0U;
         index < sliders_.Size(); ++index) {
        const VisualHandle current =
            Aero::Detail::VisualAccess::Handle(slider);
        if (sliders_[index].handle.index ==
                current.index &&
            sliders_[index].handle.generation ==
                current.generation) {
            return index;
        }
    }
    return UINT32_MAX;
}

Slider* SliderInteractionManager::Resolve(
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

void SliderInteractionManager::RemoveAt(
    std::uint32_t index) noexcept {
    if (index >= sliders_.Size()) return;
    if (index + 1U != sliders_.Size()) {
        sliders_[index] =
            sliders_.Back();
    }
    sliders_.PopBack();
}

Base::Result<void> SliderInteractionManager::Attach(
    Slider& slider) noexcept {
    if (Find(slider) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Slider is already attached");
    }
    if (Aero::Detail::VisualAccess::Tree(slider) != tree_ ||
        !Aero::Detail::VisualAccess::Handle(slider).IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Slider must be loaded in the interaction tree");
    }
    if (sliders_.Empty()) {
        Base::Result<void> capture =
            input_->TryAddPointerCaptureChanged(
                captureChangedHandler_);
        if (!capture) return capture.GetStatus();
    }
    Base::Result<void> status =
        slider.TryAddHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_);
    if (status) {
        status = slider.TryAddHandler(
            UIElement::MouseMoveEvent,
            mouseMoveHandler_);
    }
    if (status) {
        status = slider.TryAddHandler(
            UIElement::MouseUpEvent,
            mouseUpHandler_);
    }
    if (status) {
        status = slider.TryAddHandler(
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
    Base::Result<void> appended =
        sliders_.TryPushBack(
            {Aero::Detail::VisualAccess::Handle(slider), 0U, false});
    if (!appended) {
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

Base::Result<bool> SliderInteractionManager::Detach(
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
    RemoveAt(index);
    if (sliders_.Empty()) {
        static_cast<void>(
            input_->RemovePointerCaptureChanged(
                captureChangedHandler_));
    }
    return true;
}

Base::Result<void>
SliderInteractionManager::SetFromPoint(
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
    Base::Result<bool> changed =
        slider.SetValueFromPosition(
            position, length);
    return changed
        ? Base::Result<void>()
        : Base::Result<void>(
            changed.GetStatus());
}

void SliderInteractionManager::OnMouseDown(
    Base::Object* sender,
    MouseButtonEventArgs& args) noexcept {
    auto& slider =
        *static_cast<Slider*>(sender);
    const std::uint32_t index =
        Find(slider);
    if (index == UINT32_MAX ||
        !slider.GetIsEnabled() ||
        args.changedButton !=
            MouseButton::Left) {
        return;
    }
    SliderRecord& record =
        sliders_[index];
    record.pointerId = args.pointerId;
    static_cast<void>(
        input_->SetFocus(&slider));
    Point local = args.position;
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
        slider.Maximum() - slider.Minimum();
    double normalized = range > 0.0
        ? (slider.Value() - slider.Minimum()) /
            range
        : 0.0;
    if (slider.IsDirectionReversed()) {
        normalized = 1.0 - normalized;
    }
    const double thumbPosition =
        7.0 +
        std::clamp(normalized, 0.0, 1.0) *
            std::max(0.0, length - 14.0);
    record.dragging =
        slider.IsMoveToPointEnabled() ||
        std::fabs(position - thumbPosition) <=
            10.0;
    if (record.dragging) {
        static_cast<void>(
            input_->CapturePointer(
                args.pointerId, slider));
    }
    if (slider.IsMoveToPointEnabled()) {
        static_cast<void>(
            SetFromPoint(slider, args.position));
    } else if (!record.dragging) {
        const bool after =
            position >= length * 0.5;
        const bool increase =
            slider.IsDirectionReversed()
            ? !after : after;
        static_cast<void>(
            increase
            ? slider.IncreaseLarge()
            : slider.DecreaseLarge());
    }
    args.handled = true;
}

void SliderInteractionManager::OnMouseMove(
    Base::Object* sender,
    MouseEventArgs& args) noexcept {
    auto& slider =
        *static_cast<Slider*>(sender);
    const std::uint32_t index =
        Find(slider);
    if (index == UINT32_MAX ||
        !sliders_[index].dragging ||
        sliders_[index].pointerId !=
            args.pointerId) {
        return;
    }
    static_cast<void>(
        SetFromPoint(slider, args.position));
    args.handled = true;
}

void SliderInteractionManager::OnMouseUp(
    Base::Object* sender,
    MouseButtonEventArgs& args) noexcept {
    auto& slider =
        *static_cast<Slider*>(sender);
    const std::uint32_t index =
        Find(slider);
    if (index == UINT32_MAX ||
        args.changedButton !=
            MouseButton::Left ||
        !sliders_[index].dragging ||
        sliders_[index].pointerId !=
            args.pointerId) {
        return;
    }
    static_cast<void>(
        SetFromPoint(slider, args.position));
    sliders_[index].dragging = false;
    static_cast<void>(
        input_->ReleasePointer(
            args.pointerId));
    args.handled = true;
}

void SliderInteractionManager::OnKeyDown(
    Base::Object* sender,
    KeyEventArgs& args) noexcept {
    auto& slider =
        *static_cast<Slider*>(sender);
    if (Find(slider) == UINT32_MAX ||
        !slider.GetIsEnabled()) {
        return;
    }
    Base::Result<bool> changed = false;
    bool handled = true;
    const bool reversed =
        slider.IsDirectionReversed();
    if (args.key == KeyboardKeyHome) {
        changed = slider.SetValue(
            reversed
            ? slider.Maximum()
            : slider.Minimum());
    } else if (args.key == KeyboardKeyEnd) {
        changed = slider.SetValue(
            reversed
            ? slider.Minimum()
            : slider.Maximum());
    } else if (
        args.key == KeyboardKeyLeft ||
        args.key == KeyboardKeyDown) {
        changed = reversed
            ? slider.IncreaseSmall()
            : slider.DecreaseSmall();
    } else if (
        args.key == KeyboardKeyRight ||
        args.key == KeyboardKeyUp) {
        changed = reversed
            ? slider.DecreaseSmall()
            : slider.IncreaseSmall();
    } else {
        handled = false;
    }
    if (handled && changed) {
        args.handled = true;
    }
}

void SliderInteractionManager::OnCaptureChanged(
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

} // namespace Aero::Detail
