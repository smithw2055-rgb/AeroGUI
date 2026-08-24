#pragma once

#include "gui/core/Facet.hpp"
#include <Aero/Base/Result.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/FrameworkContentElement.hpp>

namespace Aero {
class EventRouter;
class InputRouter;
struct ElementHost;
} // namespace Aero

namespace Aero::Core {

// Input & Routed Events Facet
class InputEventFacet : public Facet {
public:
    static constexpr FacetType StaticType = FacetType::Input;

    explicit InputEventFacet(::Aero::Media::Visual& owner) noexcept : owner_(&owner) {}

    ::Aero::Media::Visual& Owner() const noexcept { return *owner_; }

    static EventRouter* EventRouterFor(const Aero::UIElement& element) noexcept;
    static InputRouter* InputRouterFor(const Aero::UIElement& element) noexcept;

    static Base::Result<void> SetMouseOver(Aero::UIElement& element, bool value) noexcept {
        element.SetMouseOverState(value);
        return {};
    }
    static Base::Result<void> SetPressed(Aero::UIElement& element, bool value) noexcept {
        element.SetPressedState(value);
        return {};
    }
    static Base::Result<void> SetKeyboardFocused(Aero::UIElement& element, bool value) noexcept {
        element.SetKeyboardFocusedState(value);
        return {};
    }
    static Base::Result<void> SetKeyboardFocusWithin(Aero::UIElement& element, bool value) noexcept {
        element.SetKeyboardFocusWithinState(value);
        return {};
    }

    static void InvokeHandlers(
        Aero::UIElement& element,
        RoutedEventHandle event,
        RoutedEventArgs& args) noexcept {
        element.InvokeHandlers(event, args);
    }

    static void InvokeContentHandlers(
        Aero::ContentElement& element,
        RoutedEventHandle event,
        RoutedEventArgs& args) noexcept;

private:
    ::Aero::Media::Visual* owner_ = nullptr;
};

template<>
struct FacetTrait<InputEventFacet> {
    static constexpr std::uint32_t Id = static_cast<std::uint32_t>(FacetId::Input);
    static constexpr FacetType Type = FacetType::Input;
};

} // namespace Aero::Core
