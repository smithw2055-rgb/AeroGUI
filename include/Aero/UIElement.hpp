#pragma once

#include <Aero/Layout.hpp>
#include <Aero/Visual.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/RoutedEvent.hpp>

namespace Aero::Detail {
class UiRuntimeAccess;
class ControlRuntimeAccess;
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
        Iterator(Base::Span<Visual* const> children,
            std::uint32_t index) noexcept
            : children_(children), index_(index) { Advance(); }
        UIElement* operator*() const noexcept {
            return index_ < children_.Size()
                ? children_[index_]->AsUIElement() : nullptr;
        }
        Iterator& operator++() noexcept {
            if (index_ < children_.Size()) ++index_;
            Advance();
            return *this;
        }
        bool operator!=(const Iterator& other) const noexcept {
            return index_ != other.index_ ||
                children_.Data() != other.children_.Data();
        }
    private:
        Base::Span<Visual* const> children_;
        std::uint32_t index_ = 0U;
        void Advance() noexcept {
            while (index_ < children_.Size() &&
                children_[index_]->AsUIElement() == nullptr) {
                ++index_;
            }
        }
    };

    explicit UIElementChildRange(Base::Span<Visual* const> children) noexcept
        : children_(children) {}
    Iterator begin() const noexcept { return Iterator(children_, 0U); }
    Iterator end() const noexcept {
        return Iterator(children_, children_.Size());
    }
    bool Empty() const noexcept { return !(begin() != end()); }
    std::uint32_t Size() const noexcept {
        std::uint32_t count = 0U;
        for (UIElement* child : *this) {
            (void)child;
            ++count;
        }
        return count;
    }
    UIElement* operator[](std::uint32_t index) const noexcept {
        std::uint32_t current = 0U;
        for (UIElement* child : *this) {
            if (current++ == index) return child;
        }
        return nullptr;
    }
private:
    Base::Span<Visual* const> children_;
};

class AERO_API UIElement : public Visual {
    AERO_DECLARE_TYPE(UIElement, Visual)
public:
    template<class THandler>
    class RoutedEvent_ final {
    public:
        RoutedEvent_(UIElement& element, RoutedEventHandle event) noexcept
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
    auto Event(
        const Core::RoutedEventRef<TOwner, TArgs>& event) noexcept {
        using Handler =
            Base::Delegate<void(Base::Object*, const TArgs&)>;
        return RoutedEvent_<Handler>(*this, event.Handle());
    }

    inline static constexpr Members::RoutedEvent<
        MouseEventArgs> MouseMoveEvent{"MouseMove"};
    RoutedEvent_<MouseEventHandler> MouseMove() noexcept {
        return Event(MouseMoveEvent);
    }

    inline static constexpr Members::RoutedEvent<
        MouseButtonEventArgs> MouseDownEvent{"MouseDown"};
    RoutedEvent_<MouseButtonEventHandler> MouseDown() noexcept {
        return Event(MouseDownEvent);
    }

    inline static constexpr Members::RoutedEvent<
        MouseButtonEventArgs> MouseUpEvent{"MouseUp"};
    RoutedEvent_<MouseButtonEventHandler> MouseUp() noexcept {
        return Event(MouseUpEvent);
    }

    inline static constexpr Members::RoutedEvent<
        MouseWheelEventArgs> MouseWheelEvent{"MouseWheel"};
    RoutedEvent_<MouseWheelEventHandler> MouseWheel() noexcept {
        return Event(MouseWheelEvent);
    }

    inline static constexpr Members::RoutedEvent<
        KeyboardFocusChangedEventArgs>
        GotKeyboardFocusEvent{"GotKeyboardFocus"};
    RoutedEvent_<KeyboardFocusChangedEventHandler> GotKeyboardFocus() noexcept {
        return Event(GotKeyboardFocusEvent);
    }

    inline static constexpr Members::RoutedEvent<
        KeyboardFocusChangedEventArgs>
        LostKeyboardFocusEvent{"LostKeyboardFocus"};
    RoutedEvent_<KeyboardFocusChangedEventHandler> LostKeyboardFocus() noexcept {
        return Event(LostKeyboardFocusEvent);
    }

    inline static constexpr Members::RoutedEvent<
        KeyEventArgs> KeyDownEvent{"KeyDown"};
    RoutedEvent_<KeyEventHandler> KeyDown() noexcept {
        return Event(KeyDownEvent);
    }

    inline static constexpr Members::RoutedEvent<
        KeyEventArgs> KeyUpEvent{"KeyUp"};
    RoutedEvent_<KeyEventHandler> KeyUp() noexcept {
        return Event(KeyUpEvent);
    }

    inline static constexpr Members::RoutedEvent<
        TextCompositionEventArgs> TextInputEvent{"TextInput"};
    RoutedEvent_<TextCompositionEventHandler> TextInput() noexcept {
        return Event(TextInputEvent);
    }

    explicit UIElement(TypeId runtimeType) noexcept;
    ~UIElement() override;

    UIElement* AsUIElement() noexcept override { return this; }
    const UIElement* AsUIElement() const noexcept override { return this; }
    UIElement* LayoutParent() const noexcept {
        Visual* parent = VisualParent();
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

    // Dependency properties
    inline static constexpr Members::Property<bool>
        ClipToBoundsProperty{"ClipToBounds"};
    inline static constexpr Members::Property<BlendMode>
        BlendModeProperty{"BlendMode"};
    inline static constexpr Members::Property<Base::Ref<Media::Effect>>
        EffectProperty{"Effect"};
    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        OpacityMaskProperty{"OpacityMask"};
    inline static constexpr Members::Property<bool>
        IsHitTestVisibleProperty{"IsHitTestVisible"};
    inline static constexpr Members::Property<Visibility>
        VisibilityProperty{"Visibility"};
    inline static constexpr Members::Property<bool>
        IsEnabledProperty{"IsEnabled"};
    inline static constexpr Members::Property<bool>
        AllowDropProperty{"AllowDrop"};
    inline static constexpr Members::ReadOnlyProperty<bool>
        IsMouseOverProperty{"IsMouseOver"};
    inline static constexpr Members::ReadOnlyProperty<bool>
        IsPressedProperty{"IsPressed"};
    inline static constexpr Members::ReadOnlyProperty<bool>
        IsKeyboardFocusedProperty{"IsKeyboardFocused"};
    inline static constexpr Members::ReadOnlyProperty<bool>
        IsKeyboardFocusWithinProperty{"IsKeyboardFocusWithin"};
    inline static constexpr Members::Property<bool>
        FocusableProperty{"Focusable"};
    inline static constexpr Members::Property<bool>
        IsTabStopProperty{"IsTabStop"};
    inline static constexpr Members::Property<std::uint32_t>
        TabIndexProperty{"TabIndex"};
    inline static constexpr Members::Property<bool>
        IsFocusScopeProperty{"IsFocusScope"};
    inline static constexpr Members::Property<double>
        OpacityProperty{"Opacity"};
    inline static constexpr Members::Property<Base::Ref<Media::Transform>>
        RenderTransformProperty{"RenderTransform"};
    inline static constexpr Members::Property<Point>
        RenderTransformOriginProperty{"RenderTransformOrigin"};

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
    Base::Result<void> OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept override;
    virtual Base::Result<Size> MeasureOverride(Size availableSize) noexcept;
    virtual Base::Result<Size> ArrangeOverride(Size finalSize) noexcept;
    Base::Result<void> MeasureChild(
        UIElement& child, Size availableSize) noexcept;
    Base::Result<void> ArrangeChild(
        UIElement& child, Rect finalRect) noexcept;
    UIElementChildRange LayoutChildren() const noexcept {
        return UIElementChildRange(VisualChildren());
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
