#include <Aero/Core/Layout.hpp>

#include <Aero/Base/Assert.hpp>

#include <algorithm>
#include <cmath>

namespace Aero::Core {
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

bool SameThickness(Thickness left, Thickness right) noexcept {
    return left.left == right.left && left.top == right.top &&
        left.right == right.right && left.bottom == right.bottom;
}

bool IsValidMargin(Thickness value) noexcept {
    return IsFinite(value) && value.left >= 0.0 && value.top >= 0.0 &&
        value.right >= 0.0 && value.bottom >= 0.0 &&
        std::isfinite(value.left + value.right) &&
        std::isfinite(value.top + value.bottom);
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

LayoutElement::LayoutElement(
    Dispatcher& dispatcher,
    DependencyPropertyRegistry& registry,
    TypeId runtimeType,
    Base::IAllocator* allocator) noexcept
    : TreeNode(dispatcher, registry, runtimeType, allocator),
      layoutChildren_(allocator) {}

LayoutElement::~LayoutElement() {
    AERO_ASSERT(manager_ == nullptr);
    AERO_ASSERT(layoutParent_ == nullptr);
    AERO_ASSERT(layoutChildren_.Empty());
}

Base::Result<void> LayoutElement::InvalidateMeasure() noexcept {
    if (manager_ == nullptr) {
        measureValid_ = false;
        arrangeValid_ = false;
        return {};
    }
    return manager_->InvalidateMeasure(*this);
}

Base::Result<void> LayoutElement::InvalidateArrange() noexcept {
    if (manager_ == nullptr) {
        arrangeValid_ = false;
        return {};
    }
    return manager_->InvalidateArrange(*this);
}

Base::Result<void> LayoutElement::SetLayoutRounding(
    bool enabled,
    double dpiScale) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access;
    }
    if (!std::isfinite(dpiScale) || dpiScale <= 0.0) {
        return InvalidArgument("Layout DPI scale must be finite and positive");
    }
    useLayoutRounding_ = enabled;
    dpiScale_ = dpiScale;
    return InvalidateMeasure();
}

Base::Result<void> LayoutElement::SetWidth(double value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (!std::isfinite(value) || value < 0.0) {
        return InvalidArgument("Width must be finite and nonnegative");
    }
    if (hasWidth_ && width_ == value) return {};
    width_ = value;
    hasWidth_ = true;
    return InvalidateMeasure();
}

Base::Result<void> LayoutElement::ClearWidth() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (!hasWidth_) return {};
    hasWidth_ = false;
    width_ = 0.0;
    return InvalidateMeasure();
}

Base::Result<void> LayoutElement::SetHeight(double value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (!std::isfinite(value) || value < 0.0) {
        return InvalidArgument("Height must be finite and nonnegative");
    }
    if (hasHeight_ && height_ == value) return {};
    height_ = value;
    hasHeight_ = true;
    return InvalidateMeasure();
}

Base::Result<void> LayoutElement::ClearHeight() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (!hasHeight_) return {};
    hasHeight_ = false;
    height_ = 0.0;
    return InvalidateMeasure();
}

Base::Result<void> LayoutElement::SetMinSize(Size value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (!IsValidLayoutSize(value) || value.width > maxSize_.width ||
        value.height > maxSize_.height) {
        return InvalidArgument("Minimum layout size is invalid");
    }
    if (SameSize(minSize_, value)) return {};
    minSize_ = value;
    return InvalidateMeasure();
}

Base::Result<void> LayoutElement::SetMaxSize(Size value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (!IsValidLayoutSize(value) || value.width < minSize_.width ||
        value.height < minSize_.height) {
        return InvalidArgument("Maximum layout size is invalid");
    }
    if (SameSize(maxSize_, value)) return {};
    maxSize_ = value;
    return InvalidateMeasure();
}

Base::Result<void> LayoutElement::SetMargin(Thickness value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (!IsValidMargin(value)) {
        return InvalidArgument("Margin must be finite, nonnegative, and non-overflowing");
    }
    if (SameThickness(margin_, value)) return {};
    margin_ = value;
    return InvalidateMeasure();
}

Base::Result<void> LayoutElement::SetHorizontalAlignment(
    HorizontalAlignment value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (value > HorizontalAlignment::Right) {
        return InvalidArgument("Horizontal alignment is invalid");
    }
    if (horizontalAlignment_ == value) return {};
    horizontalAlignment_ = value;
    return InvalidateArrange();
}

Base::Result<void> LayoutElement::SetVerticalAlignment(
    VerticalAlignment value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (value > VerticalAlignment::Bottom) {
        return InvalidArgument("Vertical alignment is invalid");
    }
    if (verticalAlignment_ == value) return {};
    verticalAlignment_ = value;
    return InvalidateArrange();
}

Base::Result<Size> LayoutElement::MeasureOverride(Size availableSize) noexcept {
    return availableSize;
}

Base::Result<Size> LayoutElement::ArrangeOverride(Size finalSize) noexcept {
    return finalSize;
}

Base::Result<void> LayoutElement::MeasureChild(
    LayoutElement& child,
    Size availableSize) noexcept {
    if (manager_ == nullptr || child.layoutParent_ != this) {
        return InvalidState("Layout child is not attached to this element");
    }
    return manager_->MeasureElement(child, availableSize);
}

Base::Result<void> LayoutElement::ArrangeChild(
    LayoutElement& child,
    Rect finalRect) noexcept {
    if (manager_ == nullptr || child.layoutParent_ != this) {
        return InvalidState("Layout child is not attached to this element");
    }
    return manager_->ArrangeElement(child, finalRect);
}

LayoutManager::LayoutManager(
    Dispatcher& dispatcher,
    Base::IAllocator* allocator) noexcept
    : dispatcher_(&dispatcher),
      allocator_(allocator != nullptr ? allocator : &dispatcher.Allocator()),
      measureQueue_(allocator_),
      arrangeQueue_(allocator_) {}

LayoutManager::~LayoutManager() noexcept {
    if (phaseHook_.IsValid() && dispatcher_->CheckAccess()) {
        (void)dispatcher_->RemoveFrameHook(phaseHook_);
    }
    if (root_ != nullptr) {
        root_->manager_ = nullptr;
        root_ = nullptr;
    }
}

Base::Result<void> LayoutManager::Initialize() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (phaseHook_.IsValid()) {
        return {};
    }
    Base::Result<DispatcherFrameHookHandle> hook = dispatcher_->RegisterFrameHook(
        DispatcherFramePhase::Layout,
        &LayoutManager::LayoutHook,
        this);
    if (!hook) {
        return hook.GetStatus();
    }
    phaseHook_ = hook.Value();
    return {};
}

Base::Result<void> LayoutManager::VerifyElement(
    const LayoutElement& element) const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (!phaseHook_.IsValid()) {
        return InvalidState("LayoutManager must be initialized before use");
    }
    if (&element.GetDispatcher() != dispatcher_) {
        return Base::Status::Failure(
            Base::ErrorCode::WrongThread,
            "Layout element belongs to another Dispatcher");
    }
    if (element.manager_ != nullptr && element.manager_ != this) {
        return InvalidState("Layout element belongs to another LayoutManager");
    }
    return {};
}

Base::Result<void> LayoutManager::Attach(
    LayoutElement& parent,
    LayoutElement& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) {
        return verified;
    }
    verified = VerifyElement(child);
    if (!verified) {
        return verified;
    }
    if (&parent == &child || child.layoutParent_ != nullptr) {
        return InvalidState("Layout child is already attached or self-referential");
    }
    if (child.VisualParent() != &parent) {
        return InvalidState("Layout attachment must match the visual tree parent");
    }
    for (LayoutElement* current = &parent; current != nullptr;
         current = current->layoutParent_) {
        if (current == &child) {
            return Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "Layout attachment would create a cycle");
        }
    }
    Base::Result<void> appended = parent.layoutChildren_.TryPushBack(&child);
    if (!appended) {
        return appended;
    }
    parent.manager_ = this;
    child.manager_ = this;
    child.layoutParent_ = &parent;
    return InvalidateMeasure(parent);
}

void LayoutManager::RemoveChild(
    Base::Vector<LayoutElement*>& children,
    LayoutElement& child) noexcept {
    for (std::uint32_t index = 0U; index < children.Size(); ++index) {
        if (children[index] == &child) {
            for (std::uint32_t current = index + 1U;
                 current < children.Size(); ++current) {
                children[current - 1U] = children[current];
            }
            children.PopBack();
            return;
        }
    }
}

Base::Result<void> LayoutManager::Detach(
    LayoutElement& parent,
    LayoutElement& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) {
        return verified;
    }
    if (child.layoutParent_ != &parent) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Layout parent-child relationship was not found");
    }
    RemoveChild(parent.layoutChildren_, child);
    child.layoutParent_ = nullptr;
    child.manager_ = nullptr;
    child.measureValid_ = false;
    child.arrangeValid_ = false;
    return InvalidateMeasure(parent);
}

Base::Result<void> LayoutManager::SetRoot(
    LayoutElement* root,
    Size availableSize) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (!IsValidLayoutSize(availableSize)) {
        return InvalidArgument("Root layout size must be finite and nonnegative");
    }
    if (root != nullptr) {
        Base::Result<void> verified = VerifyElement(*root);
        if (!verified) {
            return verified;
        }
        if (root->layoutParent_ != nullptr) {
            return InvalidState("Layout root cannot have a layout parent");
        }
    }
    if (root_ != nullptr && root_ != root) {
        root_->manager_ = nullptr;
    }
    root_ = root;
    rootAvailableSize_ = availableSize;
    if (root_ == nullptr) {
        return {};
    }
    root_->manager_ = this;
    return InvalidateMeasure(*root_);
}

Base::Result<void> LayoutManager::QueueMeasure(LayoutElement& element) noexcept {
    if (element.measureQueued_) {
        return {};
    }
    Base::Result<void> appended = measureQueue_.TryPushBack(&element);
    if (!appended) {
        return appended;
    }
    element.measureQueued_ = true;
    return {};
}

Base::Result<void> LayoutManager::QueueArrange(LayoutElement& element) noexcept {
    if (element.arrangeQueued_) {
        return {};
    }
    Base::Result<void> appended = arrangeQueue_.TryPushBack(&element);
    if (!appended) {
        return appended;
    }
    element.arrangeQueued_ = true;
    return {};
}

Base::Result<void> LayoutManager::InvalidateMeasure(LayoutElement& element) noexcept {
    Base::Result<void> verified = VerifyElement(element);
    if (!verified) {
        return verified;
    }
    element.measureValid_ = false;
    element.arrangeValid_ = false;
    Base::Result<void> queued = QueueMeasure(element);
    if (!queued) {
        return queued;
    }
    if (element.layoutParent_ != nullptr) {
        return InvalidateMeasure(*element.layoutParent_);
    }
    return {};
}

Base::Result<void> LayoutManager::InvalidateArrange(LayoutElement& element) noexcept {
    Base::Result<void> verified = VerifyElement(element);
    if (!verified) {
        return verified;
    }
    element.arrangeValid_ = false;
    Base::Result<void> queued = QueueArrange(element);
    if (!queued) {
        return queued;
    }
    if (element.layoutParent_ != nullptr) {
        return InvalidateArrange(*element.layoutParent_);
    }
    return {};
}

Base::Result<void> LayoutManager::MeasureElement(
    LayoutElement& element,
    Size constraint) noexcept {
    if (!IsValidLayoutSize(constraint)) {
        return InvalidArgument("Measure constraint must be finite and nonnegative");
    }
    if (element.measuring_ || element.arranging_) {
        return InvalidState("Recursive layout operation is not allowed");
    }
    if (element.measureValid_ && SameSize(element.previousMeasureConstraint_, constraint)) {
        return {};
    }
    Size available = Deflate(constraint, element.margin_);
    available = ClampSize(available, element.minSize_, element.maxSize_);
    if (element.hasWidth_) {
        available.width = ClampDimension(
            element.width_, element.minSize_.width, element.maxSize_.width);
    }
    if (element.hasHeight_) {
        available.height = ClampDimension(
            element.height_, element.minSize_.height, element.maxSize_.height);
    }

    element.measuring_ = true;
    Base::Result<Size> result = element.MeasureOverride(available);
    element.measuring_ = false;
    if (!result) {
        return result.GetStatus();
    }
    Size desired = result.Value();
    if (!IsValidLayoutSize(desired)) {
        return InvalidArgument("MeasureOverride returned an invalid size");
    }
    desired = ClampSize(desired, element.minSize_, element.maxSize_);
    if (element.hasWidth_) desired.width = available.width;
    if (element.hasHeight_) desired.height = available.height;
    desired = Inflate(desired, element.margin_);
    if (!IsValidLayoutSize(desired)) {
        return InvalidArgument("Layout constraints produced an invalid desired size");
    }
    if (element.useLayoutRounding_) {
        desired.width = RoundLayoutValue(desired.width, element.dpiScale_);
        desired.height = RoundLayoutValue(desired.height, element.dpiScale_);
    }
    element.previousMeasureConstraint_ = constraint;
    element.desiredSize_ = desired;
    element.measureValid_ = true;
    element.arrangeValid_ = false;
    element.measureQueued_ = false;
    ++element.layoutRevision_;
    ++measuredCount_;
    return QueueArrange(element);
}

Base::Result<void> LayoutManager::ArrangeElement(
    LayoutElement& element,
    Rect slot) noexcept {
    if (!IsValidLayoutRect(slot)) {
        return InvalidArgument("Arrange slot must be finite and nonnegative");
    }
    if (!element.measureValid_) {
        Base::Result<void> measured = MeasureElement(
            element, {slot.width, slot.height});
        if (!measured) {
            return measured;
        }
    }
    if (element.measuring_ || element.arranging_) {
        return InvalidState("Recursive layout operation is not allowed");
    }
    if (element.useLayoutRounding_) {
        slot.x = RoundLayoutValue(slot.x, element.dpiScale_);
        slot.y = RoundLayoutValue(slot.y, element.dpiScale_);
        slot.width = RoundLayoutValue(slot.width, element.dpiScale_);
        slot.height = RoundLayoutValue(slot.height, element.dpiScale_);
    }
    const Size contentAvailable = Deflate({slot.width, slot.height}, element.margin_);
    const Size desiredContent = Deflate(element.desiredSize_, element.margin_);
    const Size constrainedDesired = ClampSize(
        desiredContent, element.minSize_, element.maxSize_);

    Size finalSize;
    if (element.hasWidth_) {
        finalSize.width = ClampDimension(
            element.width_, element.minSize_.width, element.maxSize_.width);
    } else if (element.horizontalAlignment_ == HorizontalAlignment::Stretch) {
        finalSize.width = ClampDimension(
            contentAvailable.width, element.minSize_.width, element.maxSize_.width);
    } else {
        finalSize.width = ClampDimension(std::min(
            constrainedDesired.width, contentAvailable.width),
            element.minSize_.width, element.maxSize_.width);
    }
    if (element.hasHeight_) {
        finalSize.height = ClampDimension(
            element.height_, element.minSize_.height, element.maxSize_.height);
    } else if (element.verticalAlignment_ == VerticalAlignment::Stretch) {
        finalSize.height = ClampDimension(
            contentAvailable.height, element.minSize_.height, element.maxSize_.height);
    } else {
        finalSize.height = ClampDimension(std::min(
            constrainedDesired.height, contentAvailable.height),
            element.minSize_.height, element.maxSize_.height);
    }

    Rect contentSlot{slot.x + element.margin_.left, slot.y + element.margin_.top,
        finalSize.width, finalSize.height};
    contentSlot.x += AlignmentOffset(contentAvailable.width, finalSize.width,
        element.horizontalAlignment_ == HorizontalAlignment::Center,
        element.horizontalAlignment_ == HorizontalAlignment::Right);
    contentSlot.y += AlignmentOffset(contentAvailable.height, finalSize.height,
        element.verticalAlignment_ == VerticalAlignment::Center,
        element.verticalAlignment_ == VerticalAlignment::Bottom);

    element.arranging_ = true;
    Base::Result<Size> result = element.ArrangeOverride(finalSize);
    element.arranging_ = false;
    if (!result) {
        return result.GetStatus();
    }
    Size render = result.Value();
    if (!IsValidLayoutSize(render)) {
        return InvalidArgument("ArrangeOverride returned an invalid size");
    }
    element.layoutSlot_ = contentSlot;
    element.renderSize_ = render;
    element.layoutClip_ = element.clipToBounds_
        ? Intersect(contentSlot, {contentSlot.x, contentSlot.y, render.width, render.height})
        : Rect{contentSlot.x, contentSlot.y, render.width, render.height};
    element.arrangeValid_ = true;
    element.arrangeQueued_ = false;
    ++element.layoutRevision_;
    ++arrangedCount_;
    return {};
}

Base::Result<std::uint32_t> LayoutManager::Flush() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access.GetStatus();
    }
    if (flushing_) {
        return InvalidState("Nested layout flush is not allowed");
    }
    flushing_ = true;
    measuredCount_ = 0U;
    arrangedCount_ = 0U;

    if (root_ != nullptr && (!root_->measureValid_ || !root_->arrangeValid_)) {
        Base::Result<void> measured = MeasureElement(*root_, rootAvailableSize_);
        if (!measured) {
            flushing_ = false;
            return measured.GetStatus();
        }
        Base::Result<void> arranged = ArrangeElement(
            *root_, {0.0, 0.0, rootAvailableSize_.width, rootAvailableSize_.height});
        if (!arranged) {
            flushing_ = false;
            return arranged.GetStatus();
        }
    }

    for (LayoutElement* element : measureQueue_) {
        if (element != nullptr && element != root_ && !element->measureValid_) {
            const Size constraint = element->layoutParent_ != nullptr
                ? element->layoutParent_->renderSize_
                : rootAvailableSize_;
            Base::Result<void> measured = MeasureElement(*element, constraint);
            if (!measured) {
                flushing_ = false;
                return measured.GetStatus();
            }
        }
    }
    for (LayoutElement* element : arrangeQueue_) {
        if (element != nullptr && element != root_ && !element->arrangeValid_) {
            Rect slot = element->layoutSlot_;
            if (slot.width == 0.0 && slot.height == 0.0) {
                slot.width = element->desiredSize_.width;
                slot.height = element->desiredSize_.height;
            }
            Base::Result<void> arranged = ArrangeElement(*element, slot);
            if (!arranged) {
                flushing_ = false;
                return arranged.GetStatus();
            }
        }
    }

    measureQueue_.Clear();
    arrangeQueue_.Clear();
    ++passVersion_;
    flushing_ = false;
    return measuredCount_ + arrangedCount_;
}

LayoutDiagnostics LayoutManager::Diagnostics() const noexcept {
    LayoutDiagnostics diagnostics;
    diagnostics.passVersion = passVersion_;
    diagnostics.measuredCount = measuredCount_;
    diagnostics.arrangedCount = arrangedCount_;
    diagnostics.pendingMeasureCount = measureQueue_.Size();
    diagnostics.pendingArrangeCount = arrangeQueue_.Size();
    return diagnostics;
}

void LayoutManager::LayoutHook(void* context) noexcept {
    auto* manager = static_cast<LayoutManager*>(context);
    if (manager != nullptr) {
        (void)manager->Flush();
    }
}

} // namespace Aero::Core
