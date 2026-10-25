#include <Aero/Controls.hpp>
#include <Aero/Value.hpp>
#include <algorithm>
#include <cmath>
#include "gui/core/ValueConversion.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/input/InputManager.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Input/Mouse.hpp>
#include <Aero/TryCast.hpp>
#include <limits>
#include "gui/templates/TemplateInstance.hpp"
#include "gui/core/DependencyObjectAccess.hpp"
#include "gui/core/Describe.hpp"
#include <Aero/ClassHandler.hpp>
#include <Aero/VisualStateManager.hpp>
#include "gui/controls/ContentHosting.hpp"
#include "gui/media/BrushRendering.hpp"
#include <Aero/Events.hpp>

namespace Aero::Controls {

// ---------------------------------------------------------------------------
// Internal Scroll Support Helpers (formerly ScrollCommon.hpp)
// ---------------------------------------------------------------------------

namespace ScrollSupport {

inline constexpr double LayoutInfinity = 1.0e12;

inline bool Same(double left, double right) noexcept {
    return std::fabs(left - right) <= 0.000001;
}

inline bool SameData(
    const ScrollData& left,
    const ScrollData& right) noexcept {
    return Same(left.horizontalOffset, right.horizontalOffset) &&
        Same(left.verticalOffset, right.verticalOffset) &&
        Same(left.extentWidth, right.extentWidth) &&
        Same(left.extentHeight, right.extentHeight) &&
        Same(left.viewportWidth, right.viewportWidth) &&
        Same(left.viewportHeight, right.viewportHeight);
}

inline bool ValidNonnegative(double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

inline bool ValidData(const ScrollData& value) noexcept {
    return ValidNonnegative(value.horizontalOffset) &&
        ValidNonnegative(value.verticalOffset) &&
        ValidNonnegative(value.extentWidth) &&
        ValidNonnegative(value.extentHeight) &&
        ValidNonnegative(value.viewportWidth) &&
        ValidNonnegative(value.viewportHeight);
}

inline Visibility ComputeScrollBarVisibility(
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

inline double ClampOffset(double offset, double extent, double viewport) noexcept {
    if (!std::isfinite(offset) || offset <= 0.0) {
        return 0.0;
    }
    const double maxOffset = extent > viewport ? extent - viewport : 0.0;
    return offset > maxOffset ? maxOffset : offset;
}

template <typename TProperty>
inline double ReadDouble(
    const DependencyObject& object,
    const TProperty& property,
    double fallback = 0.0) noexcept {
    (void)fallback;
    return object.GetValue(property);
}

template <typename TProperty>
inline bool ReadBool(
    const DependencyObject& object,
    const TProperty& property,
    bool fallback = false) noexcept {
    (void)fallback;
    return object.GetValue(property);
}

template <typename TProperty>
inline Orientation ReadOrientation(
    const DependencyObject& object,
    const TProperty& property) noexcept {
    return object.GetValue(property);
}

template <typename TProperty>
inline void StoreDouble(
    DependencyObject& object,
    const TProperty& property,
    double value) noexcept {
    object.SetValue(property, value);
}

template <typename TProperty>
inline void StoreOrientation(
    DependencyObject& object,
    const TProperty& property,
    Orientation value) noexcept {
    object.SetValue(property, value);
}

} // namespace ScrollSupport

// Bring helpers into Aero::Controls for unqualified use (matches former
// anonymous-namespace amalgamation).
using ScrollSupport::LayoutInfinity;
using ScrollSupport::Same;
using ScrollSupport::SameData;
using ScrollSupport::ValidNonnegative;
using ScrollSupport::ValidData;
using ScrollSupport::ComputeScrollBarVisibility;
using ScrollSupport::ClampOffset;
using ScrollSupport::ReadDouble;
using ScrollSupport::ReadBool;
using ScrollSupport::ReadOrientation;
using ScrollSupport::StoreDouble;
using ScrollSupport::StoreOrientation;

// ---------------------------------------------------------------------------
// ScrollContentPresenter Implementation
// ---------------------------------------------------------------------------

using namespace Primitives;
using namespace ::Aero::Render;

ScrollContentPresenter::ScrollContentPresenter() noexcept
    : ScrollContentPresenter(StaticTypeId()) {}

ScrollContentPresenter::ScrollContentPresenter(
    TypeId runtimeType) noexcept
    : ContentPresenter(runtimeType) {
    // WPF: ScrollContentPresenter clips; ScrollViewer does not. Only the
    // presenter type opts in. Clipping the viewer nested two stencil clips
    // over the items host and, after Intro's offscreen fade, discarded the
    // list on the window target.
    if (runtimeType == ScrollContentPresenter::StaticTypeId()) {
        static_cast<void>(SetClipToBounds(true));
    }
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
    InvalidateMeasure();
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
    InvalidateMeasure();
}

void ScrollContentPresenter::SetCanVerticallyScroll(
    bool value) noexcept {
    if (canVerticallyScroll_ == value) return;
    canVerticallyScroll_ = value;
    InvalidateMeasure();
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
    return GetValue(CanContentScrollProperty);
}

void ScrollContentPresenter::SetLineScrollAmount(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return;
    }
    lineScrollAmount_ = value;
}

bool ScrollContentPresenter::UpdateData(
    ScrollData value,
    ScrollInputKind kind,
    bool invalidateArrange) noexcept {
    if (!ValidData(value)) {
        return false;
    }
    value.horizontalOffset = GetAllowsHorizontalScroll()
        ? ClampOffset(
            value.horizontalOffset,
            value.extentWidth,
            value.viewportWidth)
        : 0.0;
    value.verticalOffset = GetAllowsVerticalScroll()
        ? ClampOffset(
            value.verticalOffset,
            value.extentHeight,
            value.viewportHeight)
        : 0.0;
    if (SameData(data_, value)) return false;
    const ScrollData oldData = data_;
    data_ = value;
    pendingInputKind_ = kind;
    if (invalidateArrange) {
        InvalidateArrange();
    }
    OnScrollDataChanged(oldData, data_, kind);
    return true;
}

bool
ScrollContentPresenter::SyncLogicalData(
    ScrollInputKind kind) noexcept {
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical == nullptr) return false;
    ScrollData value = logical->GetData();
    if (!ValidData(value)) {
        return false;
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
    value.horizontalOffset = desiredHorizontalOffset_;
    value.verticalOffset = desiredVerticalOffset_;
    (void)UpdateData(value, pendingInputKind_, true);
}

void ScrollContentPresenter::SetHorizontalOffset(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return;
    }
    desiredHorizontalOffset_ = value;
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
    desiredVerticalOffset_ = value;
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

bool
ScrollContentPresenter::ApplyScrollDelta(
    double deltaX,
    double deltaY,
    ScrollInputKind kind) noexcept {
    if (!std::isfinite(deltaX) ||
        !std::isfinite(deltaY)) {
        return false;
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

bool
ScrollContentPresenter::LineHorizontal(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return false;
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        pendingInputKind_ = ScrollInputKind::Line;
        bool changed = logical->LineHorizontal(direction);
        bool synced = SyncLogicalData(ScrollInputKind::Line);
        pendingInputKind_ = ScrollInputKind::Line;
        return changed || synced;
    }
    return ApplyScrollDelta(
        direction * lineScrollAmount_,
        0.0,
        ScrollInputKind::Line);
}

bool
ScrollContentPresenter::LineVertical(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return false;
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        pendingInputKind_ = ScrollInputKind::Line;
        bool changed = logical->LineVertical(direction);
        bool synced = SyncLogicalData(ScrollInputKind::Line);
        pendingInputKind_ = ScrollInputKind::Line;
        return changed || synced;
    }
    return ApplyScrollDelta(
        0.0,
        direction * lineScrollAmount_,
        ScrollInputKind::Line);
}

bool
ScrollContentPresenter::PageHorizontal(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return false;
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        pendingInputKind_ = ScrollInputKind::Page;
        bool changed = logical->PageHorizontal(direction);
        bool synced = SyncLogicalData(ScrollInputKind::Page);
        pendingInputKind_ = ScrollInputKind::Line;
        return changed || synced;
    }
    return ApplyScrollDelta(
        direction * GetData().viewportWidth,
        0.0,
        ScrollInputKind::Page);
}

bool
ScrollContentPresenter::PageVertical(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return false;
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        pendingInputKind_ = ScrollInputKind::Page;
        bool changed = logical->PageVertical(direction);
        bool synced = SyncLogicalData(ScrollInputKind::Page);
        pendingInputKind_ = ScrollInputKind::Line;
        return changed || synced;
    }
    return ApplyScrollDelta(
        0.0,
        direction * GetData().viewportHeight,
        ScrollInputKind::Page);
}

Size
ScrollContentPresenter::MeasureOverride(
    Size availableSize) noexcept {
    UIElement* child = GetContent();
    if (child == nullptr) {
        data_.extentWidth = 0.0;
        data_.extentHeight = 0.0;
        data_.viewportWidth = availableSize.width;
        data_.viewportHeight = availableSize.height;
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
    MeasureChild(*child, childAvailable);

    if (logical != nullptr) {
        (void)SyncLogicalData(pendingInputKind_);
    } else {
        ScrollData value = data_;
        value.extentWidth = child->GetDesiredSize().width;
        value.extentHeight = child->GetDesiredSize().height;
        value.viewportWidth = availableSize.width;
        value.viewportHeight = availableSize.height;
        value.horizontalOffset = desiredHorizontalOffset_;
        value.verticalOffset = desiredVerticalOffset_;
        (void)UpdateData(value, pendingInputKind_, false);
    }
    const ScrollData value = GetData();
    return Size{
        std::min(value.extentWidth, availableSize.width),
        std::min(value.extentHeight, availableSize.height)};
}

Size
ScrollContentPresenter::ArrangeOverride(
    Size finalSize) noexcept {
    UIElement* child = GetContent();
    if (child == nullptr) {
        SetViewport(finalSize);
        return finalSize;
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    SetViewport(finalSize);
    const ScrollData value = GetData();
    const double arrangeWidth = GetAllowsHorizontalScroll()
        ? std::max(value.extentWidth, finalSize.width)
        : finalSize.width;
    const double arrangeHeight = GetAllowsVerticalScroll()
        ? std::max(value.extentHeight, finalSize.height)
        : finalSize.height;
    const Rect slot = logical != nullptr
        ? Rect{0.0, 0.0, finalSize.width, finalSize.height}
        : Rect{
            -value.horizontalOffset,
            -value.verticalOffset,
            arrangeWidth,
            arrangeHeight};
    ArrangeChild(*child, slot);
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
        !DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            templatedParent->RuntimeType(),
            ScrollViewer::StaticTypeId())) {
        return;
    }
    static_cast<ScrollViewer*>(templatedParent)->
        AdoptPresenterData(*this, newData, kind);
}

// ---------------------------------------------------------------------------
// ScrollViewer & Thumb Implementation
// ---------------------------------------------------------------------------

using namespace Primitives;
using namespace ::Aero::Render;

ScrollViewer::ScrollViewer() noexcept
    : ContentControl(StaticTypeId()),
      lineUpHandler_(this, &ScrollViewer::OnLineUpCommand),
      lineDownHandler_(this, &ScrollViewer::OnLineDownCommand),
      lineLeftHandler_(this, &ScrollViewer::OnLineLeftCommand),
      lineRightHandler_(this, &ScrollViewer::OnLineRightCommand),
      pageUpHandler_(this, &ScrollViewer::OnPageUpCommand),
      pageDownHandler_(this, &ScrollViewer::OnPageDownCommand),
      pageLeftHandler_(this, &ScrollViewer::OnPageLeftCommand),
      pageRightHandler_(this, &ScrollViewer::OnPageRightCommand),
      scrollToTopHandler_(this, &ScrollViewer::OnScrollToTopCommand),
      scrollToBottomHandler_(this, &ScrollViewer::OnScrollToBottomCommand),
      scrollToLeftEndHandler_(this, &ScrollViewer::OnScrollToLeftEndCommand),
      scrollToRightEndHandler_(this, &ScrollViewer::OnScrollToRightEndCommand),
      commandHandles_(&Base::GetDefaultAllocator()),
      scrollBarValueChangedHandler_(
          this,
          &ScrollViewer::OnScrollBarValueChanged) {
    UpdateComputedScrollBarVisibility(lastData_);
}

ScrollViewer::~ScrollViewer() {
    UnregisterCommands();
    DetachScrollBars();
}

void ScrollViewer::OnMouseWheel(
    MouseWheelEventArgs& args) {
    if (contentPresenter_ == nullptr) return;
    const double line = contentPresenter_->GetLineScrollAmount();
    const double horizontal = -args.GetDeltaX() * line;
    const double vertical = -args.GetDeltaY() * line;
    const bool changed = contentPresenter_->ApplyScrollDelta(
        horizontal, vertical, ScrollInputKind::Wheel);
    if (changed) {
        args.SetHandled(true);
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
    return GetValue(PanningModeProperty);
}

void ScrollViewer::SetPanningMode(
    PanningMode value) noexcept {
    SetValue(PanningModeProperty, value);
}

Visibility
ScrollViewer::GetComputedHorizontalScrollBarVisibility()
    const noexcept {
    return GetValue(ComputedHorizontalScrollBarVisibilityProperty);
}

Visibility
ScrollViewer::GetComputedVerticalScrollBarVisibility()
    const noexcept {
    return GetValue(ComputedVerticalScrollBarVisibilityProperty);
}

void ScrollViewer::SetCanHorizontallyScroll(
    bool value) noexcept {
    SetValue(CanHorizontallyScrollProperty, value);
    if (contentPresenter_ != nullptr) {
        contentPresenter_->SetCanHorizontallyScroll(value);
    }
}

void ScrollViewer::SetCanVerticallyScroll(
    bool value) noexcept {
    SetValue(CanVerticallyScrollProperty, value);
    if (contentPresenter_ != nullptr) {
        contentPresenter_->SetCanVerticallyScroll(value);
    }
}

void ScrollViewer::SetCanContentScroll(
    bool value) noexcept {
    SetValue(CanContentScrollProperty, value);
}

void ScrollViewer::SetHorizontalScrollBarVisibility(
    ScrollBarVisibility value) noexcept {
    SetHorizontalScrollBarVisibility(*this, value);
    if (contentPresenter_ != nullptr) {
        contentPresenter_->SetCanHorizontallyScroll(
            value != ScrollBarVisibility::Disabled &&
            GetValue(CanHorizontallyScrollProperty));
    }
    UpdateComputedScrollBarVisibility(lastData_);
}

void ScrollViewer::SetVerticalScrollBarVisibility(
    ScrollBarVisibility value) noexcept {
    SetVerticalScrollBarVisibility(*this, value);
    if (contentPresenter_ != nullptr) {
        contentPresenter_->SetCanVerticallyScroll(
            value != ScrollBarVisibility::Disabled &&
            GetValue(CanVerticallyScrollProperty));
    }
    UpdateComputedScrollBarVisibility(lastData_);
}

void ScrollViewer::SetHorizontalOffset(
    double value) noexcept {
    if (contentPresenter_ != nullptr) {
        contentPresenter_->SetHorizontalOffset(value);
    }
}

void ScrollViewer::SetVerticalOffset(
    double value) noexcept {
    if (contentPresenter_ != nullptr) {
        contentPresenter_->SetVerticalOffset(value);
    }
}

bool ScrollViewer::LineHorizontal(
    double direction) noexcept {
    return contentPresenter_ != nullptr
        ? contentPresenter_->LineHorizontal(direction)
        : false;
}

bool ScrollViewer::LineVertical(
    double direction) noexcept {
    return contentPresenter_ != nullptr
        ? contentPresenter_->LineVertical(direction)
        : false;
}

bool ScrollViewer::PageHorizontal(
    double direction) noexcept {
    return contentPresenter_ != nullptr
        ? contentPresenter_->PageHorizontal(direction)
        : false;
}

bool ScrollViewer::PageVertical(
    double direction) noexcept {
    return contentPresenter_ != nullptr
        ? contentPresenter_->PageVertical(direction)
        : false;
}

IScrollInfo* ScrollViewer::GetContentScrollInfo() const noexcept {
    return contentPresenter_ != nullptr
        ? contentPresenter_->GetContentScrollInfo()
        : pendingScrollInfo_;
}

void ScrollViewer::SetContentScrollInfo(IScrollInfo* value) noexcept {
    pendingScrollInfo_ = value;
    if (contentPresenter_ != nullptr) {
        contentPresenter_->SetContentScrollInfo(value);
    }
}

void ScrollViewer::AdoptPresenterData(
    ScrollContentPresenter& presenter,
    const ScrollData& data,
    ScrollInputKind kind) noexcept {
    contentPresenter_ = &presenter;
    const ScrollData oldData = lastData_;
    lastData_ = data;
    OnScrollDataChanged(oldData, data, kind);
}

void ScrollViewer::EnsureFallbackPresenter() noexcept {
    if (GetTemplateRoot() != nullptr || contentPresenter_ != nullptr) return;
    if (!ownedPresenter_) {
        ownedPresenter_ = Base::MakeRef<ScrollContentPresenter>();
        if (!ownedPresenter_) return;
    }
    contentPresenter_ = ownedPresenter_.Get();
    UIElement* content = GetContentElement();
    if (content != nullptr && content != contentPresenter_) {
        if (content->GetVisualParent() == this) RemoveVisualChild(content);
        contentPresenter_->SetContent(content);
    }
    if (contentPresenter_->GetVisualParent() == nullptr) {
        if (ElementTree* tree = VisualTree(this)) {
            static_cast<void>(tree->AttachVisualChild(*this, *contentPresenter_));
        } else {
            AddVisualChild(contentPresenter_);
        }
    }
    if (content != nullptr && content->GetVisualParent() != contentPresenter_) {
        if (ElementTree* tree = VisualTree(this)) {
            static_cast<void>(tree->AttachVisualChild(*contentPresenter_, *content));
            AttachOwnedContentSubtree(*tree, *content);
        } else {
            ElementTree::EnsureVisualChildStorage(*contentPresenter_, *content);
        }
    }
    contentPresenter_->SetContentScrollInfo(pendingScrollInfo_);
    const bool canH = (GetHorizontalScrollBarVisibility() != ScrollBarVisibility::Disabled) &&
        GetValue(CanHorizontallyScrollProperty);
    const bool canV = (GetVerticalScrollBarVisibility() != ScrollBarVisibility::Disabled) &&
        GetValue(CanVerticallyScrollProperty);
    contentPresenter_->SetCanHorizontallyScroll(canH);
    contentPresenter_->SetCanVerticallyScroll(canV);
    contentPresenter_->SetCanContentScroll(GetValue(CanContentScrollProperty));
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
    if (contentPresenter_ == nullptr) {
        EnsureFallbackPresenter();
    } else if (ownedPresenter_) {
        ownedPresenter_->ClearHostedContent();
        if (ownedPresenter_->GetVisualParent() == this) {
            if (ElementTree* tree = VisualTree(this)) {
                ::Aero::VisualAttachment state;
                state.visualParent = this;
                state.child = ownedPresenter_.Get();
                state.visualAttached = true;
                state.layoutAttached = ownedPresenter_->GetIsLayoutAttached() && ownedPresenter_->LayoutParent() != nullptr;
                state.renderAttached = false;
                if (state.IsAttached()) {
                    static_cast<void>(tree->DetachVisual(state));
                }
            } else {
                RemoveVisualChild(ownedPresenter_.Get());
            }
        }
        ownedPresenter_.Reset();
    }
    if (contentPresenter_ != nullptr) {
        UIElement* content = GetContentElement();
        if (content != nullptr && content != contentPresenter_) {
            if (content->GetVisualParent() == this) {
                RemoveVisualChild(content);
            }
            if (contentPresenter_->GetContentElement() != content) {
                contentPresenter_->SetContent(content);
            }
            if (ElementTree* tree = VisualTree(this)) {
                if (content->GetVisualParent() != contentPresenter_) {
                    static_cast<void>(tree->AttachVisualChild(*contentPresenter_, *content));
                }
                AttachOwnedContentSubtree(*tree, *content);
            } else if (content->GetVisualParent() != contentPresenter_) {
                ElementTree::EnsureVisualChildStorage(*contentPresenter_, *content);
            }
        }
        contentPresenter_->SetContentScrollInfo(pendingScrollInfo_);
        const bool canH = (GetHorizontalScrollBarVisibility() != ScrollBarVisibility::Disabled) &&
            GetValue(CanHorizontallyScrollProperty);
        const bool canV = (GetVerticalScrollBarVisibility() != ScrollBarVisibility::Disabled) &&
            GetValue(CanVerticallyScrollProperty);
        contentPresenter_->SetCanHorizontallyScroll(canH);
        contentPresenter_->SetCanVerticallyScroll(canV);
        contentPresenter_->SetCanContentScroll(GetValue(CanContentScrollProperty));
    }
    AttachScrollBars();
    EnsureCommands();
    return;
}

void ScrollViewer::OnContentChanged(const Value& oldContent, const Value& newContent) {
    ContentControl::OnContentChanged(oldContent, newContent);
    if (contentPresenter_ != nullptr) {
        UIElement* content = GetContentElement();
        if (content != nullptr && content != contentPresenter_) {
            if (content->GetVisualParent() == this) {
                RemoveVisualChild(content);
            }
            if (contentPresenter_->GetContentElement() != content) {
                contentPresenter_->SetContent(content);
            }
            if (ElementTree* tree = VisualTree(this)) {
                if (content->GetVisualParent() != contentPresenter_) {
                    static_cast<void>(tree->AttachVisualChild(*contentPresenter_, *content));
                }
                AttachOwnedContentSubtree(*tree, *content);
            } else if (content->GetVisualParent() != contentPresenter_) {
                ElementTree::EnsureVisualChildStorage(*contentPresenter_, *content);
            }
        } else if (content == nullptr) {
            contentPresenter_->SetContent(nullptr);
        }
    }
}

void ScrollViewer::OnVisualParentChanged(::Aero::Media::Visual* oldParent) noexcept {
    ContentControl::OnVisualParentChanged(oldParent);
    if (GetVisualParent() != nullptr) {
        EnsureCommands();
    } else {
        UnregisterCommands();
    }
}

void ScrollViewer::AttachScrollBars() noexcept {
    DetachScrollBars();
    DependencyObject* vert = GetTemplateChild(Base::StringView("PART_VerticalScrollBar"));
    if (vert != nullptr && DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(vert->RuntimeType(), ScrollBar::StaticTypeId())) {
        verticalScrollBar_ = static_cast<Primitives::ScrollBar*>(vert);
        static_cast<void>(verticalScrollBar_->AddValueChangedHandler(
            ScrollBar::ValueProperty, scrollBarValueChangedHandler_));
    }
    DependencyObject* horz = GetTemplateChild(Base::StringView("PART_HorizontalScrollBar"));
    if (horz != nullptr && DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(horz->RuntimeType(), ScrollBar::StaticTypeId())) {
        horizontalScrollBar_ = static_cast<Primitives::ScrollBar*>(horz);
        static_cast<void>(horizontalScrollBar_->AddValueChangedHandler(
            ScrollBar::ValueProperty, scrollBarValueChangedHandler_));
    }
}

void ScrollViewer::DetachScrollBars() noexcept {
    if (verticalScrollBar_ != nullptr) {
        static_cast<void>(verticalScrollBar_->RemoveValueChangedHandler(
            ScrollBar::ValueProperty, scrollBarValueChangedHandler_));
        verticalScrollBar_ = nullptr;
    }
    if (horizontalScrollBar_ != nullptr) {
        static_cast<void>(horizontalScrollBar_->RemoveValueChangedHandler(
            ScrollBar::ValueProperty, scrollBarValueChangedHandler_));
        horizontalScrollBar_ = nullptr;
    }
}

void ScrollViewer::OnScrollBarValueChanged(
    DependencyObject& sender,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (synchronizingScrollBars_) return;
    const double val = args.GetNewValue().Kind() == Meta::ValueKind::Double
        ? args.GetNewValue().AsDouble() : 0.0;
    if (&sender == verticalScrollBar_) {
        SetVerticalOffset(val);
    } else if (&sender == horizontalScrollBar_) {
        SetHorizontalOffset(val);
    }
}

Size ScrollViewer::MeasureOverride(
    Size availableSize) noexcept {
    if (GetTemplateRoot() == nullptr) {
        EnsureFallbackPresenter();
        if (contentPresenter_ != nullptr) {
            if (ElementTree* tree = VisualTree(this)) {
                if (VisualTree(contentPresenter_) == nullptr &&
                    contentPresenter_->GetLogicalParent() == nullptr) {
                    static_cast<void>(tree->AttachElement(*this, *contentPresenter_));
                } else if (contentPresenter_->GetVisualParent() != this ||
                           !contentPresenter_->GetIsLayoutAttached()) {
                    static_cast<void>(tree->AttachVisualChild(*this, *contentPresenter_));
                }
            }
            if (contentPresenter_->GetIsLayoutAttached() &&
                contentPresenter_->LayoutParent() == this) {
                MeasureChild(*contentPresenter_, availableSize);
                AdoptPresenterData(*contentPresenter_, contentPresenter_->GetData(), ScrollInputKind::Line);
                return contentPresenter_->GetDesiredSize();
            }
            return Size{};
        }
    }
    const Size measured = ContentControl::MeasureOverride(availableSize);
    if (contentPresenter_ != nullptr) {
        AdoptPresenterData(
            *contentPresenter_,
            contentPresenter_->GetData(),
            ScrollInputKind::Line);
    }
    return measured;
}

Size ScrollViewer::ArrangeOverride(Size finalSize) noexcept {
    if (GetTemplateRoot() == nullptr && contentPresenter_ != nullptr) {
        if (contentPresenter_->GetIsLayoutAttached() &&
            contentPresenter_->LayoutParent() == this) {
            ArrangeChild(
                *contentPresenter_, {0.0, 0.0, finalSize.width, finalSize.height});
            return finalSize;
        }
    }
    return ContentControl::ArrangeOverride(finalSize);
}

std::uint32_t ScrollViewer::GetVisualChildrenCount() const noexcept {
    if (GetTemplateRoot() == nullptr && ownedPresenter_ && ownedPresenter_->GetVisualParent() == this) {
        return 1U;
    }
    return ContentControl::GetVisualChildrenCount();
}

::Aero::Media::Visual* ScrollViewer::GetVisualChild(std::uint32_t index) const noexcept {
    if (GetTemplateRoot() == nullptr && ownedPresenter_ && ownedPresenter_->GetVisualParent() == this) {
        return index == 0U ? ownedPresenter_.Get() : nullptr;
    }
    return ContentControl::GetVisualChild(index);
}

void ScrollViewer::OnTemplateDetached() noexcept {
    UnregisterCommands();
    DetachScrollBars();
    contentPresenter_ = nullptr;
    Control::OnTemplateDetached();
}

void ScrollViewer::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    // Former OnScrollViewerVisibilityChanged delegate body: re-enter through
    // the instance setters with the committed values. SetValue is then a
    // no-op, while the setters refresh the read-only computed visibility.
    const DependencyPropertyHandle prop = args.GetProperty();
    if (prop == HorizontalScrollBarVisibilityProperty.Handle() ||
        prop == VerticalScrollBarVisibilityProperty.Handle()) {
        static_cast<void>(
            SetHorizontalScrollBarVisibility(
                GetHorizontalScrollBarVisibility()));
        static_cast<void>(
            SetVerticalScrollBarVisibility(
                GetVerticalScrollBarVisibility()));
    }
    ContentControl::OnPropertyChanged(args);
}

ScrollBarVisibility
ScrollViewer::GetHorizontalScrollBarVisibility(
    const DependencyObject& element) noexcept {
    return element.GetValue(HorizontalScrollBarVisibilityProperty);
}

ScrollBarVisibility
ScrollViewer::GetVerticalScrollBarVisibility(
    const DependencyObject& element) noexcept {
    return element.GetValue(VerticalScrollBarVisibilityProperty);
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
            !DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
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
    synchronizingScrollBars_ = true;
    synchronizeBar(
        Base::StringView(
            "PART_HorizontalScrollBar"),
        true);
    synchronizeBar(
        Base::StringView(
            "PART_VerticalScrollBar"),
        false);
    synchronizingScrollBars_ = false;

    auto* events = ElementTree::EventsOf(*this);
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

void ScrollViewer::EnsureCommands() noexcept {
    if (!commandHandles_.Empty()) return;
    Aero::InputRouter* input = ElementTree::InputOf(*this);
    if (input == nullptr) return;
    auto addCmd = [this, input](Base::StringView name, const ExecutedRoutedEventHandler& handler) {
        auto cmd = Input::RoutedCommand::ResolveStatic(ScrollBar::StaticTypeId(), name);
        if (cmd) {
            auto added = input->AddCommandBinding(*this, Input::CommandBinding(std::move(cmd).Value(), handler));
            if (added) commandHandles_.PushBack(added.Value());
        }
    };
    addCmd("LineUpCommand", lineUpHandler_);
    addCmd("LineUp", lineUpHandler_);
    addCmd("LineDownCommand", lineDownHandler_);
    addCmd("LineDown", lineDownHandler_);
    addCmd("LineLeftCommand", lineLeftHandler_);
    addCmd("LineLeft", lineLeftHandler_);
    addCmd("LineRightCommand", lineRightHandler_);
    addCmd("LineRight", lineRightHandler_);
    addCmd("PageUpCommand", pageUpHandler_);
    addCmd("PageUp", pageUpHandler_);
    addCmd("PageDownCommand", pageDownHandler_);
    addCmd("PageDown", pageDownHandler_);
    addCmd("PageLeftCommand", pageLeftHandler_);
    addCmd("PageLeft", pageLeftHandler_);
    addCmd("PageRightCommand", pageRightHandler_);
    addCmd("PageRight", pageRightHandler_);
    addCmd("ScrollToTopCommand", scrollToTopHandler_);
    addCmd("ScrollToTop", scrollToTopHandler_);
    addCmd("ScrollToBottomCommand", scrollToBottomHandler_);
    addCmd("ScrollToBottom", scrollToBottomHandler_);
    addCmd("ScrollToLeftEndCommand", scrollToLeftEndHandler_);
    addCmd("ScrollToLeftEnd", scrollToLeftEndHandler_);
    addCmd("ScrollToRightEndCommand", scrollToRightEndHandler_);
    addCmd("ScrollToRightEnd", scrollToRightEndHandler_);
}

void ScrollViewer::UnregisterCommands() noexcept {
    Aero::InputRouter* input = ElementTree::InputOf(*this);
    if (input != nullptr) {
        for (Input::CommandBindingHandle handle : commandHandles_) {
            input->RemoveCommandBinding(handle);
        }
    }
    commandHandles_.Clear();
}

void ScrollViewer::OnLineUpCommand(Base::Object*, ExecutedRoutedEventArgs& args) noexcept {
    static_cast<void>(LineVertical(-1.0));
    args.SetHandled(true);
}

void ScrollViewer::OnLineDownCommand(Base::Object*, ExecutedRoutedEventArgs& args) noexcept {
    static_cast<void>(LineVertical(1.0));
    args.SetHandled(true);
}

void ScrollViewer::OnLineLeftCommand(Base::Object*, ExecutedRoutedEventArgs& args) noexcept {
    static_cast<void>(LineHorizontal(-1.0));
    args.SetHandled(true);
}

void ScrollViewer::OnLineRightCommand(Base::Object*, ExecutedRoutedEventArgs& args) noexcept {
    static_cast<void>(LineHorizontal(1.0));
    args.SetHandled(true);
}

void ScrollViewer::OnPageUpCommand(Base::Object*, ExecutedRoutedEventArgs& args) noexcept {
    static_cast<void>(PageVertical(-1.0));
    args.SetHandled(true);
}

void ScrollViewer::OnPageDownCommand(Base::Object*, ExecutedRoutedEventArgs& args) noexcept {
    static_cast<void>(PageVertical(1.0));
    args.SetHandled(true);
}

void ScrollViewer::OnPageLeftCommand(Base::Object*, ExecutedRoutedEventArgs& args) noexcept {
    static_cast<void>(PageHorizontal(-1.0));
    args.SetHandled(true);
}

void ScrollViewer::OnPageRightCommand(Base::Object*, ExecutedRoutedEventArgs& args) noexcept {
    static_cast<void>(PageHorizontal(1.0));
    args.SetHandled(true);
}

void ScrollViewer::OnScrollToTopCommand(Base::Object*, ExecutedRoutedEventArgs& args) noexcept {
    SetVerticalOffset(0.0);
    args.SetHandled(true);
}

void ScrollViewer::OnScrollToBottomCommand(Base::Object*, ExecutedRoutedEventArgs& args) noexcept {
    SetVerticalOffset(GetScrollableHeight());
    args.SetHandled(true);
}

void ScrollViewer::OnScrollToLeftEndCommand(Base::Object*, ExecutedRoutedEventArgs& args) noexcept {
    SetHorizontalOffset(0.0);
    args.SetHandled(true);
}

void ScrollViewer::OnScrollToRightEndCommand(Base::Object*, ExecutedRoutedEventArgs& args) noexcept {
    SetHorizontalOffset(GetScrollableWidth());
    args.SetHandled(true);
}

Thumb::Thumb(TypeId runtimeType) noexcept
    : Control(runtimeType) {
}

Thumb::Thumb() noexcept
    : Thumb(StaticTypeId()) {
}

Thumb::~Thumb() = default;

void Thumb::OnApplyTemplate() noexcept {
    Control::OnApplyTemplate();
    UpdateVisualState(false);
}

void Thumb::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    Control::OnPropertyChanged(args);
    if (args.GetProperty() == UIElement::IsEnabledProperty ||
        args.GetProperty() == UIElement::IsMouseOverProperty ||
        args.GetProperty() == Thumb::IsDraggingProperty) {
        UpdateVisualState(true);
    }
}

void Thumb::UpdateVisualState(bool useTransitions) noexcept {
    Base::StringView common = "Normal";
    if (!GetIsEnabled()) {
        common = "Disabled";
    } else if (GetIsDragging()) {
        common = "Pressed";
    } else if (GetIsMouseOver()) {
        common = "MouseOver";
    }
    static_cast<void>(VisualStateManager::GoToState(*this, common, useTransitions));
}

void Thumb::BeginDrag(
    std::uint32_t pointerId,
    Point position) noexcept {
    if (!IsFinite(position) || dragging_) {
        return;
    }
    SetReadOnlyCurrentValue(IsDraggingProperty, true);
    pointerId_ = pointerId;
    lastPosition_ = position;
    dragging_ = true;
    UpdateVisualState(true);
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

bool Thumb::EndDrag(
    std::uint32_t pointerId) noexcept {
    if (!dragging_) return false;
    if (pointerId != pointerId_) {
        return false;
    }
    SetReadOnlyCurrentValue(IsDraggingProperty, false);
    pointerId_ = 0U;
    dragging_ = false;
    UpdateVisualState(true);
    return true;
}

AERO_DESCRIBE(ScrollContentPresenter) {
    using namespace Aero::Meta;
    Register<ScrollContentPresenter>(context)
        .Property(ScrollContentPresenter::CanContentScrollProperty, false, AffectsMeasure)
        .Factory();
}

AERO_DESCRIBE(ScrollViewer) {
    using namespace Aero::Meta;
    Register<ScrollViewer>(context)
        .Event(ScrollViewer::ScrollChangedEvent)
        .Property(ScrollViewer::HorizontalOffsetProperty, 0.0, FrameworkPropertyMetadataOptions::None, &::Aero::Base::Validate::NonNegative<double>)
        .Property(ScrollViewer::VerticalOffsetProperty, 0.0, FrameworkPropertyMetadataOptions::None, &::Aero::Base::Validate::NonNegative<double>)
        .Property(ScrollViewer::ExtentWidthProperty, 0.0, FrameworkPropertyMetadataOptions::None, &::Aero::Base::Validate::NonNegative<double>)
        .Property(ScrollViewer::ExtentHeightProperty, 0.0, FrameworkPropertyMetadataOptions::None, &::Aero::Base::Validate::NonNegative<double>)
        .Property(ScrollViewer::ViewportWidthProperty, 0.0, FrameworkPropertyMetadataOptions::None, &::Aero::Base::Validate::NonNegative<double>)
        .Property(ScrollViewer::ViewportHeightProperty, 0.0, FrameworkPropertyMetadataOptions::None, &::Aero::Base::Validate::NonNegative<double>)
        .Property(ScrollViewer::ScrollableWidthProperty, 0.0, FrameworkPropertyMetadataOptions::None, &::Aero::Base::Validate::NonNegative<double>)
        .Property(ScrollViewer::ScrollableHeightProperty, 0.0, FrameworkPropertyMetadataOptions::None, &::Aero::Base::Validate::NonNegative<double>)
        .Property(ScrollViewer::ComputedHorizontalScrollBarVisibilityProperty, Visibility::Collapsed, AffectsMeasure | AffectsRender)
        .Property(ScrollViewer::ComputedVerticalScrollBarVisibilityProperty, Visibility::Collapsed, AffectsMeasure | AffectsRender)
        .Property(ScrollViewer::HorizontalScrollBarVisibilityProperty, ScrollBarVisibility::Disabled, AffectsMeasure | AffectsRender)
        .Property(ScrollViewer::VerticalScrollBarVisibilityProperty, ScrollBarVisibility::Visible, AffectsMeasure | AffectsRender)
        .Property(ScrollViewer::CanHorizontallyScrollProperty, true, AffectsMeasure)
        .Property(ScrollViewer::CanVerticallyScrollProperty, true, AffectsMeasure)
        .Property(ScrollViewer::CanContentScrollProperty, false, AffectsMeasure)
        .Property(ScrollViewer::PanningModeProperty, PanningMode::None)
        .TemplatePart("PART_VerticalScrollBar", TypeOf<ScrollBar>())
        .TemplatePart("PART_HorizontalScrollBar", TypeOf<ScrollBar>())
        .Factory();
    AERO_ON(ScrollViewer, &ScrollViewer::OnMouseWheel, UIElement::MouseWheelEvent);
}

// ---------------------------------------------------------------------------
// ScrollBar, Track, Slider, ProgressBar, GridSplitter Implementation
// ---------------------------------------------------------------------------

using namespace Primitives;
using namespace ::Aero::Render;


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
    return GetValue(DragIncrementProperty);
}

double GridSplitter::GetKeyboardIncrement() const noexcept {
    return GetValue(KeyboardIncrementProperty);
}

GridResizeDirection GridSplitter::GetResizeDirection() const noexcept {
    return GetValue(ResizeDirectionProperty);
}

GridResizeBehavior GridSplitter::GetResizeBehavior() const noexcept {
    return GetValue(ResizeBehaviorProperty);
}

bool GridSplitter::GetShowsPreview() const noexcept {
    return GetValue(ShowsPreviewProperty);
}

Base::Ref<Aero::Style> GridSplitter::GetPreviewStyle() const noexcept {
    return GetValue(PreviewStyleProperty);
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
    return GetValue(IsDirectionReversedProperty);
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
    if (decreaseRepeatButton_ == value) return;
    if (decreaseRepeatButton_ != nullptr) {
        if (ElementTree* tree = VisualTree(this)) {
            Aero::ElementAttachment state{};
            state.logicalParent = this;
            state.visualParent = this;
            state.child = decreaseRepeatButton_.Get();
            state.logicalAttached = true;
            state.visualAttached = true;
            state.layoutAttached = decreaseRepeatButton_->GetIsLayoutAttached();
            state.renderAttached = decreaseRepeatButton_->IsRenderAttached();
            static_cast<void>(tree->DetachElement(state));
        }
    }
    decreaseRepeatButton_ = std::move(value);
    if (decreaseRepeatButton_ != nullptr) {
        if (ElementTree* tree = VisualTree(this)) {
            static_cast<void>(tree->AttachElement(*this, *decreaseRepeatButton_));
        }
    }
    InvalidateMeasure();
}

void Track::SetThumb(
    Base::Ref<Thumb> value) noexcept {
    if (thumb_ == value) return;
    if (thumb_ != nullptr) {
        if (ElementTree* tree = VisualTree(this)) {
            Aero::ElementAttachment state{};
            state.logicalParent = this;
            state.visualParent = this;
            state.child = thumb_.Get();
            state.logicalAttached = true;
            state.visualAttached = true;
            state.layoutAttached = thumb_->GetIsLayoutAttached();
            state.renderAttached = thumb_->IsRenderAttached();
            static_cast<void>(tree->DetachElement(state));
        }
    }
    thumb_ = std::move(value);
    if (thumb_ != nullptr) {
        if (ElementTree* tree = VisualTree(this)) {
            static_cast<void>(tree->AttachElement(*this, *thumb_));
        }
    }
    InvalidateMeasure();
}

void Track::SetIncreaseRepeatButton(
    Base::Ref<RepeatButton> value) noexcept {
    if (increaseRepeatButton_ == value) return;
    if (increaseRepeatButton_ != nullptr) {
        if (ElementTree* tree = VisualTree(this)) {
            Aero::ElementAttachment state{};
            state.logicalParent = this;
            state.visualParent = this;
            state.child = increaseRepeatButton_.Get();
            state.logicalAttached = true;
            state.visualAttached = true;
            state.layoutAttached = increaseRepeatButton_->GetIsLayoutAttached();
            state.renderAttached = increaseRepeatButton_->IsRenderAttached();
            static_cast<void>(tree->DetachElement(state));
        }
    }
    increaseRepeatButton_ = std::move(value);
    if (increaseRepeatButton_ != nullptr) {
        if (ElementTree* tree = VisualTree(this)) {
            static_cast<void>(tree->AttachElement(*this, *increaseRepeatButton_));
        }
    }
    InvalidateMeasure();
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
    const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
    const bool invert =
        GetOrientation() == Orientation::Vertical
        ? !GetIsDirectionReversed()
        : (isRtl ? !GetIsDirectionReversed() : GetIsDirectionReversed());
    return invert
        ? travel - offset
        : offset;
}

double Track::ValueFromThumbOffset(
    double offset,
    double trackLength,
    double minimumThumbLength) const noexcept {
    if (!std::isfinite(offset) ||
        !ValidNonnegative(trackLength) ||
        !ValidNonnegative(minimumThumbLength)) {
        return GetMinimum();
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
    const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
    const bool invert =
        GetOrientation() == Orientation::Vertical
        ? !GetIsDirectionReversed()
        : (isRtl ? !GetIsDirectionReversed() : GetIsDirectionReversed());
    if (invert) {
        normalized = 1.0 - normalized;
    }
    return GetMinimum() + normalized * range;
}

Size Track::MeasureOverride(
    Size availableSize) noexcept {
    if (ElementTree* tree = VisualTree(this)) {
        const auto ensureAttached = [&](UIElement* child) {
            if (child == nullptr) return;
            if (VisualTree(child) == nullptr && child->GetLogicalParent() == nullptr) {
                static_cast<void>(tree->AttachElement(*this, *child));
            } else if (child->GetVisualParent() != this || !child->GetIsLayoutAttached()) {
                static_cast<void>(tree->AttachVisualChild(*this, *child));
            }
        };
        ensureAttached(decreaseRepeatButton_.Get());
        ensureAttached(thumb_.Get());
        ensureAttached(increaseRepeatButton_.Get());
    }
    Size desired{};
    if (thumb_ != nullptr) {
        if (!thumb_->IsTemplateApplied()) {
            thumb_->ApplyTemplate();
        }
        MeasureChild(*thumb_, availableSize);
        desired = thumb_->GetDesiredSize();
    }
    if (decreaseRepeatButton_ != nullptr) {
        MeasureChild(*decreaseRepeatButton_, availableSize);
        if (GetOrientation() == Orientation::Vertical) {
            desired.width = std::max(desired.width, decreaseRepeatButton_->GetDesiredSize().width);
        } else {
            desired.height = std::max(desired.height, decreaseRepeatButton_->GetDesiredSize().height);
        }
    }
    if (increaseRepeatButton_ != nullptr) {
        MeasureChild(*increaseRepeatButton_, availableSize);
        if (GetOrientation() == Orientation::Vertical) {
            desired.width = std::max(desired.width, increaseRepeatButton_->GetDesiredSize().width);
        } else {
            desired.height = std::max(desired.height, increaseRepeatButton_->GetDesiredSize().height);
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
    const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
    const bool invert = vertical
        ? !GetIsDirectionReversed()
        : (isRtl ? !GetIsDirectionReversed() : GetIsDirectionReversed());
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
        ArrangeChild(
            *child,
            vertical
                ? Rect{0.0, offset,
                       finalSize.width, extent}
                : Rect{offset, 0.0,
                       extent, finalSize.height});
        return {};
    };
    // Arrange independently: a RepeatButton that is not yet a layout
    // child must not skip the Thumb (QuestLog's 3px gold indicator).
    static_cast<void>(arrange(first, 0.0, before));
    static_cast<void>(arrange(
        thumb_.Get(), thumbOffset, thumbLength));
    static_cast<void>(arrange(
        last,
        thumbOffset + thumbLength,
        after));
    return finalSize;
}

Orientation ScrollBar::GetOrientation() const noexcept {
    return ReadOrientation(*this, OrientationProperty);
}

ScrollBar::ScrollBar() noexcept
    : RangeBase(StaticTypeId()),
      lineUpHandler_(&ScrollBar::OnLineUpCommand),
      lineDownHandler_(&ScrollBar::OnLineDownCommand),
      lineLeftHandler_(&ScrollBar::OnLineLeftCommand),
      lineRightHandler_(&ScrollBar::OnLineRightCommand),
      pageUpHandler_(&ScrollBar::OnPageUpCommand),
      pageDownHandler_(&ScrollBar::OnPageDownCommand),
      pageLeftHandler_(&ScrollBar::OnPageLeftCommand),
      pageRightHandler_(&ScrollBar::OnPageRightCommand),
      scrollToTopHandler_(&ScrollBar::OnScrollToTopCommand),
      scrollToBottomHandler_(&ScrollBar::OnScrollToBottomCommand),
      scrollToLeftEndHandler_(&ScrollBar::OnScrollToLeftEndCommand),
      scrollToRightEndHandler_(&ScrollBar::OnScrollToRightEndCommand),
      scrollToHorizontalOffsetHandler_(&ScrollBar::OnScrollToHorizontalOffsetCommand),
      scrollToVerticalOffsetHandler_(&ScrollBar::OnScrollToVerticalOffsetCommand),
      commandHandles_(&Base::GetDefaultAllocator()) {
}

ScrollBar::~ScrollBar() {
    if (dragging_) {
        static_cast<void>(ReleasePointer(pointerId_));
        dragging_ = false;
    }
    UnregisterCommands();
}

void ScrollBar::OnApplyTemplate() noexcept {
    Control::OnApplyTemplate();
    DependencyObject* part =
        GetTemplateChild("PART_Track");
    track_ =
        part != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            part->RuntimeType(),
            Track::StaticTypeId())
        ? static_cast<Track*>(part)
        : nullptr;
    SynchronizeTrack();
    EnsureCommands();
}

void ScrollBar::OnTemplateDetached() noexcept {
    track_ = nullptr;
    Control::OnTemplateDetached();
}

void ScrollBar::OnVisualParentChanged(Visual* oldParent) noexcept {
    Control::OnVisualParentChanged(oldParent);
    if (GetVisualParent() != nullptr) {
        EnsureCommands();
    } else {
        if (dragging_) {
            static_cast<void>(ReleasePointer(pointerId_));
            dragging_ = false;
        }
        UnregisterCommands();
    }
}

void ScrollBar::EnsureCommands() noexcept {
    if (!commandHandles_.Empty()) return;
    Aero::InputRouter* input = ElementTree::InputOf(*this);
    if (input == nullptr) return;
    auto addCmd = [this, input](Base::StringView name, const ExecutedRoutedEventHandler& handler) {
        auto cmd = Input::RoutedCommand::ResolveStatic(StaticTypeId(), name);
        if (cmd) {
            auto added = input->AddCommandBinding(*this, Input::CommandBinding(std::move(cmd).Value(), handler));
            if (added) commandHandles_.PushBack(added.Value());
        }
    };
    addCmd("LineUpCommand", lineUpHandler_);
    addCmd("LineUp", lineUpHandler_);
    addCmd("LineDownCommand", lineDownHandler_);
    addCmd("LineDown", lineDownHandler_);
    addCmd("LineLeftCommand", lineLeftHandler_);
    addCmd("LineLeft", lineLeftHandler_);
    addCmd("LineRightCommand", lineRightHandler_);
    addCmd("LineRight", lineRightHandler_);
    addCmd("PageUpCommand", pageUpHandler_);
    addCmd("PageUp", pageUpHandler_);
    addCmd("PageDownCommand", pageDownHandler_);
    addCmd("PageDown", pageDownHandler_);
    addCmd("PageLeftCommand", pageLeftHandler_);
    addCmd("PageLeft", pageLeftHandler_);
    addCmd("PageRightCommand", pageRightHandler_);
    addCmd("PageRight", pageRightHandler_);
    addCmd("ScrollToTopCommand", scrollToTopHandler_);
    addCmd("ScrollToTop", scrollToTopHandler_);
    addCmd("ScrollToBottomCommand", scrollToBottomHandler_);
    addCmd("ScrollToBottom", scrollToBottomHandler_);
    addCmd("ScrollToLeftEndCommand", scrollToLeftEndHandler_);
    addCmd("ScrollToLeftEnd", scrollToLeftEndHandler_);
    addCmd("ScrollToRightEndCommand", scrollToRightEndHandler_);
    addCmd("ScrollToRightEnd", scrollToRightEndHandler_);
    addCmd("ScrollToHorizontalOffsetCommand", scrollToHorizontalOffsetHandler_);
    addCmd("ScrollToHorizontalOffset", scrollToHorizontalOffsetHandler_);
    addCmd("ScrollToVerticalOffsetCommand", scrollToVerticalOffsetHandler_);
    addCmd("ScrollToVerticalOffset", scrollToVerticalOffsetHandler_);
}

void ScrollBar::UnregisterCommands() noexcept {
    Aero::InputRouter* input = ElementTree::InputOf(*this);
    if (input != nullptr) {
        for (Input::CommandBindingHandle handle : commandHandles_) {
            input->RemoveCommandBinding(handle);
        }
    }
    commandHandles_.Clear();
}


void ScrollBar::OnMouseLeftButtonDown(MouseButtonEventArgs& args) {
    if (args.GetHandled() || !GetIsEnabled() || args.GetChangedButton() != MouseButton::Left) {
        return;
    }
    const Point local = args.GetPosition();
    const bool horizontal = GetOrientation() == Orientation::Horizontal;
    const double position = horizontal ? local.x : local.y;
    const double length = horizontal ? GetRenderSize().width : GetRenderSize().height;
    const double range = GetMaximum() - GetMinimum();
    const double normalized = range > 0.0 ? (GetValue() - GetMinimum()) / range : 0.0;
    const double thumbPosition = std::clamp(normalized, 0.0, 1.0) * std::max(0.0, length - 16.0);
    const bool isThumb = std::fabs(position - thumbPosition) <= 16.0;

    if (isThumb) {
        dragging_ = true;
        pointerId_ = args.GetPointerId();
        dragOrigin_ = local;
        dragStartValue_ = GetValue();
        static_cast<void>(CapturePointer(args.GetPointerId()));
        args.SetHandled(true);
    } else if (position < thumbPosition) {
        static_cast<void>(PageDecrement());
        args.SetHandled(true);
    } else {
        static_cast<void>(PageIncrement());
        args.SetHandled(true);
    }
}

void ScrollBar::OnMouseMove(MouseEventArgs& args) {
    if (!dragging_ || pointerId_ != args.GetPointerId()) {
        return;
    }
    const Point current = args.GetPosition();
    const Point origin = dragOrigin_;
    const double range = GetMaximum() - GetMinimum();
    if (range <= 0.0) return;

    if (GetOrientation() == Orientation::Vertical) {
        const double trackHeight = GetRenderSize().height;
        const double available = std::max(1.0, trackHeight - 16.0);
        const double deltaY = current.y - origin.y;
        const double deltaVal = deltaY * range / available;
        SetValue(std::clamp(dragStartValue_ + deltaVal, GetMinimum(), GetMaximum()));
        args.SetHandled(true);
    } else {
        const double trackWidth = GetRenderSize().width;
        const double available = std::max(1.0, trackWidth - 16.0);
        const double deltaX = current.x - origin.x;
        const double deltaVal = deltaX * range / available;
        SetValue(std::clamp(dragStartValue_ + deltaVal, GetMinimum(), GetMaximum()));
        args.SetHandled(true);
    }
}

void ScrollBar::OnMouseLeftButtonUp(MouseButtonEventArgs& args) {
    if (args.GetChangedButton() != MouseButton::Left || !dragging_) {
        return;
    }
    dragging_ = false;
    static_cast<void>(ReleasePointer(pointerId_));
    args.SetHandled(true);
}

void ScrollBar::OnKeyDown(KeyEventArgs& args) {
    if (!GetIsEnabled()) return;
    if (args.GetKey() == KeyboardKeyHome) {
        SetValue(GetMinimum());
        args.SetHandled(true);
    } else if (args.GetKey() == KeyboardKeyEnd) {
        SetValue(GetMaximum());
        args.SetHandled(true);
    } else if (args.GetKey() == KeyboardKeyUp || args.GetKey() == KeyboardKeyLeft) {
        static_cast<void>(LineDecrement());
        args.SetHandled(true);
    } else if (args.GetKey() == KeyboardKeyDown || args.GetKey() == KeyboardKeyRight) {
        static_cast<void>(LineIncrement());
        args.SetHandled(true);
    } else if (args.GetKey() == KeyboardKeyPageUp) {
        static_cast<void>(PageDecrement());
        args.SetHandled(true);
    } else if (args.GetKey() == KeyboardKeyPageDown) {
        static_cast<void>(PageIncrement());
        args.SetHandled(true);
    }
}

void ScrollBar::OnLineUpCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->LineDecrement());
        args.SetHandled(true);
    }
}
void ScrollBar::OnLineDownCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->LineIncrement());
        args.SetHandled(true);
    }
}
void ScrollBar::OnLineLeftCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->LineDecrement());
        args.SetHandled(true);
    }
}
void ScrollBar::OnLineRightCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->LineIncrement());
        args.SetHandled(true);
    }
}
void ScrollBar::OnPageUpCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->PageDecrement());
        args.SetHandled(true);
    }
}
void ScrollBar::OnPageDownCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->PageIncrement());
        args.SetHandled(true);
    }
}
void ScrollBar::OnPageLeftCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->PageDecrement());
        args.SetHandled(true);
    }
}
void ScrollBar::OnPageRightCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->PageIncrement());
        args.SetHandled(true);
    }
}
void ScrollBar::OnScrollToTopCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        bar->SetValue(bar->GetMinimum());
        args.SetHandled(true);
    }
}
void ScrollBar::OnScrollToBottomCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        bar->SetValue(bar->GetMaximum());
        args.SetHandled(true);
    }
}
void ScrollBar::OnScrollToLeftEndCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        bar->SetValue(bar->GetMinimum());
        args.SetHandled(true);
    }
}
void ScrollBar::OnScrollToRightEndCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        bar->SetValue(bar->GetMaximum());
        args.SetHandled(true);
    }
}
void ScrollBar::OnScrollToHorizontalOffsetCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        const double value = args.GetParameter().Kind() == Meta::ValueKind::Double
            ? args.GetParameter().AsDouble() : 0.0;
        bar->SetValue(std::clamp(value, bar->GetMinimum(), bar->GetMaximum()));
        args.SetHandled(true);
    }
}
void ScrollBar::OnScrollToVerticalOffsetCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        const double value = args.GetParameter().Kind() == Meta::ValueKind::Double
            ? args.GetParameter().AsDouble() : 0.0;
        bar->SetValue(std::clamp(value, bar->GetMinimum(), bar->GetMaximum()));
        args.SetHandled(true);
    }
}

void ScrollBar::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    RangeBase::OnPropertyChanged(args);
    if (args.GetProperty() == OrientationProperty ||
        args.GetProperty() == MinimumProperty ||
        args.GetProperty() == MaximumProperty ||
        args.GetProperty() == ValueProperty ||
        args.GetProperty() == ViewportSizeProperty) {
        SynchronizeTrack();
    }
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
    : Control(runtimeType) {
}

RangeBase::~RangeBase() = default;

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

void RangeBase::OnMinimumChanged(
    double,
    double) noexcept {
    SetValue(GetValue());
}

void RangeBase::OnMaximumChanged(
    double,
    double) noexcept {
    SetValue(GetValue());
}

void RangeBase::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs&
        args) noexcept {
    Control::OnPropertyChanged(args);
    if (args.GetProperty() == ValueProperty) {
        OnValueChanged(
            args.GetOldValue().AsDouble(),
            args.GetNewValue().AsDouble());
    } else if (args.GetProperty() == MinimumProperty) {
        OnMinimumChanged(
            args.GetOldValue().AsDouble(),
            args.GetNewValue().AsDouble());
    } else if (args.GetProperty() == MaximumProperty) {
        OnMaximumChanged(
            args.GetOldValue().AsDouble(),
            args.GetNewValue().AsDouble());
    }
}

void RangeBase::OnValueChanged(
    double oldValue,
    double newValue) noexcept {
    RangeValueChangedEventArgs args(oldValue, newValue);
    RaiseEvent(ValueChangedEvent, &args);
}

PropertyValue RangeBase::CoerceValueCore(
    DependencyPropertyHandle property,
    const PropertyValue& baseValue) noexcept {
    if (baseValue.Kind() != Meta::ValueKind::Double) {
        return baseValue;
    }
    const double raw = baseValue.AsDouble();
    double adjusted = raw;
    if (property == MinimumProperty.Handle()) {
        adjusted = std::min(raw, GetMaximum());
    } else if (property == MaximumProperty.Handle()) {
        adjusted = std::max(raw, GetMinimum());
    } else if (property == ValueProperty.Handle()) {
        adjusted = std::clamp(raw, GetMinimum(), GetMaximum());
    } else {
        return baseValue;
    }
    if (adjusted == raw) {
        return baseValue;
    }
    return Meta::ValueCodec<double>::Encode(adjusted);
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

bool ScrollBar::LineDecrement() noexcept {
    const double oldValue = GetValue();
    SetValue(oldValue - GetSmallChange());
    return !Same(oldValue, GetValue());
}

bool ScrollBar::LineIncrement() noexcept {
    const double oldValue = GetValue();
    SetValue(oldValue + GetSmallChange());
    return !Same(oldValue, GetValue());
}

bool ScrollBar::PageDecrement() noexcept {
    const double oldValue = GetValue();
    SetValue(oldValue - GetLargeChange());
    return !Same(oldValue, GetValue());
}

bool ScrollBar::PageIncrement() noexcept {
    const double oldValue = GetValue();
    SetValue(oldValue + GetLargeChange());
    return !Same(oldValue, GetValue());
}

bool ScrollBar::DragThumb(
    double thumbOffset,
    double trackLength,
    double minimumThumbLength) noexcept {
    Track track;
    track.SetOrientation(GetOrientation());
    track.SetRange(GetMinimum(), GetMaximum());
    track.SetViewportSize(GetViewportSize());
    const double value =
        track.ValueFromThumbOffset(
            thumbOffset,
            trackLength,
            minimumThumbLength);
    const double oldValue = GetValue();
    SetValue(value);
    return !Same(oldValue, GetValue());
}

Slider::Slider() noexcept
    : Primitives::RangeBase(StaticTypeId()),
      decreaseSmallHandler_(this, &Slider::OnDecreaseSmallCommand),
      increaseSmallHandler_(this, &Slider::OnIncreaseSmallCommand),
      decreaseLargeHandler_(this, &Slider::OnDecreaseLargeCommand),
      increaseLargeHandler_(this, &Slider::OnIncreaseLargeCommand) {
}

Slider::~Slider() {
    if (dragging_) {
        static_cast<void>(ReleasePointer(pointerId_));
        dragging_ = false;
    }
    UnregisterCommands();
}

void Slider::OnApplyTemplate() noexcept {
    Control::OnApplyTemplate();
    DependencyObject* part = GetTemplateChild("PART_Track");
    track_ = part != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            part->RuntimeType(), Track::StaticTypeId())
        ? static_cast<Track*>(part)
        : nullptr;
    SynchronizeTrack();
    EnsureCommands();
}

void Slider::OnTemplateDetached() noexcept {
    track_ = nullptr;
    Control::OnTemplateDetached();
}

void Slider::OnVisualParentChanged(Visual* oldParent) noexcept {
    Control::OnVisualParentChanged(oldParent);
    if (GetVisualParent() != nullptr) {
        EnsureCommands();
    } else {
        if (dragging_) {
            static_cast<void>(ReleasePointer(pointerId_));
            dragging_ = false;
        }
        UnregisterCommands();
    }
}

void Slider::EnsureCommands() noexcept {
    if (decreaseSmallCommand_.IsValid()) return;
    Aero::InputRouter* input = ElementTree::InputOf(*this);
    if (input == nullptr) return;
    auto addCmd = [this, input](Base::StringView name, const ExecutedRoutedEventHandler& handler, Input::CommandBindingHandle& out) {
        auto cmd = Input::RoutedCommand::ResolveStatic(StaticTypeId(), name);
        if (cmd) {
            auto added = input->AddCommandBinding(*this, Input::CommandBinding(std::move(cmd).Value(), handler));
            if (added) out = added.Value();
        }
    };
    addCmd("DecreaseSmall", decreaseSmallHandler_, decreaseSmallCommand_);
    addCmd("IncreaseSmall", increaseSmallHandler_, increaseSmallCommand_);
    addCmd("DecreaseLarge", decreaseLargeHandler_, decreaseLargeCommand_);
    addCmd("IncreaseLarge", increaseLargeHandler_, increaseLargeCommand_);
}

void Slider::UnregisterCommands() noexcept {
    Aero::InputRouter* input = ElementTree::InputOf(*this);
    if (input != nullptr) {
        if (decreaseSmallCommand_.IsValid()) input->RemoveCommandBinding(decreaseSmallCommand_);
        if (increaseSmallCommand_.IsValid()) input->RemoveCommandBinding(increaseSmallCommand_);
        if (decreaseLargeCommand_.IsValid()) input->RemoveCommandBinding(decreaseLargeCommand_);
        if (increaseLargeCommand_.IsValid()) input->RemoveCommandBinding(increaseLargeCommand_);
    }
    decreaseSmallCommand_ = {};
    increaseSmallCommand_ = {};
    decreaseLargeCommand_ = {};
    increaseLargeCommand_ = {};
}


void Slider::SetFromPoint() noexcept {
    if (track_ != nullptr) {
        SetValueFromTrackPoint(Input::Mouse::GetPosition(track_));
        return;
    }
    const Point local = Input::Mouse::GetPosition(this);
    const bool horizontal = GetOrientation() == Orientation::Horizontal;
    SetValueFromPosition(
        horizontal ? local.x : local.y,
        horizontal ? GetRenderSize().width : GetRenderSize().height);
}

void Slider::OnMouseLeftButtonDown(MouseButtonEventArgs& args) {
    if (!GetIsEnabled() || args.GetChangedButton() != MouseButton::Left) {
        return;
    }
    pointerId_ = args.GetPointerId();
    static_cast<void>(Focus());
    Track* track = track_;
    Thumb* thumb = track != nullptr ? track->GetThumbElement().Get() : nullptr;
    auto* source = ::Aero::TryCast<UIElement>(args.GetOriginalSource());
    const bool onThumb = thumb != nullptr && source != nullptr && (source == thumb || thumb->IsAncestorOf(*source));
    const bool onTrack = track != nullptr && source != nullptr && (source == track || track->IsAncestorOf(*source));
    const bool onSlider = source == nullptr || source == this || IsAncestorOf(*source);
    const bool hasRepeatButtons = track != nullptr && (track->GetDecreaseRepeatButton() || track->GetIncreaseRepeatButton());

    const bool moveToPoint = GetIsMoveToPointEnabled() || ((onTrack || onSlider) && !hasRepeatButtons);
    dragging_ = onThumb || moveToPoint;
    if (!dragging_ && track != nullptr && onTrack) {
        const Point local = Input::Mouse::GetPosition(track);
        const bool horizontal = GetOrientation() == Orientation::Horizontal;
        const Size size = track->GetRenderSize();
        const double length = horizontal ? size.width : size.height;
        const double position = horizontal ? local.x : local.y;
        const double thumbOffset = track->GetThumbOffset(length);
        const double thumbLength = track->GetThumbLength(length);
        dragging_ = std::fabs(position - (thumbOffset + thumbLength * 0.5)) <= std::max(10.0, thumbLength);
    }
    if (dragging_) {
        static_cast<void>(CapturePointer(args.GetPointerId()));
    }
    if (moveToPoint || (dragging_ && onThumb)) {
        SetFromPoint();
    } else if (!dragging_) {
        const Point local = track != nullptr
            ? Input::Mouse::GetPosition(track)
            : Input::Mouse::GetPosition(this);
        const bool horizontal = GetOrientation() == Orientation::Horizontal;
        const double position = horizontal ? local.x : local.y;
        const double length = horizontal
            ? (track != nullptr ? track->GetRenderSize().width : GetRenderSize().width)
            : (track != nullptr ? track->GetRenderSize().height : GetRenderSize().height);
        const bool after = position >= length * 0.5;
        const bool increase = GetIsDirectionReversed() ? !after : after;
        static_cast<void>(increase ? IncreaseLarge() : DecreaseLarge());
    }
    args.SetHandled(true);
}

void Slider::OnMouseMove(MouseEventArgs& args) {
    if (!dragging_ || pointerId_ != args.GetPointerId()) {
        return;
    }
    SetFromPoint();
    args.SetHandled(true);
}

void Slider::OnMouseLeftButtonUp(MouseButtonEventArgs& args) {
    if (args.GetChangedButton() != MouseButton::Left || !dragging_ || pointerId_ != args.GetPointerId()) {
        return;
    }
    SetFromPoint();
    dragging_ = false;
    static_cast<void>(ReleasePointer(args.GetPointerId()));
    args.SetHandled(true);
}

void Slider::OnKeyDown(KeyEventArgs& args) {
    if (!GetIsEnabled()) return;
    bool changed = false;
    bool handled = true;
    const bool reversed = GetIsDirectionReversed();
    if (args.GetKey() == KeyboardKeyHome) {
        const double oldValue = GetValue();
        SetValue(reversed ? GetMaximum() : GetMinimum());
        changed = !Same(oldValue, GetValue());
    } else if (args.GetKey() == KeyboardKeyEnd) {
        const double oldValue = GetValue();
        SetValue(reversed ? GetMinimum() : GetMaximum());
        changed = !Same(oldValue, GetValue());
    } else if (args.GetKey() == KeyboardKeyLeft || args.GetKey() == KeyboardKeyDown) {
        changed = reversed ? IncreaseSmall() : DecreaseSmall();
    } else if (args.GetKey() == KeyboardKeyRight || args.GetKey() == KeyboardKeyUp) {
        changed = reversed ? DecreaseSmall() : IncreaseSmall();
    } else {
        handled = false;
    }
    if (handled && changed) {
        args.SetHandled(true);
    }
}

void Slider::OnDecreaseSmallCommand(Base::Object*, ExecutedRoutedEventArgs& args) noexcept {
    static_cast<void>(DecreaseSmall());
    args.SetHandled(true);
}
void Slider::OnIncreaseSmallCommand(Base::Object*, ExecutedRoutedEventArgs& args) noexcept {
    static_cast<void>(IncreaseSmall());
    args.SetHandled(true);
}
void Slider::OnDecreaseLargeCommand(Base::Object*, ExecutedRoutedEventArgs& args) noexcept {
    static_cast<void>(DecreaseLarge());
    args.SetHandled(true);
}
void Slider::OnIncreaseLargeCommand(Base::Object*, ExecutedRoutedEventArgs& args) noexcept {
    static_cast<void>(IncreaseLarge());
    args.SetHandled(true);
}

Size Slider::MeasureOverride(Size availableSize) noexcept {
    Size desired = Control::MeasureOverride(availableSize);
    // BlendTutorial ColorSelector / Position sliders omit Height. An empty
    // default template Grid measures 0x0, which collapses Grid Height="*"
    // rows inside a StackPanel. Keep a theme-like minimum.
    constexpr double kMinThickness = 18.0;
    constexpr double kMinLength = 32.0;
    if (GetOrientation() == Orientation::Horizontal) {
        desired.height = std::max(desired.height, kMinThickness);
        desired.width = std::max(desired.width, kMinLength);
    } else {
        desired.width = std::max(desired.width, kMinThickness);
        desired.height = std::max(desired.height, kMinLength);
    }
    return desired;
}

bool Slider::ValidateValueCore(
    DependencyPropertyHandle property,
    const PropertyValue& value) const noexcept {
    if (property == TicksProperty.Handle()) {
        if (value.Kind() != Meta::ValueKind::String) {
            return false;
        }
        const Base::StringView text = value.AsString();
        std::uint32_t start = 0U;
        while (start < text.SizeBytes()) {
            while (start < text.SizeBytes() &&
                (text[start] == ' ' ||
                 text[start] == '\t' ||
                 text[start] == ',' ||
                 text[start] == ';')) {
                ++start;
            }
            if (start >= text.SizeBytes()) break;
            std::uint32_t end = start;
            while (end < text.SizeBytes() &&
                text[end] != ' ' &&
                text[end] != '\t' &&
                text[end] != ',' &&
                text[end] != ';') {
                ++end;
            }
            Base::Result<double> parsed =
                ::Aero::Base::ValueConversion::ParseDouble(
                    text.Substr(start, end - start));
            if (!parsed ||
                !std::isfinite(parsed.Value())) {
                return false;
            }
            start = end;
        }
        return true;
    }
    return RangeBase::ValidateValueCore(property, value);
}

void Slider::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    RangeBase::OnPropertyChanged(args);
    if (args.GetProperty() == OrientationProperty ||
        args.GetProperty() == MinimumProperty ||
        args.GetProperty() == MaximumProperty ||
        args.GetProperty() == ValueProperty ||
        args.GetProperty() == IsDirectionReversedProperty) {
        SynchronizeTrack();
    }
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
    return GetValue(TickPlacementProperty);
}

double Slider::GetTickFrequency() const noexcept {
    return ReadDouble(*this, TickFrequencyProperty, 1.0);
}

Base::StringView Slider::GetTicks() const noexcept {
    return GetValue(TicksProperty);
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
            ::Aero::Base::ValueConversion::ParseDouble(
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
            ::Aero::Base::ValueConversion::ParseDouble(
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

bool Slider::DecreaseSmall() noexcept {
    const double oldValue = GetValue();
    SetValue(GetSnapValue(oldValue - GetSmallChange()));
    return !Same(oldValue, GetValue());
}

bool Slider::IncreaseSmall() noexcept {
    const double oldValue = GetValue();
    SetValue(GetSnapValue(oldValue + GetSmallChange()));
    return !Same(oldValue, GetValue());
}

bool Slider::DecreaseLarge() noexcept {
    const double oldValue = GetValue();
    SetValue(GetSnapValue(oldValue - GetLargeChange()));
    return !Same(oldValue, GetValue());
}

bool Slider::IncreaseLarge() noexcept {
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

void Slider::SetValueFromTrackPoint(
    Point local) noexcept {
    if (track_ == nullptr) {
        const bool horizontal =
            GetOrientation() == Orientation::Horizontal;
        SetValueFromPosition(
            horizontal ? local.x : local.y,
            horizontal
                ? GetRenderSize().width
                : GetRenderSize().height);
        return;
    }
    const bool horizontal =
        GetOrientation() == Orientation::Horizontal;
    const Size size = track_->GetRenderSize();
    const double length =
        horizontal ? size.width : size.height;
    if (!(length > 0.0)) return;
    const double desiredThumbLength = track_->GetThumbElement()
        ? std::max(0.0, horizontal
            ? track_->GetThumbElement()->GetDesiredSize().width
            : track_->GetThumbElement()->GetDesiredSize().height)
        : 8.0;
    const double thumbLength = track_->GetThumbLength(length, desiredThumbLength);
    const double position =
        (horizontal ? local.x : local.y) - thumbLength * 0.5;
    const double value =
        track_->ValueFromThumbOffset(position, length, thumbLength);
    SetValue(GetSnapValue(value));
}

double Slider::GetNormalizedValueForLayout() const noexcept {
    const double range = GetMaximum() - GetMinimum();
    double normalized = range > 0.0
        ? std::clamp(
            (GetValue() - GetMinimum()) / range,
            0.0, 1.0)
        : 0.0;
    const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
    const bool invert = GetOrientation() == Orientation::Horizontal
        ? (isRtl ? !GetIsDirectionReversed() : GetIsDirectionReversed())
        : !GetIsDirectionReversed();
    if (invert) {
        normalized = 1.0 - normalized;
    }
    return normalized;
}

Size Slider::ArrangeOverride(
    Size finalSize) noexcept {
    return Control::ArrangeOverride(finalSize);
}

void Slider::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    auto& builder = Aero::Render::DrawingBridge::Builder(context);
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
    const Color color = ::Aero::Media::SampleBrush(GetForeground());
    if ((*this).RenderChildren().Empty()) {
        Color trackColor = color;
        trackColor.alpha *= 0.35F;
        const double normalized =
            GetNormalizedValueForLayout();
        if (horizontal) {
            builder.FillRoundedRect(
                {
                    startPixel,
                    std::max(0.0,
                        (size.height - 4.0) * 0.5),
                    travel,
                    std::min(4.0, size.height)},
                trackColor,
                2.0);
            builder.FillRoundedRect(
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
        } else {
            builder.FillRoundedRect(
                {
                    std::max(0.0,
                        (size.width - 4.0) * 0.5),
                    startPixel,
                    std::min(4.0, size.width),
                    travel},
                trackColor,
                2.0);
            builder.FillRoundedRect(
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
    if (placement == TickPlacement::None) {
        return;
    }
    // When templated, TickBar parts render ticks (WPF/Noesis parity).
    // Slider::OnRender fallback would double-draw in accent Foreground
    // over TickBar's thin Background ticks.
    if (!(*this).RenderChildren().Empty()) {
        return;
    }
    auto drawTick = [&](double value)
        noexcept -> void {
        if (value < GetMinimum() ||
            value > GetMaximum()) {
            return;
        }
        const double range =
            GetMaximum() - GetMinimum();
        double normalized = range > 0.0
            ? (value - GetMinimum()) / range
            : 0.0;
        const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
        const bool invert = horizontal
            ? (isRtl ? !GetIsDirectionReversed() : GetIsDirectionReversed())
            : !GetIsDirectionReversed();
        if (invert) {
            normalized = 1.0 - normalized;
        }
        const double pixel =
            startPixel +
            std::clamp(normalized, 0.0, 1.0) *
                travel;
        if (horizontal) {
            const double center =
                size.height * 0.5;
            if (first) {
                builder.FillRect(
                    {pixel, std::max(0.0, center - 9.0),
                     1.0, std::min(5.0, size.height)},
                    color);
            }
            if (second) {
                builder.FillRect(
                    {pixel, std::min(size.height, center + 4.0),
                     1.0, std::min(5.0, size.height)},
                    color);
            }
        } else {
            const double center =
                size.width * 0.5;
            if (first) {
                builder.FillRect(
                    {std::max(0.0, center - 9.0), pixel,
                     std::min(5.0, size.width), 1.0},
                    color);
            }
            if (second) {
                builder.FillRect(
                    {std::min(size.width, center + 4.0), pixel,
                     std::min(5.0, size.width), 1.0},
                    color);
            }
        }
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
                ::Aero::Base::ValueConversion::ParseDouble(
                    ticks.Substr(start, end - start));
            if (parsed) {
                drawTick(parsed.Value());
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
        drawTick(std::min(value, GetMaximum()));
    }
    return;
}

Base::Ref<Media::Brush> TickBar::GetFill() const noexcept {
    return GetValue(FillProperty);
}

TickBarPlacement TickBar::GetPlacement() const noexcept {
    return GetValue(PlacementProperty);
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
    ::Aero::Media::DrawingContext& context) noexcept {
    auto& builder = Aero::Render::DrawingBridge::Builder(context);
    DependencyObject* parent = GetTemplatedParent();
    if (parent == nullptr ||
        !DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
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

    const Color color = ::Aero::Media::SampleBrush(
        GetFill(), 0.5, ::Aero::Media::SampleBrush(GetForeground()));
    double thumbLength = 4.0;
    if (const Primitives::Track* track = slider.GetTrack()) {
        if (const auto thumb = track->GetThumbElement()) {
            const Size thumbDesired = thumb->GetDesiredSize();
            const double desired = horizontal ? thumbDesired.width : thumbDesired.height;
            if (desired > 0.0) {
                thumbLength = track->GetThumbLength(primary, desired);
            }
        }
    }
    const double start = std::min(
        primary * 0.5, thumbLength * 0.5);
    const double travel = std::max(0.0, primary - thumbLength);

    const bool isRtl = slider.GetFlowDirection() == FlowDirection::RightToLeft;
    const bool invert = horizontal
        ? (isRtl ? !slider.GetIsDirectionReversed() : slider.GetIsDirectionReversed())
        : !slider.GetIsDirectionReversed();

    auto drawTick = [&](double value)
        noexcept -> void {
        if (value < slider.GetMinimum() ||
            value > slider.GetMaximum()) {
            return;
        }
        double normalized = range > 0.0
            ? (value - slider.GetMinimum()) / range
            : 0.0;
        if (invert) {
            normalized = 1.0 - normalized;
        }
        const double position = start +
            std::clamp(normalized, 0.0, 1.0) * travel;
        const double tickPos = std::round(position);
        if (horizontal) {
            builder.FillRect({
                tickPos,
                GetPlacement() == TickBarPlacement::Top
                    ? 0.0
                    : std::max(0.0, size.height - 4.0),
                1.0,
                std::min(4.0, size.height)}, color);
            return;
        }
        builder.FillRect({
            GetPlacement() == TickBarPlacement::Left
                ? 0.0
                : std::max(0.0, size.width - 4.0),
            tickPos,
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
                ::Aero::Base::ValueConversion::ParseDouble(
                    ticks.Substr(begin, end - begin));
            if (parsed) {
                drawTick(parsed.Value());
            }
            begin = end;
            ++count;
        }
        return;
    }

    const double frequency = slider.GetTickFrequency();
    if (frequency <= 0.0) return;
    for (double value = slider.GetMinimum(), count = 0.0;
         value <= slider.GetMaximum() + frequency * 0.000001 &&
         count < 1024.0;
         value += frequency, count += 1.0) {
        drawTick(std::min(value, slider.GetMaximum()));
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

namespace Primitives {

AERO_DESCRIBE(Track) {
    using namespace Aero::Meta;
    Register<Track>(context)
        .Property(Track::OrientationProperty, Orientation::Vertical, AffectsMeasure)
        .Property(Track::MinimumProperty, 0.0, AffectsArrange, &::Aero::Base::Validate::Finite<double>)
        .Property(Track::MaximumProperty, 1.0, AffectsArrange, &::Aero::Base::Validate::Finite<double>)
        .Property(Track::ValueProperty, 0.0, AffectsArrange | BindsTwoWayByDefault, &::Aero::Base::Validate::Finite<double>)
        .Property(Track::ViewportSizeProperty, 0.0, AffectsArrange, &::Aero::Base::Validate::NonNegative<double>)
        .Property(Track::IsDirectionReversedProperty, false, AffectsArrange)
        .Property<Base::Ref<RepeatButton>, &Track::SetDecreaseRepeatButton>("DecreaseRepeatButton", PropertyFlags::Structural)
        .Property<Base::Ref<Thumb>, &Track::SetThumb>("Thumb", PropertyFlags::Structural)
        .Property<Base::Ref<RepeatButton>, &Track::SetIncreaseRepeatButton>("IncreaseRepeatButton", PropertyFlags::Structural)
        .Factory();
}

AERO_DESCRIBE(Thumb) {
    using namespace Aero::Meta;
    Register<Thumb>(context)
        .Property(Thumb::IsDraggingProperty, false, AffectsRender)
        .Factory();
}

AERO_DESCRIBE(RangeBase) {
    using namespace Aero::Meta;
    Register<RangeValueChangedEventArgs>(context);
    Register<RangeBase>(context, TypeFlags::Abstract)
        .Event(RangeBase::ValueChangedEvent)
        .Property(RangeBase::MinimumProperty, FrameworkPropertyMetadata(0.0, AffectsArrange).Validate(&::Aero::Base::Validate::Finite<double>))
        .Property(RangeBase::MaximumProperty, FrameworkPropertyMetadata(100.0, AffectsArrange).Validate(&::Aero::Base::Validate::Finite<double>))
        .Property(RangeBase::ValueProperty, FrameworkPropertyMetadata(0.0, AffectsArrange | BindsTwoWayByDefault).Validate(&::Aero::Base::Validate::Finite<double>));
}

AERO_DESCRIBE(ScrollBar) {
    using namespace Aero::Meta;
    Register<ScrollChangedEventArgs>(context);
    Register<ScrollBar>(context)
        .Property(ScrollBar::OrientationProperty, Orientation::Vertical, AffectsMeasure)
        .Property(ScrollBar::ViewportSizeProperty, 0.0, AffectsArrange, &::Aero::Base::Validate::NonNegative<double>)
        .Property(ScrollBar::SmallChangeProperty, 16.0, FrameworkPropertyMetadataOptions::None, &::Aero::Base::Validate::Positive<double>)
        .Property(ScrollBar::LargeChangeProperty, 0.0, FrameworkPropertyMetadataOptions::None, &::Aero::Base::Validate::NonNegative<double>)
        .TemplatePart("PART_Track", TypeOf<Track>())
        .Factory();
    AERO_ON(ScrollBar, &ScrollBar::OnMouseLeftButtonDown, UIElement::MouseLeftButtonDownEvent);
    AERO_ON(ScrollBar, &ScrollBar::OnMouseLeftButtonUp, UIElement::MouseLeftButtonUpEvent);
    AERO_ON(ScrollBar, &ScrollBar::OnMouseMove, UIElement::MouseMoveEvent);
    AERO_ON(ScrollBar, &ScrollBar::OnKeyDown, UIElement::KeyDownEvent);

    for (Base::StringView commandName : {
             Base::StringView("LineUpCommand"),
             Base::StringView("LineUp"),
             Base::StringView("LineDownCommand"),
             Base::StringView("LineDown"),
             Base::StringView("LineLeftCommand"),
             Base::StringView("LineLeft"),
             Base::StringView("LineRightCommand"),
             Base::StringView("LineRight"),
             Base::StringView("PageUpCommand"),
             Base::StringView("PageUp"),
             Base::StringView("PageDownCommand"),
             Base::StringView("PageDown"),
             Base::StringView("PageLeftCommand"),
             Base::StringView("PageLeft"),
             Base::StringView("PageRightCommand"),
             Base::StringView("PageRight"),
             Base::StringView("ScrollToTopCommand"),
             Base::StringView("ScrollToTop"),
             Base::StringView("ScrollToBottomCommand"),
             Base::StringView("ScrollToBottom"),
             Base::StringView("ScrollToLeftEndCommand"),
             Base::StringView("ScrollToLeftEnd"),
             Base::StringView("ScrollToRightEndCommand"),
             Base::StringView("ScrollToRightEnd"),
             Base::StringView("ScrollToHorizontalOffsetCommand"),
             Base::StringView("ScrollToHorizontalOffset"),
             Base::StringView("ScrollToVerticalOffsetCommand"),
             Base::StringView("ScrollToVerticalOffset"),
             Base::StringView("DeferScrollToHorizontalOffsetCommand"),
             Base::StringView("DeferScrollToHorizontalOffset"),
             Base::StringView("DeferScrollToVerticalOffsetCommand"),
             Base::StringView("DeferScrollToVerticalOffset")}) {
        (void)Input::RoutedCommand::RegisterStatic(
            ScrollBar::StaticTypeId(), commandName);
    }
}

} // namespace Primitives

AERO_DESCRIBE(Slider) {
    using namespace Aero::Meta;
    for (Base::StringView commandName : {
             Base::StringView("DecreaseSmall"),
             Base::StringView("IncreaseSmall"),
             Base::StringView("DecreaseLarge"),
             Base::StringView("IncreaseLarge")}) {
        (void)Input::RoutedCommand::RegisterStatic(
            Slider::StaticTypeId(), commandName);
    }

    Register<Slider>(context)
        .Property(Slider::OrientationProperty, Orientation::Horizontal, AffectsMeasure)
        .Property(Slider::SmallChangeProperty, 1.0, FrameworkPropertyMetadataOptions::None, &Base::Validate::Positive<double>)
        .Property(Slider::LargeChangeProperty, 10.0, FrameworkPropertyMetadataOptions::None, &Base::Validate::Positive<double>)
        .Property(Slider::TickPlacementProperty, TickPlacement::None, AffectsRender)
        .Property(Slider::TickFrequencyProperty, 1.0, AffectsRender, &Base::Validate::Positive<double>)
        .Property(Slider::TicksProperty, Base::String{}, AffectsRender)
        .Property(Slider::IsSnapToTickEnabledProperty, false)
        .Property(Slider::IsDirectionReversedProperty, false, AffectsArrange | AffectsRender)
        .Property(Slider::IsMoveToPointEnabledProperty, false)
        .Override(UIElement::IsTabStopProperty, true, FrameworkPropertyMetadataOptions::None)
        .Override(Primitives::RangeBase::MaximumProperty, 10.0, FrameworkPropertyMetadataOptions::AffectsArrange, &::Aero::Base::Validate::Finite<double>)
        .Factory();
    AERO_ON(Slider, &Slider::OnMouseLeftButtonDown, UIElement::MouseLeftButtonDownEvent);
    AERO_ON(Slider, &Slider::OnMouseLeftButtonUp, UIElement::MouseLeftButtonUpEvent);
    AERO_ON(Slider, &Slider::OnMouseMove, UIElement::MouseMoveEvent);
    AERO_ON(Slider, &Slider::OnKeyDown, UIElement::KeyDownEvent);
}

AERO_DESCRIBE(TickBar) {
    using namespace Aero::Meta;
    Register<TickBar>(context)
        .Property(TickBar::FillProperty, Base::Ref<Media::Brush>{}, AffectsRender)
        .Property(TickBar::PlacementProperty, TickBarPlacement::Top, AffectsRender)
        .Factory();
}

AERO_DESCRIBE(ProgressBar) {
    using namespace Aero::Meta;
    Register<ProgressBar>(context)
        .Property(ProgressBar::IsIndeterminateProperty, false, AffectsRender)
        .Property(ProgressBar::OrientationProperty, Orientation::Horizontal, AffectsMeasure)
        .Factory();
}

AERO_DESCRIBE(GridSplitter) {
    using namespace Aero::Meta;
    Register<GridSplitter>(context)
        .Property(GridSplitter::DragIncrementProperty, 1.0, FrameworkPropertyMetadataOptions::None, &::Aero::Base::Validate::Positive<double>)
        .Property(GridSplitter::KeyboardIncrementProperty, 10.0, FrameworkPropertyMetadataOptions::None, &::Aero::Base::Validate::Positive<double>)
        .Property(GridSplitter::ResizeDirectionProperty, GridResizeDirection::Auto)
        .Property(GridSplitter::ResizeBehaviorProperty, GridResizeBehavior::BasedOnAlignment)
        .Property(GridSplitter::ShowsPreviewProperty, false)
        .Property(GridSplitter::PreviewStyleProperty, Base::Ref<Aero::Style>{})
        .Factory();
}

} // namespace Aero::Controls
