#pragma once

#include <Aero/Layout.hpp>
#include <Aero/Visual.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/RoutedEvent.hpp>

#include <new>
#include <type_traits>

namespace Aero::Detail {
class UiRuntimeAccess;
class ControlRuntimeAccess;
}

namespace Aero::Detail {

class RoutedHandlerStorage final {
public:
    RoutedHandlerStorage() noexcept = default;

    template<class TArgs>
    explicit RoutedHandlerStorage(const Base::Delegate<void(Base::Object*, const TArgs&)>& handler) noexcept {
        static_assert(std::is_base_of<Aero::RoutedEventArgs, TArgs>::value,
            "Routed event arguments must derive from RoutedEventArgs");
        using Handler = Base::Delegate<void(Base::Object*, const TArgs&)>;
        static_assert(sizeof(Handler) <= sizeof(storage_), "Routed handler storage is too small");
        new (storage_) Handler(handler);
        operations_ = &OperationsFor<TArgs>();
        argsType_ = TArgs::StaticTypeId();
    }

    RoutedHandlerStorage(const RoutedHandlerStorage& other) noexcept;
    RoutedHandlerStorage(RoutedHandlerStorage&& other) noexcept;
    RoutedHandlerStorage& operator=(const RoutedHandlerStorage& other) noexcept;
    RoutedHandlerStorage& operator=(RoutedHandlerStorage&& other) noexcept;
    ~RoutedHandlerStorage() noexcept;

    bool Empty() const noexcept { return operations_ == nullptr; }
    Core::TypeId ArgsType() const noexcept { return argsType_; }
    bool Equals(const RoutedHandlerStorage& other) const noexcept;
    void Invoke(Base::Object* sender, const RoutedEventArgs& args) const noexcept;

private:
    struct Operations final {
        void (*copy)(void*, const void*) noexcept;
        void (*destroy)(void*) noexcept;
        bool (*equals)(const void*, const void*) noexcept;
        void (*invoke)(const void*, Base::Object*, const RoutedEventArgs&) noexcept;
    };

    template<class TArgs>
    static const Operations& OperationsFor() noexcept {
        using Handler = Base::Delegate<void(Base::Object*, const TArgs&)>;
        static const Operations operations{
            [](void* destination, const void* source) noexcept {
                new (destination) Handler(*static_cast<const Handler*>(source));
            },
            [](void* value) noexcept { static_cast<Handler*>(value)->~Handler(); },
            [](const void* left, const void* right) noexcept {
                return *static_cast<const Handler*>(left) == *static_cast<const Handler*>(right);
            },
            [](const void* value, Base::Object* sender, const RoutedEventArgs& args) noexcept {
                static_cast<const Handler*>(value)->Invoke(sender, static_cast<const TArgs&>(args));
            }};
        return operations;
    }

    void Reset() noexcept;

    alignas(void*) unsigned char storage_[4U * sizeof(void*)]{};
    const Operations* operations_ = nullptr;
    Core::TypeId argsType_ = Core::InvalidTypeId;
};

template<class T>
struct RoutedHandlerTraits;

template<class TArgs>
struct RoutedHandlerTraits<Base::Delegate<void(Base::Object*, const TArgs&)>> final {
    using Args = TArgs;
};

} // namespace Aero::Detail
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
    template<class THandler>
    class Event final {
    public:
        Event(UIElement& element, RoutedEventHandle event) noexcept
            : element_(&element), event_(event) {}

        Base::Result<void> TryAdd(
            const THandler& handler,
            bool handledEventsToo = false) noexcept {
            using Args = typename Aero::Detail::RoutedHandlerTraits<THandler>::Args;
            return element_->TryAddHandler(
                event_, Aero::Detail::RoutedHandlerStorage(
                    static_cast<const Base::Delegate<
                        void(Base::Object*, const Args&)>&>(handler)),
                handledEventsToo);
        }

        void Add(const THandler& handler,
            bool handledEventsToo = false) noexcept {
            Base::Result<void> result = TryAdd(handler, handledEventsToo);
            if (!result) {
                Base::ReportOutOfMemory(
                    sizeof(Aero::Detail::RoutedHandlerStorage),
                    alignof(Aero::Detail::RoutedHandlerStorage),
                    Base::MemoryTag::General);
            }
        }

        void operator+=(const THandler& handler) noexcept { Add(handler); }

        bool Remove(const THandler& handler) noexcept {
            using Args = typename Aero::Detail::RoutedHandlerTraits<THandler>::Args;
            return element_->RemoveHandler(
                event_, Aero::Detail::RoutedHandlerStorage(
                    static_cast<const Base::Delegate<
                        void(Base::Object*, const Args&)>&>(handler)));
        }

        void operator-=(const THandler& handler) noexcept {
            static_cast<void>(Remove(handler));
        }

    private:
        UIElement* element_ = nullptr;
        RoutedEventHandle event_;
    };

    template<class TOwner, class TArgs>
    auto GetEvent(
        const Core::RoutedEventRef<TOwner, TArgs>& event) noexcept {
        using Handler =
            Base::Delegate<void(Base::Object*, const TArgs&)>;
        return Event<Handler>(*this, event.Handle());
    }

    inline static constexpr Members::RoutedEvent<MouseEventArgs> PreviewMouseMoveEvent{"PreviewMouseMove"};
    Event<MouseEventHandler> PreviewMouseMove() noexcept { return GetEvent(PreviewMouseMoveEvent); }

    inline static constexpr Members::RoutedEvent<MouseEventArgs> MouseMoveEvent{"MouseMove"};
    Event<MouseEventHandler> MouseMove() noexcept {
        return GetEvent(MouseMoveEvent);
    }

    inline static constexpr Members::RoutedEvent<MouseButtonEventArgs> PreviewMouseDownEvent{"PreviewMouseDown"};
    Event<MouseButtonEventHandler> PreviewMouseDown() noexcept { return GetEvent(PreviewMouseDownEvent); }

    inline static constexpr Members::RoutedEvent<MouseButtonEventArgs> MouseDownEvent{"MouseDown"};
    Event<MouseButtonEventHandler> MouseDown() noexcept {
        return GetEvent(MouseDownEvent);
    }

    inline static constexpr Members::RoutedEvent<MouseButtonEventArgs> PreviewMouseUpEvent{"PreviewMouseUp"};
    Event<MouseButtonEventHandler> PreviewMouseUp() noexcept { return GetEvent(PreviewMouseUpEvent); }

    inline static constexpr Members::RoutedEvent<MouseButtonEventArgs> MouseUpEvent{"MouseUp"};
    Event<MouseButtonEventHandler> MouseUp() noexcept {
        return GetEvent(MouseUpEvent);
    }

    inline static constexpr Members::RoutedEvent<MouseWheelEventArgs> PreviewMouseWheelEvent{"PreviewMouseWheel"};
    Event<MouseWheelEventHandler> PreviewMouseWheel() noexcept { return GetEvent(PreviewMouseWheelEvent); }

    inline static constexpr Members::RoutedEvent<MouseWheelEventArgs> MouseWheelEvent{"MouseWheel"};
    Event<MouseWheelEventHandler> MouseWheel() noexcept {
        return GetEvent(MouseWheelEvent);
    }

    inline static constexpr Members::RoutedEvent<KeyboardFocusChangedEventArgs> GotKeyboardFocusEvent{"GotKeyboardFocus"};
    Event<KeyboardFocusChangedEventHandler> GotKeyboardFocus() noexcept {
        return GetEvent(GotKeyboardFocusEvent);
    }

    inline static constexpr Members::RoutedEvent<KeyboardFocusChangedEventArgs> LostKeyboardFocusEvent{"LostKeyboardFocus"};
    Event<KeyboardFocusChangedEventHandler> LostKeyboardFocus() noexcept {
        return GetEvent(LostKeyboardFocusEvent);
    }

    inline static constexpr Members::RoutedEvent<KeyEventArgs> PreviewKeyDownEvent{"PreviewKeyDown"};
    Event<KeyEventHandler> PreviewKeyDown() noexcept { return GetEvent(PreviewKeyDownEvent); }

    inline static constexpr Members::RoutedEvent<KeyEventArgs> KeyDownEvent{"KeyDown"};
    Event<KeyEventHandler> KeyDown() noexcept {
        return GetEvent(KeyDownEvent);
    }

    inline static constexpr Members::RoutedEvent<KeyEventArgs> PreviewKeyUpEvent{"PreviewKeyUp"};
    Event<KeyEventHandler> PreviewKeyUp() noexcept { return GetEvent(PreviewKeyUpEvent); }

    inline static constexpr Members::RoutedEvent<KeyEventArgs> KeyUpEvent{"KeyUp"};
    Event<KeyEventHandler> KeyUp() noexcept {
        return GetEvent(KeyUpEvent);
    }

    inline static constexpr Members::RoutedEvent<TextCompositionEventArgs> PreviewTextInputEvent{"PreviewTextInput"};
    Event<TextCompositionEventHandler> PreviewTextInput() noexcept { return GetEvent(PreviewTextInputEvent); }

    inline static constexpr Members::RoutedEvent<TextCompositionEventArgs> TextInputEvent{"TextInput"};
    Event<TextCompositionEventHandler> TextInput() noexcept {
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

    Base::Result<void> TryAddHandler(
        RoutedEventHandle event,
        const Aero::Detail::RoutedHandlerStorage& handler,
        bool handledEventsToo = false) noexcept;
    template<class TArgs>
    Base::Result<void> TryAddHandler(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, const TArgs&)>& handler,
        bool handledEventsToo = false) noexcept {
        return TryAddHandler(
            event, Aero::Detail::RoutedHandlerStorage(handler), handledEventsToo);
    }
    template<class TArgs>
    void AddHandler(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, const TArgs&)>& handler,
        bool handledEventsToo = false) noexcept {
        Base::Result<void> added = TryAddHandler(event, handler, handledEventsToo);
        if (!added) {
            Base::ReportOutOfMemory(sizeof(Aero::Detail::RoutedHandlerStorage),
                alignof(Aero::Detail::RoutedHandlerStorage), Base::MemoryTag::General);
        }
    }
    bool RemoveHandler(
        RoutedEventHandle event,
        const Aero::Detail::RoutedHandlerStorage& handler) noexcept;
    template<class TArgs>
    bool RemoveHandler(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, const TArgs&)>& handler) noexcept {
        return RemoveHandler(event, Aero::Detail::RoutedHandlerStorage(handler));
    }

    Base::Result<void> InvalidateMeasure() noexcept;
    Base::Result<void> InvalidateArrange() noexcept;
    Size DesiredSize() const noexcept { return desiredSize_; }
    Size RenderSize() const noexcept { return renderSize_; }
    Rect LayoutSlot() const noexcept { return layoutSlot_; }
    Rect LayoutClip() const noexcept { return layoutClip_; }
    bool IsMeasureValid() const noexcept { return measureValid_; }
    bool IsArrangeValid() const noexcept { return arrangeValid_; }
    bool ClipToBounds() const noexcept;
    BlendMode GetBlendMode() const noexcept;
    Base::Ref<Media::Effect> GetEffect() const noexcept;
    Base::Ref<Base::Object> OpacityMask() const noexcept {
        return GetValueOr(
            OpacityMaskProperty,
            Base::Ref<Base::Object>{});
    }
    bool IsHitTestVisible() const noexcept;
    Visibility GetVisibility() const noexcept;
    bool IsVisible() const noexcept {
        return GetVisibility() == Visibility::Visible;
    }
    bool IsEnabled() const noexcept;
    bool AllowDrop() const noexcept;
    bool IsMouseOver() const noexcept;
    bool IsPressed() const noexcept;
    bool IsKeyboardFocused() const noexcept;
    bool IsKeyboardFocusWithin() const noexcept;
    bool Focusable() const noexcept;
    bool IsTabStop() const noexcept;
    std::uint32_t TabIndex() const noexcept;
    bool IsFocusScope() const noexcept;
    Base::Ref<Media::Transform> RenderTransform() const noexcept;
    Point RenderTransformOrigin() const noexcept;
    std::uint64_t LayoutRevision() const noexcept { return layoutRevision_; }

    Size GetDesiredSize() const noexcept { return DesiredSize(); }
    Size GetRenderSize() const noexcept { return RenderSize(); }
    bool GetClipToBounds() const noexcept { return ClipToBounds(); }
    Base::Ref<Base::Object> GetOpacityMask() const noexcept { return OpacityMask(); }
    bool GetIsHitTestVisible() const noexcept { return IsHitTestVisible(); }
    bool GetIsVisible() const noexcept { return IsVisible(); }
    bool GetIsEnabled() const noexcept { return IsEnabled(); }
    bool GetAllowDrop() const noexcept { return AllowDrop(); }
    bool GetIsMouseOver() const noexcept { return IsMouseOver(); }
    bool GetIsPressed() const noexcept { return IsPressed(); }
    bool GetIsKeyboardFocused() const noexcept { return IsKeyboardFocused(); }
    bool GetIsKeyboardFocusWithin() const noexcept { return IsKeyboardFocusWithin(); }
    bool GetFocusable() const noexcept { return Focusable(); }
    bool GetIsTabStop() const noexcept { return IsTabStop(); }
    std::uint32_t GetTabIndex() const noexcept { return TabIndex(); }
    bool GetIsFocusScope() const noexcept { return IsFocusScope(); }
    Base::Ref<Media::Transform> GetRenderTransform() const noexcept;
    Point GetRenderTransformOrigin() const noexcept { return RenderTransformOrigin(); }

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
    friend class Aero::Input::RoutedCommand;

    struct HandlerRecord final {
        RoutedEventHandle event;
        Aero::Detail::RoutedHandlerStorage handler;
        std::uint64_t sequence = 0U;
        bool handledEventsToo = false;
    };

    void* manager_ = nullptr;
    void* eventRouter_ = nullptr;
    void* commandRouter_ = nullptr;
    Base::Vector<HandlerRecord> handlers_;
    std::uint64_t nextHandlerSequence_ = 1U;
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
