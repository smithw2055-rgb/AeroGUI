#pragma once

// Internal routed-event storage, route snapshots and dispatch.

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/RoutedEvent.hpp>
#include "gui/GuiPrivate.hpp"

#include <cstddef>
#include <new>
#include <type_traits>

namespace Aero::GuiPrivate::Detail {

class RoutedHandlerStorage {
public:
    RoutedHandlerStorage() noexcept = default;

    RoutedHandlerStorage(
        const void* value,
        std::size_t size,
        std::size_t alignment,
        Meta::TypeId argsType,
        void (*copy)(void*, const void*) noexcept,
        void (*destroy)(void*) noexcept,
        bool (*equals)(const void*, const void*) noexcept,
        void (*invoke)(const void*, Base::Object*, RoutedEventArgs&) noexcept) noexcept
        : size_(size), alignment_(alignment), argsType_(argsType),
          copy_(copy), destroy_(destroy), equals_(equals), invoke_(invoke) {
        AERO_ASSERT(value != nullptr && size_ <= sizeof(storage_) && alignment_ <= alignof(void*));
        copy_(storage_, value);
    }

    template<class TArgs>
    explicit RoutedHandlerStorage(
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler) noexcept
        : RoutedHandlerStorage(
              &handler,
              sizeof(handler),
              alignof(decltype(handler)),
              TArgs::StaticTypeId(),
              [](void* destination, const void* source) noexcept {
                  using Handler = Base::Delegate<void(Base::Object*, TArgs&)>;
                  new (destination) Handler(*static_cast<const Handler*>(source));
              },
              [](void* value) noexcept {
                  using Handler = Base::Delegate<void(Base::Object*, TArgs&)>;
                  static_cast<Handler*>(value)->~Handler();
              },
              [](const void* left, const void* right) noexcept {
                  using Handler = Base::Delegate<void(Base::Object*, TArgs&)>;
                  return *static_cast<const Handler*>(left) == *static_cast<const Handler*>(right);
              },
              [](const void* value, Base::Object* sender, RoutedEventArgs& args) noexcept {
                  using Handler = Base::Delegate<void(Base::Object*, TArgs&)>;
                  static_cast<const Handler*>(value)->Invoke(sender, static_cast<TArgs&>(args));
              }) {
        static_assert(std::is_base_of<RoutedEventArgs, TArgs>::value,
            "Routed event arguments must derive from RoutedEventArgs");
    }

    RoutedHandlerStorage(const RoutedHandlerStorage& other) noexcept { CopyFrom(other); }
    RoutedHandlerStorage(RoutedHandlerStorage&& other) noexcept { CopyFrom(other); other.Reset(); }

    RoutedHandlerStorage& operator=(const RoutedHandlerStorage& other) noexcept {
        if (this != &other) {
            Reset();
            CopyFrom(other);
        }
        return *this;
    }

    RoutedHandlerStorage& operator=(RoutedHandlerStorage&& other) noexcept {
        if (this != &other) {
            Reset();
            CopyFrom(other);
            other.Reset();
        }
        return *this;
    }

    ~RoutedHandlerStorage() noexcept { Reset(); }

    bool Empty() const noexcept { return copy_ == nullptr; }
    Meta::TypeId ArgsType() const noexcept { return argsType_; }

    bool Equals(const RoutedHandlerStorage& other) const noexcept {
        return copy_ == other.copy_ && destroy_ == other.destroy_ &&
            equals_ == other.equals_ && invoke_ == other.invoke_ &&
            argsType_ == other.argsType_ &&
            (Empty() || equals_(storage_, other.storage_));
    }

    void Invoke(Base::Object* sender, RoutedEventArgs& args) const noexcept {
        AERO_ASSERT(
            !Empty() &&
            (args.GetEventArgsType() == argsType_ ||
             argsType_ == RoutedEventArgs::StaticTypeId()));
        invoke_(storage_, sender, args);
    }

private:
    void CopyFrom(const RoutedHandlerStorage& other) noexcept {
        size_ = other.size_;
        alignment_ = other.alignment_;
        argsType_ = other.argsType_;
        copy_ = other.copy_;
        destroy_ = other.destroy_;
        equals_ = other.equals_;
        invoke_ = other.invoke_;
        if (!other.Empty()) copy_(storage_, other.storage_);
    }

    void Reset() noexcept {
        if (!Empty()) destroy_(storage_);
        size_ = 0U;
        alignment_ = 0U;
        argsType_ = Meta::InvalidTypeId;
        copy_ = nullptr;
        destroy_ = nullptr;
        equals_ = nullptr;
        invoke_ = nullptr;
    }

    alignas(void*) unsigned char storage_[4U * sizeof(void*)]{};
    std::size_t size_ = 0U;
    std::size_t alignment_ = 0U;
    Meta::TypeId argsType_ = Meta::InvalidTypeId;
    void (*copy_)(void*, const void*) noexcept = nullptr;
    void (*destroy_)(void*) noexcept = nullptr;
    bool (*equals_)(const void*, const void*) noexcept = nullptr;
    void (*invoke_)(const void*, Base::Object*, RoutedEventArgs&) noexcept = nullptr;
};

} // namespace Aero::GuiPrivate::Detail

#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/ContentElement.hpp>
#include <Aero/Visual.hpp>
#include <Aero/UIElement.hpp>

#include <utility>

namespace Aero::GuiPrivate::Detail {

struct EventRouteNode {
    Base::Ref<DependencyObject> retained;
    DependencyObject* borrowed = nullptr;

    static EventRouteNode Acquire(DependencyObject& value) noexcept {
        EventRouteNode node;
        node.retained = Base::Ref<DependencyObject>::TryFromBorrowed(value);
        node.borrowed = &value;
        return node;
    }

    DependencyObject* Resolve() const noexcept {
        return retained ? retained.Get() : borrowed;
    }
};

class EventRoute {
public:
    explicit EventRoute(Base::IAllocator* allocator = nullptr) noexcept
        : nodes_(allocator != nullptr
              ? allocator
              : &Base::GetDefaultAllocator()) {}

    Base::Result<void> Build(
        DependencyObject& source,
        RoutingStrategy strategy) noexcept {
        nodes_.Clear();

        DependencyObject* current = &source;
        while (current != nullptr) {
            Base::Result<void> appended =
                nodes_.PushBack(EventRouteNode::Acquire(*current));
            if (!appended) return appended.GetStatus();
            if (strategy == RoutingStrategy::Direct) break;
            current = GetParent(*current);
        }

        if (strategy == RoutingStrategy::Tunnel && nodes_.Size() > 1U) {
            for (std::uint32_t left = 0U, right = nodes_.Size() - 1U;
                 left < right; ++left, --right) {
                EventRouteNode temporary = std::move(nodes_[left]);
                nodes_[left] = std::move(nodes_[right]);
                nodes_[right] = std::move(temporary);
            }
        }
        return {};
    }

    Base::Span<const EventRouteNode> Nodes() const noexcept {
        return nodes_.AsSpan();
    }
    std::uint32_t Size() const noexcept { return nodes_.Size(); }
    bool Empty() const noexcept { return nodes_.Empty(); }

private:
    static DependencyObject* GetParent(
        DependencyObject& object) noexcept {
        const Meta::TypeRegistry& types = object.PropertyRegistry().Types();
        if (types.IsDerivedFrom(
                object.RuntimeType(), ContentElement::StaticTypeId())) {
            auto& content = static_cast<ContentElement&>(object);
            DependencyObject* parent = content.GetParent();
            return parent != nullptr
                ? parent
                : static_cast<DependencyObject*>(content.GetContentHost());
        }
        if (types.IsDerivedFrom(
                object.RuntimeType(), Visual::StaticTypeId())) {
            auto& visual = static_cast<Visual&>(object);
            if (visual.GetVisualParent() != nullptr) {
                return visual.GetVisualParent();
            }
        }
        return LogicalTreeHelper::GetParent(object);
    }

    Base::Vector<EventRouteNode> nodes_;
};

} // namespace Aero::GuiPrivate::Detail


namespace Aero { class ContentElement; }


namespace Aero::GuiPrivate::Detail {

using namespace Aero::Meta;
using namespace Aero::Threading;

class AERO_API EventRouter {
public:
    explicit EventRouter(void* eventState) noexcept;
    ~EventRouter() noexcept;

    template<class TArgs>
    Base::Result<void> RegisterClassHandler(
        RoutedEventHandle event,
        TypeId classType,
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler,
        bool handledEventsToo = false) noexcept;

    Base::Result<void> RaiseEvent(
        DependencyObject& source,
        RoutedEventHandle event,
        RoutedEventArgs* args = nullptr) noexcept;

    template<class TVisitor>
    Base::Result<void> VisitRoute(
        DependencyObject& source,
        RoutingStrategy strategy,
        TVisitor&& visitor) noexcept {
        EventRoute route;
        Base::Result<void> built = route.Build(source, strategy);
        if (!built) return built.GetStatus();
        for (const EventRouteNode& lease : route.Nodes()) {
            DependencyObject* node = lease.Resolve();
            if (node != nullptr && !visitor(*node)) break;
        }
        return {};
    }

private:
    struct ClassHandlerRecord {
        RoutedEventHandle event;
        TypeId classType = InvalidTypeId;
        Aero::GuiPrivate::Detail::RoutedHandlerStorage handler;
        std::uint64_t sequence = 0U;
        bool handledEventsToo = false;
    };

    void* eventState_ = nullptr;
    Base::Vector<ClassHandlerRecord> classHandlers_;
    std::uint64_t nextClassSequence_ = 1U;
    std::uint32_t raiseDepth_ = 0U;

    void InvokeNode(DependencyObject& node, RoutedEventArgs& args) noexcept;
    void CleanupClassHandlers() noexcept;
    Base::Result<void> ValidateClassHandler(
        RoutedEventHandle event,
        TypeId classType,
        TypeId eventArgsType) const noexcept;
};

template<class TArgs>
Base::Result<void> EventRouter::RegisterClassHandler(
    RoutedEventHandle event,
    TypeId classType,
    const Base::Delegate<void(Base::Object*, TArgs&)>& handler,
    bool handledEventsToo) noexcept {
    static_assert(std::is_base_of<RoutedEventArgs, TArgs>::value,
        "Routed event arguments must derive from RoutedEventArgs");
    if (raiseDepth_ != 0U) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Cannot mutate class handlers during routed event dispatch");
    }
    if (handler.Empty()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Class handler registration is invalid");
    }
    Base::Result<void> valid = ValidateClassHandler(
        event, classType, TArgs::StaticTypeId());
    if (!valid) return valid.GetStatus();
    ClassHandlerRecord value;
    value.event = event;
    value.classType = classType;
    value.handler = Aero::GuiPrivate::Detail::RoutedHandlerStorage(handler);
    value.handledEventsToo = handledEventsToo;
    value.sequence = nextClassSequence_++;
    return classHandlers_.PushBack(std::move(value));
}

} // namespace Aero::GuiPrivate::Detail
