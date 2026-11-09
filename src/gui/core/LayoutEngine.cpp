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
#include <Aero/Shapes.hpp>

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

float ClampDimension(float value, float minimum, float maximum) noexcept {
    return std::max(minimum, std::min(value, maximum));
}

Size ClampSize(Size value, Size minimum, Size maximum) noexcept {
    return {ClampDimension(value.width, minimum.width, maximum.width),
        ClampDimension(value.height, minimum.height, maximum.height)};
}

float AlignmentOffset(float available, float actual, bool center, bool end) noexcept {
    const float remaining = std::max(0.0f, available - actual);
    return center ? remaining * 0.5f : (end ? remaining : 0.0f);
}

Size NaturalConstraintForTransform(
    Size transformed,
    const Base::Transform2D& matrix) noexcept {
    Base::ProjectiveTransform2D inverse;
    if (!Base::Invert(Base::ToProjective(matrix), inverse)) {
        return transformed;
    }
    constexpr float kUnconstrainedProxy = 1.0e7f;
    constexpr float kUnconstrainedThreshold = 5.0e6f;
    constexpr float kUnconstrainedMax = 1.0e12f;

    const bool widthUnconstrained = !std::isfinite(transformed.width) || transformed.width >= 1.0e10f;
    const bool heightUnconstrained = !std::isfinite(transformed.height) || transformed.height >= 1.0e10f;

    if (widthUnconstrained && heightUnconstrained) {
        return {kUnconstrainedMax, kUnconstrainedMax};
    }

    const Size safeSize{
        widthUnconstrained ? kUnconstrainedProxy : transformed.width,
        heightUnconstrained ? kUnconstrainedProxy : transformed.height};

    const Rect bounds = Base::TransformBounds(
        inverse,
        {0.0f, 0.0f,
         safeSize.width,
         safeSize.height});

    float outW = std::max(0.0f, bounds.width);
    float outH = std::max(0.0f, bounds.height);

    if (outW >= kUnconstrainedThreshold) {
        outW = kUnconstrainedMax;
    }
    if (outH >= kUnconstrainedThreshold) {
        outH = kUnconstrainedMax;
    }
    return {outW, outH};
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

bool IsValidLayoutRect(Rect value) noexcept {
    return IsFinite(value) && value.width >= 0.0 && value.height >= 0.0;
}

Size Deflate(Size value, Thickness padding) noexcept {
    const float horizontal = padding.left + padding.right;
    const float vertical = padding.top + padding.bottom;
    return {std::max(0.0f, value.width - horizontal),
        std::max(0.0f, value.height - vertical)};
}

Size Inflate(Size value, Thickness padding) noexcept {
    return {value.width + padding.left + padding.right,
        value.height + padding.top + padding.bottom};
}

Rect Intersect(Rect left, Rect right) noexcept {
    const float x = std::max(left.x, right.x);
    const float y = std::max(left.y, right.y);
    const float rightEdge = std::min(left.x + left.width, right.x + right.width);
    const float bottomEdge = std::min(left.y + left.height, right.y + right.height);
    return {x, y, std::max(0.0f, rightEdge - x),
        std::max(0.0f, bottomEdge - y)};
}

float RoundLayoutValue(float value, float dpiScale) noexcept {
    if (!std::isfinite(value) || !std::isfinite(dpiScale) || dpiScale <= 0.0f) {
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
        // Allow reparenting a layout child that was previously attached to a
        // different layout parent (e.g. during template or content hosting).
        UIElement* oldParent = child.LayoutParent();
        child.Layout().SetFlag(UIElement::LayoutFlags::LayoutAttached, false);
        child.Layout().neverMeasured = true;
        child.Layout().neverArranged = true;
        child.Layout().InvalidateMeasureFlags();
        if (oldParent != nullptr && oldParent != &parent) {
            InvalidateMeasure(*oldParent);
        }
    }
    // A stale LayoutParent on a detached child is irrelevant and must not block
    // re-attachment; the GetIsLayoutAttached() check above already catches
    // genuine double-attach conflicts.

    // Queue all parent invalidation work before publishing the child state.
    InvalidateMeasure(parent);

    child.Layout().SetFlag(UIElement::LayoutFlags::LayoutAttached, true);
    child.Layout().neverMeasured = true;
    child.Layout().neverArranged = true;
    child.Layout().InvalidateMeasureFlags();
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
    child.Layout().SetFlag(UIElement::LayoutFlags::LayoutAttached, false);
    child.Layout().neverMeasured = true;
    child.Layout().neverArranged = true;
    child.Layout().InvalidateMeasureFlags();
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
        root_->Layout().SetFlag(UIElement::LayoutFlags::MeasureQueued, false);
        root_->Layout().SetFlag(UIElement::LayoutFlags::ArrangeQueued, false);
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

void LayoutEngine::ClearQueuedFlags(
    Base::Vector<VisualHandle>& queue,
    UIElement::LayoutFlags flag) const noexcept {
    for (const VisualHandle handle : queue) {
        if (UIElement* element = ResolveQueued(handle)) {
            element->Layout().SetFlag(flag, false);
        }
    }
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
    element.Layout().SetFlag(UIElement::LayoutFlags::MeasureQueued, false);
    element.Layout().SetFlag(UIElement::LayoutFlags::ArrangeQueued, false);
}

void LayoutEngine::PropagateInvalidation(
    UIElement& element,
    bool invalidateMeasure) noexcept {
    if (!invalidateMeasure && element.GetIsArranging()) {
        return;
    }
    Base::Vector<VisualHandle>& targetQueue =
        invalidateMeasure ? measureQueue_ : arrangeQueue_;
    const UIElement::LayoutFlags queuedFlag =
        invalidateMeasure
            ? UIElement::LayoutFlags::MeasureQueued
            : UIElement::LayoutFlags::ArrangeQueued;

    UIElement* current = &element;
    while (current != nullptr) {
        Base::Result<void> verified = VerifyElement(*current);
        if (!verified) { AERO_ASSERT(false); return; }

        if (invalidateMeasure && current->GetIsMeasuring()) {
            break;
        }

        const bool wasValid = invalidateMeasure
            ? current->GetIsMeasureValid()
            : current->GetIsArrangeValid();
        const bool wasQueued = current->Layout().HasFlag(queuedFlag);
        if (invalidateMeasure) {
            current->Layout().InvalidateMeasureFlags();
        } else {
            current->Layout().InvalidateArrangeFlags();
        }

        if (!wasQueued) {
            const VisualHandle handle = ElementTree::HandleOf(*current);
            if (handle.IsValid()) {
                targetQueue.PushBack(handle);
                current->Layout().SetFlag(queuedFlag, true);
            }
        }

        // Once an ancestor is already invalid and queued, all of its ancestors
        // up to the dirty root have already been invalidated and queued.
        if (current != &element && !wasValid && wasQueued) {
            break;
        }

        current = current->GetIsLayoutAttached()
            ? current->LayoutParent() : nullptr;
    }
}

void LayoutEngine::InvalidateMeasure(UIElement& element) noexcept {
    PropagateInvalidation(element, /*invalidateMeasure=*/true);
}

void LayoutEngine::InvalidateArrange(UIElement& element) noexcept {
    PropagateInvalidation(element, /*invalidateMeasure=*/false);
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

    const bool hadPreviousMeasure = !element.GetNeverMeasured();
    const Size previousDesired = element.GetDesiredSize();

    VisualHandle pendingArrange{};
    bool queueArrange = !element.GetIsArrangeQueued();
    if (queueArrange) {
        const VisualHandle handle = ElementTree::HandleOf(element);
        if (handle.IsValid()) {
            pendingArrange = handle;
            layout.arrangeQueue_.Reserve(
                layout.arrangeQueue_.Size() + 1U);
        } else {
            queueArrange = false;
        }
    }

    if (element.GetVisibility() == Visibility::Collapsed) {
        element.Layout().previousMeasureConstraint = constraint;
        element.Layout().desiredSize = {};
        element.Layout().neverMeasured = true;
        element.Layout().neverArranged = true;
        if (element.rare_ != nullptr) {
            element.rare_->hasCustomUntransformedDesiredSize = false;
        }
        element.Layout().SetFlag(LayoutFlags::MeasureValid, true);
        element.Layout().SetFlag(LayoutFlags::ArrangeValid, false);
        element.Layout().SetFlag(LayoutFlags::MeasureQueued, false);
        ++element.Layout().layoutRevision;
        ++layout.measuredCount_;
        if (queueArrange) {
            layout.arrangeQueue_.PushBack(pendingArrange);
            element.Layout().SetFlag(LayoutFlags::ArrangeQueued, true);
        }
        if (hadPreviousMeasure && !SameSize(previousDesired, Size{})) {
            if (UIElement* parent = element.GetIsLayoutAttached() ? element.LayoutParent() : nullptr) {
                if (!parent->GetIsMeasuring()) {
                    layout.InvalidateMeasure(*parent);
                }
            }
        }
        return;
    }

    element.Layout().SetFlag(LayoutFlags::Measuring, true);
    const Size result = element.MeasureOverride(constraint);
    element.Layout().SetFlag(LayoutFlags::Measuring, false);
    Size desired = result;
    if (!IsValidLayoutSize(desired)) {
        AERO_ASSERT(false && "MeasureOverride returned an invalid size");
        return;
    }
    desired.width = std::max(0.0f, desired.width);
    desired.height = std::max(0.0f, desired.height);
    if (element.rare_ != nullptr) {
        element.rare_->hasCustomUntransformedDesiredSize = false;
    }
    element.Layout().previousMeasureConstraint = constraint;
    element.Layout().desiredSize = desired;
    element.Layout().neverMeasured = false;
    element.Layout().SetFlag(LayoutFlags::MeasureValid, true);
    element.Layout().SetFlag(LayoutFlags::ArrangeValid, false);
    element.Layout().SetFlag(LayoutFlags::MeasureQueued, false);
    ++element.Layout().layoutRevision;
    ++layout.measuredCount_;
    if (queueArrange) {
        layout.arrangeQueue_.PushBack(pendingArrange);
        element.Layout().SetFlag(LayoutFlags::ArrangeQueued, true);
    }
    if (hadPreviousMeasure && !SameSize(previousDesired, desired)) {
        if (UIElement* parent = element.GetIsLayoutAttached() ? element.LayoutParent() : nullptr) {
            if (!parent->GetIsMeasuring()) {
                layout.InvalidateMeasure(*parent);
            }
        }
    } else if (element.GetNeverArranged()) {
        if (UIElement* parent = element.GetIsLayoutAttached() ? element.LayoutParent() : nullptr) {
            if (!parent->GetIsMeasuring() && !parent->GetIsArranging()) {
                layout.InvalidateArrange(*parent);
            }
        }
    }
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
    const Rect rawSlot = slot;
    if (element.GetIsArrangeValid() &&
        !element.GetNeverArranged() &&
        element.GetPreviousFinalRect() == rawSlot) {
        return;
    }
    const Rect oldLayoutSlot = element.GetLayoutSlot();
    const Size oldRenderSize = element.GetRenderSize();
    if (element.GetVisibility() == Visibility::Collapsed) {
        element.Layout().layoutSlot = {slot.x, slot.y, 0.0f, 0.0f};
        element.SetPreviousFinalRect(rawSlot);
        element.Layout().renderSize = {};
        element.Layout().neverArranged = true;
        if (element.rare_ != nullptr) {
            element.rare_->hasCustomLayoutClip = false;
        }
        element.Layout().SetFlag(LayoutFlags::ArrangeValid, true);
        element.Layout().SetFlag(LayoutFlags::ArrangeQueued, false);
        ++element.Layout().layoutRevision;
        ++layout.arrangedCount_;
        if (ElementTree::RenderTreeOf(element) != nullptr) {
            if (!SameSize(oldRenderSize, element.GetRenderSize())) {
                element.InvalidateRenderDrawing();
            } else if (oldLayoutSlot != element.GetLayoutSlot()) {
                element.InvalidateRenderState();
            }
        }
        return;
    }

    element.Layout().SetFlag(LayoutFlags::ArrangeValid, true);
    element.Layout().SetFlag(LayoutFlags::ArrangeQueued, false);
    element.Layout().neverArranged = false;
    element.Layout().SetFlag(LayoutFlags::Arranging, true);
    const Size result = element.ArrangeOverride({slot.width, slot.height});
    element.Layout().SetFlag(LayoutFlags::Arranging, false);
    Size render = result;
    if (!IsValidLayoutSize(render)) {
        element.Layout().SetFlag(LayoutFlags::ArrangeValid, false);
        AERO_ASSERT(false && "ArrangeOverride returned an invalid size");
        return;
    }
    element.Layout().layoutSlot = slot;
    element.SetPreviousFinalRect(rawSlot);
    element.Layout().renderSize = render;
    if (element.rare_ != nullptr) {
        element.rare_->hasCustomLayoutClip = false;
    }
    if (!element.GetIsMeasureValid()) {
        element.Layout().SetFlag(LayoutFlags::ArrangeValid, false);
    }
    ++element.Layout().layoutRevision;
    ++layout.arrangedCount_;
    if (ElementTree::RenderTreeOf(element) != nullptr) {
        if (!SameSize(oldRenderSize, element.GetRenderSize())) {
            element.InvalidateRenderDrawing();
        } else if (oldLayoutSlot != element.GetLayoutSlot()) {
            element.InvalidateRenderState();
        }
    }
}

void FrameworkElement::MeasureCore(
    LayoutEngine& layout,
    Size constraint) noexcept {
    FrameworkElement& element = *this;
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

    const bool hadPreviousMeasure = !element.GetNeverMeasured();
    const Size previousDesired = element.GetDesiredSize();

    VisualHandle pendingArrange{};
    bool queueArrange = !element.GetIsArrangeQueued();
    if (queueArrange) {
        const VisualHandle handle = ElementTree::HandleOf(element);
        if (handle.IsValid()) {
            pendingArrange = handle;
            layout.arrangeQueue_.Reserve(
                layout.arrangeQueue_.Size() + 1U);
        } else {
            queueArrange = false;
        }
    }

    if (element.GetVisibility() == Visibility::Collapsed) {
        element.Layout().previousMeasureConstraint = constraint;
        element.Layout().desiredSize = {};
        element.Layout().neverMeasured = true;
        element.Layout().neverArranged = true;
        if (element.rare_ != nullptr) {
            element.rare_->hasCustomUntransformedDesiredSize = false;
        }
        element.Layout().SetFlag(LayoutFlags::MeasureValid, true);
        element.Layout().SetFlag(LayoutFlags::ArrangeValid, false);
        element.Layout().SetFlag(LayoutFlags::MeasureQueued, false);
        ++element.Layout().layoutRevision;
        ++layout.measuredCount_;
        if (queueArrange) {
            layout.arrangeQueue_.PushBack(pendingArrange);
            element.Layout().SetFlag(LayoutFlags::ArrangeQueued, true);
        }
        if (hadPreviousMeasure && !SameSize(previousDesired, Size{})) {
            if (UIElement* parent = element.GetIsLayoutAttached() ? element.LayoutParent() : nullptr) {
                if (!parent->GetIsMeasuring()) {
                    layout.InvalidateMeasure(*parent);
                }
            }
        }
        return;
    }

    const Thickness margin = element.GetMargin();
    const Size minimum = element.GetMinSize();
    const Size maximum = element.GetMaxSize();
    // Window.Width/Height size the native chrome. The layout root must fill
    // the view client (DPI-converted, user-resized) so Viewbox Uniform can
    // scale. Nested elements still honor explicit Width/Height.
    const bool isLayoutRoot = layout.root_ != nullptr && &element == layout.root_;
    const bool hasWidth = !isLayoutRoot && element.GetHasWidth();
    const bool hasHeight = !isLayoutRoot && element.GetHasHeight();
    Size available = Deflate(constraint, margin);
    Base::Ref<Transform> layoutTransform = element.GetLayoutTransform();
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
            static_cast<float>(element.GetWidth()), minimum.width, maximum.width);
    }
    if (hasHeight) {
        available.height = ClampDimension(
            static_cast<float>(element.GetHeight()), minimum.height, maximum.height);
    }

    element.Layout().SetFlag(LayoutFlags::Measuring, true);
    const Size result = element.MeasureOverride(available);
    element.Layout().SetFlag(LayoutFlags::Measuring, false);
    Size desired = result;
    if (!IsValidLayoutSize(desired)) {
        AERO_ASSERT(false && "MeasureOverride returned an invalid size");
        return;
    }
    desired = ClampSize(desired, minimum, maximum);
    if (hasWidth) desired.width = available.width;
    if (hasHeight) desired.height = available.height;
    const Size untransformedDesired = desired;
    if (layoutTransform) {
        const Rect transformed =
            Base::TransformBounds(
                Base::ToProjective(layoutMatrix),
                {0.0f, 0.0f,
                 desired.width,
                 desired.height});
        desired = {
            transformed.width,
            transformed.height};
    }
    desired = Inflate(desired, margin);
    if (!std::isfinite(desired.width) ||
        !std::isfinite(desired.height)) {
        AERO_ASSERT(false && "Layout constraints produced an invalid desired size");
        return;
    }
    desired.width = std::max(0.0f, desired.width);
    desired.height = std::max(0.0f, desired.height);
    if (element.GetUseLayoutRounding()) {
        desired.width = RoundLayoutValue(desired.width, static_cast<float>(element.GetDpiScale()));
        desired.height = RoundLayoutValue(desired.height, static_cast<float>(element.GetDpiScale()));
    }
    const Size reconstructedUntransformed = Deflate(desired, margin);
    if (reconstructedUntransformed.width != untransformedDesired.width ||
        reconstructedUntransformed.height != untransformedDesired.height) {
        UIElement::Rare& rare = element.EnsureRare();
        rare.untransformedDesiredSize = untransformedDesired;
        rare.hasCustomUntransformedDesiredSize = true;
    } else if (element.rare_ != nullptr) {
        element.rare_->hasCustomUntransformedDesiredSize = false;
    }
    element.Layout().previousMeasureConstraint = constraint;
    element.Layout().desiredSize = desired;
    element.Layout().neverMeasured = false;
    element.Layout().SetFlag(LayoutFlags::MeasureValid, true);
    element.Layout().SetFlag(LayoutFlags::ArrangeValid, false);
    element.Layout().SetFlag(LayoutFlags::MeasureQueued, false);
    ++element.Layout().layoutRevision;
    ++layout.measuredCount_;
    if (queueArrange) {
        layout.arrangeQueue_.PushBack(pendingArrange);
        element.Layout().SetFlag(LayoutFlags::ArrangeQueued, true);
    }
    if (hadPreviousMeasure && !SameSize(previousDesired, desired)) {
        if (UIElement* parent = element.GetIsLayoutAttached() ? element.LayoutParent() : nullptr) {
            if (!parent->GetIsMeasuring()) {
                layout.InvalidateMeasure(*parent);
            }
        }
    } else if (element.GetNeverArranged()) {
        if (UIElement* parent = element.GetIsLayoutAttached() ? element.LayoutParent() : nullptr) {
            if (!parent->GetIsMeasuring() && !parent->GetIsArranging()) {
                layout.InvalidateArrange(*parent);
            }
        }
    }
}

void FrameworkElement::ArrangeCore(
    LayoutEngine& layout,
    Rect slot) noexcept {
    FrameworkElement& element = *this;
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
    const Rect rawSlot = slot;
    if (element.GetIsArrangeValid() &&
        !element.GetNeverArranged() &&
        element.GetPreviousFinalRect() == rawSlot) {
        return;
    }
    const Rect oldLayoutSlot = element.GetLayoutSlot();
    const Size oldRenderSize = element.GetRenderSize();
    const bool oldHasLayoutClip = element.GetHasLayoutClip();
    const Rect oldLayoutClip = element.GetLayoutClip();
    if (element.GetVisibility() == Visibility::Collapsed) {
        element.Layout().layoutSlot = {slot.x, slot.y, 0.0f, 0.0f};
        element.SetPreviousFinalRect(rawSlot);
        element.SetActualSize(0.0f, 0.0f);
        element.Layout().neverArranged = true;
        if (element.rare_ != nullptr) {
            element.rare_->hasCustomLayoutClip = false;
        }
        element.Layout().SetFlag(LayoutFlags::ArrangeValid, true);
        element.Layout().SetFlag(LayoutFlags::ArrangeQueued, false);
        ++element.Layout().layoutRevision;
        ++layout.arrangedCount_;
        if (ElementTree::RenderTreeOf(element) != nullptr) {
            if (!SameSize(oldRenderSize, element.GetRenderSize())) {
                element.InvalidateRenderDrawing();
            } else if (oldLayoutSlot != element.GetLayoutSlot()) {
                element.InvalidateRenderState();
            }
        }
        return;
    }
    if (element.GetUseLayoutRounding() || element.GetSnapsToDevicePixels()) {
        const float dpi = static_cast<float>(element.GetDpiScale());
        slot.x = RoundLayoutValue(slot.x, dpi);
        slot.y = RoundLayoutValue(slot.y, dpi);
        slot.width = RoundLayoutValue(slot.width, dpi);
        slot.height = RoundLayoutValue(slot.height, dpi);
    }
    const Thickness margin = element.GetMargin();
    const Size minimum = element.GetMinSize();
    const Size maximum = element.GetMaxSize();
    const bool isLayoutRoot = layout.root_ != nullptr && &element == layout.root_;
    const bool hasWidth = !isLayoutRoot && element.GetHasWidth();
    const bool hasHeight = !isLayoutRoot && element.GetHasHeight();
    const HorizontalAlignment horizontal = element.GetHorizontalAlignment();
    const VerticalAlignment vertical = element.GetVerticalAlignment();
    const Size contentAvailable = Deflate({slot.width, slot.height}, margin);
    Base::Ref<Transform> layoutTransform = element.GetLayoutTransform();
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
    const Size desiredContent = element.GetUntransformedDesiredSize();
    const Size constrainedDesired = ClampSize(
        desiredContent, minimum, maximum);

    Size finalSize;
    if (hasWidth) {
        finalSize.width = ClampDimension(
            static_cast<float>(element.GetWidth()), minimum.width, maximum.width);
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
            static_cast<float>(element.GetHeight()), minimum.height, maximum.height);
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
                {0.0f, 0.0f,
                 finalSize.width,
                 finalSize.height});
        layoutFootprint = {
            transformed.width,
            transformed.height};
    }
    const bool isRtl = element.GetFlowDirection() == FlowDirection::RightToLeft;
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
    constexpr float kMarginTolerance = 0.05f;
    const bool hasNegativeMargin =
        effectiveMargin.left < -kMarginTolerance ||
        effectiveMargin.top < -kMarginTolerance ||
        effectiveMargin.right < -kMarginTolerance ||
        effectiveMargin.bottom < -kMarginTolerance;

    const bool centerHorizontal =
        effectiveHorizontal == HorizontalAlignment::Center ||
        (effectiveHorizontal == HorizontalAlignment::Stretch && hasWidth && !hasNegativeMargin);
    const bool centerVertical =
        vertical == VerticalAlignment::Center ||
        (vertical == VerticalAlignment::Stretch && hasHeight && !hasNegativeMargin);

    contentSlot.x += AlignmentOffset(contentAvailable.width, layoutFootprint.width,
        centerHorizontal,
        effectiveHorizontal == HorizontalAlignment::Right);
    contentSlot.y += AlignmentOffset(contentAvailable.height, layoutFootprint.height,
        centerVertical,
        vertical == VerticalAlignment::Bottom);

    element.Layout().SetFlag(LayoutFlags::ArrangeValid, true);
    element.Layout().SetFlag(LayoutFlags::ArrangeQueued, false);
    element.Layout().neverArranged = false;
    element.Layout().SetFlag(LayoutFlags::Arranging, true);
    const Size result = element.ArrangeOverride(finalSize);
    element.Layout().SetFlag(LayoutFlags::Arranging, false);
    Size render = result;
    if (!IsValidLayoutSize(render)) {
        element.Layout().SetFlag(LayoutFlags::ArrangeValid, false);
        AERO_ASSERT(false && "ArrangeOverride returned an invalid size");
        return;
    }
    if (element.GetUseLayoutRounding() || element.GetSnapsToDevicePixels()) {
        const float dpi = static_cast<float>(element.GetDpiScale());
        contentSlot.x = RoundLayoutValue(contentSlot.x, dpi);
        contentSlot.y = RoundLayoutValue(contentSlot.y, dpi);
        render.width = RoundLayoutValue(render.width, dpi);
        render.height = RoundLayoutValue(render.height, dpi);
    }
    element.Layout().layoutSlot = contentSlot;
    element.SetPreviousFinalRect(rawSlot);
    element.SetActualSize(render.width, render.height);

    Size renderedFootprint = render;
    Rect renderedBounds{0.0f, 0.0f, render.width, render.height};
    if (layoutTransform) {
        renderedBounds =
            Base::TransformBounds(
                Base::ToProjective(layoutMatrix),
                {0.0f, 0.0f,
                 render.width,
                 render.height});
        renderedFootprint = {
            renderedBounds.width,
            renderedBounds.height};
    }
    const Rect localBounds{0.0f, 0.0f, render.width, render.height};
    const Rect localFootprint{
        0.0f, 0.0f, renderedFootprint.width, renderedFootprint.height};

    // Calculate layout slot in element-local coordinates.
    // contentSlot is the element origin in parent space, so (slot.origin + effectiveMargin)
    // relative to contentSlot yields the available area's local origin.
    Rect localSlot{
        (slot.x + effectiveMargin.left) - contentSlot.x,
        (slot.y + effectiveMargin.top) - contentSlot.y,
        std::max(0.0f, contentAvailable.width),
        std::max(0.0f, contentAvailable.height)};
    if (layoutTransform) {
        Rect offsetSlot = localSlot;
        offsetSlot.x += renderedBounds.x;
        offsetSlot.y += renderedBounds.y;
        Base::ProjectiveTransform2D inverseMatrix;
        if (Base::Invert(Base::ToProjective(layoutMatrix), inverseMatrix)) {
            localSlot = Base::TransformBounds(inverseMatrix, offsetSlot);
        }
    }

    constexpr float kLayoutTolerance = 0.05f;
    const bool overflowsSlot =
        !isLayoutRoot &&
        (render.width > 0.0f && render.height > 0.0f) &&
        (localBounds.x < localSlot.x - kLayoutTolerance ||
         localBounds.y < localSlot.y - kLayoutTolerance ||
         localBounds.x + localBounds.width > localSlot.x + localSlot.width + kLayoutTolerance ||
         localBounds.y + localBounds.height > localSlot.y + localSlot.height + kLayoutTolerance);

    const bool isShape = ::Aero::TryCast<::Aero::Shapes::Shape>(&element) != nullptr;
    const bool clipsToBounds = element.GetClipToBounds();
    const bool hasLayoutClip = overflowsSlot && !hasNegativeMargin && !isShape;
    Rect clipRect = localBounds;
    if (hasLayoutClip) {
        clipRect = Intersect(clipRect, localSlot);
    }
    if (clipsToBounds) {
        clipRect = Intersect(clipRect, localFootprint);
    }
    clipRect.width = std::max(0.0f, clipRect.width);
    clipRect.height = std::max(0.0f, clipRect.height);

    element.Layout().SetFlag(LayoutFlags::HasLayoutClip, hasLayoutClip);
    const Rect finalClip = (hasLayoutClip || clipsToBounds)
        ? clipRect
        : localBounds;
    if (finalClip.x != localBounds.x ||
        finalClip.y != localBounds.y ||
        finalClip.width != localBounds.width ||
        finalClip.height != localBounds.height) {
        UIElement::Rare& rare = element.EnsureRare();
        rare.layoutClip = finalClip;
        rare.hasCustomLayoutClip = true;
    } else if (element.rare_ != nullptr) {
        element.rare_->hasCustomLayoutClip = false;
    }
    if (!element.GetIsMeasureValid()) {
        element.Layout().SetFlag(LayoutFlags::ArrangeValid, false);
    }
    ++element.Layout().layoutRevision;
    ++layout.arrangedCount_;
    if (ElementTree::RenderTreeOf(element) != nullptr) {
        if (!SameSize(oldRenderSize, element.GetRenderSize())) {
            element.InvalidateRenderDrawing();
        } else if (oldLayoutSlot != element.GetLayoutSlot() ||
                   oldHasLayoutClip != element.GetHasLayoutClip() ||
                   oldLayoutClip != element.GetLayoutClip()) {
            element.InvalidateRenderState();
        }
    }
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

    const auto sortWorkQueueByDepth = [&](Base::Vector<VisualHandle>& work) noexcept {
        const std::uint32_t count = work.Size();
        if (count <= 1U) return;
        constexpr std::uint32_t kMaxBucketDepth = 128U;
        struct DepthEntry {
            VisualHandle handle;
            std::uint16_t depth = 0U;
        };
        Base::BasicVector<DepthEntry, 64U> entries;
        entries.Reserve(count);
        std::uint16_t minDepth = UINT16_MAX;
        std::uint16_t maxDepth = 0U;
        for (const VisualHandle h : work) {
            const UIElement* el = ResolveQueued(h);
            const std::uint16_t d = el != nullptr
                ? std::min<std::uint16_t>(el->TreeDepth(), kMaxBucketDepth - 1U)
                : static_cast<std::uint16_t>(kMaxBucketDepth - 1U);
            minDepth = std::min(minDepth, d);
            maxDepth = std::max(maxDepth, d);
            entries.PushBack({h, d});
        }
        if (minDepth == maxDepth) return;
        std::uint32_t buckets[kMaxBucketDepth]{};
        for (std::uint32_t i = 0U; i < count; ++i) {
            ++buckets[entries[i].depth];
        }
        std::uint32_t running = 0U;
        for (std::uint16_t d = minDepth; d <= maxDepth; ++d) {
            const std::uint32_t c = buckets[d];
            buckets[d] = running;
            running += c;
        }
        for (std::uint32_t i = 0U; i < count; ++i) {
            work[buckets[entries[i].depth]++] = entries[i].handle;
        }
    };

    if (root_ != nullptr &&
        (!root_->GetIsMeasureValid() || !root_->GetIsArrangeValid())) {
        MeasureElement(*root_, rootAvailableSize_);
        ArrangeElement(
            *root_, {0.0f, 0.0f,
                     rootAvailableSize_.width, rootAvailableSize_.height});
    }

    // WPF/Noesis-style depth-bucketed measure-before-arrange loop:
    // always drain shallowest dirty Measure nodes first; if MeasureCore or
    // ArrangeOverride invalidates any subtree, loop back to drain queues
    // incrementally without walking clean subtrees.
    constexpr std::uint32_t MaxQueueWaves = 16U;
    for (std::uint32_t wave = 0U;
         wave < MaxQueueWaves && (!measureQueue_.Empty() || !arrangeQueue_.Empty());
         ++wave) {
        while (!measureQueue_.Empty()) {
            measureWorkQueue_.Clear();
            measureWorkQueue_.Swap(measureQueue_);
            sortWorkQueueByDepth(measureWorkQueue_);
            ClearQueuedFlags(measureWorkQueue_, UIElement::LayoutFlags::MeasureQueued);
            for (const VisualHandle handle : measureWorkQueue_) {
                UIElement* element = ResolveQueued(handle);
                if (element == nullptr ||
                    ElementTree::LayoutOf(*element) != this ||
                    element->GetIsMeasureValid()) {
                    continue;
                }
                if (element == root_) {
                    MeasureElement(*root_, rootAvailableSize_);
                    continue;
                }
                UIElement* parent = element->GetIsLayoutAttached()
                    ? element->LayoutParent() : nullptr;
                const Size constraint = !element->GetNeverMeasured()
                    ? element->GetPreviousAvailableSize()
                    : (parent != nullptr ? parent->GetRenderSize() : rootAvailableSize_);
                MeasureElement(*element, constraint);
            }
            measureWorkQueue_.Clear();
        }

        if (!arrangeQueue_.Empty()) {
            arrangeWorkQueue_.Clear();
            arrangeWorkQueue_.Swap(arrangeQueue_);
            sortWorkQueueByDepth(arrangeWorkQueue_);
            ClearQueuedFlags(arrangeWorkQueue_, UIElement::LayoutFlags::ArrangeQueued);
            for (const VisualHandle handle : arrangeWorkQueue_) {
                UIElement* element = ResolveQueued(handle);
                if (element == nullptr ||
                    ElementTree::LayoutOf(*element) != this ||
                    element->GetIsArrangeValid()) {
                    continue;
                }
                if (element == root_) {
                    ArrangeElement(
                        *root_,
                        {0.0f, 0.0f, rootAvailableSize_.width, rootAvailableSize_.height});
                    continue;
                }
                Rect slot = !element->GetNeverArranged()
                    ? element->GetPreviousFinalRect()
                    : element->GetLayoutSlot();
                if (slot.width == 0.0f && slot.height == 0.0f && element->GetNeverArranged()) {
                    slot.width = element->GetDesiredSize().width;
                    slot.height = element->GetDesiredSize().height;
                }
                ArrangeElement(*element, slot);
            }
            arrangeWorkQueue_.Clear();
        }
    }

    ClearQueuedFlags(measureQueue_, UIElement::LayoutFlags::MeasureQueued);
    ClearQueuedFlags(arrangeQueue_, UIElement::LayoutFlags::ArrangeQueued);
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
