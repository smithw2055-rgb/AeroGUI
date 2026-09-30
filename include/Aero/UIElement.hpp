#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/ElementEnums.hpp>
#include <Aero/Visual.hpp>
#include <Aero/VisualTreeHelper.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Events/Event.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Input.hpp>

#include <cstddef>

namespace Aero {
class LayoutEngine;
class ElementTree;
class AnimationEngine;
class PointerStateMachine;
class FocusState;
class EventRouter;
namespace Controls { namespace Primitives { class ButtonBase; } }

using Point = Base::Point;
using Size = Base::Size;
using Rect = Base::Rect;

using Meta::PropertyInvalidationFlags;
using Meta::TypeId;

class UIElement;
namespace Media { class Transform; class Transform3D; class Effect; class Brush; class Geometry; class DrawingContext; }
namespace Input { class RoutedCommand; class InputBinding; class CommandBinding; }

class UIElementChildRange {
public:
    class Iterator {
    public:
        Iterator(const UIElement* owner, std::uint32_t index) noexcept : owner_(owner), index_(index) { Advance(); }
        UIElement* operator*() const noexcept;
        Iterator& operator++() noexcept { ++index_; Advance(); return *this; }
        bool operator!=(const Iterator& other) const noexcept { return owner_ != other.owner_ || index_ != other.index_; }
        bool operator==(const Iterator& other) const noexcept { return !(*this != other); }

    private:
        const UIElement* owner_ = nullptr;
        std::uint32_t index_ = 0U;
        void Advance() noexcept;
    };

    explicit UIElementChildRange(const UIElement& owner) noexcept : owner_(&owner) {}
    Iterator begin() const noexcept { return Iterator(owner_, 0U); }
    Iterator end() const noexcept;
    bool Empty() const noexcept { return begin() == end(); }
    std::uint32_t Size() const noexcept;
    UIElement* operator[](std::uint32_t index) const noexcept;

private:
    const UIElement* owner_ = nullptr;
};

class AERO_GUI_API UIElement : public ::Aero::Media::Visual {
    AERO_DECLARE_TYPE(UIElement, ::Aero::Media::Visual)

public:
    template<class TArgs> using Event = ::Aero::Event<UIElement, TArgs>;

    explicit UIElement(TypeId runtimeType) noexcept;
    ~UIElement() override;

    template<class TOwner, class TArgs> auto GetEvent(
        const Aero::RoutedEventRef<TOwner, TArgs>& event) noexcept { return Event<TArgs>(*this, event.Handle()); }
    UIElement* LayoutParent() const noexcept;
    void InvalidateMeasure() noexcept;
    void InvalidateArrange() noexcept;
    void InvalidateVisual() noexcept;
    // WPF-friendly non-virtual entry points. They forward to the internal
    // MeasureCore/ArrangeCore so LayoutEngine stays in .cpp and public
    // headers no longer leak LayoutEngine&.
    void Measure(Size availableSize) noexcept;
    void Arrange(Rect finalRect) noexcept;
    Result<void> BeginDrag(std::uint32_t pointerId, const Value& data,
        Input::DragDropEffects allowedEffects = Input::DragDropEffects::Move) noexcept;
    Result<bool> CancelDrag() noexcept;
    Result<void> CapturePointer(std::uint32_t pointerId = 0U) noexcept;
    Result<bool> ReleasePointer(std::uint32_t pointerId = 0U) noexcept;
    Result<void> CaptureMouse() noexcept { return CapturePointer(0U); }
    Result<bool> ReleaseMouseCapture() noexcept { return ReleasePointer(0U); }
    Result<bool> Focus() noexcept;
    void AddInputBinding(Ref<Input::InputBinding> binding) noexcept;
    void ClearInputBindings() noexcept;
    void AddCommandBinding(Ref<Input::CommandBinding> binding) noexcept;
    void ClearCommandBindings() noexcept;

    Size GetDesiredSize() const noexcept;
    Size GetRenderSize() const noexcept;
    Rect GetLayoutSlot() const noexcept;
    Rect GetLayoutClip() const noexcept;
    bool GetIsMeasureValid() const noexcept;
    bool GetIsArrangeValid() const noexcept;
    bool GetIsMeasureQueued() const noexcept;
    bool GetIsArrangeQueued() const noexcept;
    bool GetIsMeasuring() const noexcept;
    bool GetIsArranging() const noexcept;
    bool GetIsLayoutAttached() const noexcept;
    Size GetUntransformedDesiredSize() const noexcept;
    Size GetPreviousMeasureConstraint() const noexcept;
    bool GetClipToBounds() const noexcept;
    // Property operations
    void SetClipToBounds(bool value) noexcept;
    Ref<Media::Geometry> GetClip() const noexcept;
    void SetClip(Ref<Media::Geometry> value) noexcept;
    BlendMode GetBlendMode() const noexcept;
    void SetBlendMode(BlendMode value) noexcept;
    Ref<Media::Effect> GetEffect() const noexcept;
    void SetEffect(Ref<Media::Effect> value) noexcept;
    Ref<Media::Brush> GetOpacityMask() const noexcept;
    void SetOpacityMask(Ref<Media::Brush> value) noexcept;
    double GetOpacity() const noexcept;
    bool GetIsHitTestVisible() const noexcept;
    void SetIsHitTestVisible(bool value) noexcept;
    Visibility GetVisibility() const noexcept;
    void SetVisibility(Visibility value) noexcept;
    bool GetIsVisible() const noexcept;
    bool GetIsEnabled() const noexcept;
    void SetIsEnabled(bool value) noexcept;
    bool GetAllowDrop() const noexcept;
    void SetAllowDrop(bool value) noexcept;
    bool GetIsDragging() const noexcept;
    bool GetIsMouseOver() const noexcept;
    bool GetIsPressed() const noexcept;
    bool GetIsKeyboardFocused() const noexcept;
    bool GetIsKeyboardFocusWithin() const noexcept;
    bool GetFocusable() const noexcept;
    bool GetIsTabStop() const noexcept;
    void SetIsTabStop(bool value) noexcept;
    std::uint32_t GetTabIndex() const noexcept;
    void SetTabIndex(std::uint32_t value) noexcept;
    bool GetIsFocusScope() const noexcept;
    void SetIsFocusScope(bool value) noexcept;
    Ref<Media::Transform> GetRenderTransform() const noexcept;
    void SetRenderTransform(Ref<Media::Transform> value) noexcept;
    Ref<Media::Transform3D> GetTransform3D() const noexcept;
    void SetTransform3D(Ref<Media::Transform3D> value) noexcept;
    Point GetRenderTransformOrigin() const noexcept;
    void SetRenderTransformOrigin(Point value) noexcept;
    std::uint64_t GetLayoutRevision() const noexcept;
    Base::Span<const Ref<Input::InputBinding>> GetInputBindings() const noexcept;
    Base::Span<const Ref<Input::CommandBinding>> GetCommandBindings() const noexcept;

    inline static constexpr RoutedEvent<MouseEventArgs> PreviewMouseMoveEvent{"PreviewMouseMove"};
    Event<MouseEventArgs> PreviewMouseMove() noexcept { return GetEvent(PreviewMouseMoveEvent); }
    inline static constexpr RoutedEvent<MouseEventArgs> MouseMoveEvent{"MouseMove"};
    Event<MouseEventArgs> MouseMove() noexcept { return GetEvent(MouseMoveEvent); }
    inline static constexpr RoutedEvent<MouseEventArgs> MouseEnterEvent{"MouseEnter"};
    Event<MouseEventArgs> MouseEnter() noexcept { return GetEvent(MouseEnterEvent); }
    inline static constexpr RoutedEvent<MouseEventArgs> MouseLeaveEvent{"MouseLeave"};
    Event<MouseEventArgs> MouseLeave() noexcept { return GetEvent(MouseLeaveEvent); }
    inline static constexpr RoutedEvent<MouseButtonEventArgs> PreviewMouseDownEvent{"PreviewMouseDown"};
    Event<MouseButtonEventArgs> PreviewMouseDown() noexcept { return GetEvent(PreviewMouseDownEvent); }
    inline static constexpr RoutedEvent<MouseButtonEventArgs> MouseDownEvent{"MouseDown"};
    Event<MouseButtonEventArgs> MouseDown() noexcept { return GetEvent(MouseDownEvent); }
    inline static constexpr RoutedEvent<MouseButtonEventArgs> PreviewMouseLeftButtonDownEvent{"PreviewMouseLeftButtonDown"};
    Event<MouseButtonEventArgs> PreviewMouseLeftButtonDown() noexcept { return GetEvent(PreviewMouseLeftButtonDownEvent); }
    inline static constexpr RoutedEvent<MouseButtonEventArgs> MouseLeftButtonDownEvent{"MouseLeftButtonDown"};
    Event<MouseButtonEventArgs> MouseLeftButtonDown() noexcept { return GetEvent(MouseLeftButtonDownEvent); }
    inline static constexpr RoutedEvent<MouseButtonEventArgs> MouseRightButtonDownEvent{"MouseRightButtonDown"};
    Event<MouseButtonEventArgs> MouseRightButtonDown() noexcept { return GetEvent(MouseRightButtonDownEvent); }
    inline static constexpr RoutedEvent<MouseButtonEventArgs> PreviewMouseUpEvent{"PreviewMouseUp"};
    Event<MouseButtonEventArgs> PreviewMouseUp() noexcept { return GetEvent(PreviewMouseUpEvent); }
    inline static constexpr RoutedEvent<MouseButtonEventArgs> MouseUpEvent{"MouseUp"};
    Event<MouseButtonEventArgs> MouseUp() noexcept { return GetEvent(MouseUpEvent); }
    inline static constexpr RoutedEvent<MouseWheelEventArgs> PreviewMouseWheelEvent{"PreviewMouseWheel"};
    Event<MouseWheelEventArgs> PreviewMouseWheel() noexcept { return GetEvent(PreviewMouseWheelEvent); }
    inline static constexpr RoutedEvent<MouseWheelEventArgs> MouseWheelEvent{"MouseWheel"};
    Event<MouseWheelEventArgs> MouseWheel() noexcept { return GetEvent(MouseWheelEvent); }
    inline static constexpr RoutedEvent<MouseButtonEventArgs> PreviewMouseLeftButtonUpEvent{"PreviewMouseLeftButtonUp"};
    Event<MouseButtonEventArgs> PreviewMouseLeftButtonUp() noexcept { return GetEvent(PreviewMouseLeftButtonUpEvent); }
    inline static constexpr RoutedEvent<MouseButtonEventArgs> MouseLeftButtonUpEvent{"MouseLeftButtonUp"};
    Event<MouseButtonEventArgs> MouseLeftButtonUp() noexcept { return GetEvent(MouseLeftButtonUpEvent); }
    inline static constexpr RoutedEvent<MouseButtonEventArgs> MouseRightButtonUpEvent{"MouseRightButtonUp"};
    Event<MouseButtonEventArgs> MouseRightButtonUp() noexcept { return GetEvent(MouseRightButtonUpEvent); }
    inline static constexpr RoutedEvent<DragEventArgs> PreviewDragEnterEvent{"PreviewDragEnter"};
    Event<DragEventArgs> PreviewDragEnter() noexcept { return GetEvent(PreviewDragEnterEvent); }
    inline static constexpr RoutedEvent<DragEventArgs> DragEnterEvent{"DragEnter"};
    Event<DragEventArgs> DragEnter() noexcept { return GetEvent(DragEnterEvent); }
    inline static constexpr RoutedEvent<DragEventArgs> PreviewDragLeaveEvent{"PreviewDragLeave"};
    Event<DragEventArgs> PreviewDragLeave() noexcept { return GetEvent(PreviewDragLeaveEvent); }
    inline static constexpr RoutedEvent<DragEventArgs> DragLeaveEvent{"DragLeave"};
    Event<DragEventArgs> DragLeave() noexcept { return GetEvent(DragLeaveEvent); }
    inline static constexpr RoutedEvent<DragEventArgs> PreviewDragOverEvent{"PreviewDragOver"};
    Event<DragEventArgs> PreviewDragOver() noexcept { return GetEvent(PreviewDragOverEvent); }
    inline static constexpr RoutedEvent<DragEventArgs> DragOverEvent{"DragOver"};
    Event<DragEventArgs> DragOver() noexcept { return GetEvent(DragOverEvent); }
    inline static constexpr RoutedEvent<DragEventArgs> PreviewDropEvent{"PreviewDrop"};
    Event<DragEventArgs> PreviewDrop() noexcept { return GetEvent(PreviewDropEvent); }
    inline static constexpr RoutedEvent<DragEventArgs> DropEvent{"Drop"};
    Event<DragEventArgs> Drop() noexcept { return GetEvent(DropEvent); }
    inline static constexpr RoutedEvent<GiveFeedbackEventArgs> GiveFeedbackEvent{"GiveFeedback"};
    Event<GiveFeedbackEventArgs> GiveFeedback() noexcept { return GetEvent(GiveFeedbackEvent); }
    inline static constexpr RoutedEvent<DragCompletedEventArgs> DragCompletedEvent{"DragCompleted"};
    Event<DragCompletedEventArgs> DragCompleted() noexcept { return GetEvent(DragCompletedEvent); }
    inline static constexpr RoutedEvent<KeyboardFocusChangedEventArgs> GotKeyboardFocusEvent{"GotKeyboardFocus"};
    Event<KeyboardFocusChangedEventArgs> GotKeyboardFocus() noexcept { return GetEvent(GotKeyboardFocusEvent); }
    inline static constexpr RoutedEvent<KeyboardFocusChangedEventArgs> LostKeyboardFocusEvent{"LostKeyboardFocus"};
    Event<KeyboardFocusChangedEventArgs> LostKeyboardFocus() noexcept { return GetEvent(LostKeyboardFocusEvent); }
    inline static constexpr RoutedEvent<KeyEventArgs> PreviewKeyDownEvent{"PreviewKeyDown"};
    Event<KeyEventArgs> PreviewKeyDown() noexcept { return GetEvent(PreviewKeyDownEvent); }
    inline static constexpr RoutedEvent<KeyEventArgs> KeyDownEvent{"KeyDown"};
    Event<KeyEventArgs> KeyDown() noexcept { return GetEvent(KeyDownEvent); }
    inline static constexpr RoutedEvent<KeyEventArgs> PreviewKeyUpEvent{"PreviewKeyUp"};
    Event<KeyEventArgs> PreviewKeyUp() noexcept { return GetEvent(PreviewKeyUpEvent); }
    inline static constexpr RoutedEvent<KeyEventArgs> KeyUpEvent{"KeyUp"};
    Event<KeyEventArgs> KeyUp() noexcept { return GetEvent(KeyUpEvent); }
    inline static constexpr RoutedEvent<TextCompositionEventArgs> PreviewTextInputEvent{"PreviewTextInput"};
    Event<TextCompositionEventArgs> PreviewTextInput() noexcept { return GetEvent(PreviewTextInputEvent); }
    inline static constexpr RoutedEvent<TextCompositionEventArgs> TextInputEvent{"TextInput"};
    Event<TextCompositionEventArgs> TextInput() noexcept { return GetEvent(TextInputEvent); }
    template<class TArgs> void AddHandler(
        RoutedEventHandle event, const Base::Delegate<void(Base::Object*, TArgs&)>& handler,
        bool handledEventsToo = false) noexcept {
        if (handler.Empty()) { return; }
        AddHandlerErased(event, &handler, sizeof(handler), alignof(decltype(handler)), TArgs::StaticTypeId(),
            handledEventsToo);
    }
    template<class TArgs> bool RemoveHandler(
        RoutedEventHandle event, const Base::Delegate<void(Base::Object*, TArgs&)>& handler) noexcept {
        return RemoveHandlerErased(event, &handler, sizeof(handler), alignof(decltype(handler)), TArgs::StaticTypeId());
    }
    // Most-derived handler for one routed event. New input events register
    // here instead of adding a virtual. Built-in controls do the same.
    using ClassHandler = void (*)(UIElement& element, RoutedEventArgs& args) noexcept;
    static void RegisterClassHandler(TypeId ownerType, RoutedEventHandle event, ClassHandler handler) noexcept;

    // Dependency properties
    AERO_DEPENDENCY_PROPERTY(bool, ClipToBounds);
    AERO_DEPENDENCY_PROPERTY(Ref<Media::Geometry>, Clip);
    AERO_DEPENDENCY_PROPERTY(BlendMode, BlendMode);
    AERO_DEPENDENCY_PROPERTY(Ref<Media::Effect>, Effect);
    AERO_DEPENDENCY_PROPERTY(Ref<Media::Brush>, OpacityMask);
    AERO_DEPENDENCY_PROPERTY(bool, IsHitTestVisible);
    AERO_DEPENDENCY_PROPERTY(Visibility, Visibility);
    AERO_DEPENDENCY_PROPERTY(bool, IsEnabled);
    AERO_DEPENDENCY_PROPERTY(bool, AllowDrop);
    AERO_READONLY_PROPERTY(bool, IsMouseOver);
    AERO_READONLY_PROPERTY(bool, IsPressed);
    AERO_READONLY_PROPERTY(bool, IsKeyboardFocused);
    AERO_READONLY_PROPERTY(bool, IsKeyboardFocusWithin);
    AERO_DEPENDENCY_PROPERTY(bool, Focusable);
    AERO_DEPENDENCY_PROPERTY(bool, IsTabStop);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, TabIndex);
    AERO_DEPENDENCY_PROPERTY(bool, IsFocusScope);
    AERO_DEPENDENCY_PROPERTY(double, Opacity);
    AERO_DEPENDENCY_PROPERTY(Ref<Media::Transform>, RenderTransform);
    AERO_DEPENDENCY_PROPERTY(Ref<Media::Transform3D>, Transform3D);
    AERO_DEPENDENCY_PROPERTY(Point, RenderTransformOrigin);

protected:
    void OnVisualChildrenChanged(::Aero::Media::Visual* visualAdded,
        ::Aero::Media::Visual* visualRemoved) noexcept override;
    void OnPropertyInvalidated(PropertyInvalidationFlags flags) noexcept override;
    virtual Size MeasureOverride(Size availableSize) noexcept;
    virtual Size ArrangeOverride(Size finalSize) noexcept;
    virtual void OnRender(Media::DrawingContext& context) noexcept;
    virtual std::uint32_t GetLayoutChildrenCount() const noexcept;
    virtual UIElement* GetLayoutChild(std::uint32_t index) const noexcept;
    void OnPreviewMouseDown(MouseButtonEventArgs& args);
    void OnMouseDown(MouseButtonEventArgs& args);
    void OnMouseLeftButtonDown(MouseButtonEventArgs& args);
    void OnMouseRightButtonDown(MouseButtonEventArgs& args);
    void OnPreviewMouseUp(MouseButtonEventArgs& args);
    void OnMouseUp(MouseButtonEventArgs& args);
    void OnMouseLeftButtonUp(MouseButtonEventArgs& args);
    void OnMouseRightButtonUp(MouseButtonEventArgs& args);
    void OnPreviewMouseMove(MouseEventArgs& args);
    void OnMouseMove(MouseEventArgs& args);
    void OnMouseEnter(MouseEventArgs& args);
    void OnMouseLeave(MouseEventArgs& args);
    void OnPreviewMouseWheel(MouseWheelEventArgs& args);
    void OnMouseWheel(MouseWheelEventArgs& args);
    void OnPreviewKeyDown(KeyEventArgs& args);
    void OnKeyDown(KeyEventArgs& args);
    void OnPreviewKeyUp(KeyEventArgs& args);
    void OnKeyUp(KeyEventArgs& args);
    void OnPreviewTextInput(TextCompositionEventArgs& args);
    void OnTextInput(TextCompositionEventArgs& args);
    void OnGotKeyboardFocus(KeyboardFocusChangedEventArgs& args);
    void OnLostKeyboardFocus(KeyboardFocusChangedEventArgs& args);
    void MeasureChild(UIElement& child, Size availableSize) noexcept;
    void ArrangeChild(UIElement& child, Rect finalRect) noexcept;
    UIElementChildRange LayoutChildren() const noexcept { return UIElementChildRange(*this); }

    void RaiseEvent(RoutedEventHandle event, RoutedEventArgs* args = nullptr) noexcept;

private:
    friend class LayoutEngine;
    friend class ElementTree;
    friend class EventRouter;
    friend class AnimationEngine;
    friend class PointerStateMachine;
    friend class FocusState;
    friend class Controls::Primitives::ButtonBase;
    friend class UIElementChildRange;
    friend class UIElementChildRange::Iterator;
    friend class Aero::Input::RoutedCommand;

class EventRouter;

    struct LayoutHot {
        Size desiredSize{};
        Size untransformedDesiredSize{};
        Size renderSize{};
        Size previousMeasureConstraint{};
        Rect layoutSlot{};
        Rect layoutClip{};
        std::uint64_t layoutRevision = 0U;
        bool layoutAttached : 1;
        bool measureValid : 1;
        bool arrangeValid : 1;
        bool measureQueued : 1;
        bool arrangeQueued : 1;
        bool measuring : 1;
        bool arranging : 1;
        // Cached from the dependency properties so measure and hit-testing
        // do not look them up again.
        double opacity = 1.0;
        Visibility visibility = Visibility::Visible;
    };

    struct Rare {
        void* routedHandlers = nullptr;
        void* inputBindings = nullptr;
        void* commandBindings = nullptr;
    };

    LayoutHot& Layout() noexcept { return layout_; }
    const LayoutHot& Layout() const noexcept { return layout_; }
    void SetAnimatedOpacity(double value) noexcept { layout_.opacity = value; }
    void SetAnimatedVisibility(Visibility value) noexcept { layout_.visibility = value; }
    void InvokeHandlers(RoutedEventHandle event, RoutedEventArgs& args) noexcept;
    void SetMouseOverState(bool value) noexcept;
    void SetPressedState(bool value) noexcept;
    void SetKeyboardFocusedState(bool value) noexcept;
    void SetKeyboardFocusWithinState(bool value) noexcept;
    void MeasureCore(LayoutEngine& layout, Size constraint) noexcept;
    void ArrangeCore(LayoutEngine& layout, Rect slot) noexcept;
    void InvokeClassHandler(RoutedEventHandle event, RoutedEventArgs& args) noexcept;
    void EnsureInputClassHandlers() noexcept;
    void AddHandlerErased(RoutedEventHandle event, const void* handler, std::size_t size, std::size_t alignment,
        Meta::TypeId argsType,
        bool handledEventsToo) noexcept;
    bool RemoveHandlerErased(RoutedEventHandle event, const void* handler, std::size_t size, std::size_t alignment,
        Meta::TypeId argsType) noexcept;
    void CleanupHandlers() noexcept;
    Result<void> EnsureRoutedHandlers() noexcept;
    Rare& EnsureRare() noexcept;

    LayoutHot layout_{};
    Rare* rare_ = nullptr;
};

} // namespace Aero
