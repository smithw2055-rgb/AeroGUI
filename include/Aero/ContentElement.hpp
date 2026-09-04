#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyObject.hpp>
#include <Aero/Events/Event.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/RoutedEvent.hpp>

#include <cstddef>

namespace Aero {

class UIElement;
// Non-visual WPF content node. ContentElement participates in dependency
// properties and routed events without becoming a ::Aero::Media::Visual or UIElement.
class AERO_GUI_API ContentElement : public DependencyObject {
    AERO_DECLARE_TYPE(ContentElement, DependencyObject)
public:
    template<class TArgs>
    using Event = ::Aero::Event<ContentElement, TArgs>;

    template<class TOwner, class TArgs>
    Event<TArgs> GetEvent(
        const RoutedEventRef<TOwner, TArgs>& event) noexcept {
        return Event<TArgs>(*this, event.Handle());
    }

    explicit ContentElement(Meta::TypeId runtimeType) noexcept;
    ~ContentElement() override;

    DependencyObject* GetParent() const noexcept { return logicalParent_; }
    UIElement* GetContentHost() const noexcept { return contentHost_; }

    template<class TArgs>
    void AddHandler(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler,
        bool handledEventsToo = false) noexcept {
        if (handler.Empty()) {
            return;
        }
        static_cast<void>(AddHandlerErased(
            event,
            &handler,
            sizeof(handler),
            alignof(decltype(handler)),
            TArgs::StaticTypeId(),
            handledEventsToo));
    }

    template<class TArgs>
    bool RemoveHandler(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler) noexcept {
        return RemoveHandlerErased(
            event,
            &handler,
            sizeof(handler),
            alignof(decltype(handler)),
            TArgs::StaticTypeId());
    }

protected:
    void RaiseEvent(
        RoutedEventHandle event,
        RoutedEventArgs* args = nullptr) noexcept;

private:
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
#endif

    Result<void> AddHandlerErased(
        RoutedEventHandle event,
        const void* handler,
        std::size_t size,
        std::size_t alignment,
        Meta::TypeId argsType,
        bool handledEventsToo) noexcept;
    bool RemoveHandlerErased(
        RoutedEventHandle event,
        const void* handler,
        std::size_t size,
        std::size_t alignment,
        Meta::TypeId argsType) noexcept;
    void InvokeHandlers(
        RoutedEventHandle event,
        RoutedEventArgs& args) noexcept;
    void CleanupHandlers() noexcept;

    DependencyObject* logicalParent_ = nullptr;
    UIElement* contentHost_ = nullptr;
    void* eventRouter_ = nullptr;
    void* routedHandlers_ = nullptr;
};

} // namespace Aero
