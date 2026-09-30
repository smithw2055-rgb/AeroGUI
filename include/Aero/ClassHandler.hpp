#pragma once

#include <Aero/RoutedEvent.hpp>
#include <Aero/UIElement.hpp>

namespace Aero {

template<class T, class TArgs, void (T::*Method)(TArgs&)> void InputClassHandlerThunk(UIElement& element,
    RoutedEventArgs& args) noexcept { (static_cast<T&>(element).*Method)(static_cast<TArgs&>(args)); }

// Install Method as the class handler for event. The most-derived
// registered type wins, matching an override. Call the base method by
// name from Method when base behavior should still run.
template<class T, class TArgs, void (T::*Method)(TArgs&)> void RegisterInputClassHandler(
    const RoutedEventRef<UIElement, TArgs>& event) noexcept {
    UIElement::RegisterClassHandler(T::StaticTypeId(), event.Handle(), &InputClassHandlerThunk<T, TArgs, Method>);
}

} // namespace Aero

#include <type_traits>

// Install Method as the class handler for Event. Event's argument type is
// taken from the routed-event reference, so the call site names only the
// method and the event.
#define AERO_ON(Type, Method, Event) \
    ::Aero::RegisterInputClassHandler< \
        Type, \
        typename std::decay_t<decltype(Event)>::Args, \
        Method>(Event)
