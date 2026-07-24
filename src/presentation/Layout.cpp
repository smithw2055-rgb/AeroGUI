#include <Aero/Presentation/Layout.hpp>

#include <Aero/Base/Assert.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Presentation/Rendering.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Aero::Presentation {

using namespace Aero::Core;
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

bool IsValidMargin(Thickness value) noexcept {
    return IsFinite(value) && value.left >= 0.0 && value.top >= 0.0 &&
        value.right >= 0.0 && value.bottom >= 0.0 &&
        std::isfinite(value.left + value.right) &&
        std::isfinite(value.top + value.bottom);
}

TypeId PresentationType(const char* name) noexcept {
    return MakeTypeId(
        Base::StringView(name, static_cast<std::uint32_t>(std::strlen(name))));
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

UIElement::UIElement(TypeId runtimeType) noexcept
    : Visual(runtimeType), handlers_() {}

UIElement::~UIElement() {
    AERO_ASSERT(manager_ == nullptr);
    AERO_ASSERT(!layoutAttached_);
    CleanupHandlers();
}

Base::Result<void> UIElement::TryAddHandler(
    RoutedEventHandle event,
    const Detail::RoutedHandlerStorage& handler,
    bool handledEventsToo) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    if (!event.IsValid() || handler.Empty()) {
        return InvalidArgument(
            "Routed event handler requires a valid event and callback");
    }
    if (nextHandlerSequence_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Routed event handler sequence space exhausted");
    }
    HandlerRecord record;
    record.event = event;
    record.handler = handler;
    record.sequence = nextHandlerSequence_;
    record.handledEventsToo = handledEventsToo;
    Base::Result<void> appended = handlers_.TryPushBack(record);
    if (!appended) return appended.GetStatus();
    ++nextHandlerSequence_;
    return {};
}

bool UIElement::RemoveHandler(
    RoutedEventHandle event,
    const Detail::RoutedHandlerStorage& handler) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access || !event.IsValid() || handler.Empty()) return false;
    for (std::uint32_t index = 0U; index < handlers_.Size(); ++index) {
        if (handlers_[index].event == event &&
            handlers_[index].handler.Equals(handler)) {
            for (std::uint32_t current = index + 1U;
                 current < handlers_.Size(); ++current) {
                handlers_[current - 1U] = std::move(handlers_[current]);
            }
            handlers_.PopBack();
            return true;
        }
    }
    return false;
}

void UIElement::CleanupHandlers() noexcept {
    handlers_.Clear();
}

Base::Result<void> UIElement::InvalidateMeasure() noexcept {
    if (manager_ == nullptr) {
        measureValid_ = false;
        arrangeValid_ = false;
        return {};
    }
    return manager_->InvalidateMeasure(*this);
}

Base::Result<void> UIElement::InvalidateArrange() noexcept {
    if (manager_ == nullptr) {
        arrangeValid_ = false;
        return {};
    }
    return manager_->InvalidateArrange(*this);
}

Base::Result<void> FrameworkElement::SetLayoutRounding(
    bool enabled,
    double dpiScale) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access;
    }
    if (!std::isfinite(dpiScale) || dpiScale <= 0.0) {
        return InvalidArgument("Layout DPI scale must be finite and positive");
    }
    const bool scaleChanged = dpiScale_ != dpiScale;
    dpiScale_ = dpiScale;
    Base::Result<void> result = SetValue(UseLayoutRoundingProperty,
        Value::FromBoolean(PresentationType("Boolean"), enabled));
    if (!result) return result;
    return scaleChanged && enabled ? InvalidateMeasure() : Base::Result<void>();
}

bool UIElement::ClipToBounds() const noexcept {
    Base::Result<Value> value = GetValue(ClipToBoundsProperty);
    return value ? value.Value().AsBoolean() : false;
}
bool UIElement::IsHitTestVisible() const noexcept {
    Base::Result<Value> value = GetValue(IsHitTestVisibleProperty);
    return value ? value.Value().AsBoolean() : true;
}
bool FrameworkElement::UseLayoutRounding() const noexcept {
    Base::Result<Value> value = GetValue(UseLayoutRoundingProperty);
    return value ? value.Value().AsBoolean() : false;
}
bool FrameworkElement::HasWidth() const noexcept {
    Base::Result<Value> value = GetValue(WidthProperty);
    return value && !static_cast<const Length*>(value.Value().AsCustom())->isAuto;
}
bool FrameworkElement::HasHeight() const noexcept {
    Base::Result<Value> value = GetValue(HeightProperty);
    return value && !static_cast<const Length*>(value.Value().AsCustom())->isAuto;
}
double FrameworkElement::Width() const noexcept {
    Base::Result<Value> value = GetValue(WidthProperty);
    if (!value) return 0.0;
    const Length& length = *static_cast<const Length*>(value.Value().AsCustom());
    return length.isAuto ? 0.0 : length.value;
}
double FrameworkElement::Height() const noexcept {
    Base::Result<Value> value = GetValue(HeightProperty);
    if (!value) return 0.0;
    const Length& length = *static_cast<const Length*>(value.Value().AsCustom());
    return length.isAuto ? 0.0 : length.value;
}
Size FrameworkElement::MinSize() const noexcept {
    Base::Result<Value> width = GetValue(MinWidthProperty);
    Base::Result<Value> height = GetValue(MinHeightProperty);
    return {width ? width.Value().AsDouble() : 0.0,
        height ? height.Value().AsDouble() : 0.0};
}
Size FrameworkElement::MaxSize() const noexcept {
    Base::Result<Value> width = GetValue(MaxWidthProperty);
    Base::Result<Value> height = GetValue(MaxHeightProperty);
    return {width ? width.Value().AsDouble() : 1.0e12,
        height ? height.Value().AsDouble() : 1.0e12};
}
Thickness FrameworkElement::Margin() const noexcept {
    Base::Result<Value> value = GetValue(MarginProperty);
    return value ? *static_cast<const Thickness*>(value.Value().AsCustom()) : Thickness{};
}
HorizontalAlignment FrameworkElement::GetHorizontalAlignment() const noexcept {
    Base::Result<Value> value = GetValue(HorizontalAlignmentProperty);
    return value ? static_cast<HorizontalAlignment>(value.Value().AsUnsignedInteger())
                 : HorizontalAlignment::Stretch;
}
VerticalAlignment FrameworkElement::GetVerticalAlignment() const noexcept {
    Base::Result<Value> value = GetValue(VerticalAlignmentProperty);
    return value ? static_cast<VerticalAlignment>(value.Value().AsUnsignedInteger())
                 : VerticalAlignment::Stretch;
}

Base::Result<void> UIElement::OnPropertyInvalidated(
    PropertyInvalidationFlags flags) noexcept {
    if (HasFlag(flags, PropertyInvalidationFlags::Measure)) {
        Base::Result<void> result = InvalidateMeasure();
        if (!result) return result;
    } else if (HasFlag(flags, PropertyInvalidationFlags::Arrange)) {
        Base::Result<void> result = InvalidateArrange();
        if (!result) return result;
    }
    UIElement* parent = layoutAttached_ ? LayoutParent() : nullptr;
    if (parent != nullptr &&
        HasFlag(flags, PropertyInvalidationFlags::ParentMeasure)) {
        Base::Result<void> result = parent->InvalidateMeasure();
        if (!result) return result;
    } else if (parent != nullptr &&
        HasFlag(flags, PropertyInvalidationFlags::ParentArrange)) {
        Base::Result<void> result = parent->InvalidateArrange();
        if (!result) return result;
    }
    return DependencyObject::OnPropertyInvalidated(flags);
}

Base::Result<void> UIElement::SetClipToBounds(bool value) noexcept {
    return SetValue(ClipToBoundsProperty,
        Value::FromBoolean(PresentationType("Boolean"), value));
}

Base::Result<void> UIElement::SetHitTestVisible(bool value) noexcept {
    return SetValue(IsHitTestVisibleProperty,
        Value::FromBoolean(PresentationType("Boolean"), value));
}

Base::Result<void> FrameworkElement::SetWidth(double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) {
        return InvalidArgument("Width must be finite and nonnegative");
    }
    const Length length = Length::Pixels(value);
    Base::Result<Value> stored = TryCreateRuntimeValue(
        PresentationType("Length"), &length);
    return stored ? SetValue(WidthProperty, stored.Value()) : stored.GetStatus();
}

Base::Result<void> FrameworkElement::ClearWidth() noexcept {
    return ClearValue(WidthProperty);
}

Base::Result<void> FrameworkElement::SetHeight(double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) {
        return InvalidArgument("Height must be finite and nonnegative");
    }
    const Length length = Length::Pixels(value);
    Base::Result<Value> stored = TryCreateRuntimeValue(
        PresentationType("Length"), &length);
    return stored ? SetValue(HeightProperty, stored.Value()) : stored.GetStatus();
}

Base::Result<void> FrameworkElement::ClearHeight() noexcept {
    return ClearValue(HeightProperty);
}

Base::Result<void> FrameworkElement::SetMinSize(Size value) noexcept {
    const Size maximum = MaxSize();
    if (!IsValidLayoutSize(value) || value.width > maximum.width ||
        value.height > maximum.height) {
        return InvalidArgument("Minimum layout size is invalid");
    }
    Base::Result<void> width = SetValue(MinWidthProperty,
        Value::FromDouble(PresentationType("Double"), value.width));
    return width ? SetValue(MinHeightProperty,
        Value::FromDouble(PresentationType("Double"), value.height)) : width;
}

Base::Result<void> FrameworkElement::SetMaxSize(Size value) noexcept {
    const Size minimum = MinSize();
    if (!IsValidLayoutSize(value) || value.width < minimum.width ||
        value.height < minimum.height) {
        return InvalidArgument("Maximum layout size is invalid");
    }
    Base::Result<void> width = SetValue(MaxWidthProperty,
        Value::FromDouble(PresentationType("Double"), value.width));
    return width ? SetValue(MaxHeightProperty,
        Value::FromDouble(PresentationType("Double"), value.height)) : width;
}

Base::Result<void> FrameworkElement::SetMargin(Thickness value) noexcept {
    if (!IsValidMargin(value)) {
        return InvalidArgument("Margin must be finite, nonnegative, and non-overflowing");
    }
    Base::Result<Value> stored = TryCreateRuntimeValue(
        PresentationType("Thickness"), &value);
    return stored ? SetValue(MarginProperty, stored.Value()) : stored.GetStatus();
}

Base::Result<void> FrameworkElement::SetHorizontalAlignment(
    HorizontalAlignment value) noexcept {
    if (value > HorizontalAlignment::Right) {
        return InvalidArgument("Horizontal alignment is invalid");
    }
    return SetValue(HorizontalAlignmentProperty, Value::FromUnsignedInteger(
        PresentationType("HorizontalAlignment"), static_cast<std::uint64_t>(value)));
}

Base::Result<void> FrameworkElement::SetVerticalAlignment(
    VerticalAlignment value) noexcept {
    if (value > VerticalAlignment::Bottom) {
        return InvalidArgument("Vertical alignment is invalid");
    }
    return SetValue(VerticalAlignmentProperty, Value::FromUnsignedInteger(
        PresentationType("VerticalAlignment"), static_cast<std::uint64_t>(value)));
}

Base::Result<Size> UIElement::MeasureOverride(Size availableSize) noexcept {
    return availableSize;
}

Base::Result<Size> UIElement::ArrangeOverride(Size finalSize) noexcept {
    return finalSize;
}

Base::Result<void> UIElement::MeasureChild(
    UIElement& child,
    Size availableSize) noexcept {
    if (manager_ == nullptr || !child.layoutAttached_ ||
        child.LayoutParent() != this) {
        return InvalidState("Layout child is not attached to this element");
    }
    return manager_->MeasureElement(child, availableSize);
}

Base::Result<void> UIElement::ArrangeChild(
    UIElement& child,
    Rect finalRect) noexcept {
    if (manager_ == nullptr || !child.layoutAttached_ ||
        child.LayoutParent() != this) {
        return InvalidState("Layout child is not attached to this element");
    }
    return manager_->ArrangeElement(child, finalRect);
}

LayoutManager::LayoutManager(Dispatcher& dispatcher) noexcept
    : dispatcher_(&dispatcher),
      measureQueue_(),
      arrangeQueue_() {}

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
    const UIElement& element) const noexcept {
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
    UIElement& parent,
    UIElement& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) {
        return verified;
    }
    verified = VerifyElement(child);
    if (!verified) {
        return verified;
    }
    if (&parent == &child || child.layoutAttached_) {
        return InvalidState("Layout child is already attached or self-referential");
    }
    if (child.LayoutParent() != &parent) {
        return InvalidState("Layout attachment must match the visual tree parent");
    }
    parent.manager_ = this;
    child.manager_ = this;
    child.layoutAttached_ = true;
    return InvalidateMeasure(parent);
}

Base::Result<void> LayoutManager::Detach(
    UIElement& parent,
    UIElement& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) {
        return verified;
    }
    if (!child.layoutAttached_ || child.LayoutParent() != &parent ||
        child.manager_ != this) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Layout parent-child relationship was not found");
    }
    child.layoutAttached_ = false;
    child.manager_ = nullptr;
    child.measureValid_ = false;
    child.arrangeValid_ = false;
    return InvalidateMeasure(parent);
}

Base::Result<void> LayoutManager::SetRoot(
    UIElement* root,
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
        if (root->layoutAttached_ || root->VisualParent() != nullptr) {
            return InvalidState("Layout root cannot have a visual or layout parent");
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

Base::Result<void> LayoutManager::QueueMeasure(UIElement& element) noexcept {
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

Base::Result<void> LayoutManager::QueueArrange(UIElement& element) noexcept {
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

Base::Result<void> LayoutManager::InvalidateMeasure(UIElement& element) noexcept {
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
    UIElement* parent = element.layoutAttached_ ? element.LayoutParent() : nullptr;
    if (parent != nullptr) {
        return InvalidateMeasure(*parent);
    }
    return {};
}

Base::Result<void> LayoutManager::InvalidateArrange(UIElement& element) noexcept {
    Base::Result<void> verified = VerifyElement(element);
    if (!verified) {
        return verified;
    }
    element.arrangeValid_ = false;
    Base::Result<void> queued = QueueArrange(element);
    if (!queued) {
        return queued;
    }
    UIElement* parent = element.layoutAttached_ ? element.LayoutParent() : nullptr;
    if (parent != nullptr) {
        return InvalidateArrange(*parent);
    }
    return {};
}

Base::Result<void> LayoutManager::MeasureElement(
    UIElement& element,
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
    const FrameworkElement* framework = element.AsFrameworkElement();
    const Thickness margin = framework != nullptr
        ? framework->Margin() : Thickness{};
    const Size minimum = framework != nullptr
        ? framework->MinSize() : Size{};
    const Size maximum = framework != nullptr
        ? framework->MaxSize() : Size{1.0e12, 1.0e12};
    const bool hasWidth = framework != nullptr && framework->HasWidth();
    const bool hasHeight = framework != nullptr && framework->HasHeight();
    Size available = Deflate(constraint, margin);
    available = ClampSize(available, minimum, maximum);
    if (hasWidth) {
        available.width = ClampDimension(
            framework->Width(), minimum.width, maximum.width);
    }
    if (hasHeight) {
        available.height = ClampDimension(
            framework->Height(), minimum.height, maximum.height);
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
    desired = ClampSize(desired, minimum, maximum);
    if (hasWidth) desired.width = available.width;
    if (hasHeight) desired.height = available.height;
    desired = Inflate(desired, margin);
    if (!IsValidLayoutSize(desired)) {
        return InvalidArgument("Layout constraints produced an invalid desired size");
    }
    if (framework != nullptr && framework->UseLayoutRounding()) {
        desired.width = RoundLayoutValue(desired.width, framework->DpiScale());
        desired.height = RoundLayoutValue(desired.height, framework->DpiScale());
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
    UIElement& element,
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
    const FrameworkElement* framework = element.AsFrameworkElement();
    if (framework != nullptr && framework->UseLayoutRounding()) {
        slot.x = RoundLayoutValue(slot.x, framework->DpiScale());
        slot.y = RoundLayoutValue(slot.y, framework->DpiScale());
        slot.width = RoundLayoutValue(slot.width, framework->DpiScale());
        slot.height = RoundLayoutValue(slot.height, framework->DpiScale());
    }
    const Thickness margin = framework != nullptr
        ? framework->Margin() : Thickness{};
    const Size minimum = framework != nullptr
        ? framework->MinSize() : Size{};
    const Size maximum = framework != nullptr
        ? framework->MaxSize() : Size{1.0e12, 1.0e12};
    const bool hasWidth = framework != nullptr && framework->HasWidth();
    const bool hasHeight = framework != nullptr && framework->HasHeight();
    const HorizontalAlignment horizontal = framework != nullptr
        ? framework->GetHorizontalAlignment() : HorizontalAlignment::Stretch;
    const VerticalAlignment vertical = framework != nullptr
        ? framework->GetVerticalAlignment() : VerticalAlignment::Stretch;
    const Size contentAvailable = Deflate({slot.width, slot.height}, margin);
    const Size desiredContent = Deflate(element.desiredSize_, margin);
    const Size constrainedDesired = ClampSize(
        desiredContent, minimum, maximum);

    Size finalSize;
    if (hasWidth) {
        finalSize.width = ClampDimension(
            framework->Width(), minimum.width, maximum.width);
    } else if (horizontal == HorizontalAlignment::Stretch) {
        finalSize.width = ClampDimension(
            contentAvailable.width, minimum.width, maximum.width);
    } else {
        finalSize.width = ClampDimension(std::min(
            constrainedDesired.width, contentAvailable.width),
            minimum.width, maximum.width);
    }
    if (hasHeight) {
        finalSize.height = ClampDimension(
            framework->Height(), minimum.height, maximum.height);
    } else if (vertical == VerticalAlignment::Stretch) {
        finalSize.height = ClampDimension(
            contentAvailable.height, minimum.height, maximum.height);
    } else {
        finalSize.height = ClampDimension(std::min(
            constrainedDesired.height, contentAvailable.height),
            minimum.height, maximum.height);
    }

    Rect contentSlot{slot.x + margin.left, slot.y + margin.top,
        finalSize.width, finalSize.height};
    contentSlot.x += AlignmentOffset(contentAvailable.width, finalSize.width,
        horizontal == HorizontalAlignment::Center,
        horizontal == HorizontalAlignment::Right);
    contentSlot.y += AlignmentOffset(contentAvailable.height, finalSize.height,
        vertical == VerticalAlignment::Center,
        vertical == VerticalAlignment::Bottom);

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
    element.layoutClip_ = element.ClipToBounds()
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

    for (UIElement* element : measureQueue_) {
        if (element != nullptr && element != root_ && !element->measureValid_) {
            UIElement* parent = element->layoutAttached_
                ? element->LayoutParent() : nullptr;
            const Size constraint = parent != nullptr
                ? parent->renderSize_ : rootAvailableSize_;
            Base::Result<void> measured = MeasureElement(*element, constraint);
            if (!measured) {
                flushing_ = false;
                return measured.GetStatus();
            }
        }
    }
    for (UIElement* element : arrangeQueue_) {
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

} // namespace Aero::Presentation
