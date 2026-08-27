#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include "gui/input/InputState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include <Aero/Layout.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Transforms.hpp>

#include <Aero/Base/Assert.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Controls/Canvas.hpp>
#include <Aero/Controls/Grid.hpp>
#include <Aero/Controls/StackPanel.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

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
    if (phaseHook_.IsValid() && dispatcher_->CheckAccess()) {
        static_cast<void>(
            dispatcher_->RemoveFrameHook(phaseHook_));
    }
}

Base::Result<void> LayoutEngine::Initialize() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (phaseHook_.IsValid()) return {};
    Base::Result<DispatcherFrameHookHandle> hook =
        dispatcher_->RegisterFrameHook(
            DispatcherFramePhase::Layout,
            &LayoutEngine::LayoutHook,
            this,
            nullptr);
    if (!hook) return hook.GetStatus();
    phaseHook_ = hook.Value();
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
    if (AeroGuiInternal::LayoutEngineOf(element) != nullptr && AeroGuiInternal::LayoutEngineOf(element) != this) {
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
    if (&parent == &child || child.GetIsLayoutAttached()) {
        return InvalidState(
            "Layout child is already attached or self-referential");
    }
    if (child.LayoutParent() != &parent) {
        return InvalidState(
            "Layout attachment must match the visual tree parent");
    }

    // Queue all parent invalidation work before publishing the child state.
    Base::Result<void> invalidated = InvalidateMeasure(parent);
    if (!invalidated) return invalidated.GetStatus();

    AeroGuiInternal::Layout(child).layoutAttached = true;
    AeroGuiInternal::Layout(child).measureValid = false;
    AeroGuiInternal::Layout(child).arrangeValid = false;
    return {};
}

Base::Result<void> LayoutEngine::Detach(
    UIElement& parent,
    UIElement& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) return verified.GetStatus();
    if (!child.GetIsLayoutAttached() || child.LayoutParent() != &parent ||
        AeroGuiInternal::LayoutEngineOf(child) != this) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Layout parent-child relationship was not found");
    }

    Base::Result<void> invalidated = InvalidateMeasure(parent);
    if (!invalidated) return invalidated.GetStatus();

    RemoveQueued(child);
    AeroGuiInternal::Layout(child).layoutAttached = false;
    AeroGuiInternal::Layout(child).measureValid = false;
    AeroGuiInternal::Layout(child).arrangeValid = false;
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
        AeroGuiInternal::Layout(*root_).measureQueued = false;
        AeroGuiInternal::Layout(*root_).arrangeQueued = false;
    }
    root_ = root;
    rootAvailableSize_ = availableSize;
    if (root != nullptr) {
        Base::Result<void> invalidated = InvalidateMeasure(*root);
        if (!invalidated) return invalidated.GetStatus();
    }
    return {};
}

UIElement* LayoutEngine::ResolveQueued(VisualHandle handle) const noexcept {
    if (!handle.IsValid() || root_ == nullptr) return nullptr;
    ElementTree* tree = AeroGuiInternal::Tree(*root_);
    if (tree == nullptr) return nullptr;
    ::Aero::Media::Visual* visual = tree->ResolveHandle(handle);
    return visual != nullptr ? ::Aero::TryCast<UIElement>(visual) : nullptr;
}

Base::Result<VisualHandle> LayoutEngine::EnqueueHandle(
    UIElement& element) noexcept {
    const VisualHandle handle = AeroGuiInternal::Handle(element);
    if (!handle.IsValid()) {
        return InvalidState("Layout element has no ElementTree handle");
    }
    return handle;
}

Base::Result<void> LayoutEngine::QueueMeasure(
    UIElement& element) noexcept {
    if (element.GetIsMeasureQueued()) return {};
    Base::Result<VisualHandle> handle = EnqueueHandle(element);
    if (!handle) return handle.GetStatus();
    Base::Result<void> appended =
        measureQueue_.PushBack(handle.Value());
    if (!appended) return appended.GetStatus();
    AeroGuiInternal::Layout(element).measureQueued = true;
    return {};
}

Base::Result<void> LayoutEngine::QueueArrange(
    UIElement& element) noexcept {
    if (element.GetIsArrangeQueued()) return {};
    Base::Result<VisualHandle> handle = EnqueueHandle(element);
    if (!handle) return handle.GetStatus();
    Base::Result<void> appended =
        arrangeQueue_.PushBack(handle.Value());
    if (!appended) return appended.GetStatus();
    AeroGuiInternal::Layout(element).arrangeQueued = true;
    return {};
}

void LayoutEngine::RemoveQueued(UIElement& element) noexcept {
    const VisualHandle handle = AeroGuiInternal::Handle(element);
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
    AeroGuiInternal::Layout(element).measureQueued = false;
    AeroGuiInternal::Layout(element).arrangeQueued = false;
}

Base::Result<void> LayoutEngine::InvalidateMeasure(
    UIElement& element) noexcept {
    Base::Vector<UIElement*> path;
    UIElement* current = &element;
    while (current != nullptr) {
        Base::Result<void> verified = VerifyElement(*current);
        if (!verified) return verified.GetStatus();
        Base::Result<void> appended = path.PushBack(current);
        if (!appended) return appended.GetStatus();
        current = current->GetIsLayoutAttached()
            ? current->LayoutParent() : nullptr;
    }

    Base::Vector<VisualHandle> handles;
    Base::Result<void> reserved = handles.Reserve(path.Size());
    if (!reserved) return reserved.GetStatus();
    for (UIElement* item : path) {
        if (item->GetIsMeasureQueued()) continue;
        Base::Result<VisualHandle> handle = EnqueueHandle(*item);
        if (!handle) return handle.GetStatus();
        Base::Result<void> staged =
            handles.PushBack(handle.Value());
        if (!staged) return staged.GetStatus();
    }
    reserved = measureQueue_.Reserve(
        measureQueue_.Size() + handles.Size());
    if (!reserved) return reserved.GetStatus();

    std::uint32_t handleIndex = 0U;
    for (UIElement* item : path) {
        AeroGuiInternal::Layout(*item).measureValid = false;
        AeroGuiInternal::Layout(*item).arrangeValid = false;
        if (item->GetIsMeasureQueued()) continue;
        Base::Result<void> queued = measureQueue_.PushBack(
            handles[handleIndex++]);
        AERO_ASSERT(queued);
        (void)queued;
        AeroGuiInternal::Layout(*item).measureQueued = true;
    }
    return {};
}

Base::Result<void> LayoutEngine::InvalidateArrange(
    UIElement& element) noexcept {
    Base::Vector<UIElement*> path;
    UIElement* current = &element;
    while (current != nullptr) {
        Base::Result<void> verified = VerifyElement(*current);
        if (!verified) return verified.GetStatus();
        Base::Result<void> appended = path.PushBack(current);
        if (!appended) return appended.GetStatus();
        current = current->GetIsLayoutAttached()
            ? current->LayoutParent() : nullptr;
    }

    Base::Vector<VisualHandle> handles;
    Base::Result<void> reserved = handles.Reserve(path.Size());
    if (!reserved) return reserved.GetStatus();
    for (UIElement* item : path) {
        if (item->GetIsArrangeQueued()) continue;
        Base::Result<VisualHandle> handle = EnqueueHandle(*item);
        if (!handle) return handle.GetStatus();
        Base::Result<void> staged =
            handles.PushBack(handle.Value());
        if (!staged) return staged.GetStatus();
    }
    reserved = arrangeQueue_.Reserve(
        arrangeQueue_.Size() + handles.Size());
    if (!reserved) return reserved.GetStatus();

    std::uint32_t handleIndex = 0U;
    for (UIElement* item : path) {
        AeroGuiInternal::Layout(*item).arrangeValid = false;
        if (item->GetIsArrangeQueued()) continue;
        Base::Result<void> queued = arrangeQueue_.PushBack(
            handles[handleIndex++]);
        AERO_ASSERT(queued);
        (void)queued;
        AeroGuiInternal::Layout(*item).arrangeQueued = true;
    }
    return {};
}

Base::Result<void> LayoutEngine::MeasureElement(
    UIElement& element,
    Size constraint) noexcept {
    if (!IsValidLayoutSize(constraint)) {
        return InvalidArgument("Measure constraint must be finite and nonnegative");
    }
    if (element.GetIsMeasuring() || element.GetIsArranging()) {
        return InvalidState("Recursive layout operation is not allowed");
    }
    if (element.GetIsMeasureValid() && SameSize(element.GetPreviousMeasureConstraint(), constraint)) {
        return {};
    }

    VisualHandle pendingArrange{};
    const bool queueArrange = !element.GetIsArrangeQueued();
    if (queueArrange) {
        Base::Result<VisualHandle> handle = EnqueueHandle(element);
        if (!handle) return handle.GetStatus();
        pendingArrange = handle.Value();
        Base::Result<void> reserved = arrangeQueue_.Reserve(
            arrangeQueue_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }

    if (element.GetVisibility() == Visibility::Collapsed) {
        AeroGuiInternal::Layout(element).previousMeasureConstraint = constraint;
        AeroGuiInternal::Layout(element).desiredSize = {};
        AeroGuiInternal::Layout(element).untransformedDesiredSize = {};
        AeroGuiInternal::Layout(element).measureValid = true;
        AeroGuiInternal::Layout(element).arrangeValid = false;
        AeroGuiInternal::Layout(element).measureQueued = false;
        ++AeroGuiInternal::Layout(element).layoutRevision;
        ++measuredCount_;
        if (queueArrange) {
            Base::Result<void> queued = arrangeQueue_.PushBack(
                pendingArrange);
            AERO_ASSERT(queued);
            (void)queued;
            AeroGuiInternal::Layout(element).arrangeQueued = true;
        }
        return {};
    }

    const FrameworkElement* framework = ::Aero::TryCast<::Aero::FrameworkElement>(&(element));
    const Thickness margin = framework != nullptr
        ? framework->GetMargin() : Thickness{};
    const Size minimum = framework != nullptr
        ? framework->GetMinSize() : Size{};
    const Size maximum = framework != nullptr
        ? framework->GetMaxSize() : Size{1.0e12, 1.0e12};
    const bool hasWidth = framework != nullptr && framework->GetHasWidth();
    const bool hasHeight = framework != nullptr && framework->GetHasHeight();
    Size available = Deflate(constraint, margin);
    Base::Ref<Transform> layoutTransform =
        framework != nullptr
        ? framework->GetLayoutTransform()
        : Base::Ref<Transform>{};
    Base::Transform2D layoutMatrix;
    if (layoutTransform) {
        layoutMatrix = layoutTransform->GetMatrix();
        if (!Base::IsFiniteTransform(layoutMatrix)) {
            return InvalidArgument(
                "LayoutTransform produced an invalid matrix");
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

    AeroGuiInternal::Layout(element).measuring = true;
    const Size result = AeroGuiInternal::MeasureOverride(element, available);
    AeroGuiInternal::Layout(element).measuring = false;
    Size desired = result;
    if (!IsValidLayoutSize(desired)) {
        return InvalidArgument("MeasureOverride returned an invalid size");
    }
    desired = ClampSize(desired, minimum, maximum);
    if (hasWidth) desired.width = available.width;
    if (hasHeight) desired.height = available.height;
    AeroGuiInternal::Layout(element).untransformedDesiredSize = desired;
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
        return InvalidArgument("Layout constraints produced an invalid desired size");
    }
    desired.width = std::max(0.0, desired.width);
    desired.height = std::max(0.0, desired.height);
    if (framework != nullptr && framework->GetUseLayoutRounding()) {
        desired.width = RoundLayoutValue(desired.width, framework->GetDpiScale());
        desired.height = RoundLayoutValue(desired.height, framework->GetDpiScale());
    }
    AeroGuiInternal::Layout(element).previousMeasureConstraint = constraint;
    AeroGuiInternal::Layout(element).desiredSize = desired;
    AeroGuiInternal::Layout(element).measureValid = true;
    AeroGuiInternal::Layout(element).arrangeValid = false;
    AeroGuiInternal::Layout(element).measureQueued = false;
    ++AeroGuiInternal::Layout(element).layoutRevision;
    ++measuredCount_;
    if (queueArrange) {
        Base::Result<void> queued = arrangeQueue_.PushBack(
            pendingArrange);
        AERO_ASSERT(queued);
        (void)queued;
        AeroGuiInternal::Layout(element).arrangeQueued = true;
    }
    return {};
}

Base::Result<void> LayoutEngine::ArrangeElement(
    UIElement& element,
    Rect slot) noexcept {
    if (!IsValidLayoutRect(slot)) {
        return InvalidArgument("Arrange slot must be finite and nonnegative");
    }
    if (!element.GetIsMeasureValid()) {
        Base::Result<void> measured = MeasureElement(
            element, {slot.width, slot.height});
        if (!measured) {
            return measured;
        }
    }
    if (element.GetIsMeasuring() || element.GetIsArranging()) {
        return InvalidState("Recursive layout operation is not allowed");
    }
    if (element.GetVisibility() == Visibility::Collapsed) {
        AeroGuiInternal::Layout(element).layoutSlot = {slot.x, slot.y, 0.0, 0.0};
        AeroGuiInternal::Layout(element).renderSize = {};
        if (FrameworkElement* framework =
                ::Aero::TryCast<::Aero::FrameworkElement>(&(element))) {
            AeroGuiInternal::SetActualSize(
                *framework, 0.0, 0.0);
        }
        AeroGuiInternal::Layout(element).layoutClip = {slot.x, slot.y, 0.0, 0.0};
        AeroGuiInternal::Layout(element).arrangeValid = true;
        AeroGuiInternal::Layout(element).arrangeQueued = false;
        ++AeroGuiInternal::Layout(element).layoutRevision;
        ++arrangedCount_;
        return {};
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
    const bool hasWidth = framework != nullptr && framework->GetHasWidth();
    const bool hasHeight = framework != nullptr && framework->GetHasHeight();
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
            return InvalidArgument(
                "LayoutTransform produced an invalid matrix");
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

    AeroGuiInternal::Layout(element).arranging = true;
    const Size result = AeroGuiInternal::ArrangeOverride(element, finalSize);
    AeroGuiInternal::Layout(element).arranging = false;
    Size render = result;
    if (!IsValidLayoutSize(render)) {
        return InvalidArgument("ArrangeOverride returned an invalid size");
    }
    AeroGuiInternal::Layout(element).layoutSlot = contentSlot;
    AeroGuiInternal::Layout(element).renderSize = render;
    if (framework != nullptr) {
        AeroGuiInternal::SetActualSize(
            *framework, render.width, render.height);
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
    const Rect renderedSlot{
        contentSlot.x,
        contentSlot.y,
        renderedFootprint.width,
        renderedFootprint.height};
    AeroGuiInternal::Layout(element).layoutClip = element.GetClipToBounds()
        ? Intersect(contentSlot, renderedSlot)
        : renderedSlot;
    AeroGuiInternal::Layout(element).arrangeValid = true;
    AeroGuiInternal::Layout(element).arrangeQueued = false;
    ++AeroGuiInternal::Layout(element).layoutRevision;
    ++arrangedCount_;
    return {};
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
        Base::Result<void> measured =
            MeasureElement(*root_, rootAvailableSize_);
        if (!measured) {
            flushing_ = false;
            return measured.GetStatus();
        }
        Base::Result<void> arranged = ArrangeElement(
            *root_, {0.0, 0.0,
                     rootAvailableSize_.width, rootAvailableSize_.height});
        if (!arranged) {
            flushing_ = false;
            return arranged.GetStatus();
        }
    }

    Base::Vector<VisualHandle> measure =
        std::move(measureQueue_);
    measureQueue_ = Base::Vector<VisualHandle>();
    for (const VisualHandle handle : measure) {
        UIElement* element = ResolveQueued(handle);
        if (element != nullptr) AeroGuiInternal::Layout(*element).measureQueued = false;
    }
    for (const VisualHandle handle : measure) {
        UIElement* element = ResolveQueued(handle);
        if (element == nullptr || element == root_ ||
            AeroGuiInternal::LayoutEngineOf(*element) != this || element->GetIsMeasureValid()) {
            continue;
        }
        UIElement* parent = element->GetIsLayoutAttached()
            ? element->LayoutParent() : nullptr;
        const Size constraint = parent != nullptr
            ? parent->GetRenderSize() : rootAvailableSize_;
        Base::Result<void> measured =
            MeasureElement(*element, constraint);
        if (!measured) {
            (void)QueueMeasure(*element);
            flushing_ = false;
            return measured.GetStatus();
        }
    }

    Base::Vector<VisualHandle> arrange =
        std::move(arrangeQueue_);
    arrangeQueue_ = Base::Vector<VisualHandle>();
    for (const VisualHandle handle : arrange) {
        UIElement* element = ResolveQueued(handle);
        if (element != nullptr) AeroGuiInternal::Layout(*element).arrangeQueued = false;
    }
    for (const VisualHandle handle : arrange) {
        UIElement* element = ResolveQueued(handle);
        if (element == nullptr || element == root_ ||
            AeroGuiInternal::LayoutEngineOf(*element) != this || element->GetIsArrangeValid()) {
            continue;
        }
        Rect slot = element->GetLayoutSlot();
        if (slot.width == 0.0 && slot.height == 0.0) {
            slot.width = element->GetDesiredSize().width;
            slot.height = element->GetDesiredSize().height;
        }
        Base::Result<void> arranged = ArrangeElement(*element, slot);
        if (!arranged) {
            (void)QueueArrange(*element);
            flushing_ = false;
            return arranged.GetStatus();
        }
    }

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
        AeroGuiInternal::Layout(*root_).measureValid = false;
        AeroGuiInternal::Layout(*root_).arrangeValid = false;
        Base::Result<void> measured =
            MeasureElement(*root_, rootAvailableSize_);
        if (!measured) {
            flushing_ = false;
            return measured.GetStatus();
        }
        Base::Result<void> arranged = ArrangeElement(
            *root_, {0.0, 0.0,
                     rootAvailableSize_.width,
                     rootAvailableSize_.height});
        if (!arranged) {
            flushing_ = false;
            return arranged.GetStatus();
        }
    }
    if (root_ != nullptr && HasInvalidVisibleLayout(*root_)) {
        flushing_ = false;
        UIElement* invalid = FindInvalidVisibleLayout(*root_);
        const TypeInfo* type = invalid != nullptr
            ? invalid->PropertyRegistry().Types().FindType(
                  invalid->RuntimeType())
            : nullptr;
        const Base::StringView typeName = type != nullptr
            ? type->Name()
            : Base::StringView("<unknown>");
        UIElement* layoutParent = invalid != nullptr
            ? invalid->LayoutParent()
            : nullptr;
        const TypeInfo* parentType = layoutParent != nullptr
            ? layoutParent->PropertyRegistry().Types().FindType(
                  layoutParent->RuntimeType())
            : nullptr;
        const Base::StringView parentName = parentType != nullptr
            ? parentType->Name()
            : Base::StringView("<none>");
        thread_local char message[256];
        std::snprintf(
            message,
            sizeof(message),
            "Layout did not converge for visible '%.*s' (measure=%u arrange=%u parent='%.*s') after template application",
            static_cast<int>(typeName.SizeBytes()),
            typeName.Data(),
            invalid != nullptr && invalid->GetIsMeasureValid() ? 1U : 0U,
            invalid != nullptr && invalid->GetIsArrangeValid() ? 1U : 0U,
            static_cast<int>(parentName.SizeBytes()),
            parentName.Data());
        return InvalidState(message);
    }

    // A converged root has recursively measured and arranged every attached
    // descendant. Remove stale queue handles created during template
    // application so the next frame starts from a clean layout state.
    for (const VisualHandle handle : measureQueue_) {
        UIElement* element = ResolveQueued(handle);
        if (element != nullptr) AeroGuiInternal::Layout(*element).measureQueued = false;
    }
    for (const VisualHandle handle : arrangeQueue_) {
        UIElement* element = ResolveQueued(handle);
        if (element != nullptr) AeroGuiInternal::Layout(*element).arrangeQueued = false;
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

Size AeroGuiInternal::MeasureOverride(
    UIElement& element, Size availableSize) noexcept {
    const TypeId type = element.RuntimeType();
    if (type == Controls::StackPanel::StaticTypeId()) {
        return static_cast<Controls::StackPanel&>(element)
            .StackPanel::MeasureOverride(availableSize);
    }
    if (type == Controls::Grid::StaticTypeId()) {
        return static_cast<Controls::Grid&>(element)
            .Grid::MeasureOverride(availableSize);
    }
    if (type == Controls::Canvas::StaticTypeId()) {
        return static_cast<Controls::Canvas&>(element)
            .Canvas::MeasureOverride(availableSize);
    }
    return element.MeasureOverride(availableSize);
}

Size AeroGuiInternal::ArrangeOverride(
    UIElement& element, Size finalSize) noexcept {
    const TypeId type = element.RuntimeType();
    if (type == Controls::StackPanel::StaticTypeId()) {
        return static_cast<Controls::StackPanel&>(element)
            .StackPanel::ArrangeOverride(finalSize);
    }
    if (type == Controls::Grid::StaticTypeId()) {
        return static_cast<Controls::Grid&>(element)
            .Grid::ArrangeOverride(finalSize);
    }
    if (type == Controls::Canvas::StaticTypeId()) {
        return static_cast<Controls::Canvas&>(element)
            .Canvas::ArrangeOverride(finalSize);
    }
    return element.ArrangeOverride(finalSize);
}

} // namespace Aero
