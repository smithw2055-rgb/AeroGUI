#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Media/BlendMode.hpp>
#include <Aero/Visibility.hpp>
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

using Point = Base::Point;
using Size = Base::Size;
using Rect = Base::Rect;

using Meta::PropertyInvalidationFlags;
using Meta::TypeId;

class UIElement;
class AeroGuiInternal;
namespace Media { class Transform; class Transform3D; class Effect; class Brush; class Geometry; }
namespace Input { class RoutedCommand; class InputBinding; }

class UIElementChildRange {
public:
    class Iterator {
    public:
        Iterator(const ::Aero::Media::Visual* owner, std::uint32_t index) noexcept : owner_(owner), index_(index) { Advance(); }
        UIElement* operator*() const noexcept;
        Iterator& operator++() noexcept { ++index_; Advance(); return *this; }
        bool operator!=(const Iterator& other) const noexcept { return owner_ != other.owner_ || index_ != other.index_; }
        bool operator==(const Iterator& other) const noexcept { return !(*this != other); }

    private:
        const ::Aero::Media::Visual* owner_ = nullptr;
        std::uint32_t index_ = 0U;
        void Advance() noexcept;
    };

    explicit UIElementChildRange(const ::Aero::Media::Visual& owner) noexcept : owner_(&owner) {}
    Iterator begin() const noexcept { return Iterator(owner_, 0U); }
    Iterator end() const noexcept { return Iterator(owner_, ::Aero::Media::VisualTreeHelper::GetChildrenCount(*owner_)); }
    bool Empty() const noexcept { return begin() == end(); }
    std::uint32_t Size() const noexcept;
    UIElement* operator[](std::uint32_t index) const noexcept;

private:
    const ::Aero::Media::Visual* owner_ = nullptr;
};

class AERO_GUI_API UIElement : public ::Aero::Media::Visual {
    AERO_DECLARE_TYPE(UIElement, ::Aero::Media::Visual)
public:
    template<class TArgs>
    using Event = ::Aero::Event<UIElement, TArgs>;

    template<class TOwner, class TArgs>
    auto GetEvent(
        const Aero::RoutedEventRef<TOwner, TArgs>& event) noexcept {
        return Event<TArgs>(*this, event.Handle());
    }

    inline static constexpr RoutedEvent<MouseEventArgs> PreviewMouseMoveEvent{"PreviewMouseMove"};
    Event<MouseEventArgs> PreviewMouseMove() noexcept { return GetEvent(PreviewMouseMoveEvent); }

    inline static constexpr RoutedEvent<MouseEventArgs> MouseMoveEvent{"MouseMove"};
    Event<MouseEventArgs> MouseMove() noexcept {
        return GetEvent(MouseMoveEvent);
    }

    inline static constexpr RoutedEvent<MouseEventArgs> MouseEnterEvent{"MouseEnter"};
    Event<MouseEventArgs> MouseEnter() noexcept {
        return GetEvent(MouseEnterEvent);
    }

    inline static constexpr RoutedEvent<MouseEventArgs> MouseLeaveEvent{"MouseLeave"};
    Event<MouseEventArgs> MouseLeave() noexcept {
        return GetEvent(MouseLeaveEvent);
    }

    inline static constexpr RoutedEvent<MouseButtonEventArgs> PreviewMouseDownEvent{"PreviewMouseDown"};
    Event<MouseButtonEventArgs> PreviewMouseDown() noexcept { return GetEvent(PreviewMouseDownEvent); }

    inline static constexpr RoutedEvent<MouseButtonEventArgs> MouseDownEvent{"MouseDown"};
    Event<MouseButtonEventArgs> MouseDown() noexcept {
        return GetEvent(MouseDownEvent);
    }

    inline static constexpr RoutedEvent<MouseButtonEventArgs> PreviewMouseLeftButtonDownEvent{"PreviewMouseLeftButtonDown"};
    Event<MouseButtonEventArgs> PreviewMouseLeftButtonDown() noexcept { return GetEvent(PreviewMouseLeftButtonDownEvent); }

    inline static constexpr RoutedEvent<MouseButtonEventArgs> MouseLeftButtonDownEvent{"MouseLeftButtonDown"};
    Event<MouseButtonEventArgs> MouseLeftButtonDown() noexcept { return GetEvent(MouseLeftButtonDownEvent); }

    inline static constexpr RoutedEvent<MouseButtonEventArgs> PreviewMouseUpEvent{"PreviewMouseUp"};
    Event<MouseButtonEventArgs> PreviewMouseUp() noexcept { return GetEvent(PreviewMouseUpEvent); }

    inline static constexpr RoutedEvent<MouseButtonEventArgs> MouseUpEvent{"MouseUp"};
    Event<MouseButtonEventArgs> MouseUp() noexcept {
        return GetEvent(MouseUpEvent);
    }

    inline static constexpr RoutedEvent<MouseWheelEventArgs> PreviewMouseWheelEvent{"PreviewMouseWheel"};
    Event<MouseWheelEventArgs> PreviewMouseWheel() noexcept { return GetEvent(PreviewMouseWheelEvent); }

    inline static constexpr RoutedEvent<MouseWheelEventArgs> MouseWheelEvent{"MouseWheel"};
    Event<MouseWheelEventArgs> MouseWheel() noexcept {
        return GetEvent(MouseWheelEvent);
    }

    inline static constexpr RoutedEvent<MouseButtonEventArgs> PreviewMouseLeftButtonUpEvent{"PreviewMouseLeftButtonUp"};
    Event<MouseButtonEventArgs> PreviewMouseLeftButtonUp() noexcept {
        return GetEvent(PreviewMouseLeftButtonUpEvent);
    }
    inline static constexpr RoutedEvent<MouseButtonEventArgs> MouseLeftButtonUpEvent{"MouseLeftButtonUp"};
    Event<MouseButtonEventArgs> MouseLeftButtonUp() noexcept {
        return GetEvent(MouseLeftButtonUpEvent);
    }

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
    Event<KeyboardFocusChangedEventArgs> GotKeyboardFocus() noexcept {
        return GetEvent(GotKeyboardFocusEvent);
    }

    inline static constexpr RoutedEvent<KeyboardFocusChangedEventArgs> LostKeyboardFocusEvent{"LostKeyboardFocus"};
    Event<KeyboardFocusChangedEventArgs> LostKeyboardFocus() noexcept {
        return GetEvent(LostKeyboardFocusEvent);
    }

    inline static constexpr RoutedEvent<KeyEventArgs> PreviewKeyDownEvent{"PreviewKeyDown"};
    Event<KeyEventArgs> PreviewKeyDown() noexcept { return GetEvent(PreviewKeyDownEvent); }

    inline static constexpr RoutedEvent<KeyEventArgs> KeyDownEvent{"KeyDown"};
    Event<KeyEventArgs> KeyDown() noexcept {
        return GetEvent(KeyDownEvent);
    }

    inline static constexpr RoutedEvent<KeyEventArgs> PreviewKeyUpEvent{"PreviewKeyUp"};
    Event<KeyEventArgs> PreviewKeyUp() noexcept { return GetEvent(PreviewKeyUpEvent); }

    inline static constexpr RoutedEvent<KeyEventArgs> KeyUpEvent{"KeyUp"};
    Event<KeyEventArgs> KeyUp() noexcept {
        return GetEvent(KeyUpEvent);
    }

    inline static constexpr RoutedEvent<TextCompositionEventArgs> PreviewTextInputEvent{"PreviewTextInput"};
    Event<TextCompositionEventArgs> PreviewTextInput() noexcept { return GetEvent(PreviewTextInputEvent); }

    inline static constexpr RoutedEvent<TextCompositionEventArgs> TextInputEvent{"TextInput"};
    Event<TextCompositionEventArgs> TextInput() noexcept {
        return GetEvent(TextInputEvent);
    }

    explicit UIElement(TypeId runtimeType) noexcept;
    ~UIElement() override;

    UIElement* LayoutParent() const noexcept;

    template<class TArgs>
    Result<void> AddHandlerChecked(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler,
        bool handledEventsToo = false) noexcept {
        if (handler.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Routed event handler must not be empty");
        }
        return AddHandlerErased(
            event,
            &handler,
            sizeof(handler),
            alignof(decltype(handler)),
            TArgs::StaticTypeId(),
            handledEventsToo);
    }
    template<class TArgs>
    void AddHandler(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler,
        bool handledEventsToo = false) noexcept {
        Result<void> added = AddHandlerChecked(event, handler, handledEventsToo);
        if (added) return;
        if (added.GetStatus().code == Base::ErrorCode::OutOfMemory) {
            Base::ReportOutOfMemory(sizeof(handler), alignof(decltype(handler)), Base::MemoryTag::General);
        }
        AERO_ASSERT(false && "UIElement::AddHandler failed; use AddHandlerChecked for diagnostics");
    }
    template<class TArgs>
    bool RemoveHandler(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler) noexcept {
        return RemoveHandlerErased(
            event,
            &handler,
            sizeof(handler),
            alignof(decltype(handler)),
            TArgs::StaticTypeId());
    }

    Result<void> InvalidateMeasure() noexcept;
    Result<void> InvalidateArrange() noexcept;
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
    Ref<Media::Geometry> GetClip() const noexcept;
    BlendMode GetBlendMode() const noexcept;
    Ref<Media::Effect> GetEffect() const noexcept;
    Ref<Media::Brush> GetOpacityMask() const noexcept;
    double GetOpacity() const noexcept;
    bool GetIsHitTestVisible() const noexcept;
    Visibility GetVisibility() const noexcept;
    bool GetIsVisible() const noexcept;
    bool GetIsEnabled() const noexcept;
    bool GetAllowDrop() const noexcept;
    bool GetIsDragging() const noexcept;
    Result<void> BeginDrag(
        std::uint32_t pointerId,
        const Value& data,
        Input::DragDropEffects allowedEffects =
            Input::DragDropEffects::Move) noexcept;
    Result<bool> CancelDrag() noexcept;
    bool GetIsMouseOver() const noexcept;
    bool GetIsPressed() const noexcept;
    bool GetIsKeyboardFocused() const noexcept;
    bool GetIsKeyboardFocusWithin() const noexcept;
    bool GetFocusable() const noexcept;
    Result<bool> Focus() noexcept;
    bool GetIsTabStop() const noexcept;
    std::uint32_t GetTabIndex() const noexcept;
    bool GetIsFocusScope() const noexcept;
    Ref<Media::Transform> GetRenderTransform() const noexcept;
    Ref<Media::Transform3D> GetTransform3D() const noexcept;
    Point GetRenderTransformOrigin() const noexcept;
    std::uint64_t GetLayoutRevision() const noexcept;

    // Dependency properties
    inline static constexpr DependencyProperty<bool> ClipToBoundsProperty{"ClipToBounds"};
    inline static constexpr DependencyProperty<Ref<Media::Geometry>> ClipProperty{"Clip"};
    inline static constexpr DependencyProperty<BlendMode> BlendModeProperty{"BlendMode"};
    inline static constexpr DependencyProperty<Ref<Media::Effect>> EffectProperty{"Effect"};
    inline static constexpr DependencyProperty<Ref<Media::Brush>> OpacityMaskProperty{"OpacityMask"};
    inline static constexpr DependencyProperty<bool> IsHitTestVisibleProperty{"IsHitTestVisible"};
    inline static constexpr DependencyProperty<Visibility> VisibilityProperty{"Visibility"};
    inline static constexpr DependencyProperty<bool> IsEnabledProperty{"IsEnabled"};
    inline static constexpr DependencyProperty<bool> AllowDropProperty{"AllowDrop"};
    inline static constexpr ReadOnlyDependencyProperty<bool> IsMouseOverProperty{"IsMouseOver"};
    inline static constexpr ReadOnlyDependencyProperty<bool> IsPressedProperty{"IsPressed"};
    inline static constexpr ReadOnlyDependencyProperty<bool> IsKeyboardFocusedProperty{"IsKeyboardFocused"};
    inline static constexpr ReadOnlyDependencyProperty<bool> IsKeyboardFocusWithinProperty{"IsKeyboardFocusWithin"};
    inline static constexpr DependencyProperty<bool> FocusableProperty{"Focusable"};
    inline static constexpr DependencyProperty<bool> IsTabStopProperty{"IsTabStop"};
    inline static constexpr DependencyProperty<std::uint32_t> TabIndexProperty{"TabIndex"};
    inline static constexpr DependencyProperty<bool> IsFocusScopeProperty{"IsFocusScope"};
    inline static constexpr DependencyProperty<double> OpacityProperty{"Opacity"};
    inline static constexpr DependencyProperty<Ref<Media::Transform>> RenderTransformProperty{"RenderTransform"};
    inline static constexpr DependencyProperty<Ref<Media::Transform3D>> Transform3DProperty{"Transform3D"};
    inline static constexpr DependencyProperty<Point> RenderTransformOriginProperty{"RenderTransformOrigin"};

    // Property operations
    void SetClipToBounds(bool value) noexcept;
    void SetClip(Ref<Media::Geometry> value) noexcept;
    void SetBlendMode(
        BlendMode value) noexcept;
    void SetEffect(
        Ref<Media::Effect> value) noexcept;
    void SetOpacityMask(
        Ref<Media::Brush> value) noexcept;
    void SetIsHitTestVisible(bool value) noexcept;
    void SetVisibility(Visibility value) noexcept;
    void SetIsEnabled(bool value) noexcept;
    void SetAllowDrop(bool value) noexcept;
    void SetIsTabStop(bool value) noexcept;
    void SetTabIndex(std::uint32_t value) noexcept;
    void SetIsFocusScope(bool value) noexcept;
    Result<void> AddInputBinding(Ref<Input::InputBinding> binding) noexcept;
    void ClearInputBindings() noexcept;
    Base::Span<const Ref<Input::InputBinding>> GetInputBindings() const noexcept;
    void SetRenderTransform(
        Ref<Media::Transform> value) noexcept;
    void SetTransform3D(
        Ref<Media::Transform3D> value) noexcept;
    void SetRenderTransformOrigin(
        Point value) noexcept;

protected:
    void RaiseEvent(RoutedEventHandle event, RoutedEventArgs* args = nullptr) noexcept;
    void OnVisualChildrenChanged(
        ::Aero::Media::Visual* visualAdded,
        ::Aero::Media::Visual* visualRemoved) noexcept override;
    void OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept override;
    virtual Size MeasureOverride(Size availableSize) noexcept;
    virtual Size ArrangeOverride(Size finalSize) noexcept;
    Result<void> MeasureChild(
        UIElement& child, Size availableSize) noexcept;
    Result<void> ArrangeChild(
        UIElement& child, Rect finalRect) noexcept;
    UIElementChildRange LayoutChildren() const noexcept {
        return UIElementChildRange(*this);
    }

private:
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
#endif
    friend class Aero::Input::RoutedCommand;

    Result<void> AddHandlerErased(
        RoutedEventHandle event,
        const void* handler,
        std::size_t size,
        std::size_t alignment,
        Meta::TypeId argsType,
        bool handledEventsToo) noexcept;
    bool RemoveHandlerErased(
        RoutedEventHandle event,
        const void* handler,
        std::size_t size,
        std::size_t alignment,
        Meta::TypeId argsType) noexcept;
    void InvokeHandlers(RoutedEventHandle event, RoutedEventArgs& args) noexcept;

    void SetMouseOverState(bool value) noexcept;
    void SetPressedState(bool value) noexcept;
    void SetKeyboardFocusedState(bool value) noexcept;
    void SetKeyboardFocusWithinState(bool value) noexcept;
    void CleanupHandlers() noexcept;
    Result<void> EnsureRoutedHandlers() noexcept;

    struct LayoutHot {
        Size desiredSize{};
        Size untransformedDesiredSize{};
        Size renderSize{};
        Size previousMeasureConstraint{};
        Rect layoutSlot{};
        Rect layoutClip{};
        Rect visualRect{};
        std::uint64_t layoutRevision = 0U;
        bool layoutAttached : 1;
        bool measureValid : 1;
        bool arrangeValid : 1;
        bool measureQueued : 1;
        bool arrangeQueued : 1;
        bool measuring : 1;
        bool arranging : 1;
    };

    struct Rare {
        void* routedHandlers = nullptr;
        void* inputBindings = nullptr;
    };

    Rare& EnsureRare() noexcept;

    LayoutHot layout_{};
    Rare* rare_ = nullptr;
};

} // namespace Aero
