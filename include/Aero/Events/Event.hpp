#pragma once

#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Events/RoutedEvent.hpp>

namespace Aero {

template<class TSource, class TArgs>
class Event {
public:
    using Handler = Base::Delegate<void(Base::Object*, TArgs&)>;

    Event(TSource& source, RoutedEventHandle event) noexcept
        : source_(&source), event_(event) {}

    void Add(
        const Handler& handler,
        bool handledEventsToo = false) noexcept {
        source_->AddHandler(event_, handler, handledEventsToo);
    }

    void operator+=(const Handler& handler) noexcept { Add(handler); }

    bool Remove(const Handler& handler) noexcept {
        return source_->RemoveHandler(event_, handler);
    }

    void operator-=(const Handler& handler) noexcept {
        static_cast<void>(Remove(handler));
    }

private:
    TSource* source_ = nullptr;
    RoutedEventHandle event_;
};

} // namespace Aero
