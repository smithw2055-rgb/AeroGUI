#pragma once

namespace Aero {

class UIElement;
class ContentElement;
struct RoutedEventArgs;
class EventRouter;
class InputRouter;
class BindingEngine;
class LayoutEngine;
class StyleEngine;
class AnimationEngine;

class EventRouter {
public:
    explicit EventRouter(void* eventState) noexcept;
    ~EventRouter() noexcept;

    template<class TArgs>
    Base::Result<void> RegisterClassHandler(
        RoutedEventHandle event,
        Meta::TypeId classType,
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler,
        bool handledEventsToo = false) noexcept;

    template<class TArgs>
    Base::Result<void> RegisterClassHandler(
        RoutedEventHandle event,
        Meta::TypeId classType,
        void (*handler)(UIElement&, TArgs&),
        bool handledEventsToo = false) noexcept;

    template<class TArgs>
    Base::Result<void> RegisterClassHandler(
        RoutedEventHandle event,
        Meta::TypeId classType,
        void (*handler)(ContentElement&, TArgs&),
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
        Meta::TypeId classType = Meta::InvalidTypeId;
        Aero::RoutedHandlerStorage handler;
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
        Meta::TypeId classType,
        Meta::TypeId eventArgsType) const noexcept;
};

template<class TArgs>
Base::Result<void> EventRouter::RegisterClassHandler(
    RoutedEventHandle event,
    Meta::TypeId classType,
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
    value.handler = Aero::RoutedHandlerStorage(handler);
    value.handledEventsToo = handledEventsToo;
    value.sequence = nextClassSequence_++;
    return classHandlers_.PushBack(std::move(value));
}

template<class TArgs>
Base::Result<void> EventRouter::RegisterClassHandler(
    RoutedEventHandle event,
    Meta::TypeId classType,
    void (*handler)(UIElement&, TArgs&),
    bool handledEventsToo) noexcept {
    if (handler == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Class handler cannot be null");
    }
    return RegisterClassHandler<TArgs>(
        event, classType,
        Base::Delegate<void(Base::Object*, TArgs&)>(
            reinterpret_cast<void (*)(Base::Object*, TArgs&)>(handler)),
        handledEventsToo);
}

template<class TArgs>
Base::Result<void> EventRouter::RegisterClassHandler(
    RoutedEventHandle event,
    Meta::TypeId classType,
    void (*handler)(ContentElement&, TArgs&),
    bool handledEventsToo) noexcept {
    if (handler == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Class handler cannot be null");
    }
    return RegisterClassHandler<TArgs>(
        event, classType,
        Base::Delegate<void(Base::Object*, TArgs&)>(
            reinterpret_cast<void (*)(Base::Object*, TArgs&)>(handler)),
        handledEventsToo);
}

} // namespace Aero
