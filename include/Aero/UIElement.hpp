#pragma once

#include <Aero/Layout.hpp>
#include <Aero/Visual.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Events/Event.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/RoutedEvent.hpp>

#include <cstddef>
#include <new>
#include <type_traits>

namespace Aero::Input { class RoutedCommand; }

namespace Aero {

using Meta::PropertyInvalidationFlags;
using Meta::TypeId;

class UIElement;
namespace Media { class Transform; class Effect; class Brush; }

class UIElementChildRange {
public:
    class Iterator {
    public:
        Iterator(const Visual* owner, std::uint32_t index) noexcept : owner_(owner), index_(index) { Advance(); }
        UIElement* operator*() const noexcept;
        Iterator& operator++() noexcept { ++index_; Advance(); return *this; }
        bool operator!=(const Iterator& other) const noexcept { return owner_ != other.owner_ || index_ != other.index_; }
        bool operator==(const Iterator& other) const noexcept { return !(*this != other); }

    private:
        const Visual* owner_ = nullptr;
        std::uint32_t index_ = 0U;
        void Advance() noexcept;
    };

    explicit UIElementChildRange(const Visual& owner) noexcept : owner_(&owner) {}
    Iterator begin() const noexcept { return Iterator(owner_, 0U); }
    Iterator end() const noexcept { return Iterator(owner_, VisualTreeHelper::GetChildrenCount(*owner_)); }
    bool Empty() const noexcept { return begin() == end(); }
    std::uint32_t Size() const noexcept;
    UIElement* operator[](std::uint32_t index) const noexcept;

private:
    const Visual* owner_ = nullptr;
};

class AERO_API UIElement : public Visual {
    AERO_DECLARE_TYPE(UIElement, Visual)
public:
    struct Impl;

    template<class TArgs>
    using Event = ::Aero::Event<UIElement, TArgs>;

    template<class TOwner, class TArgs>
    auto GetEvent(
        const Aero::RoutedEventRef<TOwner, TArgs>& event) noexcept {
        return Event<TArgs>(*this, event.Handle());
    }

    inline static constexpr Members::RoutedEvent<MouseEventArgs> PreviewMouseMoveEvent{"PreviewMouseMove"};
    Event<MouseEventArgs> PreviewMouseMove() noexcept { return GetEvent(PreviewMouseMoveEvent); }

    inline static constexpr Members::RoutedEvent<MouseEventArgs> MouseMoveEvent{"MouseMove"};
    Event<MouseEventArgs> MouseMove() noexcept {
        return GetEvent(MouseMoveEvent);
    }

    inline static constexpr Members::RoutedEvent<MouseEventArgs> MouseEnterEvent{"MouseEnter"};
    Event<MouseEventArgs> MouseEnter() noexcept {
        return GetEvent(MouseEnterEvent);
    }

    inline static constexpr Members::RoutedEvent<MouseEventArgs> MouseLeaveEvent{"MouseLeave"};
    Event<MouseEventArgs> MouseLeave() noexcept {
        return GetEvent(MouseLeaveEvent);
    }

    inline static constexpr Members::RoutedEvent<MouseButtonEventArgs> PreviewMouseDownEvent{"PreviewMouseDown"};
    Event<MouseButtonEventArgs> PreviewMouseDown() noexcept { return GetEvent(PreviewMouseDownEvent); }

    inline static constexpr Members::RoutedEvent<MouseButtonEventArgs> MouseDownEvent{"MouseDown"};
    Event<MouseButtonEventArgs> MouseDown() noexcept {
        return GetEvent(MouseDownEvent);
    }

    inline static constexpr Members::RoutedEvent<MouseButtonEventArgs> PreviewMouseLeftButtonDownEvent{"PreviewMouseLeftButtonDown"};
    Event<MouseButtonEventArgs> PreviewMouseLeftButtonDown() noexcept { return GetEvent(PreviewMouseLeftButtonDownEvent); }

    inline static constexpr Members::RoutedEvent<MouseButtonEventArgs> MouseLeftButtonDownEvent{"MouseLeftButtonDown"};
    Event<MouseButtonEventArgs> MouseLeftButtonDown() noexcept { return GetEvent(MouseLeftButtonDownEvent); }

    inline static constexpr Members::RoutedEvent<MouseButtonEventArgs> PreviewMouseUpEvent{"PreviewMouseUp"};
    Event<MouseButtonEventArgs> PreviewMouseUp() noexcept { return GetEvent(PreviewMouseUpEvent); }

    inline static constexpr Members::RoutedEvent<MouseButtonEventArgs> MouseUpEvent{"MouseUp"};
    Event<MouseButtonEventArgs> MouseUp() noexcept {
        return GetEvent(MouseUpEvent);
    }

    inline static constexpr Members::RoutedEvent<MouseWheelEventArgs> PreviewMouseWheelEvent{"PreviewMouseWheel"};
    Event<MouseWheelEventArgs> PreviewMouseWheel() noexcept { return GetEvent(PreviewMouseWheelEvent); }

    inline static constexpr Members::RoutedEvent<MouseWheelEventArgs> MouseWheelEvent{"MouseWheel"};
    Event<MouseWheelEventArgs> MouseWheel() noexcept {
        return GetEvent(MouseWheelEvent);
    }

    inline static constexpr Members::RoutedEvent<KeyboardFocusChangedEventArgs> GotKeyboardFocusEvent{"GotKeyboardFocus"};
    Event<KeyboardFocusChangedEventArgs> GotKeyboardFocus() noexcept {
        return GetEvent(GotKeyboardFocusEvent);
    }

    inline static constexpr Members::RoutedEvent<KeyboardFocusChangedEventArgs> LostKeyboardFocusEvent{"LostKeyboardFocus"};
    Event<KeyboardFocusChangedEventArgs> LostKeyboardFocus() noexcept {
        return GetEvent(LostKeyboardFocusEvent);
    }

    inline static constexpr Members::RoutedEvent<KeyEventArgs> PreviewKeyDownEvent{"PreviewKeyDown"};
    Event<KeyEventArgs> PreviewKeyDown() noexcept { return GetEvent(PreviewKeyDownEvent); }

    inline static constexpr Members::RoutedEvent<KeyEventArgs> KeyDownEvent{"KeyDown"};
    Event<KeyEventArgs> KeyDown() noexcept {
        return GetEvent(KeyDownEvent);
    }

    inline static constexpr Members::RoutedEvent<KeyEventArgs> PreviewKeyUpEvent{"PreviewKeyUp"};
    Event<KeyEventArgs> PreviewKeyUp() noexcept { return GetEvent(PreviewKeyUpEvent); }

    inline static constexpr Members::RoutedEvent<KeyEventArgs> KeyUpEvent{"KeyUp"};
    Event<KeyEventArgs> KeyUp() noexcept {
        return GetEvent(KeyUpEvent);
    }

    inline static constexpr Members::RoutedEvent<TextCompositionEventArgs> PreviewTextInputEvent{"PreviewTextInput"};
    Event<TextCompositionEventArgs> PreviewTextInput() noexcept { return GetEvent(PreviewTextInputEvent); }

    inline static constexpr Members::RoutedEvent<TextCompositionEventArgs> TextInputEvent{"TextInput"};
    Event<TextCompositionEventArgs> TextInput() noexcept {
        return GetEvent(TextInputEvent);
    }

    explicit UIElement(TypeId runtimeType) noexcept;
    ~UIElement() override;

    UIElement* AsUIElement() noexcept override { return this; }
    const UIElement* AsUIElement() const noexcept override { return this; }
    UIElement* LayoutParent() const noexcept {
        Visual* parent = GetVisualParent();
        return parent != nullptr ? parent->AsUIElement() : nullptr;
    }

    template<class TArgs>
    Base::Result<void> AddHandlerChecked(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler,
        bool handledEventsToo = false) noexcept {
        if (handler.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Routed event handler must not be empty");
        }
        return AddHandlerCore(event, DescribeHandler(handler), handledEventsToo);
    }
    template<class TArgs>
    void AddHandler(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler,
        bool handledEventsToo = false) noexcept {
        Base::Result<void> added = AddHandlerChecked(event, handler, handledEventsToo);
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
        return RemoveHandlerCore(event, DescribeHandler(handler));
    }

    Base::Result<void> InvalidateMeasure() noexcept;
    Base::Result<void> InvalidateArrange() noexcept;
    Size GetDesiredSize() const noexcept { return desiredSize_; }
    Size GetRenderSize() const noexcept { return renderSize_; }
    Rect GetLayoutSlot() const noexcept { return layoutSlot_; }
    Rect GetLayoutClip() const noexcept { return layoutClip_; }
    bool GetIsMeasureValid() const noexcept { return measureValid_; }
    bool GetIsArrangeValid() const noexcept { return arrangeValid_; }
    bool GetClipToBounds() const noexcept;
    BlendMode GetBlendMode() const noexcept;
    Base::Ref<Media::Effect> GetEffect() const noexcept;
    Base::Ref<Media::Brush> GetOpacityMask() const noexcept;
    double GetOpacity() const noexcept;
    bool GetIsHitTestVisible() const noexcept;
    Visibility GetVisibility() const noexcept;
    bool GetIsVisible() const noexcept {
        return GetVisibility() == Visibility::Visible;
    }
    bool GetIsEnabled() const noexcept;
    bool GetAllowDrop() const noexcept;
    bool GetIsMouseOver() const noexcept;
    bool GetIsPressed() const noexcept;
    bool GetIsKeyboardFocused() const noexcept;
    bool GetIsKeyboardFocusWithin() const noexcept;
    bool GetFocusable() const noexcept;
    bool GetIsTabStop() const noexcept;
    std::uint32_t GetTabIndex() const noexcept;
    bool GetIsFocusScope() const noexcept;
    Base::Ref<Media::Transform> GetRenderTransform() const noexcept;
    Point GetRenderTransformOrigin() const noexcept;
    std::uint64_t GetLayoutRevision() const noexcept { return layoutRevision_; }

    // Dependency properties
    inline static constexpr Members::Property<bool> ClipToBoundsProperty{"ClipToBounds"};
    inline static constexpr Members::Property<BlendMode> BlendModeProperty{"BlendMode"};
    inline static constexpr Members::Property<Base::Ref<Media::Effect>> EffectProperty{"Effect"};
    inline static constexpr Members::Property<Base::Ref<Media::Brush>> OpacityMaskProperty{"OpacityMask"};
    inline static constexpr Members::Property<bool> IsHitTestVisibleProperty{"IsHitTestVisible"};
    inline static constexpr Members::Property<Visibility> VisibilityProperty{"Visibility"};
    inline static constexpr Members::Property<bool> IsEnabledProperty{"IsEnabled"};
    inline static constexpr Members::Property<bool> AllowDropProperty{"AllowDrop"};
    inline static constexpr Members::ReadOnlyProperty<bool> IsMouseOverProperty{"IsMouseOver"};
    inline static constexpr Members::ReadOnlyProperty<bool> IsPressedProperty{"IsPressed"};
    inline static constexpr Members::ReadOnlyProperty<bool> IsKeyboardFocusedProperty{"IsKeyboardFocused"};
    inline static constexpr Members::ReadOnlyProperty<bool> IsKeyboardFocusWithinProperty{"IsKeyboardFocusWithin"};
    inline static constexpr Members::Property<bool> FocusableProperty{"Focusable"};
    inline static constexpr Members::Property<bool> IsTabStopProperty{"IsTabStop"};
    inline static constexpr Members::Property<std::uint32_t> TabIndexProperty{"TabIndex"};
    inline static constexpr Members::Property<bool> IsFocusScopeProperty{"IsFocusScope"};
    inline static constexpr Members::Property<double> OpacityProperty{"Opacity"};
    inline static constexpr Members::Property<Base::Ref<Media::Transform>> RenderTransformProperty{"RenderTransform"};
    inline static constexpr Members::Property<Point> RenderTransformOriginProperty{"RenderTransformOrigin"};

    // Property operations
    void SetClipToBounds(bool value) noexcept;
    void SetBlendMode(
        BlendMode value) noexcept;
    void SetEffect(
        Base::Ref<Media::Effect> value) noexcept;
    void SetOpacityMask(
        Base::Ref<Media::Brush> value) noexcept;
    void SetIsHitTestVisible(bool value) noexcept;
    void SetVisibility(Visibility value) noexcept;
    void SetIsEnabled(bool value) noexcept;
    void SetIsTabStop(bool value) noexcept;
    void SetTabIndex(std::uint32_t value) noexcept;
    void SetIsFocusScope(bool value) noexcept;
    void SetRenderTransform(
        Base::Ref<Media::Transform> value) noexcept;
    void SetRenderTransformOrigin(
        Point value) noexcept;

protected:
    void RaiseEvent(RoutedEventHandle event, RoutedEventArgs* args = nullptr) noexcept;
    void OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept override;
    virtual Size MeasureOverride(Size availableSize) noexcept;
    virtual Size ArrangeOverride(Size finalSize) noexcept;
    Base::Result<void> MeasureChild(
        UIElement& child, Size availableSize) noexcept;
    Base::Result<void> ArrangeChild(
        UIElement& child, Rect finalRect) noexcept;
    UIElementChildRange LayoutChildren() const noexcept {
        return UIElementChildRange(*this);
    }

private:
    friend struct Impl;
    friend struct ::Aero::Visual::Impl;
    friend class Aero::Input::RoutedCommand;

    struct HandlerOperations {
        std::size_t size = 0U;
        std::size_t alignment = 0U;
        void (*copy)(void*, const void*) noexcept = nullptr;
        void (*destroy)(void*) noexcept = nullptr;
        bool (*equals)(const void*, const void*) noexcept = nullptr;
        void (*invoke)(const void*, Base::Object*, RoutedEventArgs&) noexcept = nullptr;
    };

    struct HandlerDescriptor {
        const void* value = nullptr;
        const HandlerOperations* operations = nullptr;
        Meta::TypeId argsType = Meta::InvalidTypeId;
    };

    template<class TArgs>
    static HandlerDescriptor DescribeHandler(
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler) noexcept {
        using Handler = Base::Delegate<void(Base::Object*, TArgs&)>;
        static const HandlerOperations operations{
            sizeof(Handler),
            alignof(Handler),
            [](void* destination, const void* source) noexcept {
                new (destination) Handler(*static_cast<const Handler*>(source));
            },
            [](void* value) noexcept { static_cast<Handler*>(value)->~Handler(); },
            [](const void* left, const void* right) noexcept {
                return *static_cast<const Handler*>(left) == *static_cast<const Handler*>(right);
            },
            [](const void* value, Base::Object* sender, RoutedEventArgs& args) noexcept {
                static_cast<const Handler*>(value)->Invoke(sender, static_cast<TArgs&>(args));
            }};
        return {&handler, &operations, TArgs::StaticTypeId()};
    }

    Base::Result<void> AddHandlerCore(
        RoutedEventHandle event,
        const HandlerDescriptor& handler,
        bool handledEventsToo) noexcept;
    bool RemoveHandlerCore(
        RoutedEventHandle event,
        const HandlerDescriptor& handler) noexcept;
    void InvokeHandlers(RoutedEventHandle event, RoutedEventArgs& args) noexcept;

    void* routedHandlers_ = nullptr;
    Size desiredSize_;
    Size untransformedDesiredSize_;
    Size renderSize_;
    Size previousMeasureConstraint_;
    Rect layoutSlot_;
    Rect layoutClip_;
    std::uint64_t layoutRevision_ = 0U;
    bool layoutAttached_ = false;
    bool measureValid_ = false;
    bool arrangeValid_ = false;
    bool measureQueued_ = false;
    bool arrangeQueued_ = false;
    bool measuring_ = false;
    bool arranging_ = false;

    void SetMouseOverState(bool value) noexcept;
    void SetPressedState(bool value) noexcept;
    void SetKeyboardFocusedState(bool value) noexcept;
    void SetKeyboardFocusWithinState(bool value) noexcept;
    void CleanupHandlers() noexcept;
};

} // namespace Aero
