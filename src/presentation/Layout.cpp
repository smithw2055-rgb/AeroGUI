#include <Aero/Presentation/Layout.hpp>
#include <Aero/Presentation/Effects.hpp>
#include <Aero/Presentation/Transforms.hpp>

#include <Aero/Base/Assert.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Presentation/Rendering.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include "RuntimeManagers.hpp"

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
    Base::Transform2D inverse;
    if (!TryInvertTransform(matrix, inverse)) {
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
    Base::Result<void> result =
        SetValue(UseLayoutRoundingProperty, enabled);
    if (!result) return result;
    return scaleChanged && enabled ? InvalidateMeasure() : Base::Result<void>();
}

bool UIElement::ClipToBounds() const noexcept {
    return GetValueOr(ClipToBoundsProperty, false);
}
BlendMode UIElement::GetBlendMode() const noexcept {
    return GetValueOr(
        BlendModeProperty, BlendMode::Normal);
}

Base::Ref<Effect> UIElement::GetEffect() const noexcept {
    return GetValueOr(
        EffectProperty,
        Base::Ref<Effect>{});
}
bool UIElement::IsHitTestVisible() const noexcept {
    return GetValueOr(IsHitTestVisibleProperty, true);
}
Visibility UIElement::GetVisibility() const noexcept {
    return GetValueOr(
        VisibilityProperty, Visibility::Visible);
}
bool UIElement::IsEnabled() const noexcept {
    if (!GetValueOr(IsEnabledProperty, true)) return false;
    Visual* parent = LogicalParent() != nullptr
        ? LogicalParent() : VisualParent();
    const UIElement* parentElement =
        parent != nullptr ? parent->AsUIElement() : nullptr;
    return parentElement == nullptr || parentElement->IsEnabled();
}
bool UIElement::AllowDrop() const noexcept {
    return GetValueOr(AllowDropProperty, false);
}
bool UIElement::IsMouseOver() const noexcept {
    return GetValueOr(IsMouseOverProperty, false);
}
bool UIElement::IsPressed() const noexcept {
    return GetValueOr(IsPressedProperty, false);
}
bool UIElement::IsKeyboardFocused() const noexcept {
    return GetValueOr(IsKeyboardFocusedProperty, false);
}
bool UIElement::IsKeyboardFocusWithin() const noexcept {
    return GetValueOr(IsKeyboardFocusWithinProperty, false);
}
bool UIElement::Focusable() const noexcept {
    return GetValueOr(FocusableProperty, false);
}
bool UIElement::IsTabStop() const noexcept {
    return GetValueOr(IsTabStopProperty, false);
}
std::uint32_t UIElement::TabIndex() const noexcept {
    return GetValueOr(TabIndexProperty, 0U);
}
bool UIElement::IsFocusScope() const noexcept {
    return GetValueOr(IsFocusScopeProperty, false);
}
bool FrameworkElement::UseLayoutRounding() const noexcept {
    return GetValueOr(UseLayoutRoundingProperty, false);
}
bool FrameworkElement::HasWidth() const noexcept {
    return !GetValueOr(
        WidthProperty, Length::Auto()).isAuto;
}
bool FrameworkElement::HasHeight() const noexcept {
    return !GetValueOr(
        HeightProperty, Length::Auto()).isAuto;
}
double FrameworkElement::Width() const noexcept {
    const Length length =
        GetValueOr(WidthProperty, Length::Auto());
    return length.isAuto ? 0.0 : length.value;
}
double FrameworkElement::Height() const noexcept {
    const Length length =
        GetValueOr(HeightProperty, Length::Auto());
    return length.isAuto ? 0.0 : length.value;
}
Size FrameworkElement::MinSize() const noexcept {
    return {
        GetValueOr(MinWidthProperty, 0.0),
        GetValueOr(MinHeightProperty, 0.0)};
}
Size FrameworkElement::MaxSize() const noexcept {
    return {
        GetValueOr(MaxWidthProperty, 1.0e12),
        GetValueOr(MaxHeightProperty, 1.0e12)};
}
Thickness FrameworkElement::Margin() const noexcept {
    return GetValueOr(MarginProperty, Thickness{});
}
HorizontalAlignment FrameworkElement::GetHorizontalAlignment() const noexcept {
    return GetValueOr(
        HorizontalAlignmentProperty,
        HorizontalAlignment::Stretch);
}
VerticalAlignment FrameworkElement::GetVerticalAlignment() const noexcept {
    return GetValueOr(
        VerticalAlignmentProperty,
        VerticalAlignment::Stretch);
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
    return SetValue(ClipToBoundsProperty, value);
}

Base::Result<void> UIElement::SetBlendMode(
    BlendMode value) noexcept {
    return SetValue(BlendModeProperty, value);
}

Base::Result<void> UIElement::SetEffect(
    Base::Ref<Effect> value) noexcept {
    return SetValue(
        EffectProperty, std::move(value));
}

Base::Result<void> UIElement::SetHitTestVisible(bool value) noexcept {
    return SetValue(IsHitTestVisibleProperty, value);
}
Base::Result<void> UIElement::SetVisibility(
    Visibility value) noexcept {
    return SetValue(VisibilityProperty, value);
}
Base::Result<void> UIElement::SetEnabled(bool value) noexcept {
    return SetValue(IsEnabledProperty, value);
}
Base::Result<void> UIElement::SetTabStop(bool value) noexcept {
    return SetValue(IsTabStopProperty, value);
}
Base::Result<void> UIElement::SetTabIndex(std::uint32_t value) noexcept {
    return SetValue(TabIndexProperty, value);
}
Base::Result<void> UIElement::SetFocusScope(bool value) noexcept {
    return SetValue(IsFocusScopeProperty, value);
}
Base::Ref<Transform> UIElement::RenderTransform() const noexcept {
    Base::Result<Base::Ref<Transform>> value =
        GetValue(RenderTransformProperty);
    return value ? std::move(value).Value() : Base::Ref<Transform>{};
}
Point UIElement::RenderTransformOrigin() const noexcept {
    return GetValueOr(RenderTransformOriginProperty, Point{});
}
Base::Result<void> UIElement::SetRenderTransform(
    Base::Ref<Transform> value) noexcept {
    return SetValue(RenderTransformProperty, std::move(value));
}
Base::Result<void> UIElement::SetRenderTransformOrigin(
    Point value) noexcept {
    return SetValue(RenderTransformOriginProperty, value);
}
Base::Result<void> UIElement::SetMouseOverState(bool value) noexcept {
    return SetReadOnlyCurrentValue(IsMouseOverProperty, value);
}
Base::Result<void> UIElement::SetPressedState(bool value) noexcept {
    return SetReadOnlyCurrentValue(IsPressedProperty, value);
}
Base::Result<void> UIElement::SetKeyboardFocusedState(bool value) noexcept {
    return SetReadOnlyCurrentValue(
        IsKeyboardFocusedProperty, value);
}
Base::Result<void> UIElement::SetKeyboardFocusWithinState(
    bool value) noexcept {
    return SetReadOnlyCurrentValue(
        IsKeyboardFocusWithinProperty, value);
}

Base::Result<void> FrameworkElement::SetWidth(double value) noexcept {
    return SetValue(WidthProperty, Length::Pixels(value));
}

Base::Result<void> FrameworkElement::ClearWidth() noexcept {
    return ClearValue(WidthProperty);
}

Base::Result<void> FrameworkElement::SetHeight(double value) noexcept {
    return SetValue(HeightProperty, Length::Pixels(value));
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
    Base::Result<void> width =
        SetValue(MinWidthProperty, value.width);
    return width
        ? SetValue(MinHeightProperty, value.height)
        : width;
}

Base::Result<void> FrameworkElement::SetMaxSize(Size value) noexcept {
    const Size minimum = MinSize();
    if (!IsValidLayoutSize(value) || value.width < minimum.width ||
        value.height < minimum.height) {
        return InvalidArgument("Maximum layout size is invalid");
    }
    Base::Result<void> width =
        SetValue(MaxWidthProperty, value.width);
    return width
        ? SetValue(MaxHeightProperty, value.height)
        : width;
}

Base::Result<void> FrameworkElement::SetMargin(Thickness value) noexcept {
    return SetValue(MarginProperty, value);
}

Base::Result<void> FrameworkElement::SetHorizontalAlignment(
    HorizontalAlignment value) noexcept {
    return SetValue(HorizontalAlignmentProperty, value);
}

Base::Result<void> FrameworkElement::SetVerticalAlignment(
    VerticalAlignment value) noexcept {
    return SetValue(VerticalAlignmentProperty, value);
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
        thread_local char message[512];
        const TypeInfo* parentType =
            PropertyRegistry().Types().FindType(
                RuntimeType());
        const TypeInfo* childType =
            child.PropertyRegistry().Types().FindType(
                child.RuntimeType());
        const Base::StringView parentName =
            parentType != nullptr
            ? parentType->Name()
            : Base::StringView("<unknown>");
        const Base::StringView childName =
            childType != nullptr
            ? childType->Name()
            : Base::StringView("<unknown>");
        const TypeInfo* actualParentType =
            child.LayoutParent() != nullptr
            ? PropertyRegistry().Types().FindType(
                  child.LayoutParent()->
                      RuntimeType())
            : nullptr;
        const Base::StringView actualParentName =
            actualParentType != nullptr
            ? actualParentType->Name()
            : Base::StringView("<none>");
        std::snprintf(
            message,
            sizeof(message),
            "Layout child '%.*s' is not attached to parent '%.*s' "
            "(expectedParent=%p, layoutAttached=%u, actualParent='%.*s' %p, visualParent=%p)",
            static_cast<int>(
                childName.SizeBytes()),
            childName.Data(),
            static_cast<int>(
                parentName.SizeBytes()),
            parentName.Data(),
            static_cast<void*>(this),
            child.layoutAttached_ ? 1U : 0U,
            static_cast<int>(
                actualParentName.SizeBytes()),
            actualParentName.Data(),
            static_cast<void*>(
                child.LayoutParent()),
            static_cast<void*>(
                child.VisualParent()));
        return InvalidState(message);
    }
    return manager_->MeasureElement(child, availableSize);
}

Base::Result<void> UIElement::ArrangeChild(
    UIElement& child,
    Rect finalRect) noexcept {
    if (manager_ == nullptr || !child.layoutAttached_ ||
        child.LayoutParent() != this) {
        thread_local char message[512];
        const TypeInfo* parentType =
            PropertyRegistry().Types().FindType(
                RuntimeType());
        const TypeInfo* childType =
            child.PropertyRegistry().Types().FindType(
                child.RuntimeType());
        const Base::StringView parentName =
            parentType != nullptr
            ? parentType->Name()
            : Base::StringView("<unknown>");
        const Base::StringView childName =
            childType != nullptr
            ? childType->Name()
            : Base::StringView("<unknown>");
        const TypeInfo* actualParentType =
            child.LayoutParent() != nullptr
            ? PropertyRegistry().Types().FindType(
                  child.LayoutParent()->
                      RuntimeType())
            : nullptr;
        const Base::StringView actualParentName =
            actualParentType != nullptr
            ? actualParentType->Name()
            : Base::StringView("<none>");
        std::snprintf(
            message,
            sizeof(message),
            "Layout child '%.*s' is not attached to parent '%.*s' "
            "(expectedParent=%p, layoutAttached=%u, actualParent='%.*s' %p, visualParent=%p)",
            static_cast<int>(
                childName.SizeBytes()),
            childName.Data(),
            static_cast<int>(
                parentName.SizeBytes()),
            parentName.Data(),
            static_cast<void*>(this),
            child.layoutAttached_ ? 1U : 0U,
            static_cast<int>(
                actualParentName.SizeBytes()),
            actualParentName.Data(),
            static_cast<void*>(
                child.LayoutParent()),
            static_cast<void*>(
                child.VisualParent()));
        return InvalidState(message);
    }
    return manager_->ArrangeElement(child, finalRect);
}

} // namespace Aero::Presentation

namespace Aero::Detail {

using namespace Aero::Core;
using namespace Aero::Presentation;

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
    if (!verified) return verified.GetStatus();
    verified = VerifyElement(child);
    if (!verified) return verified.GetStatus();
    if (&parent == &child || child.layoutAttached_) {
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

    parent.manager_ = this;
    child.manager_ = this;
    child.layoutAttached_ = true;
    child.measureValid_ = false;
    child.arrangeValid_ = false;
    return {};
}

Base::Result<void> LayoutManager::Detach(
    UIElement& parent,
    UIElement& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) return verified.GetStatus();
    if (!child.layoutAttached_ || child.LayoutParent() != &parent ||
        child.manager_ != this) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Layout parent-child relationship was not found");
    }

    Base::Result<void> invalidated = InvalidateMeasure(parent);
    if (!invalidated) return invalidated.GetStatus();

    RemoveQueued(child);
    child.layoutAttached_ = false;
    child.manager_ = nullptr;
    child.measureValid_ = false;
    child.arrangeValid_ = false;
    return {};
}

Base::Result<void> LayoutManager::SetRoot(
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
        if (root->layoutAttached_ || root->VisualParent() != nullptr) {
            return InvalidState(
                "Layout root cannot have a visual or layout parent");
        }
        Base::Result<void> invalidated = InvalidateMeasure(*root);
        if (!invalidated) return invalidated.GetStatus();
    }

    if (root_ != nullptr && root_ != root) {
        RemoveQueued(*root_);
        root_->manager_ = nullptr;
    }
    root_ = root;
    rootAvailableSize_ = availableSize;
    if (root_ != nullptr) root_->manager_ = this;
    return {};
}

Base::Result<void> LayoutManager::QueueMeasure(
    UIElement& element) noexcept {
    if (element.measureQueued_) return {};
    Base::Result<Presentation::Detail::VisualLease> lease =
        Presentation::Detail::VisualLease::Acquire(element);
    if (!lease) return lease.GetStatus();
    Base::Result<void> appended =
        measureQueue_.TryPushBack(std::move(lease).Value());
    if (!appended) return appended.GetStatus();
    element.measureQueued_ = true;
    return {};
}

Base::Result<void> LayoutManager::QueueArrange(
    UIElement& element) noexcept {
    if (element.arrangeQueued_) return {};
    Base::Result<Presentation::Detail::VisualLease> lease =
        Presentation::Detail::VisualLease::Acquire(element);
    if (!lease) return lease.GetStatus();
    Base::Result<void> appended =
        arrangeQueue_.TryPushBack(std::move(lease).Value());
    if (!appended) return appended.GetStatus();
    element.arrangeQueued_ = true;
    return {};
}

void LayoutManager::RemoveQueued(UIElement& element) noexcept {
    auto remove = [&](Base::Vector<Presentation::Detail::VisualLease>& queue) noexcept {
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
    element.measureQueued_ = false;
    element.arrangeQueued_ = false;
}

Base::Result<void> LayoutManager::InvalidateMeasure(
    UIElement& element) noexcept {
    Base::Vector<UIElement*> path;
    UIElement* current = &element;
    while (current != nullptr) {
        Base::Result<void> verified = VerifyElement(*current);
        if (!verified) return verified.GetStatus();
        Base::Result<void> appended = path.TryPushBack(current);
        if (!appended) return appended.GetStatus();
        current = current->layoutAttached_
            ? current->LayoutParent() : nullptr;
    }

    Base::Vector<Presentation::Detail::VisualLease> leases;
    Base::Result<void> reserved = leases.TryReserve(path.Size());
    if (!reserved) return reserved.GetStatus();
    for (UIElement* item : path) {
        if (item->measureQueued_) continue;
        Base::Result<Presentation::Detail::VisualLease> lease =
            Presentation::Detail::VisualLease::Acquire(*item);
        if (!lease) return lease.GetStatus();
        Base::Result<void> staged =
            leases.TryPushBack(std::move(lease).Value());
        if (!staged) return staged.GetStatus();
    }
    reserved = measureQueue_.TryReserve(
        measureQueue_.Size() + leases.Size());
    if (!reserved) return reserved.GetStatus();

    std::uint32_t leaseIndex = 0U;
    for (UIElement* item : path) {
        item->measureValid_ = false;
        item->arrangeValid_ = false;
        if (item->measureQueued_) continue;
        Base::Result<void> queued = measureQueue_.TryPushBack(
            std::move(leases[leaseIndex++]));
        AERO_ASSERT(queued);
        (void)queued;
        item->measureQueued_ = true;
    }
    return {};
}

Base::Result<void> LayoutManager::InvalidateArrange(
    UIElement& element) noexcept {
    Base::Vector<UIElement*> path;
    UIElement* current = &element;
    while (current != nullptr) {
        Base::Result<void> verified = VerifyElement(*current);
        if (!verified) return verified.GetStatus();
        Base::Result<void> appended = path.TryPushBack(current);
        if (!appended) return appended.GetStatus();
        current = current->layoutAttached_
            ? current->LayoutParent() : nullptr;
    }

    Base::Vector<Presentation::Detail::VisualLease> leases;
    Base::Result<void> reserved = leases.TryReserve(path.Size());
    if (!reserved) return reserved.GetStatus();
    for (UIElement* item : path) {
        if (item->arrangeQueued_) continue;
        Base::Result<Presentation::Detail::VisualLease> lease =
            Presentation::Detail::VisualLease::Acquire(*item);
        if (!lease) return lease.GetStatus();
        Base::Result<void> staged =
            leases.TryPushBack(std::move(lease).Value());
        if (!staged) return staged.GetStatus();
    }
    reserved = arrangeQueue_.TryReserve(
        arrangeQueue_.Size() + leases.Size());
    if (!reserved) return reserved.GetStatus();

    std::uint32_t leaseIndex = 0U;
    for (UIElement* item : path) {
        item->arrangeValid_ = false;
        if (item->arrangeQueued_) continue;
        Base::Result<void> queued = arrangeQueue_.TryPushBack(
            std::move(leases[leaseIndex++]));
        AERO_ASSERT(queued);
        (void)queued;
        item->arrangeQueued_ = true;
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

    Presentation::Detail::VisualLease pendingArrange;
    const bool queueArrange = !element.arrangeQueued_;
    if (queueArrange) {
        Base::Result<Presentation::Detail::VisualLease> lease =
            Presentation::Detail::VisualLease::Acquire(element);
        if (!lease) return lease.GetStatus();
        pendingArrange = std::move(lease).Value();
        Base::Result<void> reserved = arrangeQueue_.TryReserve(
            arrangeQueue_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }

    if (element.GetVisibility() == Visibility::Collapsed) {
        element.previousMeasureConstraint_ = constraint;
        element.desiredSize_ = {};
        element.untransformedDesiredSize_ = {};
        element.measureValid_ = true;
        element.arrangeValid_ = false;
        element.measureQueued_ = false;
        ++element.layoutRevision_;
        ++measuredCount_;
        if (queueArrange) {
            Base::Result<void> queued = arrangeQueue_.TryPushBack(
                std::move(pendingArrange));
            AERO_ASSERT(queued);
            (void)queued;
            element.arrangeQueued_ = true;
        }
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
    Base::Ref<Transform> layoutTransform =
        framework != nullptr
        ? framework->LayoutTransform()
        : Base::Ref<Transform>{};
    Base::Transform2D layoutMatrix;
    if (layoutTransform) {
        layoutMatrix = layoutTransform->Matrix();
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
    element.untransformedDesiredSize_ = desired;
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
    if (queueArrange) {
        Base::Result<void> queued = arrangeQueue_.TryPushBack(
            std::move(pendingArrange));
        AERO_ASSERT(queued);
        (void)queued;
        element.arrangeQueued_ = true;
    }
    return {};
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
    if (element.GetVisibility() == Visibility::Collapsed) {
        element.layoutSlot_ = {slot.x, slot.y, 0.0, 0.0};
        element.renderSize_ = {};
        if (FrameworkElement* framework =
                element.AsFrameworkElement()) {
            Base::Result<void> width =
                framework->SetReadOnlyCurrentValue(
                    FrameworkElement::ActualWidthProperty,
                    0.0);
            if (!width) return width;
            Base::Result<void> height =
                framework->SetReadOnlyCurrentValue(
                    FrameworkElement::ActualHeightProperty,
                    0.0);
            if (!height) return height;
        }
        element.layoutClip_ = {slot.x, slot.y, 0.0, 0.0};
        element.arrangeValid_ = true;
        element.arrangeQueued_ = false;
        ++element.layoutRevision_;
        ++arrangedCount_;
        return {};
    }
    FrameworkElement* framework =
        element.AsFrameworkElement();
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
    Base::Ref<Transform> layoutTransform =
        framework != nullptr
        ? framework->LayoutTransform()
        : Base::Ref<Transform>{};
    Base::Transform2D layoutMatrix;
    Size naturalAvailable = contentAvailable;
    if (layoutTransform) {
        layoutMatrix = layoutTransform->Matrix();
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
        element.untransformedDesiredSize_;
    const Size constrainedDesired = ClampSize(
        desiredContent, minimum, maximum);

    Size finalSize;
    if (hasWidth) {
        finalSize.width = ClampDimension(
            framework->Width(), minimum.width, maximum.width);
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
            framework->Height(), minimum.height, maximum.height);
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
        horizontal == HorizontalAlignment::Center,
        horizontal == HorizontalAlignment::Right);
    contentSlot.y += AlignmentOffset(contentAvailable.height, layoutFootprint.height,
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
    if (framework != nullptr) {
        Base::Result<void> width =
            framework->SetReadOnlyCurrentValue(
                FrameworkElement::ActualWidthProperty,
                render.width);
        if (!width) return width;
        Base::Result<void> height =
            framework->SetReadOnlyCurrentValue(
                FrameworkElement::ActualHeightProperty,
                render.height);
        if (!height) return height;
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
    element.layoutClip_ = element.ClipToBounds()
        ? Intersect(contentSlot, renderedSlot)
        : renderedSlot;
    element.arrangeValid_ = true;
    element.arrangeQueued_ = false;
    ++element.layoutRevision_;
    ++arrangedCount_;
    return {};
}

Base::Result<std::uint32_t> LayoutManager::Flush() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (flushing_) return InvalidState("Nested layout flush is not allowed");

    flushing_ = true;
    measuredCount_ = 0U;
    arrangedCount_ = 0U;

    if (root_ != nullptr &&
        (!root_->measureValid_ || !root_->arrangeValid_)) {
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

    Base::Vector<Presentation::Detail::VisualLease> measure =
        std::move(measureQueue_);
    measureQueue_ = Base::Vector<Presentation::Detail::VisualLease>();
    for (const Presentation::Detail::VisualLease& lease : measure) {
        Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element != nullptr) element->measureQueued_ = false;
    }
    for (const Presentation::Detail::VisualLease& lease : measure) {
        Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element == nullptr || element == root_ ||
            element->manager_ != this || element->measureValid_) {
            continue;
        }
        UIElement* parent = element->layoutAttached_
            ? element->LayoutParent() : nullptr;
        const Size constraint = parent != nullptr
            ? parent->renderSize_ : rootAvailableSize_;
        Base::Result<void> measured =
            MeasureElement(*element, constraint);
        if (!measured) {
            (void)QueueMeasure(*element);
            flushing_ = false;
            return measured.GetStatus();
        }
    }

    Base::Vector<Presentation::Detail::VisualLease> arrange =
        std::move(arrangeQueue_);
    arrangeQueue_ = Base::Vector<Presentation::Detail::VisualLease>();
    for (const Presentation::Detail::VisualLease& lease : arrange) {
        Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element != nullptr) element->arrangeQueued_ = false;
    }
    for (const Presentation::Detail::VisualLease& lease : arrange) {
        Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element == nullptr || element == root_ ||
            element->manager_ != this || element->arrangeValid_) {
            continue;
        }
        Rect slot = element->layoutSlot_;
        if (slot.width == 0.0 && slot.height == 0.0) {
            slot.width = element->desiredSize_.width;
            slot.height = element->desiredSize_.height;
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
           (!root_->measureValid_ || !root_->arrangeValid_) &&
           convergencePass < MaxConvergencePasses) {
        ++convergencePass;
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
    if (root_ != nullptr &&
        (!root_->measureValid_ || !root_->arrangeValid_)) {
        flushing_ = false;
        return InvalidState(
            "Layout did not converge after template application");
    }

    // A converged root has recursively measured and arranged every attached
    // descendant. Remove stale queue leases created during template
    // application so the next frame starts from a clean layout state.
    for (const Presentation::Detail::VisualLease& lease : measureQueue_) {
        Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element != nullptr) element->measureQueued_ = false;
    }
    for (const Presentation::Detail::VisualLease& lease : arrangeQueue_) {
        Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element != nullptr) element->arrangeQueued_ = false;
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
        Base::Result<std::uint32_t> result =
            manager->Flush();
        manager->lastFlushStatus_ = result
            ? Base::Status{}
            : result.GetStatus();
    }
}

} // namespace Aero::Detail
