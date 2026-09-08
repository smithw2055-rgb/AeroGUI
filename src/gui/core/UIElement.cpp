// Auto-relocated base-class method definitions (WPF semantic kernel).
#include <Aero/UIElement.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Allocator.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Events.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Controls.hpp>
#include <cmath>
#include <cstdio>
#include <new>
#include <Aero/FocusManager.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/FrameworkElement.hpp>
#include "gui/meta/ElementsFill.hpp"
#include "gui/meta/RenderStateCallbacks.hpp"
#include "gui/core/state/ElementTree.hpp"
#include "gui/core/state/LayoutEngine.hpp"
#include "gui/core/state/FreezableState.hpp"
#include "gui/core/state/EffectiveValueEngine.hpp"
#include "gui/core/state/RoutedEvents.hpp"
#include "gui/core/state/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/input/InputState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/meta/MetadataState.hpp"
#include "gui/core/DependencyPropertyRegistry.hpp"
#include "gui/internal/ErasedRoutedHandler.hpp"

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

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

[[maybe_unused]] Base::Status NotFound(const char* message) noexcept {
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
            AeroGuiInternal::PropertyRegistry(*this).Types().FindType(
                RuntimeType());
        const TypeInfo* childType =
            AeroGuiInternal::PropertyRegistry(child).Types().FindType(
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
            ? AeroGuiInternal::PropertyRegistry(*this).Types().FindType(
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
            AeroGuiInternal::PropertyRegistry(*this).Types().FindType(
                RuntimeType());
        const TypeInfo* childType =
            AeroGuiInternal::PropertyRegistry(child).Types().FindType(
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
            ? AeroGuiInternal::PropertyRegistry(*this).Types().FindType(
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

std::uint32_t UIElement::GetLayoutChildrenCount() const noexcept {
    return GetVisualChildrenCount();
}

UIElement* UIElement::GetLayoutChild(std::uint32_t index) const noexcept {
    ::Aero::Media::Visual* child = GetVisualChild(index);
    return child != nullptr ? ::Aero::TryCast<::Aero::UIElement>(child) : nullptr;
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
    return GetValue(RenderTransformOriginProperty);
}

// from src/gui/controls/Layout.cpp
Base::Ref<Transform> UIElement::GetRenderTransform() const noexcept {
    Base::Result<Base::Ref<Transform>> value =
        GetValue(RenderTransformProperty);
    return value ? std::move(value).Value() : Base::Ref<Transform>{};
}

Base::Ref<Media::Transform3D> UIElement::GetTransform3D() const noexcept {
    Base::Result<Base::Ref<Media::Transform3D>> value =
        GetValue(Transform3DProperty);
    if (value && value.Value()) {
        return std::move(value).Value();
    }
    value = GetValue(Element::Transform3DProperty);
    return value
        ? std::move(value).Value()
        : Base::Ref<Media::Transform3D>{};
}

void UIElement::SetTransform3D(
    Base::Ref<Media::Transform3D> value) noexcept {
    SetValue(Transform3DProperty, std::move(value));
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
    return GetValue(OpacityMaskProperty);
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

void UIElement::SetClip(Base::Ref<Geometry> value) noexcept {
    SetValue(ClipProperty, std::move(value));
}

Base::Ref<Geometry> UIElement::GetClip() const noexcept {
    return GetValue(ClipProperty);
}

void UIElement::AddInputBinding(
    Base::Ref<Input::InputBinding> binding) noexcept {
    if (!binding) { AERO_ASSERT(false); return; }
    Base::Result<void> finalized = binding->Finalize();
    if (!finalized) { AERO_ASSERT(false); return; }
    Rare& rare = EnsureRare();
    auto*& storage = reinterpret_cast<Base::Vector<Base::Ref<Input::InputBinding>>*&>(
        rare.inputBindings);
    if (storage == nullptr) {
        storage = new (std::nothrow) Base::Vector<Base::Ref<Input::InputBinding>>();
        if (storage == nullptr) { AERO_ASSERT(false); return; }
    }
    Base::Result<void> pushed = storage->PushBack(std::move(binding));
    if (!pushed) { AERO_ASSERT(false); return; }
}

void UIElement::ClearInputBindings() noexcept {
    if (rare_ == nullptr || rare_->inputBindings == nullptr) return;
    static_cast<Base::Vector<Base::Ref<Input::InputBinding>>*>(
        rare_->inputBindings)->Clear();
}

Base::Span<const Base::Ref<Input::InputBinding>>
UIElement::GetInputBindings() const noexcept {
    if (rare_ == nullptr || rare_->inputBindings == nullptr) {
        return {};
    }
    const auto* storage = static_cast<
        const Base::Vector<Base::Ref<Input::InputBinding>>*>(rare_->inputBindings);
    return {storage->Data(), storage->Size()};
}

void UIElement::AddCommandBinding(
    Base::Ref<Input::CommandBinding> binding) noexcept {
    if (!binding) { AERO_ASSERT(false); return; }
    Base::Result<void> finalized = binding->Finalize();
    if (!finalized) { AERO_ASSERT(false); return; }
    Rare& rare = EnsureRare();
    auto*& storage = reinterpret_cast<Base::Vector<Base::Ref<Input::CommandBinding>>*&>(
        rare.commandBindings);
    if (storage == nullptr) {
        storage = new (std::nothrow) Base::Vector<Base::Ref<Input::CommandBinding>>();
        if (storage == nullptr) { AERO_ASSERT(false); return; }
    }
    Base::Result<void> pushed = storage->PushBack(std::move(binding));
    if (!pushed) { AERO_ASSERT(false); return; }
}

void UIElement::ClearCommandBindings() noexcept {
    if (rare_ == nullptr || rare_->commandBindings == nullptr) return;
    static_cast<Base::Vector<Base::Ref<Input::CommandBinding>>*>(
        rare_->commandBindings)->Clear();
}

Base::Span<const Base::Ref<Input::CommandBinding>>
UIElement::GetCommandBindings() const noexcept {
    if (rare_ == nullptr || rare_->commandBindings == nullptr) {
        return {};
    }
    const auto* storage = static_cast<
        const Base::Vector<Base::Ref<Input::CommandBinding>>*>(rare_->commandBindings);
    return {storage->Data(), storage->Size()};
}

// from src/gui/controls/Layout.cpp

void UIElement::OnPropertyInvalidated(
    PropertyInvalidationFlags flags) noexcept {
    if (HasFlag(flags, PropertyInvalidationFlags::Measure)) {
        InvalidateMeasure();
    } else if (HasFlag(flags, PropertyInvalidationFlags::Arrange)) {
        InvalidateArrange();
    }
    UIElement* parent = AeroGuiInternal::Layout(*this).layoutAttached ? LayoutParent() : nullptr;
    if (parent != nullptr &&
        HasFlag(flags, PropertyInvalidationFlags::ParentMeasure)) {
        parent->InvalidateMeasure();
    } else if (parent != nullptr &&
        HasFlag(flags, PropertyInvalidationFlags::ParentArrange)) {
        parent->InvalidateArrange();
    }
    if (HasFlag(flags, PropertyInvalidationFlags::Render)) {
        static_cast<void>(
            AeroGuiInternal::InvalidateRenderState(*this));
    }
    DependencyObject::OnPropertyInvalidated(flags);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsFocusScope() const noexcept {
    return GetValue(IsFocusScopeProperty);
}

// from src/gui/controls/Layout.cpp
std::uint32_t UIElement::GetTabIndex() const noexcept {
    return GetValue(TabIndexProperty);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsTabStop() const noexcept {
    return GetValue(IsTabStopProperty);
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
    return GetValue(FocusableProperty);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsKeyboardFocusWithin() const noexcept {
    return GetValue(IsKeyboardFocusWithinProperty);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsKeyboardFocused() const noexcept {
    return GetValue(IsKeyboardFocusedProperty);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsPressed() const noexcept {
    return GetValue(IsPressedProperty);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsMouseOver() const noexcept {
    return GetValue(IsMouseOverProperty);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetAllowDrop() const noexcept {
    return GetValue(AllowDropProperty);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsEnabled() const noexcept {
    if (!GetValue(IsEnabledProperty)) return false;
    ::Aero::Media::Visual* parent = ::Aero::TryCast<::Aero::Media::Visual>(GetLogicalParent());
    if (parent == nullptr) parent = GetVisualParent();
    const UIElement* parentElement =
        parent != nullptr ? ::Aero::TryCast<::Aero::UIElement>(parent) : nullptr;
    return parentElement == nullptr || parentElement->GetIsEnabled();
}

void UIElement::OnVisualChildrenChanged(
    ::Aero::Media::Visual*,
    ::Aero::Media::Visual*) noexcept {
    InvalidateMeasure();
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsVisible() const noexcept {
    const ::Aero::Media::Visual* current = this;
    while (current != nullptr) {
        const UIElement* element = ::Aero::TryCast<::Aero::UIElement>(current);
        if (element != nullptr &&
            element->GetVisibility() != Visibility::Visible) {
            return false;
        }
        current = ::Aero::TryCast<::Aero::Media::Visual>(current->GetLogicalParent()) != nullptr ? ::Aero::TryCast<::Aero::Media::Visual>(current->GetLogicalParent()) : current->GetVisualParent();
    }
    return true;
}

// from src/gui/controls/Layout.cpp
Visibility UIElement::GetVisibility() const noexcept {
    return GetValue(VisibilityProperty);
}

// from src/gui/controls/Layout.cpp
bool UIElement::GetIsHitTestVisible() const noexcept {
    return GetValue(IsHitTestVisibleProperty);
}

// from src/gui/controls/Layout.cpp
double UIElement::GetOpacity() const noexcept {
    return GetValue(OpacityProperty);
}

// from src/gui/controls/Layout.cpp

Base::Ref<Effect> UIElement::GetEffect() const noexcept {
    return GetValue(EffectProperty);
}

// from src/gui/controls/Layout.cpp
BlendMode UIElement::GetBlendMode() const noexcept {
    return GetValue(BlendModeProperty);
}

// from src/gui/controls/Layout.cpp

bool UIElement::GetClipToBounds() const noexcept {
    return GetValue(ClipToBoundsProperty);
}

// from src/gui/controls/Layout.cpp

void UIElement::InvalidateArrange() noexcept {
    auto* layout = static_cast<Aero::LayoutEngine*>(
        AeroGuiInternal::LayoutEngineOf(*this));
    if (layout == nullptr) {
        AeroGuiInternal::Layout(*this).arrangeValid = false;
        return;
    }
    layout->InvalidateArrange(*this);
}

// from src/gui/controls/Layout.cpp

void UIElement::InvalidateMeasure() noexcept {
    auto* layout = static_cast<Aero::LayoutEngine*>(
        AeroGuiInternal::LayoutEngineOf(*this));
    if (layout == nullptr) {
        AeroGuiInternal::Layout(*this).measureValid = false;
        AeroGuiInternal::Layout(*this).arrangeValid = false;
        return;
    }
    layout->InvalidateMeasure(*this);
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

Base::Result<void> UIElement::EnsureRoutedHandlers() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    auto* state = static_cast<UIElementHandlerState*>(
        (rare_ != nullptr ? rare_->routedHandlers : nullptr));
    if (state != nullptr) return Base::Result<void>();
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
    return Base::Result<void>();
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

bool UIElement::RemoveHandlerErased(
    RoutedEventHandle event,
    const void* handler,
    std::size_t size,
    std::size_t alignment,
    Meta::TypeId argsType) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access || !event.IsValid() || handler == nullptr ||
        (rare_ != nullptr ? rare_->routedHandlers : nullptr) == nullptr) {
        return false;
    }
    Aero::RoutedHandlerStorage probe(
        handler,
        size,
        alignment,
        argsType,
        &CopyErasedDelegate,
        &DestroyErasedDelegate,
        &EqualsErasedDelegate,
        &InvokeErasedDelegate);
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

void UIElement::AddHandlerErased(
    RoutedEventHandle event,
    const void* handler,
    std::size_t size,
    std::size_t alignment,
    Meta::TypeId argsType,
    bool handledEventsToo) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    if (!event.IsValid() || handler == nullptr ||
        size > 4U * sizeof(void*) ||
        alignment > alignof(void*)) {
        return;
    }

    auto* state = static_cast<UIElementHandlerState*>((rare_ != nullptr ? rare_->routedHandlers : nullptr));
    if (state == nullptr) {
        Base::IAllocator& allocator = Base::GetDefaultAllocator();
        void* memory = allocator.Allocate({
            sizeof(UIElementHandlerState),
            alignof(UIElementHandlerState),
            Base::MemoryTag::Ui});
        if (memory == nullptr) {
            return;
        }
        state = new (memory) UIElementHandlerState();
        EnsureRare().routedHandlers = state;
    }
    if (state->nextSequence == 0U) {
        return;
    }

    RoutedHandlerRecord record;
    record.event = event;
    record.handler = Aero::RoutedHandlerStorage(
        handler,
        size,
        alignment,
        argsType,
        &CopyErasedDelegate,
        &DestroyErasedDelegate,
        &EqualsErasedDelegate,
        &InvokeErasedDelegate);
    record.sequence = state->nextSequence++;
    record.handledEventsToo = handledEventsToo;
    (void)state->handlers.PushBack(std::move(record));
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

UIElement* UIElementChildRange::Iterator::operator*() const noexcept {
    return owner_ != nullptr ? owner_->GetLayoutChild(index_) : nullptr;
}

void UIElementChildRange::Iterator::Advance() noexcept {
    if (owner_ == nullptr) return;
    const std::uint32_t count = owner_->GetLayoutChildrenCount();
    while (index_ < count) {
        if (owner_->GetLayoutChild(index_) != nullptr) return;
        ++index_;
    }
}

UIElementChildRange::Iterator UIElementChildRange::end() const noexcept {
    const std::uint32_t count =
        owner_ != nullptr ? owner_->GetLayoutChildrenCount() : 0U;
    return Iterator(owner_, count);
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

UIElement::UIElement(TypeId runtimeType) noexcept
    : ::Aero::Media::Visual(runtimeType) {}

UIElement* UIElement::LayoutParent() const noexcept {
    ::Aero::Media::Visual* parent = GetVisualParent();
    return parent != nullptr ? ::Aero::TryCast<UIElement>(parent) : nullptr;
}

UIElement::~UIElement() {
    layout_.layoutAttached = false;
    layout_.measureQueued = false;
    layout_.arrangeQueued = false;
    CleanupHandlers();
    if (rare_ != nullptr && rare_->inputBindings != nullptr) {
        delete static_cast<Base::Vector<Base::Ref<Input::InputBinding>>*>(
            rare_->inputBindings);
        rare_->inputBindings = nullptr;
    }
    if (rare_ != nullptr && rare_->commandBindings != nullptr) {
        delete static_cast<Base::Vector<Base::Ref<Input::CommandBinding>>*>(
            rare_->commandBindings);
        rare_->commandBindings = nullptr;
    }
    delete rare_;
    rare_ = nullptr;
}

} // namespace Aero

// ---- Shared RenderState Callbacks (declared in gui/meta/RenderStateCallbacks.hpp) ----
namespace Aero {

bool ValidateUnitDouble(
    const double& value) noexcept {
    return std::isfinite(value) &&
        value >= 0.0 && value <= 1.0;
}

void OnRenderStateChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept {
    auto& visual =
        static_cast<UIElement&>(object);
    static_cast<void>(
        AeroGuiInternal::
            InvalidateRenderState(visual));
}

void OnOpacityMaskChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept {
    FrameworkElement* owner =
        ::Aero::TryCast<::Aero::FrameworkElement>(&object);
    if (owner == nullptr) return;
    static_cast<void>(
        AeroGuiInternal::
            InvalidateRenderState(*owner));
}

void OnRenderTransformChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept {
    FrameworkElement* owner =
        ::Aero::TryCast<::Aero::FrameworkElement>(&object);
    if (owner == nullptr) return;
    static_cast<void>(
        AeroGuiInternal::
            InvalidateRenderState(*owner));
}

void OnEffectChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept {
    FrameworkElement* owner =
        ::Aero::TryCast<::Aero::FrameworkElement>(&object);
    if (owner == nullptr) return;
    static_cast<void>(
        AeroGuiInternal::
            InvalidateRenderState(*owner));
}

} // namespace Aero

// ---- Fill helpers (single TU use) ----
namespace {

void AddUiElementInputBinding(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Input::InputBinding* binding = ::Aero::TryCast<Input::InputBinding>(value.Get());
    if (binding == nullptr) return;
    Base::Ref<Input::InputBinding> retained =
        Base::Ref<Input::InputBinding>::TryFromBorrowed(*binding);
    if (retained) {
        (void)static_cast<UIElement&>(owner).AddInputBinding(
            std::move(retained));
    }
}

void ClearUiElementInputBindings(
    Base::Object& owner,
    void*) noexcept {
    static_cast<UIElement&>(owner).ClearInputBindings();
}

void AddUiElementCommandBinding(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Input::CommandBinding* binding = ::Aero::TryCast<Input::CommandBinding>(value.Get());
    if (binding == nullptr) return;
    Base::Ref<Input::CommandBinding> retained =
        Base::Ref<Input::CommandBinding>::TryFromBorrowed(*binding);
    if (retained) {
        (void)static_cast<UIElement&>(owner).AddCommandBinding(
            std::move(retained));
    }
}

void ClearUiElementCommandBindings(
    Base::Object& owner,
    void*) noexcept {
    static_cast<UIElement&>(owner).ClearCommandBindings();
}

} // namespace

// ---- Builtin metadata Fill (colocated from meta/Elements.inl) ----
namespace Aero::Meta {

using namespace ::Aero::Input;

Base::Result<void> FillUIElementMetadata(
    Registration& context) noexcept {
    Register<UIElement>(context, TypeFlags::Abstract)
        .Event(UIElement::PreviewMouseMoveEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::MouseMoveEvent)
        .Event(UIElement::MouseEnterEvent, RoutingStrategy::Direct)
        .Event(UIElement::MouseLeaveEvent, RoutingStrategy::Direct)
        .Event(UIElement::PreviewMouseDownEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::MouseDownEvent)
        .Event(UIElement::PreviewMouseLeftButtonDownEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::MouseLeftButtonDownEvent)
        .Event(UIElement::PreviewMouseUpEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::MouseUpEvent)
        .Event(UIElement::PreviewMouseWheelEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::MouseWheelEvent)
        .Event(UIElement::PreviewMouseLeftButtonUpEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::MouseLeftButtonUpEvent)
        .Event(UIElement::PreviewDragEnterEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::DragEnterEvent)
        .Event(UIElement::PreviewDragLeaveEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::DragLeaveEvent)
        .Event(UIElement::PreviewDragOverEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::DragOverEvent)
        .Event(UIElement::PreviewDropEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::DropEvent)
        .Event(UIElement::GiveFeedbackEvent)
        .Event(UIElement::DragCompletedEvent)
        .Event(UIElement::GotKeyboardFocusEvent)
        .Event(UIElement::LostKeyboardFocusEvent)
        .Event(UIElement::PreviewKeyDownEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::KeyDownEvent)
        .Event(UIElement::PreviewKeyUpEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::KeyUpEvent)
        .Event(UIElement::PreviewTextInputEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::TextInputEvent)
        .Property(
            UIElement::ClipToBoundsProperty,
            FrameworkPropertyMetadata(false, AffectsArrange)
                .Changed(&OnRenderStateChanged))
        .Property(
            UIElement::ClipProperty,
            FrameworkPropertyMetadata(Base::Ref<Geometry>{}, AffectsRender)
                .Changed(&OnRenderStateChanged))
        .Property(
            UIElement::BlendModeProperty,
            FrameworkPropertyMetadata(BlendMode::Normal)
                .Changed(&OnRenderStateChanged))
        .Property(
            UIElement::EffectProperty,
            FrameworkPropertyMetadata(Base::Ref<Effect>{}, AffectsRender)
                .Changed(&OnEffectChanged))
        .Property(
            UIElement::OpacityMaskProperty,
            FrameworkPropertyMetadata(Base::Ref<Brush>{}, AffectsRender)
                .Changed(&OnOpacityMaskChanged))
        .Property(
            UIElement::IsHitTestVisibleProperty,
            true)
        .Property(
            UIElement::VisibilityProperty,
            FrameworkPropertyMetadata(Visibility::Visible, AffectsMeasure)
                .Changed(&OnRenderStateChanged))
        .Property(
            UIElement::IsEnabledProperty,
            true, Inherits | AffectsRender)
        .Property(
            UIElement::AllowDropProperty,
            false)
        .Property(
            UIElement::IsMouseOverProperty,
            false, AffectsRender)
        .Property(
            UIElement::IsPressedProperty,
            false, AffectsRender)
        .Property(
            UIElement::IsKeyboardFocusedProperty,
            false, AffectsRender)
        .Property(
            UIElement::IsKeyboardFocusWithinProperty,
            false, AffectsRender)
        .Property(
            UIElement::FocusableProperty,
            false)
        .AddOwner(
            KeyboardNavigation::IsTabStopProperty,
            false)
        .AddOwner(
            KeyboardNavigation::TabIndexProperty,
            std::uint32_t{0})
        .AddOwner(
            FocusManager::IsFocusScopeProperty,
            false)
        .Property(
            UIElement::OpacityProperty,
            FrameworkPropertyMetadata(1.0)
                .Changed(&OnRenderStateChanged)
                .Validate(&ValidateUnitDouble))
        .Property(
            UIElement::RenderTransformProperty,
            FrameworkPropertyMetadata(Base::Ref<Transform>{}, AffectsRender)
                .Changed(&OnRenderTransformChanged))
        .Property(
            UIElement::Transform3DProperty,
            FrameworkPropertyMetadata(Base::Ref<Media::Transform3D>{}, AffectsRender)
                .Changed(&OnRenderTransformChanged))
        .Property(
            UIElement::RenderTransformOriginProperty,
            FrameworkPropertyMetadata(Point{})
                .Changed(&OnRenderStateChanged))
        .Collection<InputBinding>(
            "InputBindings",
            &AddUiElementInputBinding,
            &ClearUiElementInputBindings)
        .Collection<CommandBinding>(
            "CommandBindings",
            &AddUiElementCommandBinding,
            &ClearUiElementCommandBindings);
    return {};
}

} // namespace Aero::Meta

