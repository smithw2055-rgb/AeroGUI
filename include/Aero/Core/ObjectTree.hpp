#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/EffectiveValueEngine.hpp>
#include <Aero/Core/TypeRegistry.hpp>

#include <cstdint>

namespace Aero::Core {

class ObjectTree;
class RoutedEventRegistry;
class TreeNode;

using RoutedEventId = MemberId;

struct RoutedEventHandle final {
    RoutedEventId value = InvalidMemberId;
    AERO_NODISCARD constexpr bool IsValid() const noexcept {
        return value != InvalidMemberId;
    }
};

AERO_NODISCARD constexpr bool operator==(
    RoutedEventHandle left, RoutedEventHandle right) noexcept {
    return left.value == right.value;
}

AERO_NODISCARD constexpr bool operator!=(
    RoutedEventHandle left, RoutedEventHandle right) noexcept {
    return !(left == right);
}

enum class RoutingStrategy : std::uint8_t {
    Direct = 0U,
    Tunnel,
    Bubble
};

enum class PointerAction : std::uint8_t { Move = 0U, Down, Up };

struct RoutedEventRegistration final {
    Base::StringView name;
    TypeId ownerType = InvalidTypeId;
    TypeId eventArgsType = InvalidTypeId;
    RoutingStrategy strategy = RoutingStrategy::Bubble;
};

struct RoutedEventArgs final {
    RoutedEventHandle event;
    TreeNode* source = nullptr;
    TreeNode* originalSource = nullptr;
    bool handled = false;
    bool hasPointer = false;
    PointerAction pointerAction = PointerAction::Move;
    std::uint32_t pointerId = 0U;
    double pointerX = 0.0;
    double pointerY = 0.0;
};

using RoutedEventHandler = void (*)(
    TreeNode& sender,
    RoutedEventArgs& args,
    void* context) noexcept;
using RoutedEventCleanup = void (*)(void* context) noexcept;

struct RoutedEventHandlerToken final {
    std::uint64_t value = 0U;
    AERO_NODISCARD constexpr bool IsValid() const noexcept {
        return value != 0U;
    }
};

struct TreeNodeHandle final {
    std::uint32_t index = UINT32_MAX;
    std::uint32_t generation = 0U;
    AERO_NODISCARD constexpr bool IsValid() const noexcept {
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
public:
    TreeNode(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& registry,
        TypeId runtimeType,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~TreeNode() override;

    AERO_NODISCARD ObjectTree* OwningTree() const noexcept { return tree_; }
    AERO_NODISCARD TreeNode* LogicalParent() const noexcept { return logicalParent_; }
    AERO_NODISCARD TreeNode* VisualParent() const noexcept { return visualParent_; }
    AERO_NODISCARD Base::Span<TreeNode* const> LogicalChildren() const noexcept {
        return {logicalChildren_.Data(), logicalChildren_.Size()};
    }
    AERO_NODISCARD Base::Span<TreeNode* const> VisualChildren() const noexcept {
        return {visualChildren_.Data(), visualChildren_.Size()};
    }
    AERO_NODISCARD bool IsLoaded() const noexcept { return loaded_; }
    AERO_NODISCARD TreeNodeHandle Handle() const noexcept { return handle_; }

    AERO_NODISCARD Base::Result<RoutedEventHandlerToken> AddHandler(
        RoutedEventHandle event,
        RoutedEventHandler handler,
        void* context = nullptr,
        RoutedEventCleanup cleanup = nullptr,
        bool handledEventsToo = false) noexcept;
    AERO_NODISCARD Base::Result<bool> RemoveHandler(
        RoutedEventHandlerToken token) noexcept;

private:
    friend class ObjectTree;
    friend class RoutedEventRegistry;

    struct HandlerRecord final {
        RoutedEventHandlerToken token;
        RoutedEventHandle event;
        RoutedEventHandler handler = nullptr;
        RoutedEventCleanup cleanup = nullptr;
        void* context = nullptr;
        std::uint64_t sequence = 0U;
        bool handledEventsToo = false;
    };

    ObjectTree* tree_ = nullptr;
    TreeNode* logicalParent_ = nullptr;
    TreeNode* visualParent_ = nullptr;
    Base::Vector<TreeNode*> logicalChildren_;
    Base::Vector<TreeNode*> visualChildren_;
    Base::Vector<HandlerRecord> handlers_;
    std::uint64_t nextHandlerToken_ = 1U;
    std::uint64_t nextHandlerSequence_ = 1U;
    bool loaded_ = false;
    TreeNodeHandle handle_;

    void CleanupHandlers() noexcept;
};

class AERO_API ObjectTree final {
public:
    ObjectTree(
        Dispatcher& dispatcher,
        EffectiveValueEngine& values,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~ObjectTree() noexcept;

    ObjectTree(const ObjectTree&) = delete;
    ObjectTree& operator=(const ObjectTree&) = delete;

    AERO_NODISCARD Base::Result<void> Initialize() noexcept;
    AERO_NODISCARD Base::Result<void> SetRoot(TreeNode* root) noexcept;
    AERO_NODISCARD TreeNode* Root() const noexcept { return root_; }
    AERO_NODISCARD Base::Result<TreeNodeHandle> GetHandle(
        const TreeNode& node) const noexcept;
    AERO_NODISCARD TreeNode* ResolveHandle(TreeNodeHandle handle) const noexcept;

    AERO_NODISCARD Base::Result<void> AttachLogical(
        TreeNode& parent, TreeNode& child) noexcept;
    AERO_NODISCARD Base::Result<void> DetachLogical(
        TreeNode& parent, TreeNode& child) noexcept;
    AERO_NODISCARD Base::Result<void> AttachVisual(
        TreeNode& parent, TreeNode& child) noexcept;
    AERO_NODISCARD Base::Result<void> DetachVisual(
        TreeNode& parent, TreeNode& child) noexcept;
    AERO_NODISCARD Base::Result<void> DetachNode(TreeNode& node) noexcept;

    void SetLifecycleHandler(
        TreeLifecycleHandler handler,
        void* context = nullptr) noexcept {
        lifecycleHandler_ = handler;
        lifecycleContext_ = context;
    }

    AERO_NODISCARD std::uint64_t Version() const noexcept { return version_; }
    AERO_NODISCARD bool IsMutating() const noexcept { return mutating_; }
    AERO_NODISCARD std::uint32_t PendingLifecycleCount() const noexcept {
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
    Base::IAllocator* allocator_ = nullptr;
    TreeNode* root_ = nullptr;
    Base::Vector<LifecycleRecord> lifecycleQueue_;
    Base::Vector<HandleEntry> handles_;
    DispatcherFrameHookHandle lifecycleHook_;
    TreeLifecycleHandler lifecycleHandler_ = nullptr;
    void* lifecycleContext_ = nullptr;
    std::uint64_t nextLifecycleSequence_ = 1U;
    std::uint64_t version_ = 0U;
    bool mutating_ = false;

    AERO_NODISCARD Base::Result<void> VerifyMutation(
        const TreeNode& first,
        const TreeNode* second = nullptr) const noexcept;
    AERO_NODISCARD bool IsLogicalAncestor(
        const TreeNode& possibleAncestor,
        const TreeNode& node) const noexcept;
    AERO_NODISCARD bool IsVisualAncestor(
        const TreeNode& possibleAncestor,
        const TreeNode& node) const noexcept;
    AERO_NODISCARD Base::Result<void> QueueLifecycleSubtree(
        TreeNode& node,
        bool loaded) noexcept;
    AERO_NODISCARD Base::Result<void> SetLoadedSubtree(
        TreeNode& node,
        bool loaded) noexcept;
    AERO_NODISCARD Base::Result<std::uint32_t> FlushLifecycle() noexcept;
    AERO_NODISCARD Base::Result<void> RegisterHandleSubtree(TreeNode& node) noexcept;
    void InvalidateHandleSubtree(TreeNode& node) noexcept;
    void RemoveChild(Base::Vector<TreeNode*>& children, TreeNode& child) noexcept;
    static void LifecycleHook(void* context) noexcept;
};

class AERO_API RoutedEventRegistry final {
public:
    RoutedEventRegistry(
        TypeRegistry& types,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~RoutedEventRegistry() noexcept;

    AERO_NODISCARD Base::Result<RoutedEventHandle> TryRegister(
        const RoutedEventRegistration& registration) noexcept;
    AERO_NODISCARD Base::Result<void> Freeze() noexcept;
    AERO_NODISCARD bool IsFrozen() const noexcept { return frozen_; }

    AERO_NODISCARD Base::Result<void> RegisterClassHandler(
        RoutedEventHandle event,
        TypeId classType,
        RoutedEventHandler handler,
        void* context = nullptr,
        RoutedEventCleanup cleanup = nullptr,
        bool handledEventsToo = false) noexcept;

    AERO_NODISCARD Base::Result<void> RaiseEvent(
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
        explicit EventRecord(Base::IAllocator* allocator) noexcept : name(allocator) {}
    };

    struct ClassHandlerRecord final {
        RoutedEventHandle event;
        TypeId classType = InvalidTypeId;
        RoutedEventHandler handler = nullptr;
        RoutedEventCleanup cleanup = nullptr;
        void* context = nullptr;
        std::uint64_t sequence = 0U;
        bool handledEventsToo = false;
    };

    TypeRegistry* types_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<EventRecord> events_;
    Base::Vector<ClassHandlerRecord> classHandlers_;
    std::uint64_t nextClassSequence_ = 1U;
    bool frozen_ = false;
    bool raising_ = false;

    AERO_NODISCARD const EventRecord* Find(RoutedEventHandle event) const noexcept;
    AERO_NODISCARD Base::Result<void> BuildRoute(
        TreeNode& source,
        RoutingStrategy strategy,
        Base::Vector<TreeNode*>& route) noexcept;
    void InvokeNode(TreeNode& node, RoutedEventArgs& args) noexcept;
    void CleanupClassHandlers() noexcept;
};

} // namespace Aero::Core
