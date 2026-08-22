#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp"
#include "gui/core/State.hpp"
#include "gui/core/State.hpp"
#include "gui/input/InputState.hpp"
#include "gui/core/State.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include <Aero/Layout.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Transforms.hpp>

#include <Aero/Base/Assert.hpp>
#include <Aero/FrameworkElement.hpp>

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

struct RoutedHandlerRecord {
    RoutedEventHandle event;
    Aero::RoutedHandlerStorage handler;
    std::uint64_t sequence = 0U;
    bool handledEventsToo = false;
};

struct UIElementHandlerState {
    Base::Vector<RoutedHandlerRecord> handlers;
    std::uint64_t nextSequence = 1U;
};


double AlignmentOffset(double available, double actual, bool center, bool end) noexcept {
    const double remaining = std::max(0.0, available - actual);
    return center ? remaining * 0.5 : (end ? remaining : 0.0);
}

Size NaturalConstraintForTransform(
    Size transformed,
    const Base::Transform2D& matrix) noexcept {
    Base::Transform2D inverse;
    if (!InvertTransform(matrix, inverse)) {
        return transformed;
    }
    const Rect bounds = TransformBounds(
        inverse,
        {0.0, 0.0,
         transformed.width,
         transformed.height});
    return {
        std::max(0.0, bounds.width),
        std::max(0.0, bounds.height)};
}

} // namespace

UIElement* UIElementChildRange::Iterator::operator*() const noexcept {
    ::Aero::Media::Visual* child = owner_ != nullptr ? ::Aero::Media::VisualTreeHelper::GetChild(*owner_, index_) : nullptr;
    return child != nullptr ? child->AsUIElement() : nullptr;
}

void UIElementChildRange::Iterator::Advance() noexcept {
    if (owner_ == nullptr) return;
    const std::uint32_t count = ::Aero::Media::VisualTreeHelper::GetChildrenCount(*owner_);
    while (index_ < count) {
        ::Aero::Media::Visual* child = ::Aero::Media::VisualTreeHelper::GetChild(*owner_, index_);
        if (child != nullptr && child->AsUIElement() != nullptr) return;
        ++index_;
    }
}

std::uint32_t UIElementChildRange::Size() const noexcept {
    std::uint32_t count = 0U;
    for (UIElement* child : *this) {
        (void)child;
        ++count;
    }
    return count;
}

UIElement* UIElementChildRange::operator[](std::uint32_t index) const noexcept {
    std::uint32_t current = 0U;
    for (UIElement* child : *this) {
        if (current++ == index) return child;
    }
    return nullptr;
}

FrameworkElement* FrameworkElementChildRange::Iterator::operator*() const noexcept {
    ::Aero::Media::Visual* child = owner_ != nullptr ? ::Aero::Media::VisualTreeHelper::GetChild(*owner_, index_) : nullptr;
    return child != nullptr ? child->AsFrameworkElement() : nullptr;
}

void FrameworkElementChildRange::Iterator::Advance() noexcept {
    if (owner_ == nullptr) return;
    const std::uint32_t count = ::Aero::Media::VisualTreeHelper::GetChildrenCount(*owner_);
    while (index_ < count) {
        ::Aero::Media::Visual* child = ::Aero::Media::VisualTreeHelper::GetChild(*owner_, index_);
        if (child != nullptr && child->AsFrameworkElement() != nullptr) return;
        ++index_;
    }
}

std::uint32_t FrameworkElementChildRange::Size() const noexcept {
    std::uint32_t count = 0U;
    for (FrameworkElement* child : *this) {
        (void)child;
        ++count;
    }
    return count;
}

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
    UIElement* element = visual.AsUIElement();
    if (element != nullptr &&
        element->GetIsVisible() &&
        (!UIElement::Access::MeasureValid(*element) ||
         !UIElement::Access::ArrangeValid(*element))) {
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

UIElement::UIElement(TypeId runtimeType) noexcept
    : ::Aero::Media::Visual(runtimeType) {}

UIElement::~UIElement() {
    AERO_ASSERT(Aero::UIElement::Access::LayoutManager(*this) == nullptr);
    AERO_ASSERT(!layoutAttached_);
    CleanupHandlers();
}


























} // namespace Aero

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero;

LayoutEngine::LayoutEngine(Dispatcher& dispatcher) noexcept
    : dispatcher_(&dispatcher),
      measureQueue_(),
      arrangeQueue_() {}

LayoutEngine::~LayoutEngine() noexcept {
    if (phaseHook_.IsValid() && dispatcher_->CheckAccess()) {
        (void)dispatcher_->RemoveFrameHook(phaseHook_);
    }
    root_ = nullptr;
}

Base::Result<void> LayoutEngine::Initialize() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (phaseHook_.IsValid()) {
        return {};
    }
    Base::Result<DispatcherFrameHookHandle> hook = dispatcher_->RegisterFrameHook(
        DispatcherFramePhase::Layout,
        &LayoutEngine::LayoutHook,
        this);
    if (!hook) {
        return hook.GetStatus();
    }
    phaseHook_ = hook.Value();
    return {};
}

Base::Result<void> LayoutEngine::VerifyElement(
    const UIElement& element) const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (!phaseHook_.IsValid()) {
        return InvalidState("LayoutEngine must be initialized before use");
    }
    if (&element.GetDispatcher() != dispatcher_) {
        return Base::Status::Failure(
            Base::ErrorCode::WrongThread,
            "Layout element belongs to another Dispatcher");
    }
    if (UIElement::Access::LayoutManager(element) != nullptr && UIElement::Access::LayoutManager(element) != this) {
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
    if (&parent == &child || UIElement::Access::LayoutAttached(child)) {
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

    UIElement::Access::LayoutAttached(child) = true;
    UIElement::Access::MeasureValid(child) = false;
    UIElement::Access::ArrangeValid(child) = false;
    return {};
}

Base::Result<void> LayoutEngine::Detach(
    UIElement& parent,
    UIElement& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) return verified.GetStatus();
    if (!UIElement::Access::LayoutAttached(child) || child.LayoutParent() != &parent ||
        UIElement::Access::LayoutManager(child) != this) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Layout parent-child relationship was not found");
    }

    Base::Result<void> invalidated = InvalidateMeasure(parent);
    if (!invalidated) return invalidated.GetStatus();

    RemoveQueued(child);
    UIElement::Access::LayoutAttached(child) = false;
    UIElement::Access::MeasureValid(child) = false;
    UIElement::Access::ArrangeValid(child) = false;
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
        if (UIElement::Access::LayoutAttached(*root) || root->GetVisualParent() != nullptr) {
            return InvalidState(
                "Layout root cannot have a visual or layout parent");
        }
        Base::Result<void> invalidated = InvalidateMeasure(*root);
        if (!invalidated) return invalidated.GetStatus();
    }

    if (root_ != nullptr && root_ != root) {
        RemoveQueued(*root_);
    }
    root_ = root;
    rootAvailableSize_ = availableSize;
    return {};
}

Base::Result<void> LayoutEngine::QueueMeasure(
    UIElement& element) noexcept {
    if (UIElement::Access::MeasureQueued(element)) return {};
    Base::Result<Aero::VisualLease> lease =
        Aero::VisualLease::Acquire(element);
    if (!lease) return lease.GetStatus();
    Base::Result<void> appended =
        measureQueue_.PushBack(std::move(lease).Value());
    if (!appended) return appended.GetStatus();
    UIElement::Access::MeasureQueued(element) = true;
    return {};
}

Base::Result<void> LayoutEngine::QueueArrange(
    UIElement& element) noexcept {
    if (UIElement::Access::ArrangeQueued(element)) return {};
    Base::Result<Aero::VisualLease> lease =
        Aero::VisualLease::Acquire(element);
    if (!lease) return lease.GetStatus();
    Base::Result<void> appended =
        arrangeQueue_.PushBack(std::move(lease).Value());
    if (!appended) return appended.GetStatus();
    UIElement::Access::ArrangeQueued(element) = true;
    return {};
}

void LayoutEngine::RemoveQueued(UIElement& element) noexcept {
    auto remove = [&](Base::Vector<Aero::VisualLease>& queue) noexcept {
        for (std::uint32_t index = 0U; index < queue.Size();) {
            if (queue[index].Resolve() != &element) {
                ++index;
                continue;
            }
            for (std::uint32_t next = index + 1U;
                 next < queue.Size(); ++next) {
                queue[next - 1U] = std::move(queue[next]);
            }
            queue.PopBack();
        }
    };
    remove(measureQueue_);
    remove(arrangeQueue_);
    UIElement::Access::MeasureQueued(element) = false;
    UIElement::Access::ArrangeQueued(element) = false;
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
        current = UIElement::Access::LayoutAttached(*current)
            ? current->LayoutParent() : nullptr;
    }

    Base::Vector<Aero::VisualLease> leases;
    Base::Result<void> reserved = leases.Reserve(path.Size());
    if (!reserved) return reserved.GetStatus();
    for (UIElement* item : path) {
        if (UIElement::Access::MeasureQueued(*item)) continue;
        Base::Result<Aero::VisualLease> lease =
            Aero::VisualLease::Acquire(*item);
        if (!lease) return lease.GetStatus();
        Base::Result<void> staged =
            leases.PushBack(std::move(lease).Value());
        if (!staged) return staged.GetStatus();
    }
    reserved = measureQueue_.Reserve(
        measureQueue_.Size() + leases.Size());
    if (!reserved) return reserved.GetStatus();

    std::uint32_t leaseIndex = 0U;
    for (UIElement* item : path) {
        UIElement::Access::MeasureValid(*item) = false;
        UIElement::Access::ArrangeValid(*item) = false;
        if (UIElement::Access::MeasureQueued(*item)) continue;
        Base::Result<void> queued = measureQueue_.PushBack(
            std::move(leases[leaseIndex++]));
        AERO_ASSERT(queued);
        (void)queued;
        UIElement::Access::MeasureQueued(*item) = true;
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
        current = UIElement::Access::LayoutAttached(*current)
            ? current->LayoutParent() : nullptr;
    }

    Base::Vector<Aero::VisualLease> leases;
    Base::Result<void> reserved = leases.Reserve(path.Size());
    if (!reserved) return reserved.GetStatus();
    for (UIElement* item : path) {
        if (UIElement::Access::ArrangeQueued(*item)) continue;
        Base::Result<Aero::VisualLease> lease =
            Aero::VisualLease::Acquire(*item);
        if (!lease) return lease.GetStatus();
        Base::Result<void> staged =
            leases.PushBack(std::move(lease).Value());
        if (!staged) return staged.GetStatus();
    }
    reserved = arrangeQueue_.Reserve(
        arrangeQueue_.Size() + leases.Size());
    if (!reserved) return reserved.GetStatus();

    std::uint32_t leaseIndex = 0U;
    for (UIElement* item : path) {
        UIElement::Access::ArrangeValid(*item) = false;
        if (UIElement::Access::ArrangeQueued(*item)) continue;
        Base::Result<void> queued = arrangeQueue_.PushBack(
            std::move(leases[leaseIndex++]));
        AERO_ASSERT(queued);
        (void)queued;
        UIElement::Access::ArrangeQueued(*item) = true;
    }
    return {};
}

Base::Result<void> LayoutEngine::MeasureElement(
    UIElement& element,
    Size constraint) noexcept {
    if (!IsValidLayoutSize(constraint)) {
        return InvalidArgument("Measure constraint must be finite and nonnegative");
    }
    if (UIElement::Access::Measuring(element) || UIElement::Access::Arranging(element)) {
        return InvalidState("Recursive layout operation is not allowed");
    }
    if (UIElement::Access::MeasureValid(element) && SameSize(UIElement::Access::PreviousMeasureConstraint(element), constraint)) {
        return {};
    }

    Aero::VisualLease pendingArrange;
    const bool queueArrange = !UIElement::Access::ArrangeQueued(element);
    if (queueArrange) {
        Base::Result<Aero::VisualLease> lease =
            Aero::VisualLease::Acquire(element);
        if (!lease) return lease.GetStatus();
        pendingArrange = std::move(lease).Value();
        Base::Result<void> reserved = arrangeQueue_.Reserve(
            arrangeQueue_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }

    if (element.GetVisibility() == Visibility::Collapsed) {
        UIElement::Access::PreviousMeasureConstraint(element) = constraint;
        UIElement::Access::DesiredSize(element) = {};
        UIElement::Access::UntransformedDesiredSize(element) = {};
        UIElement::Access::MeasureValid(element) = true;
        UIElement::Access::ArrangeValid(element) = false;
        UIElement::Access::MeasureQueued(element) = false;
        ++UIElement::Access::LayoutRevision(element);
        ++measuredCount_;
        if (queueArrange) {
            Base::Result<void> queued = arrangeQueue_.PushBack(
                std::move(pendingArrange));
            AERO_ASSERT(queued);
            (void)queued;
            UIElement::Access::ArrangeQueued(element) = true;
        }
        return {};
    }

    const FrameworkElement* framework = element.AsFrameworkElement();
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

    UIElement::Access::Measuring(element) = true;
    const Size result = UIElement::Access::MeasureOverride(element, available);
    UIElement::Access::Measuring(element) = false;
    Size desired = result;
    if (!IsValidLayoutSize(desired)) {
        return InvalidArgument("MeasureOverride returned an invalid size");
    }
    desired = ClampSize(desired, minimum, maximum);
    if (hasWidth) desired.width = available.width;
    if (hasHeight) desired.height = available.height;
    UIElement::Access::UntransformedDesiredSize(element) = desired;
    if (layoutTransform) {
        const Rect transformed =
            TransformBounds(
                layoutMatrix,
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
    UIElement::Access::PreviousMeasureConstraint(element) = constraint;
    UIElement::Access::DesiredSize(element) = desired;
    UIElement::Access::MeasureValid(element) = true;
    UIElement::Access::ArrangeValid(element) = false;
    UIElement::Access::MeasureQueued(element) = false;
    ++UIElement::Access::LayoutRevision(element);
    ++measuredCount_;
    if (queueArrange) {
        Base::Result<void> queued = arrangeQueue_.PushBack(
            std::move(pendingArrange));
        AERO_ASSERT(queued);
        (void)queued;
        UIElement::Access::ArrangeQueued(element) = true;
    }
    return {};
}

Base::Result<void> LayoutEngine::ArrangeElement(
    UIElement& element,
    Rect slot) noexcept {
    if (!IsValidLayoutRect(slot)) {
        return InvalidArgument("Arrange slot must be finite and nonnegative");
    }
    if (!UIElement::Access::MeasureValid(element)) {
        Base::Result<void> measured = MeasureElement(
            element, {slot.width, slot.height});
        if (!measured) {
            return measured;
        }
    }
    if (UIElement::Access::Measuring(element) || UIElement::Access::Arranging(element)) {
        return InvalidState("Recursive layout operation is not allowed");
    }
    if (element.GetVisibility() == Visibility::Collapsed) {
        UIElement::Access::LayoutSlot(element) = {slot.x, slot.y, 0.0, 0.0};
        UIElement::Access::RenderSize(element) = {};
        if (FrameworkElement* framework =
                element.AsFrameworkElement()) {
            UIElement::Access::SetActualSize(
                *framework, 0.0, 0.0);
        }
        UIElement::Access::LayoutClip(element) = {slot.x, slot.y, 0.0, 0.0};
        UIElement::Access::ArrangeValid(element) = true;
        UIElement::Access::ArrangeQueued(element) = false;
        ++UIElement::Access::LayoutRevision(element);
        ++arrangedCount_;
        return {};
    }
    FrameworkElement* framework =
        element.AsFrameworkElement();
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
        UIElement::Access::UntransformedDesiredSize(element);
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
            TransformBounds(
                layoutMatrix,
                {0.0, 0.0,
                 finalSize.width,
                 finalSize.height});
        layoutFootprint = {
            transformed.width,
            transformed.height};
    }
    Rect contentSlot{
        slot.x + margin.left,
        slot.y + margin.top,
        layoutFootprint.width,
        layoutFootprint.height};
    contentSlot.x += AlignmentOffset(contentAvailable.width, layoutFootprint.width,
        horizontal == HorizontalAlignment::Center ||
            horizontal == HorizontalAlignment::Stretch,
        horizontal == HorizontalAlignment::Right);
    contentSlot.y += AlignmentOffset(contentAvailable.height, layoutFootprint.height,
        vertical == VerticalAlignment::Center ||
            vertical == VerticalAlignment::Stretch,
        vertical == VerticalAlignment::Bottom);

    UIElement::Access::Arranging(element) = true;
    const Size result = UIElement::Access::ArrangeOverride(element, finalSize);
    UIElement::Access::Arranging(element) = false;
    Size render = result;
    if (!IsValidLayoutSize(render)) {
        return InvalidArgument("ArrangeOverride returned an invalid size");
    }
    UIElement::Access::LayoutSlot(element) = contentSlot;
    UIElement::Access::RenderSize(element) = render;
    if (framework != nullptr) {
        UIElement::Access::SetActualSize(
            *framework, render.width, render.height);
    }
    Size renderedFootprint = render;
    if (layoutTransform) {
        const Rect transformed =
            TransformBounds(
                layoutMatrix,
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
    UIElement::Access::LayoutClip(element) = element.GetClipToBounds()
        ? Intersect(contentSlot, renderedSlot)
        : renderedSlot;
    UIElement::Access::ArrangeValid(element) = true;
    UIElement::Access::ArrangeQueued(element) = false;
    ++UIElement::Access::LayoutRevision(element);
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
        (!UIElement::Access::MeasureValid(*root_) || !UIElement::Access::ArrangeValid(*root_))) {
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

    Base::Vector<Aero::VisualLease> measure =
        std::move(measureQueue_);
    measureQueue_ = Base::Vector<Aero::VisualLease>();
    for (const Aero::VisualLease& lease : measure) {
        ::Aero::Media::Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element != nullptr) UIElement::Access::MeasureQueued(*element) = false;
    }
    for (const Aero::VisualLease& lease : measure) {
        ::Aero::Media::Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element == nullptr || element == root_ ||
            UIElement::Access::LayoutManager(*element) != this || UIElement::Access::MeasureValid(*element)) {
            continue;
        }
        UIElement* parent = UIElement::Access::LayoutAttached(*element)
            ? element->LayoutParent() : nullptr;
        const Size constraint = parent != nullptr
            ? UIElement::Access::RenderSize(*parent) : rootAvailableSize_;
        Base::Result<void> measured =
            MeasureElement(*element, constraint);
        if (!measured) {
            (void)QueueMeasure(*element);
            flushing_ = false;
            return measured.GetStatus();
        }
    }

    Base::Vector<Aero::VisualLease> arrange =
        std::move(arrangeQueue_);
    arrangeQueue_ = Base::Vector<Aero::VisualLease>();
    for (const Aero::VisualLease& lease : arrange) {
        ::Aero::Media::Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element != nullptr) UIElement::Access::ArrangeQueued(*element) = false;
    }
    for (const Aero::VisualLease& lease : arrange) {
        ::Aero::Media::Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element == nullptr || element == root_ ||
            UIElement::Access::LayoutManager(*element) != this || UIElement::Access::ArrangeValid(*element)) {
            continue;
        }
        Rect slot = UIElement::Access::LayoutSlot(*element);
        if (slot.width == 0.0 && slot.height == 0.0) {
            slot.width = UIElement::Access::DesiredSize(*element).width;
            slot.height = UIElement::Access::DesiredSize(*element).height;
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
        UIElement::Access::MeasureValid(*root_) = false;
        UIElement::Access::ArrangeValid(*root_) = false;
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
            invalid != nullptr && UIElement::Access::MeasureValid(*invalid) ? 1U : 0U,
            invalid != nullptr && UIElement::Access::ArrangeValid(*invalid) ? 1U : 0U,
            static_cast<int>(parentName.SizeBytes()),
            parentName.Data());
        return InvalidState(message);
    }

    // A converged root has recursively measured and arranged every attached
    // descendant. Remove stale queue leases created during template
    // application so the next frame starts from a clean layout state.
    for (const Aero::VisualLease& lease : measureQueue_) {
        ::Aero::Media::Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element != nullptr) UIElement::Access::MeasureQueued(*element) = false;
    }
    for (const Aero::VisualLease& lease : arrangeQueue_) {
        ::Aero::Media::Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element != nullptr) UIElement::Access::ArrangeQueued(*element) = false;
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
