#pragma once

#include <cstdint>
#include <type_traits>

namespace Aero {
class UIElement;
class DependencyObject;
class Freezable;
class ContentElement;
namespace Media { class Visual; }

struct VisualHandle {
    std::uint32_t index = UINT32_MAX;
    std::uint32_t generation = 0U;

    constexpr bool IsValid() const noexcept {
        return index != UINT32_MAX && generation != 0U;
    }
};

} // namespace Aero

namespace Aero::Core {

using VisualHandle = ::Aero::VisualHandle;

// Core Facet Types matching WPF Facet Pattern Architecture Specification
enum class FacetType : std::uint32_t {
    Dependency   = 0,
    Layout       = 1,
    Render       = 2,
    Input        = 3,
    Text         = 4,
    Interaction  = 5,
    Visual       = 6,
    Count        = 7
};

// Stable compile-time identifier for each facet type. Used to index the
// per-element facet bag or view-level services.
enum class FacetId : std::uint32_t {
    DependencyProperty = 0,
    Layout             = 1,
    Render             = 2,
    Input              = 3,
    Text               = 4,
    Interaction        = 5,
    Visual             = 6,
    EventRouter        = 7,
    InputRouter        = 8,
    BindingEngine      = 9,
    LayoutEngine       = 10,
    StyleEngine        = 11,
    AnimationEngine    = 12,
    EffectiveValueEngine = 13,
    ContentElement     = 14,
    Freezable          = 15,
    DependencyObject   = 16,
    TemplateEngine     = 17,
    VisualState        = 18,
    TextLayoutService  = 19,
    ControlBehavior    = 20,
    MeshResources      = 21,
    NameScope          = 22,
    Count              = 23,
};

// Abstract Facet base class (C++17)
class Facet {
public:
    virtual ~Facet() = default;
    explicit Facet(UIElement* owner = nullptr) noexcept : owner_(owner) {}
    virtual void OnAttached(UIElement* owner) noexcept { owner_ = owner; }
    virtual void OnDetached() noexcept { owner_ = nullptr; }
    
    UIElement* GetOwner() const noexcept { return owner_; }

protected:
    UIElement* owner_ = nullptr;
};

// Per-facet-type identity trait. Specialized by each concrete facet.
template<class T>
struct FacetTrait;

// Resolvers. Visual-derived elements resolve through the ElementTree (fast,
// handle-indexed). Non-visual core types (ContentElement, Freezable,
// DependencyObject) host their facet directly and resolve without a tree.
template<class TFacet>
TFacet* GetFacet(const ::Aero::Media::Visual& visual) noexcept;

template<class TFacet>
TFacet* GetFacet(const ::Aero::UIElement& element) noexcept;

template<class TFacet>
TFacet* GetFacet(const ::Aero::ContentElement& element) noexcept;

template<class TFacet>
TFacet* GetFacet(const ::Aero::Freezable& value) noexcept;

template<class TFacet>
TFacet* GetFacet(const ::Aero::DependencyObject& object) noexcept;

} // namespace Aero::Core
