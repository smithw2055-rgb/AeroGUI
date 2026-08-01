#pragma once

#include "EventRoute.hpp"
#include "RoutedHandlerStorage.hpp"

#include <Aero/Base/Vector.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/UIElement.hpp>

#include <type_traits>
#include <utility>

namespace Aero::Detail {

using namespace Aero::Core;

class AERO_API EventRouter final {
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
        UIElement& source,
        RoutedEventHandle event,
        RoutedEventArgs* args = nullptr) noexcept;

    template<class TVisitor>
    Base::Result<void> VisitRoute(
        UIElement& source,
        RoutingStrategy strategy,
        TVisitor&& visitor) noexcept {
        EventRoute route;
        Base::Result<void> built = route.Build(source, strategy);
        if (!built) return built.GetStatus();
        for (const VisualLease& lease : route.Nodes()) {
            Visual* node = lease.Resolve();
            if (node != nullptr && !visitor(*node)) break;
        }
        return {};
    }

private:
    struct ClassHandlerRecord final {
        RoutedEventHandle event;
        TypeId classType = InvalidTypeId;
        Aero::Detail::RoutedHandlerStorage handler;
        std::uint64_t sequence = 0U;
        bool handledEventsToo = false;
    };

    void* eventState_ = nullptr;
    Base::Vector<ClassHandlerRecord> classHandlers_;
    std::uint64_t nextClassSequence_ = 1U;
    std::uint32_t raiseDepth_ = 0U;

    void InvokeNode(Visual& node, RoutedEventArgs& args) noexcept;
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
    value.handler = Aero::Detail::RoutedHandlerStorage(handler);
    value.handledEventsToo = handledEventsToo;
    value.sequence = nextClassSequence_++;
    return classHandlers_.TryPushBack(std::move(value));
}

} // namespace Aero::Detail
