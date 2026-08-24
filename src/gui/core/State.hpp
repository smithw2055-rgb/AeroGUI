#pragma once

// Private element state and direct Gui runtime declarations.

#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Visual.hpp>
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/FrameworkElement.hpp>

#include "gui/core/Facet.hpp"

#include <cstdint>
#include <utility>

namespace Aero::Controls {
class Control;
class Decorator;
class MenuItem;
class Panel;
class TreeViewItem;
}

namespace Aero { class VisualStateManager; }

namespace Aero::Controls::Primitives { class Selector; }

namespace Aero::Shapes { class Path; }

namespace Aero {

class EventRouter;
class InputRouter;
class LayoutEngine;
class BindingEngine;
class AnimationEngine;
class StyleEngine;

// View-affine facet matrix. Engines are stored here indexed by their
// FacetTrait Id and resolved uniformly through Core::GetFacet<T>(element).
struct ElementHost {
    static constexpr std::size_t FacetCount =
        static_cast<std::size_t>(Core::FacetId::Count);

    Core::Facet* facets_[FacetCount] = {};

    template<class T>
    T* GetFacet() const noexcept {
        return static_cast<T*>(
            facets_[static_cast<std::size_t>(Core::FacetTrait<T>::Id)]);
    }

    template<class T>
    void SetFacet(T* facet) noexcept {
        facets_[static_cast<std::size_t>(Core::FacetTrait<T>::Id)] = facet;
    }
};



struct ElementTreeLifecycleEvent {
    ::Aero::Media::Visual* node = nullptr;
    bool loaded = false;
    std::uint64_t treeVersion = 0U;
};

using ElementTreeLifecycleHandler = void (*)(
    const ElementTreeLifecycleEvent& event,
    void* context) noexcept;

constexpr std::uint32_t InvalidIndex = UINT32_MAX;

} // namespace Aero

// Tree and Host definition
#include "gui/core/state/ElementTree.hpp"

// Facet matrix headers
#include "gui/core/facets/VisualFacet.hpp"
#include "gui/core/facets/LayoutFacet.hpp"
#include "gui/core/facets/LayoutFacets.hpp"
#include "gui/core/facets/RenderFacet.hpp"
#include "gui/core/facets/InputEventFacet.hpp"
#include "gui/core/facets/InteractionStateFacet.hpp"
#include "gui/core/facets/TextLayoutFacet.hpp"
#include "gui/core/facets/DependencyPropertyFacet.hpp"
#include "gui/core/facets/ServiceFacets.hpp"

// State headers
#include "gui/core/state/LayoutEngine.hpp"
#include "gui/core/state/FreezableState.hpp"
#include "gui/core/state/PropertyEngine.hpp"
#include "gui/core/state/RoutedEvents.hpp"
#include "gui/core/state/EventRouter.hpp"

// Unified facet resolution. Every view-affine facet is stored in the
// ElementHost matrix and retrieved by its FacetTrait Id, replacing the
// former per-type GetFacet specializations and the retired *Access friends.
namespace Aero::Core {

template<class T>
inline T* GetFacet(const ::Aero::UIElement& element) noexcept {
    ::Aero::ElementTree* tree = element.GetTree();
    ::Aero::ElementHost* host =
        tree != nullptr ? tree->Host() : nullptr;
    return host != nullptr ? host->GetFacet<T>() : nullptr;
}

template<class T>
inline T* GetFacet(const ::Aero::Media::Visual& visual) noexcept {
    ::Aero::ElementTree* tree = visual.GetTree();
    ::Aero::ElementHost* host =
        tree != nullptr ? tree->Host() : nullptr;
    return host != nullptr ? host->GetFacet<T>() : nullptr;
}

// Non-visual core types do not participate in an ElementTree yet; their
// facet hosting is deferred (see plan phase for ContentElement/Freezable/
// DependencyObject). Resolving through the tree returns null for now.
template<class T>
inline T* GetFacet(const ::Aero::ContentElement& element) noexcept {
    static_cast<void>(element);
    return nullptr;
}

template<class T>
inline T* GetFacet(const ::Aero::Freezable& value) noexcept {
    static_cast<void>(value);
    return nullptr;
}

template<class T>
inline T* GetFacet(const ::Aero::DependencyObject& object) noexcept {
    static_cast<void>(object);
    return nullptr;
}

} // namespace Aero::Core

namespace Aero {

// Per-element facet bag accessors (defined here so Core::FacetTrait is visible).
template<class T>
inline T* UIElement::ElementFacet() const noexcept {
    return static_cast<T*>(
        elementFacets_[static_cast<std::size_t>(Core::FacetTrait<T>::Id)]);
}

template<class T>
inline void UIElement::SetElementFacet(T* facet) noexcept {
    elementFacets_[static_cast<std::size_t>(Core::FacetTrait<T>::Id)] = facet;
}

inline void UIElement::AttachElementFacets() noexcept {
    for (std::size_t index = 0U; index < ElementFacetCount; ++index) {
        if (auto* facet = elementFacets_[index]) {
            facet->OnAttached(this);
        }
    }
}

inline void UIElement::DetachElementFacets() noexcept {
    for (std::size_t index = 0U; index < ElementFacetCount; ++index) {
        if (auto* facet = elementFacets_[index]) {
            facet->OnDetached();
        }
    }
}

} // namespace Aero
