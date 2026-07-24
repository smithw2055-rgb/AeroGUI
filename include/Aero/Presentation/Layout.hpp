#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Presentation/ObjectTree.hpp>

#include <cstdint>

namespace Aero::Presentation {

using namespace Aero::Core;

using Point = Base::Point;
using Size = Base::Size;
using Rect = Base::Rect;
using Thickness = Base::Thickness;

enum class HorizontalAlignment : std::uint8_t { Stretch = 0U, Left, Center, Right };
enum class VerticalAlignment : std::uint8_t { Stretch = 0U, Top, Center, Bottom };

} // namespace Aero::Presentation

namespace Aero::Core {

template<>
struct MetaTypeTraits<Presentation::HorizontalAlignment> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("HorizontalAlignment");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "HorizontalAlignment";
    }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct MetaTypeTraits<Presentation::VerticalAlignment> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("VerticalAlignment");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "VerticalAlignment";
    }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct MetaTypeTraits<Base::Thickness> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("Thickness"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "Thickness"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

} // namespace Aero::Core

namespace Aero::Presentation {

using namespace Aero::Core;

struct Length final {
    AERO_TYPED_META(Length, NoMetadataBase)
    double value = 0.0;
    bool isAuto = true;

    static constexpr Length Auto() noexcept { return {}; }
    static constexpr Length Pixels(double value) noexcept {
        return {value, false};
    }
};

AERO_API bool IsFinite(Point value) noexcept;
AERO_API bool IsFinite(Size value) noexcept;
AERO_API bool IsFinite(Rect value) noexcept;
AERO_API bool IsFinite(Thickness value) noexcept;
AERO_API bool IsValidLayoutSize(Size value) noexcept;
AERO_API bool IsValidLayoutRect(Rect value) noexcept;
AERO_API Size Deflate(Size value, Thickness padding) noexcept;
AERO_API Size Inflate(Size value, Thickness padding) noexcept;
AERO_API Rect Intersect(Rect left, Rect right) noexcept;
AERO_API double RoundLayoutValue(double value, double dpiScale) noexcept;

class LayoutManager;
class UIElement;

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
    AERO_TYPED_META(UIElement, Visual)
public:
    template<class THandler>
    class RoutedEvent_ final {
    public:
        RoutedEvent_(UIElement& element, RoutedEventHandle event) noexcept
            : element_(&element), event_(event) {}

        Base::Result<void> TryAdd(
            const THandler& handler,
            bool handledEventsToo = false) noexcept {
            using Args = typename Detail::RoutedHandlerTraits<THandler>::Args;
            return element_->TryAddHandler(
                event_, Detail::RoutedHandlerStorage(
                    static_cast<const Base::Delegate<
                        void(Base::Object*, const Args&)>&>(handler)),
                handledEventsToo);
        }

        void Add(const THandler& handler,
            bool handledEventsToo = false) noexcept {
            Base::Result<void> result = TryAdd(handler, handledEventsToo);
            if (!result) {
                Base::ReportOutOfMemory(
                    sizeof(Detail::RoutedHandlerStorage),
                    alignof(Detail::RoutedHandlerStorage),
                    Base::MemoryTag::General);
            }
        }

        void operator+=(const THandler& handler) noexcept { Add(handler); }

        bool Remove(const THandler& handler) noexcept {
            using Args = typename Detail::RoutedHandlerTraits<THandler>::Args;
            return element_->RemoveHandler(
                event_, Detail::RoutedHandlerStorage(
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

    inline static constexpr RoutedEventHandle MouseMoveEvent =
        MakeRoutedEventHandle(StaticTypeIdValue_, "MouseMove");
    RoutedEvent_<MouseEventHandler> MouseMove() noexcept {
        return {*this, MouseMoveEvent};
    }

    inline static constexpr RoutedEventHandle MouseDownEvent =
        MakeRoutedEventHandle(StaticTypeIdValue_, "MouseDown");
    RoutedEvent_<MouseButtonEventHandler> MouseDown() noexcept {
        return {*this, MouseDownEvent};
    }

    inline static constexpr RoutedEventHandle MouseUpEvent =
        MakeRoutedEventHandle(StaticTypeIdValue_, "MouseUp");
    RoutedEvent_<MouseButtonEventHandler> MouseUp() noexcept {
        return {*this, MouseUpEvent};
    }

    inline static constexpr RoutedEventHandle GotKeyboardFocusEvent =
        MakeRoutedEventHandle(StaticTypeIdValue_, "GotKeyboardFocus");
    RoutedEvent_<KeyboardFocusChangedEventHandler> GotKeyboardFocus() noexcept {
        return {*this, GotKeyboardFocusEvent};
    }

    inline static constexpr RoutedEventHandle LostKeyboardFocusEvent =
        MakeRoutedEventHandle(StaticTypeIdValue_, "LostKeyboardFocus");
    RoutedEvent_<KeyboardFocusChangedEventHandler> LostKeyboardFocus() noexcept {
        return {*this, LostKeyboardFocusEvent};
    }

    inline static constexpr RoutedEventHandle KeyDownEvent =
        MakeRoutedEventHandle(StaticTypeIdValue_, "KeyDown");
    RoutedEvent_<KeyEventHandler> KeyDown() noexcept {
        return {*this, KeyDownEvent};
    }

    inline static constexpr RoutedEventHandle KeyUpEvent =
        MakeRoutedEventHandle(StaticTypeIdValue_, "KeyUp");
    RoutedEvent_<KeyEventHandler> KeyUp() noexcept {
        return {*this, KeyUpEvent};
    }

    inline static constexpr RoutedEventHandle TextInputEvent =
        MakeRoutedEventHandle(StaticTypeIdValue_, "TextInput");
    RoutedEvent_<TextCompositionEventHandler> TextInput() noexcept {
        return {*this, TextInputEvent};
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
        const Detail::RoutedHandlerStorage& handler,
        bool handledEventsToo = false) noexcept;
    template<class TArgs>
    Base::Result<void> TryAddHandler(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, const TArgs&)>& handler,
        bool handledEventsToo = false) noexcept {
        return TryAddHandler(
            event, Detail::RoutedHandlerStorage(handler), handledEventsToo);
    }
    bool RemoveHandler(
        RoutedEventHandle event,
        const Detail::RoutedHandlerStorage& handler) noexcept;
    template<class TArgs>
    bool RemoveHandler(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, const TArgs&)>& handler) noexcept {
        return RemoveHandler(event, Detail::RoutedHandlerStorage(handler));
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
    bool IsHitTestVisible() const noexcept;
    bool IsEnabled() const noexcept;
    bool IsMouseOver() const noexcept;
    bool IsPressed() const noexcept;
    bool IsKeyboardFocused() const noexcept;
    bool IsTabStop() const noexcept;
    std::uint32_t TabIndex() const noexcept;
    bool IsFocusScope() const noexcept;
    std::uint64_t LayoutRevision() const noexcept { return layoutRevision_; }

    // Dependency properties
    inline static constexpr Aero::Core::DependencyPropertyHandle
        ClipToBoundsProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "ClipToBounds");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        IsHitTestVisibleProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "IsHitTestVisible");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        IsEnabledProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "IsEnabled");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        IsMouseOverProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "IsMouseOver");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        IsPressedProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "IsPressed");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        IsKeyboardFocusedProperty =
            Aero::Core::MakeDependencyPropertyHandle(
                StaticTypeIdValue_, "IsKeyboardFocused");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        IsTabStopProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "IsTabStop");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        TabIndexProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "TabIndex");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        IsFocusScopeProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "IsFocusScope");

    // Property operations
    Base::Result<void> SetClipToBounds(bool value) noexcept;
    Base::Result<void> SetHitTestVisible(bool value) noexcept;
    Base::Result<void> SetEnabled(bool value) noexcept;
    Base::Result<void> SetTabStop(bool value) noexcept;
    Base::Result<void> SetTabIndex(std::uint32_t value) noexcept;
    Base::Result<void> SetFocusScope(bool value) noexcept;

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
    friend class LayoutManager;
    friend class RoutedEventManager;
    friend class PointerInputManager;
    friend class FocusManager;

    struct HandlerRecord final {
        RoutedEventHandle event;
        Detail::RoutedHandlerStorage handler;
        std::uint64_t sequence = 0U;
        bool handledEventsToo = false;
    };

    LayoutManager* manager_ = nullptr;
    Base::Vector<HandlerRecord> handlers_;
    std::uint64_t nextHandlerSequence_ = 1U;
    Size desiredSize_;
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
    void CleanupHandlers() noexcept;
};

struct LayoutDiagnostics final {
    std::uint64_t passVersion = 0U;
    std::uint32_t measuredCount = 0U;
    std::uint32_t arrangedCount = 0U;
    std::uint32_t pendingMeasureCount = 0U;
    std::uint32_t pendingArrangeCount = 0U;
};

class AERO_API LayoutManager final {
public:
    explicit LayoutManager(Dispatcher& dispatcher) noexcept;
    ~LayoutManager() noexcept;
    LayoutManager(const LayoutManager&) = delete;
    LayoutManager& operator=(const LayoutManager&) = delete;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> Attach(UIElement& parent, UIElement& child) noexcept;
    Base::Result<void> Detach(UIElement& parent, UIElement& child) noexcept;
    Base::Result<void> SetRoot(UIElement* root, Size availableSize) noexcept;
    Base::Result<void> InvalidateMeasure(UIElement& element) noexcept;
    Base::Result<void> InvalidateArrange(UIElement& element) noexcept;
    Base::Result<std::uint32_t> Flush() noexcept;
    LayoutDiagnostics Diagnostics() const noexcept;
    std::uint64_t PassVersion() const noexcept { return passVersion_; }

private:
    friend class UIElement;
    Dispatcher* dispatcher_ = nullptr;
    UIElement* root_ = nullptr;
    Size rootAvailableSize_;
    Base::Vector<UIElement*> measureQueue_;
    Base::Vector<UIElement*> arrangeQueue_;
    DispatcherFrameHookHandle phaseHook_;
    std::uint64_t passVersion_ = 0U;
    std::uint32_t measuredCount_ = 0U;
    std::uint32_t arrangedCount_ = 0U;
    bool flushing_ = false;

    Base::Result<void> VerifyElement(const UIElement& element) const noexcept;
    Base::Result<void> QueueMeasure(UIElement& element) noexcept;
    Base::Result<void> QueueArrange(UIElement& element) noexcept;
    Base::Result<void> MeasureElement(UIElement& element, Size constraint) noexcept;
    Base::Result<void> ArrangeElement(UIElement& element, Rect slot) noexcept;
    static void LayoutHook(void* context) noexcept;
};

} // namespace Aero::Presentation
