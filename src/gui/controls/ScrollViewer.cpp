#include "gui/controls/ScrollCommon.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp"
#include "gui/input/InputState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include "gui/media/MediaState.hpp"
#include <Aero/Input/Mouse.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Value.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include "ControlBehavior.hpp"
#include "gui/templates/TemplateState.hpp"

namespace Aero::Controls {
using namespace Primitives;
using namespace ::Aero::Render;

ScrollViewer::ScrollViewer() noexcept
    : ScrollContentPresenter(StaticTypeId()),
      scrollBarValueChangedHandler_(
          this,
          &ScrollViewer::OnScrollBarValueChanged) {
    UpdateComputedScrollBarVisibility(GetData());
}

ScrollViewer::~ScrollViewer() {
    DetachScrollBars();
    auto* behaviors = static_cast<ControlBehavior*>(
        AeroGuiInternal::ControlBehaviorRuntime(*this));
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
    AttachScrollBars();
    return;
}

void ScrollViewer::AttachScrollBars() noexcept {
    DetachScrollBars();
    DependencyObject* vert = GetTemplateChild(Base::StringView("PART_VerticalScrollBar"));
    if (vert != nullptr && PropertyRegistry().Types().IsDerivedFrom(vert->RuntimeType(), ScrollBar::StaticTypeId())) {
        verticalScrollBar_ = static_cast<Primitives::ScrollBar*>(vert);
        static_cast<void>(verticalScrollBar_->AddValueChangedHandler(
            ScrollBar::ValueProperty, scrollBarValueChangedHandler_));
    }
    DependencyObject* horz = GetTemplateChild(Base::StringView("PART_HorizontalScrollBar"));
    if (horz != nullptr && PropertyRegistry().Types().IsDerivedFrom(horz->RuntimeType(), ScrollBar::StaticTypeId())) {
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
    DetachScrollBars();
    contentPresenter_ = nullptr;
    ScrollContentPresenter::OnTemplateDetached();
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

    auto* events = AeroGuiInternal::EventRouterOf(*this);
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

} // namespace Aero::Controls
