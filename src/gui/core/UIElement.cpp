// Auto-relocated base-class method definitions (WPF semantic kernel).
#include <Aero/UIElement.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Allocator.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Events.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Controls.hpp>
#include <cstdio>
#include <new>
#include "gui/core/State.hpp" 
#include "gui/input/InputState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/meta/MetadataState.hpp"

using namespace Aero;
using namespace Aero::Media;
using namespace Aero::Meta;
using namespace Aero::Threading;

namespace Aero {
namespace {

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

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status NotFound(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotFound, message);
}

} // namespace

// from src/gui/controls/Layout.cpp

Base::Result<void> UIElement::ArrangeChild(
    UIElement& child,
    Rect finalRect) noexcept {
    auto* layout = static_cast<Aero::LayoutEngine*>(
        AeroGuiInternal::LayoutEngineOf(*this));
    if (layout == nullptr || !AeroGuiInternal::Layout(child).layoutAttached ||
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
            AeroGuiInternal::Layout(child).layoutAttached ? 1U : 0U,
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

// Layout hot state lives on UIElement.
Size UIElement::GetDesiredSize() const noexcept {
    return layout_.desiredSize;
}
Size UIElement::GetRenderSize() const noexcept {
    return layout_.renderSize;
}
Rect UIElement::GetLayoutSlot() const noexcept {
    return layout_.layoutSlot;
}
Rect UIElement::GetLayoutClip() const noexcept {
    return layout_.layoutClip;
}
bool UIElement::GetIsMeasureValid() const noexcept {
    return layout_.measureValid;
}
bool UIElement::GetIsArrangeValid() const noexcept {
    return layout_.arrangeValid;
}
bool UIElement::GetIsMeasureQueued() const noexcept {
    return layout_.measureQueued;
}
bool UIElement::GetIsArrangeQueued() const noexcept {
    return layout_.arrangeQueued;
}
bool UIElement::GetIsMeasuring() const noexcept {
    return layout_.measuring;
}
bool UIElement::GetIsArranging() const noexcept {
    return layout_.arranging;
}
bool UIElement::GetIsLayoutAttached() const noexcept {
    return layout_.layoutAttached;
}
Size UIElement::GetUntransformedDesiredSize() const noexcept {
    return layout_.untransformedDesiredSize;
}
Size UIElement::GetPreviousMeasureConstraint() const noexcept {
    return layout_.previousMeasureConstraint;
}
std::uint64_t UIElement::GetLayoutRevision() const noexcept {
    return layout_.layoutRevision;
}

// from src/gui/controls/Layout.cpp

Base::Result<void> UIElement::MeasureChild(
    UIElement& child,
    Size availableSize) noexcept {
    auto* layout = static_cast<Aero::LayoutEngine*>(
        AeroGuiInternal::LayoutEngineOf(*this));
    if (layout == nullptr || !AeroGuiInternal::Layout(child).layoutAttached ||
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
            AeroGuiInternal::Layout(child).layoutAttached ? 1U : 0U,
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

// from src/gui/controls/Layout.cpp

Size UIElement::ArrangeOverride(Size finalSize) noexcept {
    return finalSize;
}

// from src/gui/controls/Layout.cpp

Size UIElement::MeasureOverride(Size availableSize) noexcept {
    return availableSize;
}

// from src/gui/controls/Layout.cpp
void UIElement::SetKeyboardFocusWithinState(
    bool value) noexcept {
    SetReadOnlyCurrentValue(IsKeyboardFocusWithinProperty, value);
}

// from src/gui/controls/Layout.cpp
void UIElement::SetKeyboardFocusedState(bool value) noexcept {
    SetReadOnlyCurrentValue(IsKeyboardFocusedProperty, value);
}

// from src/gui/controls/Layout.cpp
void UIElement::SetPressedState(bool value) noexcept {
    SetReadOnlyCurrentValue(IsPressedProperty, value);
}

// from src/gui/controls/Layout.cpp
void UIElement::SetMouseOverState(bool value) noexcept {
    SetReadOnlyCurrentValue(IsMouseOverProperty, value);
}

// from src/gui/controls/Layout.cpp
void UIElement::SetRenderTransformOrigin(
    Point value) noexcept {
    SetValue(RenderTransformOriginProperty, value);
}

// from src/gui/controls/Layout.cpp
void UIElement::SetRenderTransform(
    Base::Ref<Transform> value) noexcept {
    SetValue(RenderTransformProperty, std::move(value));
}

// from src/gui/controls/Layout.cpp
Point UIElement::GetRenderTransformOrigin() const noexcept {
    return GetValueOr(RenderTransformOriginProperty, Point{});
}

// from src/gui/controls/Layout.cpp
Base::Ref<Transform> UIElement::GetRenderTransform() const noexcept {
    Base::Result<Base::Ref<Transform>> value =
        GetValue(RenderTransformProperty);
    return value ? std::move(value).Value() : Base::Ref<Transform>{};
}

// from src/gui/controls/Layout.cpp
void UIElement::SetIsFocusScope(bool value) noexcept {
    SetValue(IsFocusScopeProperty, value);
}

// from src/gui/controls/Layout.cpp
void UIElement::SetTabIndex(std::uint32_t value) noexcept {
    SetValue(TabIndexProperty, value);
}

// from src/gui/controls/Layout.cpp
void UIElement::SetIsTabStop(bool value) noexcept {
    SetValue(IsTabStopProperty, value);
}

// from src/gui/controls/Layout.cpp
void UIElement::SetIsEnabled(bool value) noexcept {
    SetValue(IsEnabledProperty, value);
}

// from src/gui/controls/Layout.cpp
void UIElement::SetVisibility(
    Visibility value) noexcept {
    SetValue(VisibilityProperty, value);
}

// from src/gui/controls/Layout.cpp

void UIElement::SetIsHitTestVisible(bool value) noexcept {
    SetValue(IsHitTestVisibleProperty, value);
}

// from src/gui/controls/Layout.cpp

void UIElement::SetOpacityMask(
    Base::Ref<Media::Brush> value) noexcept {
    SetValue(OpacityMaskProperty, std::move(value));
}

// from src/gui/controls/Layout.cpp

Base::Ref<Media::Brush> UIElement::GetOpacityMask() const noexcept {
    return GetValueOr(
        OpacityMaskProperty,
        Base::Ref<Media::Brush>{});
}

// from src/gui/controls/Layout.cpp

void UIElement::SetEffect(
    Base::Ref<Effect> value) noexcept {
    SetValue(EffectProperty, std::move(value));
}

// from src/gui/controls/Layout.cpp

void UIElement::SetBlendMode(
    BlendMode value) noexcept {
    SetValue(BlendModeProperty, value);
}

// from src/gui/controls/Layout.cpp

void UIElement::SetClipToBounds(bool value) noexcept {
    SetValue(ClipToBoundsProperty, value);
}

// from src/gui/controls/Layout.cpp

void UIElement::OnPropertyInvalidated(
    PropertyInvalidationFlags flags) noexcept {
    if (HasFlag(flags, PropertyInvalidationFlags::Measure)) {
        (void)InvalidateMeasure();
    } else if (HasFlag(flags, PropertyInvalidationFlags::Arrange)) {
        (void)InvalidateArrange();
    }
    UIElement* parent = AeroGuiInternal::Layout(*this).layoutAttached ? LayoutParent() : nullptr;
    if (parent != nullptr &&
        HasFlag(flags, PropertyInvalidationFlags::ParentMeasure)) {
        (void)parent->InvalidateMeasure();
    } else if (parent != nullptr &&
        HasFlag(flags, PropertyInvalidationFlags::ParentArrange)) {
        (void)parent->InvalidateArrange();
    }
    if (HasFlag(flags, PropertyInvalidationFlags::Render)) {
        static_cast<void>(
            AeroGuiInternal::InvalidateRenderState(*this));
    }
    DependencyObject::OnPropertyInvalidated(flags);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsFocusScope() const noexcept {
    return GetValueOr(IsFocusScopeProperty, false);
}

// from src/gui/controls/Layout.cpp
std::uint32_t UIElement::GetTabIndex() const noexcept {
    return GetValueOr(TabIndexProperty, 0U);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsTabStop() const noexcept {
    return GetValueOr(IsTabStopProperty, false);
}

// from src/gui/controls/Layout.cpp
Base::Result<bool> UIElement::Focus() noexcept {
    Aero::InputRouter* input =
        AeroGuiInternal::InputRouterOf(*this);
    if (input == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "UIElement Focus requires a mounted View");
    }
    return input->SetFocus(this);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetFocusable() const noexcept {
    return GetValueOr(FocusableProperty, false);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsKeyboardFocusWithin() const noexcept {
    return GetValueOr(IsKeyboardFocusWithinProperty, false);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsKeyboardFocused() const noexcept {
    return GetValueOr(IsKeyboardFocusedProperty, false);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsPressed() const noexcept {
    return GetValueOr(IsPressedProperty, false);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsMouseOver() const noexcept {
    return GetValueOr(IsMouseOverProperty, false);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetAllowDrop() const noexcept {
    return GetValueOr(AllowDropProperty, false);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsEnabled() const noexcept {
    if (!GetValueOr(IsEnabledProperty, true)) return false;
    ::Aero::Media::Visual* parent = GetLogicalParent() != nullptr
        ? GetLogicalParent() : GetVisualParent();
    const UIElement* parentElement =
        parent != nullptr ? parent->AsUIElement() : nullptr;
    return parentElement == nullptr || parentElement->GetIsEnabled();
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsVisible() const noexcept {
    const ::Aero::Media::Visual* current = this;
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

// from src/gui/controls/Layout.cpp
Visibility UIElement::GetVisibility() const noexcept {
    return GetValueOr(
        VisibilityProperty, Visibility::Visible);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsHitTestVisible() const noexcept {
    return GetValueOr(IsHitTestVisibleProperty, true);
}

// from src/gui/controls/Layout.cpp
double UIElement::GetOpacity() const noexcept {
    return GetValueOr(OpacityProperty, 1.0);
}

// from src/gui/controls/Layout.cpp

Base::Ref<Effect> UIElement::GetEffect() const noexcept {
    return GetValueOr(
        EffectProperty,
        Base::Ref<Effect>{});
}

// from src/gui/controls/Layout.cpp
BlendMode UIElement::GetBlendMode() const noexcept {
    return GetValueOr(
        BlendModeProperty, BlendMode::Normal);
}

// from src/gui/controls/Layout.cpp

bool UIElement::GetClipToBounds() const noexcept {
    return GetValueOr(ClipToBoundsProperty, false);
}

// from src/gui/controls/Layout.cpp

Base::Result<void> UIElement::InvalidateArrange() noexcept {
    auto* layout = static_cast<Aero::LayoutEngine*>(
        AeroGuiInternal::LayoutEngineOf(*this));
    if (layout == nullptr) {
        AeroGuiInternal::Layout(*this).arrangeValid = false;
        return {};
    }
    return layout->InvalidateArrange(*this);
}

// from src/gui/controls/Layout.cpp

Base::Result<void> UIElement::InvalidateMeasure() noexcept {
    auto* layout = static_cast<Aero::LayoutEngine*>(
        AeroGuiInternal::LayoutEngineOf(*this));
    if (layout == nullptr) {
        AeroGuiInternal::Layout(*this).measureValid = false;
        AeroGuiInternal::Layout(*this).arrangeValid = false;
        return {};
    }
    return layout->InvalidateMeasure(*this);
}

// from src/gui/controls/Layout.cpp

void UIElement::RaiseEvent(
    RoutedEventHandle event,
    RoutedEventArgs* args) noexcept {
    Aero::EventRouter* eventRouter =
        AeroGuiInternal::EventRouterOf(*this);
    if (eventRouter == nullptr) {
        return;
    }
    static_cast<void>(eventRouter->RaiseEvent(*this, event, args));
}

// from src/gui/controls/Layout.cpp

void UIElement::CleanupHandlers() noexcept {
    auto* state = static_cast<UIElementHandlerState*>((rare_ != nullptr ? rare_->routedHandlers : nullptr));
    if (state == nullptr) return;
    state->~UIElementHandlerState();
    Base::GetDefaultAllocator().Deallocate(
        state,
        sizeof(UIElementHandlerState),
        alignof(UIElementHandlerState),
        Base::MemoryTag::Ui);
    if (rare_ != nullptr) rare_->routedHandlers = nullptr;
}

// from src/gui/controls/Layout.cpp

void UIElement::InvokeHandlers(
    RoutedEventHandle event,
    RoutedEventArgs& args) noexcept {
    auto* state = static_cast<UIElementHandlerState*>((rare_ != nullptr ? rare_->routedHandlers : nullptr));
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

// from src/gui/controls/Layout.cpp

bool UIElement::RemoveHandlerCore(
    RoutedEventHandle event,
    const HandlerDescriptor& handler) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access || !event.IsValid() || handler.value == nullptr ||
        handler.operations == nullptr || (rare_ != nullptr ? rare_->routedHandlers : nullptr) == nullptr) {
        return false;
    }
    Aero::RoutedHandlerStorage probe(
        handler.value,
        handler.operations->size,
        handler.operations->alignment,
        handler.argsType,
        handler.operations->copy,
        handler.operations->destroy,
        handler.operations->equals,
        handler.operations->invoke);
    auto& handlers = static_cast<UIElementHandlerState*>((rare_ != nullptr ? rare_->routedHandlers : nullptr))->handlers;
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

// from src/gui/controls/Layout.cpp

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

    auto* state = static_cast<UIElementHandlerState*>((rare_ != nullptr ? rare_->routedHandlers : nullptr));
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
        EnsureRare().routedHandlers = state;
    }
    if (state->nextSequence == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Routed event handler sequence space exhausted");
    }

    RoutedHandlerRecord record;
    record.event = event;
    record.handler = Aero::RoutedHandlerStorage(
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

// from src/gui/input/Input.cpp

void UIElement::SetAllowDrop(bool value) noexcept {
    SetValue(AllowDropProperty, value);
}

// from src/gui/input/Input.cpp

Base::Result<bool> UIElement::CancelDrag() noexcept {
    Aero::InputRouter* input =
        AeroGuiInternal::InputRouterOf(*this);
    if (input == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "UIElement CancelDrag requires a mounted View");
    }
    return input->CancelDrag();
}

// from src/gui/input/Input.cpp

Base::Result<void> UIElement::BeginDrag(
    std::uint32_t pointerId,
    const Value& data,
    Input::DragDropEffects allowedEffects) noexcept {
    Aero::InputRouter* input =
        AeroGuiInternal::InputRouterOf(*this);
    if (input == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "UIElement BeginDrag requires a mounted View");
    }
    return input->BeginDrag(
        *this, pointerId, data, allowedEffects);
}

// from src/gui/input/Input.cpp

bool UIElement::GetIsDragging() const noexcept {
    Aero::InputRouter* input =
        AeroGuiInternal::InputRouterOf(*this);
    return input != nullptr && input->IsDragSource(*this);
}

UIElement::Rare& UIElement::EnsureRare() noexcept {
    if (rare_ == nullptr) {
        rare_ = new (std::nothrow) Rare();
        AERO_ASSERT(rare_ != nullptr);
    }
    return *rare_;
}

} // namespace Aero
