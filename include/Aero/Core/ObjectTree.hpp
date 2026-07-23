#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/EffectiveValueEngine.hpp>
#include <Aero/Core/TypeRegistry.hpp>

#include <cstdint>

namespace Aero::Core {

struct MetaRegistrationContext;

class ObjectTree;
class RoutedEventRegistry;
class TreeNode;

using RoutedEventId = MemberId;

struct RoutedEventHandle final {
    RoutedEventId value = InvalidMemberId;
    constexpr bool IsValid() const noexcept {
        return value != InvalidMemberId;
    }
};

constexpr bool operator==(
    RoutedEventHandle left, RoutedEventHandle right) noexcept {
    return left.value == right.value;
}

constexpr bool operator!=(
    RoutedEventHandle left, RoutedEventHandle right) noexcept {
    return !(left == right);
}

enum class RoutingStrategy : std::uint8_t {
    Direct = 0U,
    Tunnel,
    Bubble
};

constexpr RoutedEventHandle MakeRoutedEventHandle(
    TypeId ownerType,
    Base::StringView name) noexcept {
    return {MakeMemberId(ownerType, MemberKind::Event, name)};
}

#define AERO_DECLARE_ROUTED_EVENT(eventName, handlerType) \
    inline static constexpr Aero::Core::RoutedEventHandle eventName##Event = \
        Aero::Core::MakeRoutedEventHandle(StaticTypeIdValue_, #eventName); \
    RoutedEvent_<handlerType> eventName() noexcept { \
        return {*this, eventName##Event}; \
    }

enum class PointerAction : std::uint8_t { Move = 0U, Down, Up };
enum class KeyboardAction : std::uint8_t { Down = 0U, Up };
enum class MouseButton : std::uint8_t { Left = 0U, Right, Middle, XButton1, XButton2 };
enum class MouseButtonState : std::uint8_t { Released = 0U, Pressed };

struct RoutedEventRegistration final {
    Base::StringView name;
    TypeId ownerType = InvalidTypeId;
    TypeId eventArgsType = InvalidTypeId;
    RoutingStrategy strategy = RoutingStrategy::Bubble;
};

struct EventArgs {
    AERO_DECLARE_TYPE_ID(EventArgs);
    explicit constexpr EventArgs(TypeId type = StaticTypeId()) noexcept
        : eventArgsType(type) {}
    TypeId eventArgsType = StaticTypeId();
};

struct RoutedEventArgs : EventArgs {
    AERO_DECLARE_TYPE_ID(RoutedEventArgs);
    explicit constexpr RoutedEventArgs(
        TypeId type = StaticTypeId()) noexcept : EventArgs(type) {}
    RoutedEventHandle routedEvent;
    Base::Object* source = nullptr;
    Base::Object* originalSource = nullptr;
    mutable bool handled = false;
};

struct InputEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE_ID(InputEventArgs);
    explicit constexpr InputEventArgs(
        TypeId type = StaticTypeId()) noexcept : RoutedEventArgs(type) {}
    std::uint32_t modifiers = 0U;
};

struct MouseEventArgs : InputEventArgs {
    AERO_DECLARE_TYPE_ID(MouseEventArgs);
    explicit constexpr MouseEventArgs(
        TypeId type = StaticTypeId()) noexcept : InputEventArgs(type) {}
    std::uint32_t pointerId = 0U;
    Base::Point position;
};

struct MouseButtonEventArgs final : MouseEventArgs {
    AERO_DECLARE_TYPE_ID(MouseButtonEventArgs);
    constexpr MouseButtonEventArgs() noexcept : MouseEventArgs(StaticTypeId()) {}
    MouseButton changedButton = MouseButton::Left;
    MouseButtonState buttonState = MouseButtonState::Released;
};

struct KeyEventArgs final : InputEventArgs {
    AERO_DECLARE_TYPE_ID(KeyEventArgs);
    constexpr KeyEventArgs() noexcept : InputEventArgs(StaticTypeId()) {}
    KeyboardAction action = KeyboardAction::Down;
    std::uint32_t key = 0U;
    bool isRepeat = false;
};

struct TextCompositionEventArgs final : InputEventArgs {
    AERO_DECLARE_TYPE_ID(TextCompositionEventArgs);
    constexpr TextCompositionEventArgs() noexcept : InputEventArgs(StaticTypeId()) {}
    Base::StringView text;
};

struct KeyboardFocusChangedEventArgs final : RoutedEventArgs {
    AERO_DECLARE_TYPE_ID(KeyboardFocusChangedEventArgs);
    constexpr KeyboardFocusChangedEventArgs() noexcept
        : RoutedEventArgs(StaticTypeId()) {}
    TreeNode* oldFocus = nullptr;
    TreeNode* newFocus = nullptr;
};

using EventHandler = Base::Delegate<void(Base::Object*, const EventArgs&)>;
using RoutedEventHandler =
    Base::Delegate<void(Base::Object*, const RoutedEventArgs&)>;
using MouseEventHandler =
    Base::Delegate<void(Base::Object*, const MouseEventArgs&)>;
using MouseButtonEventHandler =
    Base::Delegate<void(Base::Object*, const MouseButtonEventArgs&)>;
using KeyEventHandler =
    Base::Delegate<void(Base::Object*, const KeyEventArgs&)>;
using TextCompositionEventHandler =
    Base::Delegate<void(Base::Object*, const TextCompositionEventArgs&)>;
using KeyboardFocusChangedEventHandler =
    Base::Delegate<void(Base::Object*, const KeyboardFocusChangedEventArgs&)>;

namespace Detail {

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

struct TreeNodeHandle final {
    std::uint32_t index = UINT32_MAX;
    std::uint32_t generation = 0U;
    constexpr bool IsValid() const noexcept {
        return index != UINT32_MAX && generation != 0U;
    }
};

struct TreeLifecycleEvent final {
    TreeNode* node = nullptr;
    bool loaded = false;
    std::uint64_t treeVersion = 0U;
};

using TreeLifecycleHandler = void (*)(
    const TreeLifecycleEvent& event,
    void* context) noexcept;

class AERO_API TreeNode : public DependencyObject {
    AERO_DECLARE_METADATA(TreeNode, DependencyObject)
public:
    template<class THandler>
    class RoutedEvent_ final {
    public:
        RoutedEvent_(TreeNode& node, RoutedEventHandle event) noexcept
            : node_(&node), event_(event) {}

        Base::Result<void> TryAdd(
            const THandler& handler,
            bool handledEventsToo = false) noexcept {
            using Args = typename Detail::RoutedHandlerTraits<THandler>::Args;
            return node_->TryAddHandler(
                event_, Detail::RoutedHandlerStorage(
                    static_cast<const Base::Delegate<
                        void(Base::Object*, const Args&)>&>(handler)),
                handledEventsToo);
        }

        void Add(const THandler& handler, bool handledEventsToo = false) noexcept {
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
            return node_->RemoveHandler(
                event_, Detail::RoutedHandlerStorage(
                    static_cast<const Base::Delegate<
                        void(Base::Object*, const Args&)>&>(handler)));
        }

        void operator-=(const THandler& handler) noexcept {
            static_cast<void>(Remove(handler));
        }

    private:
        TreeNode* node_ = nullptr;
        RoutedEventHandle event_;
    };

    // Routed events
    AERO_DECLARE_ROUTED_EVENT(MouseMove, MouseEventHandler);
    AERO_DECLARE_ROUTED_EVENT(MouseDown, MouseButtonEventHandler);
    AERO_DECLARE_ROUTED_EVENT(MouseUp, MouseButtonEventHandler);
    AERO_DECLARE_ROUTED_EVENT(
        GotKeyboardFocus, KeyboardFocusChangedEventHandler);
    AERO_DECLARE_ROUTED_EVENT(
        LostKeyboardFocus, KeyboardFocusChangedEventHandler);
    AERO_DECLARE_ROUTED_EVENT(KeyDown, KeyEventHandler);
    AERO_DECLARE_ROUTED_EVENT(KeyUp, KeyEventHandler);
    AERO_DECLARE_ROUTED_EVENT(TextInput, TextCompositionEventHandler);

    // Construction and tree operations
    explicit TreeNode(TypeId runtimeType) noexcept;
    ~TreeNode() override;

    ObjectTree* OwningTree() const noexcept { return tree_; }
    TreeNode* LogicalParent() const noexcept { return logicalParent_; }
    TreeNode* VisualParent() const noexcept { return visualParent_; }
    Base::Span<TreeNode* const> LogicalChildren() const noexcept {
        return {logicalChildren_.Data(), logicalChildren_.Size()};
    }
    Base::Span<TreeNode* const> VisualChildren() const noexcept {
        return {visualChildren_.Data(), visualChildren_.Size()};
    }
    bool IsLoaded() const noexcept { return loaded_; }
    TreeNodeHandle Handle() const noexcept { return handle_; }

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

private:
    friend class ObjectTree;
    friend class RoutedEventRegistry;

    struct HandlerRecord final {
        RoutedEventHandle event;
        Detail::RoutedHandlerStorage handler;
        std::uint64_t sequence = 0U;
        bool handledEventsToo = false;
    };

    ObjectTree* tree_ = nullptr;
    TreeNode* logicalParent_ = nullptr;
    TreeNode* visualParent_ = nullptr;
    Base::Vector<TreeNode*> logicalChildren_;
    Base::Vector<TreeNode*> visualChildren_;
    Base::Vector<HandlerRecord> handlers_;
    std::uint64_t nextHandlerSequence_ = 1U;
    bool loaded_ = false;
    TreeNodeHandle handle_;

    void CleanupHandlers() noexcept;
};

class AERO_API ObjectTree final {
public:
    ObjectTree(
        Dispatcher& dispatcher,
        EffectiveValueEngine& values) noexcept;
    ~ObjectTree() noexcept;

    ObjectTree(const ObjectTree&) = delete;
    ObjectTree& operator=(const ObjectTree&) = delete;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> SetRoot(TreeNode* root) noexcept;
    TreeNode* Root() const noexcept { return root_; }
    Base::Result<TreeNodeHandle> GetHandle(
        const TreeNode& node) const noexcept;
    TreeNode* ResolveHandle(TreeNodeHandle handle) const noexcept;

    Base::Result<void> AttachLogical(
        TreeNode& parent, TreeNode& child) noexcept;
    Base::Result<void> DetachLogical(
        TreeNode& parent, TreeNode& child) noexcept;
    Base::Result<void> AttachVisual(
        TreeNode& parent, TreeNode& child) noexcept;
    Base::Result<void> DetachVisual(
        TreeNode& parent, TreeNode& child) noexcept;
    Base::Result<void> DetachNode(TreeNode& node) noexcept;

    void SetLifecycleHandler(
        TreeLifecycleHandler handler,
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
        TreeNode* node = nullptr;
        bool loaded = false;
        std::uint64_t sequence = 0U;
        std::uint64_t treeVersion = 0U;
    };
    struct HandleEntry final {
        TreeNode* node = nullptr;
        std::uint32_t generation = 1U;
    };

    Dispatcher* dispatcher_ = nullptr;
    EffectiveValueEngine* values_ = nullptr;
    TreeNode* root_ = nullptr;
    Base::Vector<LifecycleRecord> lifecycleQueue_;
    Base::Vector<HandleEntry> handles_;
    DispatcherFrameHookHandle lifecycleHook_;
    TreeLifecycleHandler lifecycleHandler_ = nullptr;
    void* lifecycleContext_ = nullptr;
    std::uint64_t nextLifecycleSequence_ = 1U;
    std::uint64_t version_ = 0U;
    bool mutating_ = false;

    Base::Result<void> VerifyMutation(
        const TreeNode& first,
        const TreeNode* second = nullptr) const noexcept;
    bool IsLogicalAncestor(
        const TreeNode& possibleAncestor,
        const TreeNode& node) const noexcept;
    bool IsVisualAncestor(
        const TreeNode& possibleAncestor,
        const TreeNode& node) const noexcept;
    Base::Result<void> QueueLifecycleSubtree(
        TreeNode& node,
        bool loaded) noexcept;
    Base::Result<void> SetLoadedSubtree(
        TreeNode& node,
        bool loaded) noexcept;
    Base::Result<std::uint32_t> FlushLifecycle() noexcept;
    Base::Result<void> RegisterHandleSubtree(TreeNode& node) noexcept;
    void InvalidateHandleSubtree(TreeNode& node) noexcept;
    void RemoveChild(Base::Vector<TreeNode*>& children, TreeNode& child) noexcept;
    static void LifecycleHook(void* context) noexcept;
};

class AERO_API RoutedEventRegistry final {
public:
    explicit RoutedEventRegistry(TypeRegistry& types) noexcept;
    ~RoutedEventRegistry() noexcept;

    Base::Result<RoutedEventHandle> TryRegister(
        const RoutedEventRegistration& registration) noexcept;
    Base::Result<void> Freeze() noexcept;
    bool IsFrozen() const noexcept { return frozen_; }

    template<class TArgs>
    Base::Result<void> RegisterClassHandler(
        RoutedEventHandle event,
        TypeId classType,
        const Base::Delegate<void(Base::Object*, const TArgs&)>& handler,
        bool handledEventsToo = false) noexcept;

    Base::Result<void> RaiseEvent(
        TreeNode& source,
        RoutedEventHandle event,
        RoutedEventArgs* args = nullptr) noexcept;

private:
    struct EventRecord final {
        RoutedEventHandle handle;
        TypeId ownerType = InvalidTypeId;
        TypeId argsType = InvalidTypeId;
        RoutingStrategy strategy = RoutingStrategy::Bubble;
        Base::String name;
        EventRecord() noexcept : name() {}
    };

    struct ClassHandlerRecord final {
        RoutedEventHandle event;
        TypeId classType = InvalidTypeId;
        Detail::RoutedHandlerStorage handler;
        std::uint64_t sequence = 0U;
        bool handledEventsToo = false;
    };

    TypeRegistry* types_ = nullptr;
    Base::Vector<EventRecord> events_;
    Base::Vector<ClassHandlerRecord> classHandlers_;
    std::uint64_t nextClassSequence_ = 1U;
    bool frozen_ = false;
    bool raising_ = false;

    const EventRecord* Find(RoutedEventHandle event) const noexcept;
    Base::Result<void> BuildRoute(
        TreeNode& source,
        RoutingStrategy strategy,
        Base::Vector<TreeNode*>& route) noexcept;
    void InvokeNode(TreeNode& node, RoutedEventArgs& args) noexcept;
    void CleanupClassHandlers() noexcept;
};

template<class TArgs>
Base::Result<void> RoutedEventRegistry::RegisterClassHandler(
    RoutedEventHandle event,
    TypeId classType,
    const Base::Delegate<void(Base::Object*, const TArgs&)>& handler,
    bool handledEventsToo) noexcept {
    static_assert(std::is_base_of<RoutedEventArgs, TArgs>::value,
        "Routed event arguments must derive from RoutedEventArgs");
    if (!frozen_) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "RoutedEventRegistry must be frozen before handlers");
    }
    if (raising_) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Cannot mutate class handlers during routed event dispatch");
    }
    const EventRecord* record = Find(event);
    if (record == nullptr || types_->FindType(classType) == nullptr ||
        handler.Empty() || record->argsType != TArgs::StaticTypeId()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Class handler registration is invalid");
    }
    ClassHandlerRecord value;
    value.event = event;
    value.classType = classType;
    value.handler = Detail::RoutedHandlerStorage(handler);
    value.handledEventsToo = handledEventsToo;
    value.sequence = nextClassSequence_++;
    return classHandlers_.TryPushBack(std::move(value));
}

} // namespace Aero::Core
