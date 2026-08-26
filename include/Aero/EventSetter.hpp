#pragma once

#include <Aero/Triggers/TriggerBase.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/String.hpp>

namespace Aero {

class AERO_GUI_API EventSetter : public SetterBase {
    AERO_DECLARE_TYPE(EventSetter, SetterBase)
public:
    EventSetter() noexcept : SetterBase(StaticTypeId()) {}

    RoutedEventHandle GetEvent() const noexcept { return event_; }
    void SetEvent(RoutedEventHandle value) noexcept { event_ = value; }

    StringView GetHandlerName() const noexcept { return handlerName_.View(); }
    void SetHandlerName(StringView value) noexcept;

    using Handler = Base::Delegate<void(Base::Object*, RoutedEventArgs&)>;
    const Handler& GetHandler() const noexcept { return handler_; }
    void SetHandler(Handler handler) noexcept { handler_ = std::move(handler); }

private:
    RoutedEventHandle event_;
    String handlerName_;
    Handler handler_;
};

} // namespace Aero
