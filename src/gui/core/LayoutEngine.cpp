#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/input/InputManager.hpp"
#include <Aero/Layout.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>

#include <Aero/Base/Assert.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Controls/Grid.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Media;
namespace {

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

bool SameSize(Size left, Size right) noexcept {
    return left.width == right.width && left.height == right.height;
}

double ClampDimension(double value, double minimum, double maximum) noexcept {
    return std::max(minimum, std::min(value, maximum));
}

Size ClampSize(Size value, Size minimum, Size maximum) noexcept {
    return {ClampDimension(value.width, minimum.width, maximum.width),
        ClampDimension(value.height, minimum.height, maximum.height)};
}

double AlignmentOffset(double available, double actual, bool center, bool end) noexcept {
    const double remaining = std::max(0.0, available - actual);
    return center ? remaining * 0.5 : (end ? remaining : 0.0);
}

Size NaturalConstraintForTransform(
    Size transformed,
    const Base::Transform2D& matrix) noexcept {
    Base::ProjectiveTransform2D inverse;
    if (!Base::Invert(Base::ToProjective(matrix), inverse)) {
        return transformed;
    }
    const Rect bounds = Base::TransformBounds(
        inverse,
        {0.0, 0.0,
         transformed.width,
         transformed.height});
    return {
        std::max(0.0, bounds.width),
        std::max(0.0, bounds.height)};
}

} // namespace

bool IsFinite(Point value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool IsFinite(Size value) noexcept {
    return std::isfinite(value.width) && std::isfinite(value.height);
}

bool IsFinite(Rect value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.width) && std::isfinite(value.height);
}

bool IsFinite(Thickness value) noexcept {
    return std::isfinite(value.left) && std::isfinite(value.top) &&
        std::isfinite(value.right) && std::isfinite(value.bottom);
}

bool IsValidLayoutSize(Size value) noexcept {
    return IsFinite(value) && value.width >= 0.0 && value.height >= 0.0;
}

UIElement* FindInvalidVisibleLayout(::Aero::Media::Visual& visual) noexcept {
    UIElement* element = ::Aero::TryCast<::Aero::UIElement>(&(visual));
    if (element != nullptr &&
        element->GetIsVisible() &&
        (!element->GetIsMeasureValid() ||
         !element->GetIsArrangeValid())) {
        return element;
    }
    const std::uint32_t childCount =
        ::Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
    for (std::uint32_t index = 0U; index < childCount; ++index) {
        ::Aero::Media::Visual* child = ::Aero::Media::VisualTreeHelper::GetChild(visual, index);
        if (child != nullptr) {
            UIElement* invalid = FindInvalidVisibleLayout(*child);
            if (invalid != nullptr) return invalid;
        }
    }
    return nullptr;
}

bool HasInvalidVisibleLayout(::Aero::Media::Visual& visual) noexcept {
    return FindInvalidVisibleLayout(visual) != nullptr;
}

bool IsValidLayoutRect(Rect value) noexcept {
    return IsFinite(value) && value.width >= 0.0 && value.height >= 0.0;
}

Size Deflate(Size value, Thickness padding) noexcept {
    const double horizontal = padding.left + padding.right;
    const double vertical = padding.top + padding.bottom;
    return {std::max(0.0, value.width - horizontal),
        std::max(0.0, value.height - vertical)};
}

Size Inflate(Size value, Thickness padding) noexcept {
    return {value.width + padding.left + padding.right,
        value.height + padding.top + padding.bottom};
}

Rect Intersect(Rect left, Rect right) noexcept {
    const double x = std::max(left.x, right.x);
    const double y = std::max(left.y, right.y);
    const double rightEdge = std::min(left.x + left.width, right.x + right.width);
    const double bottomEdge = std::min(left.y + left.height, right.y + right.height);
    return {x, y, std::max(0.0, rightEdge - x),
        std::max(0.0, bottomEdge - y)};
}

double RoundLayoutValue(double value, double dpiScale) noexcept {
    if (!std::isfinite(value) || !std::isfinite(dpiScale) || dpiScale <= 0.0) {
        return value;
    }
    return std::round(value * dpiScale) / dpiScale;
}

LayoutEngine::LayoutEngine(Dispatcher& dispatcher) noexcept
    : dispatcher_(&dispatcher) {}

LayoutEngine::~LayoutEngine() {
    // P3.2: no frame-hook registration; nothing to unregister.
}

Base::Result<void> LayoutEngine::Initialize() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (initialized_) return {};
    // P3.2: ViewFrame drives LayoutHook() directly; no Layout hook.
    initialized_ = true;
    return {};
}

Base::Result<void> LayoutEngine::VerifyElement(
    const UIElement& element) const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (&element.GetDispatcher() != dispatcher_) {
        return Base::Status::Failure(
            Base::ErrorCode::WrongThread,
            "Layout element belongs to another Dispatcher");
    }
    if (ElementTree::LayoutOf(element) != nullptr && ElementTree::LayoutOf(element) != this) {
        return InvalidState("Layout element belongs to another LayoutEngine");
    }
    return {};
}

Base::Result<void> LayoutEngine::Attach(
    UIElement& parent,
    UIElement& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) return verified.GetStatus();
    verified = VerifyElement(child);
    if (!verified) return verified.GetStatus();
    if (&parent == &child) {
        return InvalidState(
            "Layout child is already attached or self-referential");
    }
    if (child.GetIsLayoutAttached()) {
        if (child.LayoutParent() == &parent) {
            return {};
        }
        // LoadComponent / UserControl content can reparent a Grid that was
        // first joined under the Window (or left layoutAttached with a stale
        // parent). Fail-closed blocked ColorSelector from hosting LayoutRoot.
        UIElement* oldParent = child.LayoutParent();
        (child).Layout().layoutAttached = false;
        (child).Layout().measureValid = false;
        (child).Layout().arrangeValid = false;
        if (oldParent != nullptr && oldParent != &parent) {
            InvalidateMeasure(*oldParent);
        }
    }
    // A stale LayoutParent on a detached child is irrelevant and must not block
    // re-attachment; the GetIsLayoutAttached() check above already catches
    // genuine double-attach conflicts.

    // Queue all parent invalidation work before publishing the child state.
    InvalidateMeasure(parent);

    (child).Layout().layoutAttached = true;
    (child).Layout().measureValid = false;
    (child).Layout().arrangeValid = false;
    return {};
}

Base::Result<void> LayoutEngine::Detach(
    UIElement& parent,
    UIElement& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) return verified.GetStatus();
    if (!child.GetIsLayoutAttached() || ElementTree::LayoutOf(child) != this) {
        // Already detached (idempotent). Template substitution can leave the
        // element-tree edge naming a logical parent that differs from the
        // layout parent, so a missing relationship is benign here.
        return {};
    }
    // The element-tree edge may name a logical parent that is not the layout
    // (visual) parent. Detach from wherever the child is actually attached.
    UIElement* attachParent = child.LayoutParent();
    if (attachParent == nullptr) attachParent = &parent;

    InvalidateMeasure(*attachParent);

    RemoveQueued(child);
    (child).Layout().layoutAttached = false;
    (child).Layout().measureValid = false;
    (child).Layout().arrangeValid = false;
    return {};
}

Base::Result<void> LayoutEngine::SetRoot(
    UIElement* root,
    Size availableSize) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!IsValidLayoutSize(availableSize)) {
        return InvalidArgument(
            "Root layout size must be finite and nonnegative");
    }
    if (root != nullptr) {
        Base::Result<void> verified = VerifyElement(*root);
        if (!verified) return verified.GetStatus();
        if (root->GetIsLayoutAttached() || root->GetVisualParent() != nullptr) {
            return InvalidState(
                "Layout root cannot have a visual or layout parent");
        }
    }

    // Drop every queued handle without resolving it. The previous tree may
    // already be unmounted, so walking those VisualHandles would TryCast
    // dangling nodes (Scoreboard-after-QuestLog sample host SIGSEGV).
    measureQueue_.Clear();
    arrangeQueue_.Clear();
    if (root_ != nullptr && root_ != root) {
        (*root_).Layout().measureQueued = false;
        (*root_).Layout().arrangeQueued = false;
    }
    root_ = root;
    rootAvailableSize_ = availableSize;
    if (root != nullptr) {
        InvalidateMeasure(*root);
    }
    return {};
}

UIElement* LayoutEngine::ResolveQueued(VisualHandle handle) const noexcept {
    if (!handle.IsValid() || root_ == nullptr) return nullptr;
    ElementTree* tree = ElementTree::Of(*root_);
    if (tree == nullptr) return nullptr;
    ::Aero::Media::Visual* visual = tree->ResolveHandle(handle);
    return visual != nullptr ? ::Aero::TryCast<UIElement>(visual) : nullptr;
}

Base::Result<VisualHandle> LayoutEngine::EnqueueHandle(
    UIElement& element) noexcept {
    const VisualHandle handle = ElementTree::HandleOf(element);
    if (!handle.IsValid()) {
        std::fprintf(stderr, "DEBUG_ENQUEUE_FAIL: elem=%p type=%llu tree=%p vparent=%p lparent=%p layoutParent=%p\n",
            static_cast<void*>(&element),
            static_cast<unsigned long long>(element.RuntimeType()),
            static_cast<void*>(ElementTree::Of(element)),
            static_cast<void*>(element.GetVisualParent()),
            static_cast<void*>(element.GetLogicalParent()),
            static_cast<void*>(element.LayoutParent()));
        return InvalidState("Layout element has no ElementTree handle");
    }
    return handle;
}

Base::Result<void> LayoutEngine::QueueMeasure(
    UIElement& element) noexcept {
    if (element.GetIsMeasureQueued()) return {};
    const VisualHandle handle = ElementTree::HandleOf(element);
    if (!handle.IsValid()) {
        (element).Layout().measureValid = false;
        return {};
    }
    measureQueue_.PushBack(handle);
    (element).Layout().measureQueued = true;
    return {};
}

Base::Result<void> LayoutEngine::QueueArrange(
    UIElement& element) noexcept {
    if (element.GetIsArrangeQueued()) return {};
    const VisualHandle handle = ElementTree::HandleOf(element);
    if (!handle.IsValid()) {
        (element).Layout().arrangeValid = false;
        return {};
    }
    arrangeQueue_.PushBack(handle);
    (element).Layout().arrangeQueued = true;
    return {};
}

void LayoutEngine::RemoveQueued(UIElement& element) noexcept {
    const VisualHandle handle = ElementTree::HandleOf(element);
    auto remove = [&](Base::Vector<VisualHandle>& queue) noexcept {
        for (std::uint32_t index = 0U; index < queue.Size();) {
            if (queue[index] != handle) {
                ++index;
                continue;
            }
            for (std::uint32_t next = index + 1U;
                 next < queue.Size(); ++next) {
                queue[next - 1U] = queue[next];
            }
            queue.PopBack();
        }
    };
    remove(measureQueue_);
    remove(arrangeQueue_);
    (element).Layout().measureQueued = false;
    (element).Layout().arrangeQueued = false;
}

void LayoutEngine::InvalidateMeasure(
    UIElement& element) noexcept {
    Base::Vector<UIElement*> path;
    UIElement* current = &element;
    while (current != nullptr) {
        Base::Result<void> verified = VerifyElement(*current);
        if (!verified) { AERO_ASSERT(false); return; }
        path.PushBack(current);
        current = current->GetIsLayoutAttached()
            ? current->LayoutParent() : nullptr;
    }

    Base::Vector<VisualHandle> handles;
    handles.Reserve(path.Size());
    for (UIElement* item : path) {
        if (item->GetIsMeasureQueued()) continue;
        const VisualHandle handle = ElementTree::HandleOf(*item);
        if (handle.IsValid()) {
            handles.PushBack(handle);
        }
    }
    measureQueue_.Reserve(
        measureQueue_.Size() + handles.Size());

    std::uint32_t handleIndex = 0U;
    for (UIElement* item : path) {
        (*item).Layout().measureValid = false;
        (*item).Layout().arrangeValid = false;
        if (item->GetIsMeasureQueued()) continue;
        const VisualHandle handle = ElementTree::HandleOf(*item);
        if (handle.IsValid()) {
            measureQueue_.PushBack(
                handles[handleIndex++]);
            (*item).Layout().measureQueued = true;
        }
    }
}

void LayoutEngine::InvalidateArrange(
    UIElement& element) noexcept {
    Base::Vector<UIElement*> path;
    UIElement* current = &element;
    while (current != nullptr) {
        Base::Result<void> verified = VerifyElement(*current);
        if (!verified) { AERO_ASSERT(false); return; }
        path.PushBack(current);
        current = current->GetIsLayoutAttached()
            ? current->LayoutParent() : nullptr;
    }

    Base::Vector<VisualHandle> handles;
    handles.Reserve(path.Size());
    for (UIElement* item : path) {
        if (item->GetIsArrangeQueued()) continue;
        const VisualHandle handle = ElementTree::HandleOf(*item);
        if (handle.IsValid()) {
            handles.PushBack(handle);
        }
    }
    arrangeQueue_.Reserve(
        arrangeQueue_.Size() + handles.Size());

    std::uint32_t handleIndex = 0U;
    for (UIElement* item : path) {
        (*item).Layout().arrangeValid = false;
        if (item->GetIsArrangeQueued()) continue;
        const VisualHandle handle = ElementTree::HandleOf(*item);
        if (handle.IsValid()) {
            arrangeQueue_.PushBack(
                handles[handleIndex++]);
            (*item).Layout().arrangeQueued = true;
        }
    }
}

void UIElement::MeasureCore(
    LayoutEngine& layout,
    Size constraint) noexcept {
    UIElement& element = *this;
    if (!IsValidLayoutSize(constraint)) {
        AERO_ASSERT(false && "Measure constraint must be finite and nonnegative");
        return;
    }
    if (element.GetIsMeasuring() || element.GetIsArranging()) {
        AERO_ASSERT(false && "Recursive layout operation is not allowed");
        return;
    }
    if (element.GetIsMeasureValid() && SameSize(element.GetPreviousMeasureConstraint(), constraint)) {
        return;
    }

    VisualHandle pendingArrange{};
    const bool queueArrange = !element.GetIsArrangeQueued();
    if (queueArrange) {
        Base::Result<VisualHandle> handle = layout.EnqueueHandle(element);
        if (!handle) { AERO_ASSERT(false); return; }
        pendingArrange = handle.Value();
        layout.arrangeQueue_.Reserve(
            layout.arrangeQueue_.Size() + 1U);
    }

    if (element.GetVisibility() == Visibility::Collapsed) {
        (element).Layout().previousMeasureConstraint = constraint;
        (element).Layout().desiredSize = {};
        (element).Layout().untransformedDesiredSize = {};
        (element).Layout().measureValid = true;
        (element).Layout().arrangeValid = false;
        (element).Layout().measureQueued = false;
        ++(element).Layout().layoutRevision;
        ++layout.measuredCount_;
        if (queueArrange) {
            layout.arrangeQueue_.PushBack(
                pendingArrange);
            (element).Layout().arrangeQueued = true;
        }
        return;
    }

    const FrameworkElement* framework = ::Aero::TryCast<::Aero::FrameworkElement>(&(element));
    const Thickness margin = framework != nullptr
        ? framework->GetMargin() : Thickness{};
    const Size minimum = framework != nullptr
        ? framework->GetMinSize() : Size{};
    const Size maximum = framework != nullptr
        ? framework->GetMaxSize() : Size{1.0e12, 1.0e12};
    // Window.Width/Height size the native chrome. The layout root must fill
    // the view client (DPI-converted, user-resized) so Viewbox Uniform can
    // scale. Nested elements still honor explicit Width/Height.
    const bool isLayoutRoot = layout.root_ != nullptr && &element == layout.root_;
    const bool hasWidth =
        !isLayoutRoot && framework != nullptr && framework->GetHasWidth();
    const bool hasHeight =
        !isLayoutRoot && framework != nullptr && framework->GetHasHeight();
    Size available = Deflate(constraint, margin);
    Base::Ref<Transform> layoutTransform =
        framework != nullptr
        ? framework->GetLayoutTransform()
        : Base::Ref<Transform>{};
    Base::Transform2D layoutMatrix;
    if (layoutTransform) {
        layoutMatrix = layoutTransform->GetMatrix();
        if (!Base::IsFiniteTransform(layoutMatrix)) {
            AERO_ASSERT(false && "LayoutTransform produced an invalid matrix");
            return;
        }
        available =
            NaturalConstraintForTransform(
                available,
                layoutMatrix);
    }
    available = ClampSize(available, minimum, maximum);
    if (hasWidth) {
        available.width = ClampDimension(
            framework->GetWidth(), minimum.width, maximum.width);
    }
    if (hasHeight) {
        available.height = ClampDimension(
            framework->GetHeight(), minimum.height, maximum.height);
    }

    (element).Layout().measuring = true;
    const Size result = (element).MeasureOverride( available);
    (element).Layout().measuring = false;
    Size desired = result;
    if (!IsValidLayoutSize(desired)) {
        AERO_ASSERT(false && "MeasureOverride returned an invalid size");
        return;
    }
    desired = ClampSize(desired, minimum, maximum);
    if (hasWidth) desired.width = available.width;
    if (hasHeight) desired.height = available.height;
    (element).Layout().untransformedDesiredSize = desired;
    if (layoutTransform) {
        const Rect transformed =
            Base::TransformBounds(
                Base::ToProjective(layoutMatrix),
                {0.0, 0.0,
                 desired.width,
                 desired.height});
        desired = {
            transformed.width,
            transformed.height};
    }
    desired = Inflate(desired, margin);
    if (!std::isfinite(desired.width) ||
        !std::isfinite(desired.height)) {
        std::fprintf(
            stderr,
            "Aero layout invalid desired type=%llu constraint=%.3f,%.3f available=%.3f,%.3f min=%.3f,%.3f max=%.3f,%.3f margin=%.3f,%.3f,%.3f,%.3f desired=%.3f,%.3f\n",
            static_cast<unsigned long long>(
                element.RuntimeType()),
            constraint.width,
            constraint.height,
            available.width,
            available.height,
            minimum.width,
            minimum.height,
            maximum.width,
            maximum.height,
            margin.left,
            margin.top,
            margin.right,
            margin.bottom,
            desired.width,
            desired.height);
        AERO_ASSERT(false && "Layout constraints produced an invalid desired size");
        return;
    }
    desired.width = std::max(0.0, desired.width);
    desired.height = std::max(0.0, desired.height);
    if (framework != nullptr && framework->GetUseLayoutRounding()) {
        desired.width = RoundLayoutValue(desired.width, framework->GetDpiScale());
        desired.height = RoundLayoutValue(desired.height, framework->GetDpiScale());
    }
    (element).Layout().previousMeasureConstraint = constraint;
    (element).Layout().desiredSize = desired;
    (element).Layout().measureValid = true;
    (element).Layout().arrangeValid = false;
    (element).Layout().measureQueued = false;
    ++(element).Layout().layoutRevision;
    ++layout.measuredCount_;
    if (queueArrange) {
        layout.arrangeQueue_.PushBack(
            pendingArrange);
        (element).Layout().arrangeQueued = true;
    }
    return;
}

void UIElement::ArrangeCore(
    LayoutEngine& layout,
    Rect slot) noexcept {
    UIElement& element = *this;
    if (!IsValidLayoutRect(slot)) {
        AERO_ASSERT(false && "Arrange slot must be finite and nonnegative");
        return;
    }
    if (!element.GetIsMeasureValid()) {
        MeasureCore(layout, {slot.width, slot.height});
    }
    if (element.GetIsMeasuring() || element.GetIsArranging()) {
        AERO_ASSERT(false && "Recursive layout operation is not allowed");
        return;
    }
    if (element.GetVisibility() == Visibility::Collapsed) {
        (element).Layout().layoutSlot = {slot.x, slot.y, 0.0, 0.0};
        (element).Layout().renderSize = {};
        if (FrameworkElement* framework =
                ::Aero::TryCast<::Aero::FrameworkElement>(&(element))) {
            (*framework).SetActualSize( 0.0, 0.0);
        }
        (element).Layout().layoutClip = {0.0, 0.0, 0.0, 0.0};
        (element).Layout().arrangeValid = true;
        (element).Layout().arrangeQueued = false;
        ++(element).Layout().layoutRevision;
        ++layout.arrangedCount_;
        return;
    }
    FrameworkElement* framework =
        ::Aero::TryCast<::Aero::FrameworkElement>(&(element));
    if (framework != nullptr && (framework->GetUseLayoutRounding() ||
            framework->GetSnapsToDevicePixels())) {
        slot.x = RoundLayoutValue(slot.x, framework->GetDpiScale());
        slot.y = RoundLayoutValue(slot.y, framework->GetDpiScale());
        slot.width = RoundLayoutValue(slot.width, framework->GetDpiScale());
        slot.height = RoundLayoutValue(slot.height, framework->GetDpiScale());
    }
    const Thickness margin = framework != nullptr
        ? framework->GetMargin() : Thickness{};
    const Size minimum = framework != nullptr
        ? framework->GetMinSize() : Size{};
    const Size maximum = framework != nullptr
        ? framework->GetMaxSize() : Size{1.0e12, 1.0e12};
    const bool isLayoutRoot = layout.root_ != nullptr && &element == layout.root_;
    const bool hasWidth =
        !isLayoutRoot && framework != nullptr && framework->GetHasWidth();
    const bool hasHeight =
        !isLayoutRoot && framework != nullptr && framework->GetHasHeight();
    const HorizontalAlignment horizontal = framework != nullptr
        ? framework->GetHorizontalAlignment() : HorizontalAlignment::Stretch;
    const VerticalAlignment vertical = framework != nullptr
        ? framework->GetVerticalAlignment() : VerticalAlignment::Stretch;
    const Size contentAvailable = Deflate({slot.width, slot.height}, margin);
    Base::Ref<Transform> layoutTransform =
        framework != nullptr
        ? framework->GetLayoutTransform()
        : Base::Ref<Transform>{};
    Base::Transform2D layoutMatrix;
    Size naturalAvailable = contentAvailable;
    if (layoutTransform) {
        layoutMatrix = layoutTransform->GetMatrix();
        if (!Base::IsFiniteTransform(layoutMatrix)) {
            AERO_ASSERT(false && "LayoutTransform produced an invalid matrix");
            return;
        }
        naturalAvailable =
            NaturalConstraintForTransform(
                contentAvailable,
                layoutMatrix);
    }
    const Size desiredContent =
        element.GetUntransformedDesiredSize();
    const Size constrainedDesired = ClampSize(
        desiredContent, minimum, maximum);

    Size finalSize;
    if (hasWidth) {
        finalSize.width = ClampDimension(
            framework->GetWidth(), minimum.width, maximum.width);
    } else if (horizontal == HorizontalAlignment::Stretch) {
        finalSize.width = ClampDimension(
            naturalAvailable.width, minimum.width, maximum.width);
    } else {
        finalSize.width = ClampDimension(std::min(
            constrainedDesired.width, naturalAvailable.width),
            minimum.width, maximum.width);
    }
    if (hasHeight) {
        finalSize.height = ClampDimension(
            framework->GetHeight(), minimum.height, maximum.height);
    } else if (vertical == VerticalAlignment::Stretch) {
        finalSize.height = ClampDimension(
            naturalAvailable.height, minimum.height, maximum.height);
    } else {
        finalSize.height = ClampDimension(std::min(
            constrainedDesired.height, naturalAvailable.height),
            minimum.height, maximum.height);
    }

    Size layoutFootprint = finalSize;
    if (layoutTransform) {
        const Rect transformed =
            Base::TransformBounds(
                Base::ToProjective(layoutMatrix),
                {0.0, 0.0,
                 finalSize.width,
                 finalSize.height});
        layoutFootprint = {
            transformed.width,
            transformed.height};
    }
    const bool isRtl = framework != nullptr && framework->GetFlowDirection() == FlowDirection::RightToLeft;
    Thickness effectiveMargin = margin;
    HorizontalAlignment effectiveHorizontal = horizontal;
    if (isRtl) {
        effectiveMargin.left = margin.right;
        effectiveMargin.right = margin.left;
        if (horizontal == HorizontalAlignment::Left) {
            effectiveHorizontal = HorizontalAlignment::Right;
        } else if (horizontal == HorizontalAlignment::Right) {
            effectiveHorizontal = HorizontalAlignment::Left;
        }
    }
    Rect contentSlot{
        slot.x + effectiveMargin.left,
        slot.y + effectiveMargin.top,
        layoutFootprint.width,
        layoutFootprint.height};
    contentSlot.x += AlignmentOffset(contentAvailable.width, layoutFootprint.width,
        effectiveHorizontal == HorizontalAlignment::Center ||
            effectiveHorizontal == HorizontalAlignment::Stretch,
        effectiveHorizontal == HorizontalAlignment::Right);
    contentSlot.y += AlignmentOffset(contentAvailable.height, layoutFootprint.height,
        vertical == VerticalAlignment::Center ||
            vertical == VerticalAlignment::Stretch,
        vertical == VerticalAlignment::Bottom);

    (element).Layout().arranging = true;
    const Size result = (element).ArrangeOverride( finalSize);
    (element).Layout().arranging = false;
    Size render = result;
    if (!IsValidLayoutSize(render)) {
        AERO_ASSERT(false && "ArrangeOverride returned an invalid size");
        return;
    }
    (element).Layout().layoutSlot = contentSlot;
    (element).Layout().renderSize = render;
    if (framework != nullptr) {
        (*framework).SetActualSize( render.width, render.height);
    }
    Size renderedFootprint = render;
    if (layoutTransform) {
        const Rect transformed =
            Base::TransformBounds(
                Base::ToProjective(layoutMatrix),
                {0.0, 0.0,
                 render.width,
                 render.height});
        renderedFootprint = {
            transformed.width,
            transformed.height};
    }
    // ClipToBounds is a local-space clip of RenderSize (WPF
    // UIElement.GetLayoutClip). layoutSlot is in parent coordinates and is
    // already applied as a translation when the node is drawn; storing the
    // parent slot here would double-offset the stencil clip and hide
    // ScrollViewer/VirtualizingStackPanel content once the tree is drawn
    // inline (offscreen compositing resets the clip stack, so the same
    // subtree can still appear while a parent is fading in).
    const Rect localBounds{0.0, 0.0, render.width, render.height};
    const Rect localFootprint{
        0.0, 0.0, renderedFootprint.width, renderedFootprint.height};
    (element).Layout().layoutClip = element.GetClipToBounds()
        ? Intersect(localBounds, localFootprint)
        : localFootprint;
    (element).Layout().arrangeValid = true;
    (element).Layout().arrangeQueued = false;
    ++(element).Layout().layoutRevision;
    ++layout.arrangedCount_;
    return;
}


void LayoutEngine::MeasureElement(
    UIElement& element,
    Size constraint) noexcept {
    element.MeasureCore(*this, constraint);
}

void LayoutEngine::ArrangeElement(
    UIElement& element,
    Rect slot) noexcept {
    element.ArrangeCore(*this, slot);
}


Base::Result<std::uint32_t> LayoutEngine::Flush() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (flushing_) return InvalidState("Nested layout flush is not allowed");

    flushing_ = true;
    measuredCount_ = 0U;
    arrangedCount_ = 0U;

    if (root_ != nullptr &&
        (!root_->GetIsMeasureValid() || !root_->GetIsArrangeValid())) {
        MeasureElement(*root_, rootAvailableSize_);
        ArrangeElement(
            *root_, {0.0, 0.0,
                     rootAvailableSize_.width, rootAvailableSize_.height});
    }

    measureWorkQueue_.Clear();
    measureWorkQueue_.Swap(measureQueue_);
    for (const VisualHandle handle : measureWorkQueue_) {
        UIElement* element = ResolveQueued(handle);
        if (element != nullptr) (*element).Layout().measureQueued = false;
    }
    for (const VisualHandle handle : measureWorkQueue_) {
        UIElement* element = ResolveQueued(handle);
        if (element == nullptr || element == root_ ||
            ElementTree::LayoutOf(*element) != this || element->GetIsMeasureValid()) {
            continue;
        }
        UIElement* parent = element->GetIsLayoutAttached()
            ? element->LayoutParent() : nullptr;
        const Size constraint = parent != nullptr
            ? parent->GetRenderSize() : rootAvailableSize_;
        MeasureElement(*element, constraint);
    }
    measureWorkQueue_.Clear();

    arrangeWorkQueue_.Clear();
    arrangeWorkQueue_.Swap(arrangeQueue_);
    for (const VisualHandle handle : arrangeWorkQueue_) {
        UIElement* element = ResolveQueued(handle);
        if (element != nullptr) (*element).Layout().arrangeQueued = false;
    }
    for (const VisualHandle handle : arrangeWorkQueue_) {
        UIElement* element = ResolveQueued(handle);
        if (element == nullptr || element == root_ ||
            ElementTree::LayoutOf(*element) != this || element->GetIsArrangeValid()) {
            continue;
        }
        Rect slot = element->GetLayoutSlot();
        if (slot.width == 0.0 && slot.height == 0.0) {
            slot.width = element->GetDesiredSize().width;
            slot.height = element->GetDesiredSize().height;
        }
        ArrangeElement(*element, slot);
    }
    arrangeWorkQueue_.Clear();

    // Applying a template during ArrangeOverride can attach new visuals and
    // invalidate the root after the root's first arrange has completed. Drive
    // those re-entrant invalidations to a stable layout in the same frame so
    // render commit never observes an invalid visible root.
    constexpr std::uint32_t MaxConvergencePasses = 8U;
    std::uint32_t convergencePass = 0U;
    while (root_ != nullptr &&
           HasInvalidVisibleLayout(*root_) &&
           convergencePass < MaxConvergencePasses) {
        ++convergencePass;
        (*root_).Layout().measureValid = false;
        (*root_).Layout().arrangeValid = false;
        MeasureElement(*root_, rootAvailableSize_);
        ArrangeElement(
            *root_, {0.0, 0.0,
                     rootAvailableSize_.width,
                     rootAvailableSize_.height});
    }
    if (root_ != nullptr && HasInvalidVisibleLayout(*root_)) {
        flushing_ = false;
        UIElement* invalid = FindInvalidVisibleLayout(*root_);
        const TypeInfo* type = invalid != nullptr
            ? DependencyObjectAccess::PropertyRegistry(invalid).Types().FindType(
                  invalid->RuntimeType())
            : nullptr;
        const Base::StringView typeName = type != nullptr
            ? type->Name()
            : Base::StringView("<unknown>");
        UIElement* layoutParent = invalid != nullptr
            ? invalid->LayoutParent()
            : nullptr;
        const TypeInfo* parentType = layoutParent != nullptr
            ? DependencyObjectAccess::PropertyRegistry(layoutParent).Types().FindType(
                  layoutParent->RuntimeType())
            : nullptr;
        const Base::StringView parentName = parentType != nullptr
            ? parentType->Name()
            : Base::StringView("<none>");
        thread_local char message[512];
        std::snprintf(
            message,
            sizeof(message),
            "Layout did not converge for visible '%.*s' %p (measure=%u arrange=%u parent='%.*s' %p, isVisible=%d, vis=%u, layoutAttached=%d, layoutParent=%p, visualParent=%p) after template application",
            static_cast<int>(typeName.SizeBytes()),
            typeName.Data(),
            static_cast<void*>(invalid),
            invalid != nullptr && invalid->GetIsMeasureValid() ? 1U : 0U,
            invalid != nullptr && invalid->GetIsArrangeValid() ? 1U : 0U,
            static_cast<int>(parentName.SizeBytes()),
            parentName.Data(),
            static_cast<void*>(layoutParent),
            invalid != nullptr ? (int)invalid->GetIsVisible() : -1,
            invalid != nullptr ? static_cast<unsigned>(invalid->GetVisibility()) : 99U,
            invalid != nullptr ? (int)(*invalid).Layout().layoutAttached : -1,
            invalid != nullptr ? static_cast<void*>(invalid->LayoutParent()) : nullptr,
            invalid != nullptr ? static_cast<void*>(invalid->GetVisualParent()) : nullptr);
        return InvalidState(message);
    }

    // A converged root has recursively measured and arranged every attached
    // descendant. Remove stale queue handles created during template
    // application so the next frame starts from a clean layout state.
    for (const VisualHandle handle : measureQueue_) {
        UIElement* element = ResolveQueued(handle);
        if (element != nullptr) (*element).Layout().measureQueued = false;
    }
    for (const VisualHandle handle : arrangeQueue_) {
        UIElement* element = ResolveQueued(handle);
        if (element != nullptr) (*element).Layout().arrangeQueued = false;
    }
    measureQueue_.Clear();
    arrangeQueue_.Clear();

    ++passVersion_;
    flushing_ = false;
    return measuredCount_ + arrangedCount_;
}

LayoutDiagnostics LayoutEngine::Diagnostics() const noexcept {
    LayoutDiagnostics diagnostics;
    diagnostics.passVersion = passVersion_;
    diagnostics.measuredCount = measuredCount_;
    diagnostics.arrangedCount = arrangedCount_;
    diagnostics.pendingMeasureCount = measureQueue_.Size();
    diagnostics.pendingArrangeCount = arrangeQueue_.Size();
    return diagnostics;
}

void LayoutEngine::LayoutHook(void* context) noexcept {
    auto* manager = static_cast<LayoutEngine*>(context);
    if (manager != nullptr) {
        Base::Result<std::uint32_t> result =
            manager->Flush();
        manager->lastFlushStatus_ = result
            ? Base::Status{}
            : result.GetStatus();
    }
}

} // namespace Aero
