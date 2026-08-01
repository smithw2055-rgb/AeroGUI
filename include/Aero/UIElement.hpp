#pragma once

#include <Aero/Layout.hpp>
#include <Aero/Visual.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/RoutedEvent.hpp>

#include <cstddef>
#include <new>
#include <type_traits>

namespace Aero::Detail {
class UiRuntimeAccess;
class ControlRuntimeAccess;
class ContentElementAccess;
class LayoutManager;
}

namespace Aero::Input { class RoutedCommand; }

namespace Aero {

using namespace Aero::Core;
class UIElement;
namespace Media { class Transform; class Effect; }

class UIElementChildRange final {
public:
    class Iterator final {
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
    template<class TArgs>
    class Event final {
    public:
        using Handler = Base::Delegate<void(Base::Object*, TArgs&)>;
        Event(UIElement& element, RoutedEventHandle event) noexcept
            : element_(&element), event_(event) {}

        Base::Result<void> TryAdd(
            const Handler& handler,
            bool handledEventsToo = false) noexcept {
            return element_->TryAddHandler(event_, handler, handledEventsToo);
        }

        void Add(const Handler& handler,
            bool handledEventsToo = false) noexcept {
            Base::Result<void> result = TryAdd(handler, handledEventsToo);
            if (!result) {
                Base::ReportOutOfMemory(
                    sizeof(Handler),
                    alignof(Handler),
                    Base::MemoryTag::General);
            }
        }

        void operator+=(const Handler& handler) noexcept { Add(handler); }

        bool Remove(const Handler& handler) noexcept {
            return element_->RemoveHandler(event_, handler);
        }

        void operator-=(const Handler& handler) noexcept {
            static_cast<void>(Remove(handler));
        }

    private:
        UIElement* element_ = nullptr;
        RoutedEventHandle event_;
    };

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

    inline static constexpr Members::RoutedEvent<MouseButtonEventArgs> PreviewMouseDownEvent{"PreviewMouseDown"};
    Event<MouseButtonEventArgs> PreviewMouseDown() noexcept { return GetEvent(PreviewMouseDownEvent); }

    inline static constexpr Members::RoutedEvent<MouseButtonEventArgs> MouseDownEvent{"MouseDown"};
    Event<MouseButtonEventArgs> MouseDown() noexcept {
        return GetEvent(MouseDownEvent);
    }

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
    Base::Result<void> TryAddHandler(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler,
        bool handledEventsToo = false) noexcept {
        if (handler.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Routed event handler must not be empty");
        }
        return TryAddHandlerCore(event, DescribeHandler(handler), handledEventsToo);
    }
    template<class TArgs>
    void AddHandler(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler,
        bool handledEventsToo = false) noexcept {
        Base::Result<void> added = TryAddHandler(event, handler, handledEventsToo);
        if (!added) {
            Base::ReportOutOfMemory(sizeof(handler), alignof(decltype(handler)), Base::MemoryTag::General);
        }
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
    Base::Ref<Base::Object> GetOpacityMask() const noexcept {
        return GetValueOr(
            OpacityMaskProperty,
            Base::Ref<Base::Object>{});
    }
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
    inline static constexpr Members::Property<Base::Ref<Base::Object>> OpacityMaskProperty{"OpacityMask"};
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
    Base::Result<void> SetClipToBounds(bool value) noexcept;
    Base::Result<void> SetBlendMode(
        BlendMode value) noexcept;
    Base::Result<void> SetEffect(
        Base::Ref<Media::Effect> value) noexcept;
    Base::Result<void> SetHitTestVisible(bool value) noexcept;
    Base::Result<void> SetVisibility(Visibility value) noexcept;
    Base::Result<void> SetEnabled(bool value) noexcept;
    Base::Result<void> SetTabStop(bool value) noexcept;
    Base::Result<void> SetTabIndex(std::uint32_t value) noexcept;
    Base::Result<void> SetFocusScope(bool value) noexcept;
    Base::Result<void> SetRenderTransform(
        Base::Ref<Media::Transform> value) noexcept;
    Base::Result<void> SetRenderTransformOrigin(
        Point value) noexcept;

protected:
    Base::Result<void> RaiseEvent(RoutedEventHandle event, RoutedEventArgs* args = nullptr) noexcept;
    Base::Result<void> OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept override;
    virtual Base::Result<Size> MeasureOverride(Size availableSize) noexcept;
    virtual Base::Result<Size> ArrangeOverride(Size finalSize) noexcept;
    Base::Result<void> MeasureChild(
        UIElement& child, Size availableSize) noexcept;
    Base::Result<void> ArrangeChild(
        UIElement& child, Rect finalRect) noexcept;
    UIElementChildRange LayoutChildren() const noexcept {
        return UIElementChildRange(*this);
    }

private:
    friend class Aero::Detail::UiRuntimeAccess;
    friend class Aero::Detail::ControlRuntimeAccess;
    friend class Aero::Detail::ContentElementAccess;
    friend class Aero::Detail::LayoutManager;
    friend class Aero::Input::RoutedCommand;

    struct HandlerOperations final {
        std::size_t size = 0U;
        std::size_t alignment = 0U;
        void (*copy)(void*, const void*) noexcept = nullptr;
        void (*destroy)(void*) noexcept = nullptr;
        bool (*equals)(const void*, const void*) noexcept = nullptr;
        void (*invoke)(const void*, Base::Object*, RoutedEventArgs&) noexcept = nullptr;
    };

    struct HandlerDescriptor final {
        const void* value = nullptr;
        const HandlerOperations* operations = nullptr;
        Core::TypeId argsType = Core::InvalidTypeId;
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

    Base::Result<void> TryAddHandlerCore(
        RoutedEventHandle event,
        const HandlerDescriptor& handler,
        bool handledEventsToo) noexcept;
    bool RemoveHandlerCore(
        RoutedEventHandle event,
        const HandlerDescriptor& handler) noexcept;
    void InvokeHandlers(RoutedEventHandle event, RoutedEventArgs& args) noexcept;

    void* manager_ = nullptr;
    void* eventRouter_ = nullptr;
    void* commandRouter_ = nullptr;
    void* handlerState_ = nullptr;
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

    Base::Result<void> SetMouseOverState(bool value) noexcept;
    Base::Result<void> SetPressedState(bool value) noexcept;
    Base::Result<void> SetKeyboardFocusedState(bool value) noexcept;
    Base::Result<void> SetKeyboardFocusWithinState(bool value) noexcept;
    void CleanupHandlers() noexcept;
};

} // namespace Aero
