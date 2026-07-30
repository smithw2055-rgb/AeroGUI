#pragma once

#include <Aero/Detail/RuntimeManagersFwd.hpp>

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/RoutedEvent.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Core/Metadata/TypeRegistry.hpp>
#include <Aero/Presentation/InputValues.hpp>

#include <cstdint>

namespace Aero::Core {
}

namespace Aero::Presentation {

using namespace Aero::Core;

class ObjectTree;
class Visual;
class UIElement;
class FrameworkElement;

struct EventArgs {
    AERO_DECLARE_TYPE(EventArgs, NoMetadataBase)
    explicit constexpr EventArgs(TypeId type = StaticTypeId()) noexcept
        : eventArgsType(type) {}
    TypeId eventArgsType = StaticTypeId();
};

struct RoutedEventArgs : EventArgs {
    AERO_DECLARE_TYPE(RoutedEventArgs, EventArgs)
    explicit constexpr RoutedEventArgs(
        TypeId type = StaticTypeId()) noexcept : EventArgs(type) {}
    RoutedEventHandle routedEvent;
    Base::Object* source = nullptr;
    Base::Object* originalSource = nullptr;
    mutable bool handled = false;
};

struct InputEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(InputEventArgs, RoutedEventArgs)
    explicit constexpr InputEventArgs(
        TypeId type = StaticTypeId()) noexcept : RoutedEventArgs(type) {}
    std::uint32_t modifiers = 0U;
};

struct MouseEventArgs : InputEventArgs {
    AERO_DECLARE_TYPE(MouseEventArgs, InputEventArgs)
    explicit constexpr MouseEventArgs(
        TypeId type = StaticTypeId()) noexcept : InputEventArgs(type) {}
    std::uint32_t pointerId = 0U;
    Base::Point position;
};

struct MouseButtonEventArgs final : MouseEventArgs {
    AERO_DECLARE_TYPE(MouseButtonEventArgs, MouseEventArgs)
    constexpr MouseButtonEventArgs() noexcept : MouseEventArgs(StaticTypeId()) {}
    MouseButton changedButton = MouseButton::Left;
    MouseButtonState buttonState = MouseButtonState::Released;
};

struct MouseWheelEventArgs final : MouseEventArgs {
    AERO_DECLARE_TYPE(MouseWheelEventArgs, MouseEventArgs)
    constexpr MouseWheelEventArgs() noexcept : MouseEventArgs(StaticTypeId()) {}
    double deltaX = 0.0;
    double deltaY = 0.0;
};

struct KeyEventArgs final : InputEventArgs {
    AERO_DECLARE_TYPE(KeyEventArgs, InputEventArgs)
    constexpr KeyEventArgs() noexcept : InputEventArgs(StaticTypeId()) {}
    KeyboardAction action = KeyboardAction::Down;
    std::uint32_t key = 0U;
    bool isRepeat = false;
};

struct TextCompositionEventArgs final : InputEventArgs {
    AERO_DECLARE_TYPE(TextCompositionEventArgs, InputEventArgs)
    constexpr TextCompositionEventArgs() noexcept : InputEventArgs(StaticTypeId()) {}
    Base::StringView text;
};

struct KeyboardFocusChangedEventArgs final : RoutedEventArgs {
    AERO_DECLARE_TYPE(KeyboardFocusChangedEventArgs, RoutedEventArgs)
    constexpr KeyboardFocusChangedEventArgs() noexcept
        : RoutedEventArgs(StaticTypeId()) {}
    UIElement* oldFocus = nullptr;
    UIElement* newFocus = nullptr;
};

using EventHandler = Base::Delegate<void(Base::Object*, const EventArgs&)>;
using RoutedEventHandler =
    Base::Delegate<void(Base::Object*, const RoutedEventArgs&)>;
using MouseEventHandler =
    Base::Delegate<void(Base::Object*, const MouseEventArgs&)>;
using MouseButtonEventHandler =
    Base::Delegate<void(Base::Object*, const MouseButtonEventArgs&)>;
using MouseWheelEventHandler =
    Base::Delegate<void(Base::Object*, const MouseWheelEventArgs&)>;
using KeyEventHandler =
    Base::Delegate<void(Base::Object*, const KeyEventArgs&)>;
using TextCompositionEventHandler =
    Base::Delegate<void(Base::Object*, const TextCompositionEventArgs&)>;
using KeyboardFocusChangedEventHandler =
    Base::Delegate<void(Base::Object*, const KeyboardFocusChangedEventArgs&)>;

namespace Detail {

// A separately allocated lifetime cell is used for stack/embedded Visuals that
// cannot participate in intrusive ownership. Managed Visuals are retained by a
// strong Ref; unmanaged Visuals invalidate this cell from their destructor.
class VisualLifetime final : public Base::Object {
public:
    explicit VisualLifetime(Visual& node) noexcept : node_(&node) {}
    ~VisualLifetime() override = default;

    Visual* Node() const noexcept { return node_; }
    void Invalidate() noexcept { node_ = nullptr; }

private:
    Visual* node_ = nullptr;
};

struct VisualLease;

class RoutedHandlerStorage final {
public:
    RoutedHandlerStorage() noexcept = default;

    template<class TArgs>
    explicit RoutedHandlerStorage(
        const Base::Delegate<void(Base::Object*, const TArgs&)>& handler) noexcept {
        static_assert(std::is_base_of<RoutedEventArgs, TArgs>::value,
            "Routed event arguments must derive from RoutedEventArgs");
        using Handler = Base::Delegate<void(Base::Object*, const TArgs&)>;
        static_assert(sizeof(Handler) <= sizeof(storage_),
            "Routed handler storage is too small");
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
    TypeId ArgsType() const noexcept { return argsType_; }
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
                return *static_cast<const Handler*>(left) ==
                    *static_cast<const Handler*>(right);
            },
            [](const void* value, Base::Object* sender,
                const RoutedEventArgs& args) noexcept {
                static_cast<const Handler*>(value)->Invoke(
                    sender, static_cast<const TArgs&>(args));
            }};
        return operations;
    }

    void Reset() noexcept;

    alignas(void*) unsigned char storage_[4U * sizeof(void*)]{};
    const Operations* operations_ = nullptr;
    TypeId argsType_ = InvalidTypeId;
};

template<class T>
struct RoutedHandlerTraits;

template<class TArgs>
struct RoutedHandlerTraits<
    Base::Delegate<void(Base::Object*, const TArgs&)>> final {
    using Args = TArgs;
};

} // namespace Detail

struct VisualHandle final {
    std::uint32_t index = UINT32_MAX;
    std::uint32_t generation = 0U;
    constexpr bool IsValid() const noexcept {
        return index != UINT32_MAX && generation != 0U;
    }
};

struct ObjectTreeLifecycleEvent final {
    Visual* node = nullptr;
    bool loaded = false;
    std::uint64_t treeVersion = 0U;
};

using ObjectTreeLifecycleHandler = void (*)(
    const ObjectTreeLifecycleEvent& event,
    void* context) noexcept;

class AERO_API Visual : public DependencyObject {
    AERO_DECLARE_TYPE(Visual, DependencyObject)
public:
    // Construction and tree operations
    explicit Visual(TypeId runtimeType) noexcept;
    ~Visual() override;

    ObjectTree* OwningTree() const noexcept { return tree_; }
    Visual* LogicalParent() const noexcept { return logicalParent_; }
    Visual* VisualParent() const noexcept { return visualParent_; }
    Base::Span<Visual* const> LogicalChildren() const noexcept {
        return {logicalChildren_.Data(), logicalChildren_.Size()};
    }
    Base::Span<Visual* const> VisualChildren() const noexcept {
        return {visualChildren_.Data(), visualChildren_.Size()};
    }
    bool IsLoaded() const noexcept { return loaded_; }
    VisualHandle Handle() const noexcept { return handle_; }

    virtual UIElement* AsUIElement() noexcept { return nullptr; }
    virtual const UIElement* AsUIElement() const noexcept { return nullptr; }
    virtual FrameworkElement* AsFrameworkElement() noexcept { return nullptr; }
    virtual const FrameworkElement* AsFrameworkElement() const noexcept { return nullptr; }


private:
    friend class ObjectTree;
    friend class Aero::Detail::PresentationRuntimeAccess;
    friend struct Detail::VisualLease;

    Base::Result<Base::Ref<Detail::VisualLifetime>>
    AcquireLifetime() noexcept;

    ObjectTree* tree_ = nullptr;
    Visual* logicalParent_ = nullptr;
    Visual* visualParent_ = nullptr;
    Base::Vector<Visual*> logicalChildren_;
    Base::Vector<Visual*> visualChildren_;
    bool loaded_ = false;
    VisualHandle handle_;
    Base::Ref<Detail::VisualLifetime> lifetime_;

};

namespace Detail {

struct VisualLease final {
    Base::Ref<Visual> strong;
    Base::Ref<VisualLifetime> lifetime;

    static Base::Result<VisualLease> Acquire(Visual& node) noexcept;
    Visual* Resolve() const noexcept {
        return strong ? strong.Get()
                      : (lifetime ? lifetime->Node() : nullptr);
    }
};

} // namespace Detail

class AERO_API ObjectTree final {
public:
    ObjectTree(
        Dispatcher& dispatcher,
        EffectiveValueEngine& values) noexcept;
    ~ObjectTree() noexcept;

    ObjectTree(const ObjectTree&) = delete;
    ObjectTree& operator=(const ObjectTree&) = delete;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> SetRoot(Visual* root) noexcept;
    Visual* Root() const noexcept { return root_; }
    Base::Result<VisualHandle> GetHandle(
        const Visual& node) const noexcept;
    Visual* ResolveHandle(VisualHandle handle) const noexcept;

    Base::Result<void> AttachLogical(
        Visual& parent, Visual& child) noexcept;
    Base::Result<void> DetachLogical(
        Visual& parent, Visual& child) noexcept;
    Base::Result<void> AttachVisual(
        Visual& parent, Visual& child) noexcept;
    Base::Result<void> DetachVisual(
        Visual& parent, Visual& child) noexcept;
    Base::Result<void> DetachNode(Visual& node) noexcept;

    void SetLifecycleHandler(
        ObjectTreeLifecycleHandler handler,
        void* context = nullptr) noexcept {
        lifecycleHandler_ = handler;
        lifecycleContext_ = context;
    }

    std::uint64_t Version() const noexcept { return version_; }
    bool IsMutating() const noexcept { return mutating_; }
    std::uint32_t PendingLifecycleCount() const noexcept {
        return lifecycleQueue_.Size();
    }

private:
    struct LifecycleRecord final {
        Detail::VisualLease node;
        bool loaded = false;
        std::uint64_t sequence = 0U;
        std::uint64_t treeVersion = 0U;
    };
    struct HandleEntry final {
        Visual* node = nullptr;
        std::uint32_t generation = 1U;
    };

    Dispatcher* dispatcher_ = nullptr;
    EffectiveValueEngine* values_ = nullptr;
    Visual* root_ = nullptr;
    Base::Vector<LifecycleRecord> lifecycleQueue_;
    Base::Vector<HandleEntry> handles_;
    DispatcherFrameHookHandle lifecycleHook_;
    ObjectTreeLifecycleHandler lifecycleHandler_ = nullptr;
    void* lifecycleContext_ = nullptr;
    DependencyPropertyChangedEventHandler dataContextChangedHandler_;
    std::uint64_t nextLifecycleSequence_ = 1U;
    std::uint64_t version_ = 0U;
    bool mutating_ = false;

    Base::Result<void> VerifyMutation(
        const Visual& first,
        const Visual* second = nullptr) const noexcept;
    bool IsLogicalAncestor(
        const Visual& possibleAncestor,
        const Visual& node) const noexcept;
    bool IsVisualAncestor(
        const Visual& possibleAncestor,
        const Visual& node) const noexcept;
    Base::Result<void> CollectLogicalSubtree(
        Visual& node,
        Base::Vector<Visual*>& nodes) noexcept;
    Base::Result<void> StageLifecycleSubtree(
        Visual& node,
        bool loaded,
        Base::Vector<LifecycleRecord>& staged) noexcept;
    void PublishLifecycle(
        Base::Vector<LifecycleRecord>& staged) noexcept;
    void ApplyLoadedSubtree(Visual& node, bool loaded) noexcept;
    void SetTreeSubtree(Visual& node, ObjectTree* tree) noexcept;
    Base::Result<std::uint32_t> FlushLifecycle() noexcept;
    Base::Result<void> RegisterHandleSubtree(Visual& node) noexcept;
    void InvalidateHandleSubtree(Visual& node) noexcept;
    Base::Result<void> TrackInheritedValues(Visual& node) noexcept;
    void UntrackInheritedValues(Visual& node) noexcept;
    void OnDataContextChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void RemoveChild(Base::Vector<Visual*>& children, Visual& child) noexcept;
    static void LifecycleHook(void* context) noexcept;
};



} // namespace Aero::Presentation
