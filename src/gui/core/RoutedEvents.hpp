#pragma once


// Internal routed-event storage, route snapshots and dispatch.

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Events.hpp>
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/LogicalTreeHelper.hpp>
#include <Aero/Visual.hpp>

#include "gui/core/TypeRegistryCore.hpp"

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero {

class RoutedHandlerStorage {
public:
    struct Operations {
        void (*copy)(void*, const void*) noexcept = nullptr;
        void (*destroy)(void*) noexcept = nullptr;
        bool (*equals)(const void*, const void*) noexcept = nullptr;
        void (*invoke)(const void*, Base::Object*, RoutedEventArgs&) noexcept = nullptr;
    };

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
        : ops_(InternOperations(copy, destroy, equals, invoke)),
          argsType_(argsType) {
        AERO_ASSERT(value != nullptr && size <= sizeof(storage_) && alignment <= alignof(void*));
        ops_->copy(storage_, value);
    }

    template<class TArgs>
    explicit RoutedHandlerStorage(
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler) noexcept {
        static_assert(std::is_base_of<RoutedEventArgs, TArgs>::value,
            "Routed event arguments must derive from RoutedEventArgs");
        using Handler = Base::Delegate<void(Base::Object*, TArgs&)>;
        static_assert(sizeof(Handler) <= sizeof(storage_) && alignof(Handler) <= alignof(void*),
            "Handler must fit inline storage");
        static constexpr Operations kOps{
            &TypedOperations<TArgs>::Copy,
            &TypedOperations<TArgs>::Destroy,
            &TypedOperations<TArgs>::Equals,
            &TypedOperations<TArgs>::Invoke
        };
        ops_ = &kOps;
        argsType_ = TArgs::StaticTypeId();
        kOps.copy(storage_, &handler);
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

    bool Empty() const noexcept { return ops_ == nullptr; }
    Meta::TypeId ArgsType() const noexcept { return argsType_; }

    bool Equals(const RoutedHandlerStorage& other) const noexcept {
        if (argsType_ != other.argsType_) return false;
        if (ops_ != other.ops_) {
            if (ops_ == nullptr || other.ops_ == nullptr) return false;
            if (ops_->copy != other.ops_->copy ||
                ops_->destroy != other.ops_->destroy ||
                ops_->equals != other.ops_->equals ||
                ops_->invoke != other.ops_->invoke) {
                return false;
            }
        }
        return Empty() || ops_->equals(storage_, other.storage_);
    }

    void Invoke(Base::Object* sender, RoutedEventArgs& args) const noexcept {
        AERO_ASSERT(
            !Empty() &&
            (args.GetEventArgsType() == argsType_ ||
             argsType_ == RoutedEventArgs::StaticTypeId()));
        ops_->invoke(storage_, sender, args);
    }

private:
    template<class TArgs>
    struct TypedOperations {
        using Handler = Base::Delegate<void(Base::Object*, TArgs&)>;
        static void Copy(void* destination, const void* source) noexcept {
            new (destination) Handler(*static_cast<const Handler*>(source));
        }
        static void Destroy(void* value) noexcept {
            static_cast<Handler*>(value)->~Handler();
        }
        static bool Equals(const void* left, const void* right) noexcept {
            return *static_cast<const Handler*>(left) == *static_cast<const Handler*>(right);
        }
        static void Invoke(const void* value, Base::Object* sender, RoutedEventArgs& args) noexcept {
            static_cast<const Handler*>(value)->Invoke(sender, static_cast<TArgs&>(args));
        }
    };

    static const Operations* InternOperations(
        void (*copy)(void*, const void*) noexcept,
        void (*destroy)(void*) noexcept,
        bool (*equals)(const void*, const void*) noexcept,
        void (*invoke)(const void*, Base::Object*, RoutedEventArgs&) noexcept) noexcept {
        static Operations slots[8]{};
        for (Operations& slot : slots) {
            if (slot.copy == copy && slot.destroy == destroy &&
                slot.equals == equals && slot.invoke == invoke) {
                return &slot;
            }
            if (slot.copy == nullptr) {
                slot.copy = copy;
                slot.destroy = destroy;
                slot.equals = equals;
                slot.invoke = invoke;
                return &slot;
            }
        }
        AERO_ASSERT(false);
        return &slots[0];
    }

    void CopyFrom(const RoutedHandlerStorage& other) noexcept {
        ops_ = other.ops_;
        argsType_ = other.argsType_;
        if (!other.Empty()) ops_->copy(storage_, other.storage_);
    }

    void Reset() noexcept {
        if (!Empty()) ops_->destroy(storage_);
        ops_ = nullptr;
        argsType_ = Meta::InvalidTypeId;
    }

    alignas(void*) unsigned char storage_[4U * sizeof(void*)]{};
    const Operations* ops_ = nullptr;
    Meta::TypeId argsType_ = Meta::InvalidTypeId;
};
static_assert(sizeof(RoutedHandlerStorage) <= 48U, "RoutedHandlerStorage must remain compact");

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
            nodes_.PushBack(EventRouteNode::Acquire(*current));
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
        if (object.IsVisualCore()) {
            auto& visual = static_cast<::Aero::Media::Visual&>(object);
            if (visual.GetVisualParent() != nullptr) {
                return visual.GetVisualParent();
            }
        } else if (auto* content = ::Aero::TryCast<ContentElement>(&object)) {
            DependencyObject* parent = content->GetParent();
            return parent != nullptr
                ? parent
                : static_cast<DependencyObject*>(content->GetContentHost());
        }
        return LogicalTreeHelper::GetParent(object);
    }

    Base::BasicVector<EventRouteNode, 32U> nodes_;
};

} // namespace Aero
