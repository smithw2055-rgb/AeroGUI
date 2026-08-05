#include "gui/GuiPrivate.hpp"
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
    Aero::GuiPrivate::Detail::RoutedHandlerStorage handler;
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
    Visual* child = owner_ != nullptr ? VisualTreeHelper::GetChild(*owner_, index_) : nullptr;
    return child != nullptr ? child->AsUIElement() : nullptr;
}

void UIElementChildRange::Iterator::Advance() noexcept {
    if (owner_ == nullptr) return;
    const std::uint32_t count = VisualTreeHelper::GetChildrenCount(*owner_);
    while (index_ < count) {
        Visual* child = VisualTreeHelper::GetChild(*owner_, index_);
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
    Visual* child = owner_ != nullptr ? VisualTreeHelper::GetChild(*owner_, index_) : nullptr;
    return child != nullptr ? child->AsFrameworkElement() : nullptr;
}

void FrameworkElementChildRange::Iterator::Advance() noexcept {
    if (owner_ == nullptr) return;
    const std::uint32_t count = VisualTreeHelper::GetChildrenCount(*owner_);
    while (index_ < count) {
        Visual* child = VisualTreeHelper::GetChild(*owner_, index_);
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

UIElement* FindInvalidVisibleLayout(Visual& visual) noexcept {
    UIElement* element = visual.AsUIElement();
    if (element != nullptr &&
        element->GetIsVisible() &&
        (!UIElement::Impl::MeasureValid(*element) ||
         !UIElement::Impl::ArrangeValid(*element))) {
        return element;
    }
    const std::uint32_t childCount =
        VisualTreeHelper::GetChildrenCount(visual);
    for (std::uint32_t index = 0U; index < childCount; ++index) {
        Visual* child = VisualTreeHelper::GetChild(visual, index);
        if (child != nullptr) {
            UIElement* invalid = FindInvalidVisibleLayout(*child);
            if (invalid != nullptr) return invalid;
        }
    }
    return nullptr;
}

bool HasInvalidVisibleLayout(Visual& visual) noexcept {
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
    : Visual(runtimeType) {}

UIElement::~UIElement() {
    AERO_ASSERT(Aero::UIElement::Impl::LayoutManager(*this) == nullptr);
    AERO_ASSERT(!layoutAttached_);
    CleanupHandlers();
}

Base::Result<void> UIElement::AddHandlerCore(
    RoutedEventHandle event,
    const HandlerDescriptor& handler,
    bool handledEventsToo) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    if (!event.IsValid() || handler.value == nullptr || handler.operations == nullptr ||
        handler.operations->copy == nullptr || handler.operations->destroy == nullptr ||
        handler.operations->equals == nullptr || handler.operations->invoke == nullptr ||
        handler.operations->size > 4U * sizeof(void*) ||
        handler.operations->alignment > alignof(void*)) {
        return InvalidArgument("Routed event handler requires a valid event and callback");
    }

    auto* state = static_cast<UIElementHandlerState*>(routedHandlers_);
    if (state == nullptr) {
        Base::IAllocator& allocator = Base::GetDefaultAllocator();
        void* memory = allocator.Allocate({
            sizeof(UIElementHandlerState),
            alignof(UIElementHandlerState),
            Base::MemoryTag::Ui});
        if (memory == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Routed event handler state allocation failed");
        }
        state = new (memory) UIElementHandlerState();
        routedHandlers_ = state;
    }
    if (state->nextSequence == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Routed event handler sequence space exhausted");
    }

    RoutedHandlerRecord record;
    record.event = event;
    record.handler = Aero::GuiPrivate::Detail::RoutedHandlerStorage(
        handler.value,
        handler.operations->size,
        handler.operations->alignment,
        handler.argsType,
        handler.operations->copy,
        handler.operations->destroy,
        handler.operations->equals,
        handler.operations->invoke);
    record.sequence = state->nextSequence++;
    record.handledEventsToo = handledEventsToo;
    return state->handlers.PushBack(std::move(record));
}

bool UIElement::RemoveHandlerCore(
    RoutedEventHandle event,
    const HandlerDescriptor& handler) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access || !event.IsValid() || handler.value == nullptr ||
        handler.operations == nullptr || routedHandlers_ == nullptr) {
        return false;
    }
    Aero::GuiPrivate::Detail::RoutedHandlerStorage probe(
        handler.value,
        handler.operations->size,
        handler.operations->alignment,
        handler.argsType,
        handler.operations->copy,
        handler.operations->destroy,
        handler.operations->equals,
        handler.operations->invoke);
    auto& handlers = static_cast<UIElementHandlerState*>(routedHandlers_)->handlers;
    for (std::uint32_t index = 0U; index < handlers.Size(); ++index) {
        if (handlers[index].event == event && handlers[index].handler.Equals(probe)) {
            for (std::uint32_t current = index + 1U; current < handlers.Size(); ++current) {
                handlers[current - 1U] = std::move(handlers[current]);
            }
            handlers.PopBack();
            return true;
        }
    }
    return false;
}

void UIElement::InvokeHandlers(
    RoutedEventHandle event,
    RoutedEventArgs& args) noexcept {
    auto* state = static_cast<UIElementHandlerState*>(routedHandlers_);
    if (state == nullptr) return;
    const std::uint32_t count = state->handlers.Size();
    for (std::uint32_t index = 0U;
         index < count && index < state->handlers.Size();
         ++index) {
        const RoutedHandlerRecord record = state->handlers[index];
        if (record.event == event && (!args.GetHandled() || record.handledEventsToo)) {
            record.handler.Invoke(this, args);
        }
    }
}

void UIElement::CleanupHandlers() noexcept {
    auto* state = static_cast<UIElementHandlerState*>(routedHandlers_);
    if (state == nullptr) return;
    state->~UIElementHandlerState();
    Base::GetDefaultAllocator().Deallocate(
        state,
        sizeof(UIElementHandlerState),
        alignof(UIElementHandlerState),
        Base::MemoryTag::Ui);
    routedHandlers_ = nullptr;
}

void UIElement::RaiseEvent(
    RoutedEventHandle event,
    RoutedEventArgs* args) noexcept {
    Aero::GuiPrivate::Detail::EventRouter* eventRouter =
        Aero::GuiPrivate::Detail::ElementPrivate::EventRouterFor(*this);
    if (eventRouter == nullptr) {
        return;
    }
    static_cast<void>(eventRouter->RaiseEvent(*this, event, args));
}

Base::Result<void> UIElement::InvalidateMeasure() noexcept {
    auto* layout = static_cast<Aero::GuiPrivate::Detail::LayoutEngine*>(
        Aero::UIElement::Impl::LayoutManager(*this));
    if (layout == nullptr) {
        measureValid_ = false;
        arrangeValid_ = false;
        return {};
    }
    return layout->InvalidateMeasure(*this);
}

Base::Result<void> UIElement::InvalidateArrange() noexcept {
    auto* layout = static_cast<Aero::GuiPrivate::Detail::LayoutEngine*>(
        Aero::UIElement::Impl::LayoutManager(*this));
    if (layout == nullptr) {
        arrangeValid_ = false;
        return {};
    }
    return layout->InvalidateArrange(*this);
}

void FrameworkElement::SetUseLayoutRounding(
    bool enabled,
    double dpiScale) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    if (!std::isfinite(dpiScale) || dpiScale <= 0.0) {
        return;
    }
    const bool scaleChanged = dpiScale_ != dpiScale;
    dpiScale_ = dpiScale;
    SetValue(UseLayoutRoundingProperty, enabled);
    if (scaleChanged && enabled) (void)InvalidateMeasure();
}

bool UIElement::GetClipToBounds() const noexcept {
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
double UIElement::GetOpacity() const noexcept {
    return GetValueOr(OpacityProperty, 1.0);
}
bool UIElement::GetIsHitTestVisible() const noexcept {
    return GetValueOr(IsHitTestVisibleProperty, true);
}
Visibility UIElement::GetVisibility() const noexcept {
    return GetValueOr(
        VisibilityProperty, Visibility::Visible);
}
bool UIElement::GetIsVisible() const noexcept {
    const Visual* current = this;
    while (current != nullptr) {
        const UIElement* element = current->AsUIElement();
        if (element != nullptr &&
            element->GetVisibility() != Visibility::Visible) {
            return false;
        }
        current = current->GetLogicalParent() != nullptr
            ? current->GetLogicalParent()
            : current->GetVisualParent();
    }
    return true;
}
bool UIElement::GetIsEnabled() const noexcept {
    if (!GetValueOr(IsEnabledProperty, true)) return false;
    Visual* parent = GetLogicalParent() != nullptr
        ? GetLogicalParent() : GetVisualParent();
    const UIElement* parentElement =
        parent != nullptr ? parent->AsUIElement() : nullptr;
    return parentElement == nullptr || parentElement->GetIsEnabled();
}
bool UIElement::GetAllowDrop() const noexcept {
    return GetValueOr(AllowDropProperty, false);
}
bool UIElement::GetIsMouseOver() const noexcept {
    return GetValueOr(IsMouseOverProperty, false);
}
bool UIElement::GetIsPressed() const noexcept {
    return GetValueOr(IsPressedProperty, false);
}
bool UIElement::GetIsKeyboardFocused() const noexcept {
    return GetValueOr(IsKeyboardFocusedProperty, false);
}
bool UIElement::GetIsKeyboardFocusWithin() const noexcept {
    return GetValueOr(IsKeyboardFocusWithinProperty, false);
}
bool UIElement::GetFocusable() const noexcept {
    return GetValueOr(FocusableProperty, false);
}
Base::Result<bool> UIElement::Focus() noexcept {
    Aero::GuiPrivate::Detail::InputRouter* input =
        Visual::Impl::InputRouterFor(*this);
    if (input == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "UIElement Focus requires a mounted View");
    }
    return input->SetFocus(this);
}
bool UIElement::GetIsTabStop() const noexcept {
    return GetValueOr(IsTabStopProperty, false);
}
std::uint32_t UIElement::GetTabIndex() const noexcept {
    return GetValueOr(TabIndexProperty, 0U);
}
bool UIElement::GetIsFocusScope() const noexcept {
    return GetValueOr(IsFocusScopeProperty, false);
}
bool FrameworkElement::GetUseLayoutRounding() const noexcept {
    return GetValueOr(UseLayoutRoundingProperty, false);
}
bool FrameworkElement::GetHasWidth() const noexcept {
    return !GetValueOr(
        WidthProperty, Length::Auto()).isAuto;
}
bool FrameworkElement::GetHasHeight() const noexcept {
    return !GetValueOr(
        HeightProperty, Length::Auto()).isAuto;
}
double FrameworkElement::GetWidth() const noexcept {
    const Length length =
        GetValueOr(WidthProperty, Length::Auto());
    return length.isAuto ? 0.0 : length.value;
}
double FrameworkElement::GetHeight() const noexcept {
    const Length length =
        GetValueOr(HeightProperty, Length::Auto());
    return length.isAuto ? 0.0 : length.value;
}
Size FrameworkElement::GetMinSize() const noexcept {
    return {
        GetValueOr(MinWidthProperty, 0.0),
        GetValueOr(MinHeightProperty, 0.0)};
}
Size FrameworkElement::GetMaxSize() const noexcept {
    const Size minimum = GetMinSize();
    const double authoredWidth =
        GetValueOr(MaxWidthProperty, 1.0e12);
    const double authoredHeight =
        GetValueOr(MaxHeightProperty, 1.0e12);
    // Resolve contradictory template/style ordering at layout time. Min values
    // take precedence without rejecting an otherwise valid WPF template.
    return {
        authoredWidth < minimum.width ? minimum.width : authoredWidth,
        authoredHeight < minimum.height ? minimum.height : authoredHeight};
}
Thickness FrameworkElement::GetMargin() const noexcept {
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

void UIElement::OnPropertyInvalidated(
    PropertyInvalidationFlags flags) noexcept {
    if (HasFlag(flags, PropertyInvalidationFlags::Measure)) {
        (void)InvalidateMeasure();
    } else if (HasFlag(flags, PropertyInvalidationFlags::Arrange)) {
        (void)InvalidateArrange();
    }
    UIElement* parent = layoutAttached_ ? LayoutParent() : nullptr;
    if (parent != nullptr &&
        HasFlag(flags, PropertyInvalidationFlags::ParentMeasure)) {
        (void)parent->InvalidateMeasure();
    } else if (parent != nullptr &&
        HasFlag(flags, PropertyInvalidationFlags::ParentArrange)) {
        (void)parent->InvalidateArrange();
    }
    if (HasFlag(flags, PropertyInvalidationFlags::Render)) {
        static_cast<void>(
            Aero::GuiPrivate::Detail::ElementPrivate::
                InvalidateRenderState(*this));
    }
    DependencyObject::OnPropertyInvalidated(flags);
}

void UIElement::SetClipToBounds(bool value) noexcept {
    SetValue(ClipToBoundsProperty, value);
}

void UIElement::SetBlendMode(
    BlendMode value) noexcept {
    SetValue(BlendModeProperty, value);
}

void UIElement::SetEffect(
    Base::Ref<Effect> value) noexcept {
    SetValue(EffectProperty, std::move(value));
}

Base::Ref<Media::Brush> UIElement::GetOpacityMask() const noexcept {
    return GetValueOr(
        OpacityMaskProperty,
        Base::Ref<Media::Brush>{});
}

void UIElement::SetOpacityMask(
    Base::Ref<Media::Brush> value) noexcept {
    SetValue(OpacityMaskProperty, std::move(value));
}

void UIElement::SetIsHitTestVisible(bool value) noexcept {
    SetValue(IsHitTestVisibleProperty, value);
}
void UIElement::SetVisibility(
    Visibility value) noexcept {
    SetValue(VisibilityProperty, value);
}
void UIElement::SetIsEnabled(bool value) noexcept {
    SetValue(IsEnabledProperty, value);
}
void UIElement::SetIsTabStop(bool value) noexcept {
    SetValue(IsTabStopProperty, value);
}
void UIElement::SetTabIndex(std::uint32_t value) noexcept {
    SetValue(TabIndexProperty, value);
}
void UIElement::SetIsFocusScope(bool value) noexcept {
    SetValue(IsFocusScopeProperty, value);
}
Base::Ref<Transform> UIElement::GetRenderTransform() const noexcept {
    Base::Result<Base::Ref<Transform>> value =
        GetValue(RenderTransformProperty);
    return value ? std::move(value).Value() : Base::Ref<Transform>{};
}
Point UIElement::GetRenderTransformOrigin() const noexcept {
    return GetValueOr(RenderTransformOriginProperty, Point{});
}
void UIElement::SetRenderTransform(
    Base::Ref<Transform> value) noexcept {
    SetValue(RenderTransformProperty, std::move(value));
}
void UIElement::SetRenderTransformOrigin(
    Point value) noexcept {
    SetValue(RenderTransformOriginProperty, value);
}
void UIElement::SetMouseOverState(bool value) noexcept {
    SetReadOnlyCurrentValue(IsMouseOverProperty, value);
}
void UIElement::SetPressedState(bool value) noexcept {
    SetReadOnlyCurrentValue(IsPressedProperty, value);
}
void UIElement::SetKeyboardFocusedState(bool value) noexcept {
    SetReadOnlyCurrentValue(IsKeyboardFocusedProperty, value);
}
void UIElement::SetKeyboardFocusWithinState(
    bool value) noexcept {
    SetReadOnlyCurrentValue(IsKeyboardFocusWithinProperty, value);
}

void FrameworkElement::SetWidth(double value) noexcept {
    SetValue(WidthProperty, Length::Pixels(value));
}

void FrameworkElement::ClearWidth() noexcept {
    ClearValue(WidthProperty);
}

void FrameworkElement::SetHeight(double value) noexcept {
    SetValue(HeightProperty, Length::Pixels(value));
}

void FrameworkElement::ClearHeight() noexcept {
    ClearValue(HeightProperty);
}

void FrameworkElement::SetMinSize(Size value) noexcept {
    const Size maximum = GetMaxSize();
    if (!IsValidLayoutSize(value) || value.width > maximum.width ||
        value.height > maximum.height) {
        return;
    }
    SetValue(MinWidthProperty, value.width);
    SetValue(MinHeightProperty, value.height);
}

void FrameworkElement::SetMaxSize(Size value) noexcept {
    const Size minimum = GetMinSize();
    if (!IsValidLayoutSize(value) || value.width < minimum.width ||
        value.height < minimum.height) {
        return;
    }
    SetValue(MaxWidthProperty, value.width);
    SetValue(MaxHeightProperty, value.height);
}

void FrameworkElement::SetMargin(Thickness value) noexcept {
    SetValue(MarginProperty, value);
}

void FrameworkElement::SetHorizontalAlignment(
    HorizontalAlignment value) noexcept {
    SetValue(HorizontalAlignmentProperty, value);
}

void FrameworkElement::SetVerticalAlignment(
    VerticalAlignment value) noexcept {
    SetValue(VerticalAlignmentProperty, value);
}

Size UIElement::MeasureOverride(Size availableSize) noexcept {
    return availableSize;
}

Size UIElement::ArrangeOverride(Size finalSize) noexcept {
    return finalSize;
}

Base::Result<void> UIElement::MeasureChild(
    UIElement& child,
    Size availableSize) noexcept {
    auto* layout = static_cast<Aero::GuiPrivate::Detail::LayoutEngine*>(
        Aero::UIElement::Impl::LayoutManager(*this));
    if (layout == nullptr || !child.layoutAttached_ ||
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
                child.GetVisualParent()));
        return InvalidState(message);
    }
    return layout->MeasureElement(child, availableSize);
}

Base::Result<void> UIElement::ArrangeChild(
    UIElement& child,
    Rect finalRect) noexcept {
    auto* layout = static_cast<Aero::GuiPrivate::Detail::LayoutEngine*>(
        Aero::UIElement::Impl::LayoutManager(*this));
    if (layout == nullptr || !child.layoutAttached_ ||
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
                child.GetVisualParent()));
        return InvalidState(message);
    }
    return layout->ArrangeElement(child, finalRect);
}

} // namespace Aero

namespace Aero::GuiPrivate::Detail {

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
    if (UIElement::Impl::LayoutManager(element) != nullptr && UIElement::Impl::LayoutManager(element) != this) {
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
    if (&parent == &child || UIElement::Impl::LayoutAttached(child)) {
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

    UIElement::Impl::LayoutAttached(child) = true;
    UIElement::Impl::MeasureValid(child) = false;
    UIElement::Impl::ArrangeValid(child) = false;
    return {};
}

Base::Result<void> LayoutEngine::Detach(
    UIElement& parent,
    UIElement& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) return verified.GetStatus();
    if (!UIElement::Impl::LayoutAttached(child) || child.LayoutParent() != &parent ||
        UIElement::Impl::LayoutManager(child) != this) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Layout parent-child relationship was not found");
    }

    Base::Result<void> invalidated = InvalidateMeasure(parent);
    if (!invalidated) return invalidated.GetStatus();

    RemoveQueued(child);
    UIElement::Impl::LayoutAttached(child) = false;
    UIElement::Impl::MeasureValid(child) = false;
    UIElement::Impl::ArrangeValid(child) = false;
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
        if (UIElement::Impl::LayoutAttached(*root) || root->GetVisualParent() != nullptr) {
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
    if (UIElement::Impl::MeasureQueued(element)) return {};
    Base::Result<Aero::GuiPrivate::Detail::VisualLease> lease =
        Aero::GuiPrivate::Detail::VisualLease::Acquire(element);
    if (!lease) return lease.GetStatus();
    Base::Result<void> appended =
        measureQueue_.PushBack(std::move(lease).Value());
    if (!appended) return appended.GetStatus();
    UIElement::Impl::MeasureQueued(element) = true;
    return {};
}

Base::Result<void> LayoutEngine::QueueArrange(
    UIElement& element) noexcept {
    if (UIElement::Impl::ArrangeQueued(element)) return {};
    Base::Result<Aero::GuiPrivate::Detail::VisualLease> lease =
        Aero::GuiPrivate::Detail::VisualLease::Acquire(element);
    if (!lease) return lease.GetStatus();
    Base::Result<void> appended =
        arrangeQueue_.PushBack(std::move(lease).Value());
    if (!appended) return appended.GetStatus();
    UIElement::Impl::ArrangeQueued(element) = true;
    return {};
}

void LayoutEngine::RemoveQueued(UIElement& element) noexcept {
    auto remove = [&](Base::Vector<Aero::GuiPrivate::Detail::VisualLease>& queue) noexcept {
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
    UIElement::Impl::MeasureQueued(element) = false;
    UIElement::Impl::ArrangeQueued(element) = false;
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
        current = UIElement::Impl::LayoutAttached(*current)
            ? current->LayoutParent() : nullptr;
    }

    Base::Vector<Aero::GuiPrivate::Detail::VisualLease> leases;
    Base::Result<void> reserved = leases.Reserve(path.Size());
    if (!reserved) return reserved.GetStatus();
    for (UIElement* item : path) {
        if (UIElement::Impl::MeasureQueued(*item)) continue;
        Base::Result<Aero::GuiPrivate::Detail::VisualLease> lease =
            Aero::GuiPrivate::Detail::VisualLease::Acquire(*item);
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
        UIElement::Impl::MeasureValid(*item) = false;
        UIElement::Impl::ArrangeValid(*item) = false;
        if (UIElement::Impl::MeasureQueued(*item)) continue;
        Base::Result<void> queued = measureQueue_.PushBack(
            std::move(leases[leaseIndex++]));
        AERO_ASSERT(queued);
        (void)queued;
        UIElement::Impl::MeasureQueued(*item) = true;
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
        current = UIElement::Impl::LayoutAttached(*current)
            ? current->LayoutParent() : nullptr;
    }

    Base::Vector<Aero::GuiPrivate::Detail::VisualLease> leases;
    Base::Result<void> reserved = leases.Reserve(path.Size());
    if (!reserved) return reserved.GetStatus();
    for (UIElement* item : path) {
        if (UIElement::Impl::ArrangeQueued(*item)) continue;
        Base::Result<Aero::GuiPrivate::Detail::VisualLease> lease =
            Aero::GuiPrivate::Detail::VisualLease::Acquire(*item);
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
        UIElement::Impl::ArrangeValid(*item) = false;
        if (UIElement::Impl::ArrangeQueued(*item)) continue;
        Base::Result<void> queued = arrangeQueue_.PushBack(
            std::move(leases[leaseIndex++]));
        AERO_ASSERT(queued);
        (void)queued;
        UIElement::Impl::ArrangeQueued(*item) = true;
    }
    return {};
}

Base::Result<void> LayoutEngine::MeasureElement(
    UIElement& element,
    Size constraint) noexcept {
    if (!IsValidLayoutSize(constraint)) {
        return InvalidArgument("Measure constraint must be finite and nonnegative");
    }
    if (UIElement::Impl::Measuring(element) || UIElement::Impl::Arranging(element)) {
        return InvalidState("Recursive layout operation is not allowed");
    }
    if (UIElement::Impl::MeasureValid(element) && SameSize(UIElement::Impl::PreviousMeasureConstraint(element), constraint)) {
        return {};
    }

    Aero::GuiPrivate::Detail::VisualLease pendingArrange;
    const bool queueArrange = !UIElement::Impl::ArrangeQueued(element);
    if (queueArrange) {
        Base::Result<Aero::GuiPrivate::Detail::VisualLease> lease =
            Aero::GuiPrivate::Detail::VisualLease::Acquire(element);
        if (!lease) return lease.GetStatus();
        pendingArrange = std::move(lease).Value();
        Base::Result<void> reserved = arrangeQueue_.Reserve(
            arrangeQueue_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }

    if (element.GetVisibility() == Visibility::Collapsed) {
        UIElement::Impl::PreviousMeasureConstraint(element) = constraint;
        UIElement::Impl::DesiredSize(element) = {};
        UIElement::Impl::UntransformedDesiredSize(element) = {};
        UIElement::Impl::MeasureValid(element) = true;
        UIElement::Impl::ArrangeValid(element) = false;
        UIElement::Impl::MeasureQueued(element) = false;
        ++UIElement::Impl::LayoutRevision(element);
        ++measuredCount_;
        if (queueArrange) {
            Base::Result<void> queued = arrangeQueue_.PushBack(
                std::move(pendingArrange));
            AERO_ASSERT(queued);
            (void)queued;
            UIElement::Impl::ArrangeQueued(element) = true;
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

    UIElement::Impl::Measuring(element) = true;
    const Size result = UIElement::Impl::MeasureOverride(element, available);
    UIElement::Impl::Measuring(element) = false;
    Size desired = result;
    if (!IsValidLayoutSize(desired)) {
        return InvalidArgument("MeasureOverride returned an invalid size");
    }
    desired = ClampSize(desired, minimum, maximum);
    if (hasWidth) desired.width = available.width;
    if (hasHeight) desired.height = available.height;
    UIElement::Impl::UntransformedDesiredSize(element) = desired;
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
    UIElement::Impl::PreviousMeasureConstraint(element) = constraint;
    UIElement::Impl::DesiredSize(element) = desired;
    UIElement::Impl::MeasureValid(element) = true;
    UIElement::Impl::ArrangeValid(element) = false;
    UIElement::Impl::MeasureQueued(element) = false;
    ++UIElement::Impl::LayoutRevision(element);
    ++measuredCount_;
    if (queueArrange) {
        Base::Result<void> queued = arrangeQueue_.PushBack(
            std::move(pendingArrange));
        AERO_ASSERT(queued);
        (void)queued;
        UIElement::Impl::ArrangeQueued(element) = true;
    }
    return {};
}

Base::Result<void> LayoutEngine::ArrangeElement(
    UIElement& element,
    Rect slot) noexcept {
    if (!IsValidLayoutRect(slot)) {
        return InvalidArgument("Arrange slot must be finite and nonnegative");
    }
    if (!UIElement::Impl::MeasureValid(element)) {
        Base::Result<void> measured = MeasureElement(
            element, {slot.width, slot.height});
        if (!measured) {
            return measured;
        }
    }
    if (UIElement::Impl::Measuring(element) || UIElement::Impl::Arranging(element)) {
        return InvalidState("Recursive layout operation is not allowed");
    }
    if (element.GetVisibility() == Visibility::Collapsed) {
        UIElement::Impl::LayoutSlot(element) = {slot.x, slot.y, 0.0, 0.0};
        UIElement::Impl::RenderSize(element) = {};
        if (FrameworkElement* framework =
                element.AsFrameworkElement()) {
            UIElement::Impl::SetActualSize(
                *framework, 0.0, 0.0);
        }
        UIElement::Impl::LayoutClip(element) = {slot.x, slot.y, 0.0, 0.0};
        UIElement::Impl::ArrangeValid(element) = true;
        UIElement::Impl::ArrangeQueued(element) = false;
        ++UIElement::Impl::LayoutRevision(element);
        ++arrangedCount_;
        return {};
    }
    FrameworkElement* framework =
        element.AsFrameworkElement();
    if (framework != nullptr && framework->GetUseLayoutRounding()) {
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
        UIElement::Impl::UntransformedDesiredSize(element);
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
        horizontal == HorizontalAlignment::Center,
        horizontal == HorizontalAlignment::Right);
    contentSlot.y += AlignmentOffset(contentAvailable.height, layoutFootprint.height,
        vertical == VerticalAlignment::Center,
        vertical == VerticalAlignment::Bottom);

    UIElement::Impl::Arranging(element) = true;
    const Size result = UIElement::Impl::ArrangeOverride(element, finalSize);
    UIElement::Impl::Arranging(element) = false;
    Size render = result;
    if (!IsValidLayoutSize(render)) {
        return InvalidArgument("ArrangeOverride returned an invalid size");
    }
    UIElement::Impl::LayoutSlot(element) = contentSlot;
    UIElement::Impl::RenderSize(element) = render;
    if (framework != nullptr) {
        UIElement::Impl::SetActualSize(
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
    UIElement::Impl::LayoutClip(element) = element.GetClipToBounds()
        ? Intersect(contentSlot, renderedSlot)
        : renderedSlot;
    UIElement::Impl::ArrangeValid(element) = true;
    UIElement::Impl::ArrangeQueued(element) = false;
    ++UIElement::Impl::LayoutRevision(element);
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
        (!UIElement::Impl::MeasureValid(*root_) || !UIElement::Impl::ArrangeValid(*root_))) {
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

    Base::Vector<Aero::GuiPrivate::Detail::VisualLease> measure =
        std::move(measureQueue_);
    measureQueue_ = Base::Vector<Aero::GuiPrivate::Detail::VisualLease>();
    for (const Aero::GuiPrivate::Detail::VisualLease& lease : measure) {
        Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element != nullptr) UIElement::Impl::MeasureQueued(*element) = false;
    }
    for (const Aero::GuiPrivate::Detail::VisualLease& lease : measure) {
        Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element == nullptr || element == root_ ||
            UIElement::Impl::LayoutManager(*element) != this || UIElement::Impl::MeasureValid(*element)) {
            continue;
        }
        UIElement* parent = UIElement::Impl::LayoutAttached(*element)
            ? element->LayoutParent() : nullptr;
        const Size constraint = parent != nullptr
            ? UIElement::Impl::RenderSize(*parent) : rootAvailableSize_;
        Base::Result<void> measured =
            MeasureElement(*element, constraint);
        if (!measured) {
            (void)QueueMeasure(*element);
            flushing_ = false;
            return measured.GetStatus();
        }
    }

    Base::Vector<Aero::GuiPrivate::Detail::VisualLease> arrange =
        std::move(arrangeQueue_);
    arrangeQueue_ = Base::Vector<Aero::GuiPrivate::Detail::VisualLease>();
    for (const Aero::GuiPrivate::Detail::VisualLease& lease : arrange) {
        Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element != nullptr) UIElement::Impl::ArrangeQueued(*element) = false;
    }
    for (const Aero::GuiPrivate::Detail::VisualLease& lease : arrange) {
        Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element == nullptr || element == root_ ||
            UIElement::Impl::LayoutManager(*element) != this || UIElement::Impl::ArrangeValid(*element)) {
            continue;
        }
        Rect slot = UIElement::Impl::LayoutSlot(*element);
        if (slot.width == 0.0 && slot.height == 0.0) {
            slot.width = UIElement::Impl::DesiredSize(*element).width;
            slot.height = UIElement::Impl::DesiredSize(*element).height;
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
        UIElement::Impl::MeasureValid(*root_) = false;
        UIElement::Impl::ArrangeValid(*root_) = false;
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
            invalid != nullptr && UIElement::Impl::MeasureValid(*invalid) ? 1U : 0U,
            invalid != nullptr && UIElement::Impl::ArrangeValid(*invalid) ? 1U : 0U,
            static_cast<int>(parentName.SizeBytes()),
            parentName.Data());
        return InvalidState(message);
    }

    // A converged root has recursively measured and arranged every attached
    // descendant. Remove stale queue leases created during template
    // application so the next frame starts from a clean layout state.
    for (const Aero::GuiPrivate::Detail::VisualLease& lease : measureQueue_) {
        Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element != nullptr) UIElement::Impl::MeasureQueued(*element) = false;
    }
    for (const Aero::GuiPrivate::Detail::VisualLease& lease : arrangeQueue_) {
        Visual* visual = lease.Resolve();
        UIElement* element = visual != nullptr
            ? visual->AsUIElement() : nullptr;
        if (element != nullptr) UIElement::Impl::ArrangeQueued(*element) = false;
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

} // namespace Aero::GuiPrivate::Detail
