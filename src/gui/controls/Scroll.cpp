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
#include "gui/media/BrushRendering.hpp"
#include <Aero/Events.hpp>

#include "gui/controls/ScrollSupport.hpp"

namespace Aero::Controls {

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
    return !ScrollSupport::Same(current.horizontalOffset, afterVertical.horizontalOffset) ||
        !ScrollSupport::Same(current.verticalOffset, afterVertical.verticalOffset);
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
        std::min(static_cast<float>(value.extentWidth), availableSize.width),
        std::min(static_cast<float>(value.extentHeight), availableSize.height)};
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
        ? std::max(value.extentWidth, static_cast<double>(finalSize.width))
        : finalSize.width;
    const double arrangeHeight = GetAllowsVerticalScroll()
        ? std::max(value.extentHeight, static_cast<double>(finalSize.height))
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
    : ContentControl(StaticTypeId()) {
    UpdateComputedScrollBarVisibility(lastData_);
}

ScrollViewer::~ScrollViewer() {
    ClearFallbackPresenter();
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

void ScrollViewer::SyncContentToPresenter() noexcept {
    if (contentPresenter_ == nullptr) return;
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
        } else if (content->GetVisualParent() != contentPresenter_) {
            ElementTree::EnsureVisualChildStorage(*contentPresenter_, *content);
        }
    } else if (content == nullptr) {
        contentPresenter_->SetContent(nullptr);
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

void ScrollViewer::EnsureFallbackPresenter() noexcept {
    if (IsTemplateApplied()) return;
    if (contentPresenter_ != nullptr) return;
    if (!ownedPresenter_) {
        ownedPresenter_ = Base::MakeRef<ScrollContentPresenter>();
        if (!ownedPresenter_) return;
    }
    contentPresenter_ = ownedPresenter_.Get();
    if (contentPresenter_->GetVisualParent() == nullptr) {
        if (ElementTree* tree = VisualTree(this)) {
            static_cast<void>(tree->AttachVisualChild(*this, *contentPresenter_));
        } else {
            AddVisualChild(contentPresenter_);
        }
    }
    SetTemplateChildCore(contentPresenter_);
    SyncContentToPresenter();
}

void ScrollViewer::ClearFallbackPresenter() noexcept {
    if (ownedPresenter_) {
        ScrollContentPresenter* const presenter = ownedPresenter_.Get();
        presenter->ClearHostedContent();
        if (presenter->GetVisualParent() == this) {
            if (ElementTree* tree = VisualTree(this)) {
                ::Aero::VisualAttachment state;
                state.visualParent = this;
                state.child = presenter;
                state.visualAttached = true;
                state.layoutAttached = presenter->GetIsLayoutAttached() && presenter->LayoutParent() != nullptr;
                state.renderAttached = false;
                if (state.IsAttached()) {
                    static_cast<void>(tree->DetachVisual(state));
                }
            } else {
                RemoveVisualChild(presenter);
            }
        }
        if (GetTemplateRoot() == presenter) {
            SetTemplateChildCore(nullptr);
        }
        if (contentPresenter_ == presenter) {
            contentPresenter_ = nullptr;
        }
        ownedPresenter_.Reset();
    }
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
    } else {
        ClearFallbackPresenter();
    }
    if (contentPresenter_ != nullptr) {
        SyncContentToPresenter();
    }
    AttachScrollBars();
    return;
}

void ScrollViewer::OnContentChanged(const Value& oldContent, const Value& newContent) {
    ContentControl::OnContentChanged(oldContent, newContent);
    SyncContentToPresenter();
}

void ScrollViewer::AttachScrollBars() noexcept {
    DetachScrollBars();
    DependencyObject* vert = GetTemplateChild(Base::StringView("PART_VerticalScrollBar"));
    if (vert != nullptr && DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(vert->RuntimeType(), ScrollBar::StaticTypeId())) {
        verticalScrollBar_ = static_cast<Primitives::ScrollBar*>(vert);
        static_cast<void>(verticalScrollBar_->AddValueChangedHandler(
            ScrollBar::ValueProperty, DependencyPropertyChangedEventHandler(this, &ScrollViewer::OnScrollBarValueChanged)));
    }
    DependencyObject* horz = GetTemplateChild(Base::StringView("PART_HorizontalScrollBar"));
    if (horz != nullptr && DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(horz->RuntimeType(), ScrollBar::StaticTypeId())) {
        horizontalScrollBar_ = static_cast<Primitives::ScrollBar*>(horz);
        static_cast<void>(horizontalScrollBar_->AddValueChangedHandler(
            ScrollBar::ValueProperty, DependencyPropertyChangedEventHandler(this, &ScrollViewer::OnScrollBarValueChanged)));
    }
}

void ScrollViewer::DetachScrollBars() noexcept {
    if (verticalScrollBar_ != nullptr) {
        static_cast<void>(verticalScrollBar_->RemoveValueChangedHandler(
            ScrollBar::ValueProperty, DependencyPropertyChangedEventHandler(this, &ScrollViewer::OnScrollBarValueChanged)));
        verticalScrollBar_ = nullptr;
    }
    if (horizontalScrollBar_ != nullptr) {
        static_cast<void>(horizontalScrollBar_->RemoveValueChangedHandler(
            ScrollBar::ValueProperty, DependencyPropertyChangedEventHandler(this, &ScrollViewer::OnScrollBarValueChanged)));
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

Size ScrollViewer::MeasureOverride(Size availableSize) noexcept {
    if (GetTemplateRoot() == nullptr && !IsTemplateApplied()) {
        EnsureFallbackPresenter();
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
    return ContentControl::ArrangeOverride(finalSize);
}

std::uint32_t ScrollViewer::GetVisualChildrenCount() const noexcept {
    return ContentControl::GetVisualChildrenCount();
}

::Aero::Media::Visual* ScrollViewer::GetVisualChild(std::uint32_t index) const noexcept {
    return ContentControl::GetVisualChild(index);
}

void ScrollViewer::OnTemplateDetached() noexcept {
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
        ScrollBar* bar,
        Base::StringView fallbackName,
        bool horizontal) noexcept {
        if (bar == nullptr) {
            DependencyObject* part = GetTemplateChild(fallbackName);
            if (part != nullptr &&
                DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                    part->RuntimeType(),
                    ScrollBar::StaticTypeId())) {
                bar = static_cast<ScrollBar*>(part);
            }
        }
        if (bar == nullptr) {
            return;
        }
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
        static_cast<void>(bar->SetRange(
            0.0,
            std::max(0.0, extent - viewport)));
        static_cast<void>(
            bar->SetViewportSize(viewport));
        static_cast<void>(bar->SetValue(offset));
        static_cast<void>(bar->SetVisibility(
            ComputeScrollBarVisibility(
                mode, extent, viewport)));
    };
    synchronizingScrollBars_ = true;
    synchronizeBar(
        horizontalScrollBar_,
        Base::StringView("PART_HorizontalScrollBar"),
        true);
    synchronizeBar(
        verticalScrollBar_,
        Base::StringView("PART_VerticalScrollBar"),
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

void ScrollViewer::OnLineUpCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    if (auto* viewer = static_cast<ScrollViewer*>(sender)) {
        static_cast<void>(viewer->LineVertical(-1.0));
        args.SetHandled(true);
    }
}

void ScrollViewer::OnLineDownCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    if (auto* viewer = static_cast<ScrollViewer*>(sender)) {
        static_cast<void>(viewer->LineVertical(1.0));
        args.SetHandled(true);
    }
}

void ScrollViewer::OnLineLeftCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    if (auto* viewer = static_cast<ScrollViewer*>(sender)) {
        static_cast<void>(viewer->LineHorizontal(-1.0));
        args.SetHandled(true);
    }
}

void ScrollViewer::OnLineRightCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    if (auto* viewer = static_cast<ScrollViewer*>(sender)) {
        static_cast<void>(viewer->LineHorizontal(1.0));
        args.SetHandled(true);
    }
}

void ScrollViewer::OnPageUpCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    if (auto* viewer = static_cast<ScrollViewer*>(sender)) {
        static_cast<void>(viewer->PageVertical(-1.0));
        args.SetHandled(true);
    }
}

void ScrollViewer::OnPageDownCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    if (auto* viewer = static_cast<ScrollViewer*>(sender)) {
        static_cast<void>(viewer->PageVertical(1.0));
        args.SetHandled(true);
    }
}

void ScrollViewer::OnPageLeftCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    if (auto* viewer = static_cast<ScrollViewer*>(sender)) {
        static_cast<void>(viewer->PageHorizontal(-1.0));
        args.SetHandled(true);
    }
}

void ScrollViewer::OnPageRightCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    if (auto* viewer = static_cast<ScrollViewer*>(sender)) {
        static_cast<void>(viewer->PageHorizontal(1.0));
        args.SetHandled(true);
    }
}

void ScrollViewer::OnScrollToTopCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    if (auto* viewer = static_cast<ScrollViewer*>(sender)) {
        viewer->SetVerticalOffset(0.0);
        args.SetHandled(true);
    }
}

void ScrollViewer::OnScrollToBottomCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    if (auto* viewer = static_cast<ScrollViewer*>(sender)) {
        viewer->SetVerticalOffset(viewer->GetScrollableHeight());
        args.SetHandled(true);
    }
}

void ScrollViewer::OnScrollToLeftEndCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    if (auto* viewer = static_cast<ScrollViewer*>(sender)) {
        viewer->SetHorizontalOffset(0.0);
        args.SetHandled(true);
    }
}

void ScrollViewer::OnScrollToRightEndCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    if (auto* viewer = static_cast<ScrollViewer*>(sender)) {
        viewer->SetHorizontalOffset(viewer->GetScrollableWidth());
        args.SetHandled(true);
    }
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

ThumbDragDelta Thumb::DragTo(
    std::uint32_t pointerId,
    Point position) noexcept {
    if (!dragging_ || pointerId != pointerId_ || !IsFinite(position)) {
        return {};
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

    using StaticCommandFn = void (*)(Base::Object*, ExecutedRoutedEventArgs&) noexcept;
    struct CommandEntry {
        Base::StringView primary;
        Base::StringView alias;
        StaticCommandFn fn;
    };
    static constexpr CommandEntry kCommands[] = {
        {"LineUpCommand", "LineUp", &ScrollViewer::OnLineUpCommand},
        {"LineDownCommand", "LineDown", &ScrollViewer::OnLineDownCommand},
        {"LineLeftCommand", "LineLeft", &ScrollViewer::OnLineLeftCommand},
        {"LineRightCommand", "LineRight", &ScrollViewer::OnLineRightCommand},
        {"PageUpCommand", "PageUp", &ScrollViewer::OnPageUpCommand},
        {"PageDownCommand", "PageDown", &ScrollViewer::OnPageDownCommand},
        {"PageLeftCommand", "PageLeft", &ScrollViewer::OnPageLeftCommand},
        {"PageRightCommand", "PageRight", &ScrollViewer::OnPageRightCommand},
        {"ScrollToTopCommand", "ScrollToTop", &ScrollViewer::OnScrollToTopCommand},
        {"ScrollToBottomCommand", "ScrollToBottom", &ScrollViewer::OnScrollToBottomCommand},
        {"ScrollToLeftEndCommand", "ScrollToLeftEnd", &ScrollViewer::OnScrollToLeftEndCommand},
        {"ScrollToRightEndCommand", "ScrollToRightEnd", &ScrollViewer::OnScrollToRightEndCommand},
    };
    auto addClassCmd = [](Base::StringView name, const ExecutedRoutedEventHandler& handler) noexcept {
        Input::RoutedCommand::RegisterStatic(ScrollBar::StaticTypeId(), name);
        if (auto cmd = Input::RoutedCommand::ResolveStatic(ScrollBar::StaticTypeId(), name)) {
            Input::CommandBinding::RegisterClassBinding(
                ScrollViewer::StaticTypeId(),
                Input::CommandBinding(std::move(cmd), handler));
        }
    };
    for (const CommandEntry& entry : kCommands) {
        const ExecutedRoutedEventHandler handler(entry.fn);
        addClassCmd(entry.primary, handler);
        addClassCmd(entry.alias, handler);
    }
}


} // namespace Aero::Controls
