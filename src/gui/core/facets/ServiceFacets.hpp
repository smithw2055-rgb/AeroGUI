#pragma once

#include "gui/core/Facet.hpp"
#include <Aero/Base/Object.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Meta.hpp>

namespace Aero {
class VisualStateManager;
namespace Controls {
class TemplateEngine;
class ControlBehavior;
class TextBlockLayout;
} // namespace Controls
namespace Render { struct MeshResources; }
} // namespace Aero

namespace Aero::Core {

// View-affine service facets: typed, type-safe replacements for the former
// `void*` fields of ElementHost (templates / visualStates / textLayout /
// controlBehaviors / meshResources / nameScopeContext+findName). The facet
// objects are owned by ViewState and registered (non-owning) in the
// ElementHost matrix, resolved uniformly through Core::GetFacet<T>(element).

class TemplateEngineFacet : public Facet {
public:
    explicit TemplateEngineFacet(
        ::Aero::Controls::TemplateEngine* engine = nullptr) noexcept
        : engine_(engine) {}
    void SetEngine(::Aero::Controls::TemplateEngine* engine) noexcept {
        engine_ = engine;
    }
    ::Aero::Controls::TemplateEngine* Engine() const noexcept { return engine_; }
private:
    ::Aero::Controls::TemplateEngine* engine_ = nullptr;
};

class VisualStateServiceFacet : public Facet {
public:
    explicit VisualStateServiceFacet(
        VisualStateManager* manager = nullptr) noexcept
        : manager_(manager) {}
    void SetManager(VisualStateManager* manager) noexcept { manager_ = manager; }
    VisualStateManager* Manager() const noexcept { return manager_; }
private:
    VisualStateManager* manager_ = nullptr;
};

class TextLayoutServiceFacet : public Facet {
public:
    explicit TextLayoutServiceFacet(
        ::Aero::Controls::TextBlockLayout* layout = nullptr) noexcept
        : layout_(layout) {}
    void SetLayout(::Aero::Controls::TextBlockLayout* layout) noexcept {
        layout_ = layout;
    }
    ::Aero::Controls::TextBlockLayout* Layout() const noexcept { return layout_; }
private:
    ::Aero::Controls::TextBlockLayout* layout_ = nullptr;
};

class ControlBehaviorFacet : public Facet {
public:
    explicit ControlBehaviorFacet(
        ::Aero::Controls::ControlBehavior* behaviors = nullptr) noexcept
        : behaviors_(behaviors) {}
    void SetBehaviors(::Aero::Controls::ControlBehavior* behaviors) noexcept {
        behaviors_ = behaviors;
    }
    ::Aero::Controls::ControlBehavior* Behaviors() const noexcept {
        return behaviors_;
    }
private:
    ::Aero::Controls::ControlBehavior* behaviors_ = nullptr;
};

class MeshResourceFacet : public Facet {
public:
    explicit MeshResourceFacet(
        ::Aero::Render::MeshResources* resources = nullptr) noexcept
        : resources_(resources) {}
    void SetResources(::Aero::Render::MeshResources* resources) noexcept {
        resources_ = resources;
    }
    ::Aero::Render::MeshResources* Resources() const noexcept {
        return resources_;
    }
private:
    ::Aero::Render::MeshResources* resources_ = nullptr;
};

class NameScopeFacet : public Facet {
public:
    using FindNameFn = Base::Object* (*)(
        void*, Base::StringView, Meta::TypeId) noexcept;

    NameScopeFacet(void* context = nullptr, FindNameFn findName = nullptr) noexcept
        : context_(context), findName_(findName) {}

    void Set(void* context, FindNameFn findName) noexcept {
        context_ = context;
        findName_ = findName;
    }

    Base::Object* FindName(
        Base::StringView name,
        Meta::TypeId expectedType = Meta::InvalidTypeId) const noexcept {
        return findName_ != nullptr ? findName_(context_, name, expectedType)
                                     : nullptr;
    }
private:
    void* context_ = nullptr;
    FindNameFn findName_ = nullptr;
};

template<>
struct FacetTrait<TemplateEngineFacet> {
    static constexpr std::uint32_t Id =
        static_cast<std::uint32_t>(FacetId::TemplateEngine);
    static constexpr FacetType Type = FacetType::Dependency;
};

template<>
struct FacetTrait<VisualStateServiceFacet> {
    static constexpr std::uint32_t Id =
        static_cast<std::uint32_t>(FacetId::VisualState);
    static constexpr FacetType Type = FacetType::Interaction;
};

template<>
struct FacetTrait<TextLayoutServiceFacet> {
    static constexpr std::uint32_t Id =
        static_cast<std::uint32_t>(FacetId::TextLayoutService);
    static constexpr FacetType Type = FacetType::Text;
};

template<>
struct FacetTrait<ControlBehaviorFacet> {
    static constexpr std::uint32_t Id =
        static_cast<std::uint32_t>(FacetId::ControlBehavior);
    static constexpr FacetType Type = FacetType::Interaction;
};

template<>
struct FacetTrait<MeshResourceFacet> {
    static constexpr std::uint32_t Id =
        static_cast<std::uint32_t>(FacetId::MeshResources);
    static constexpr FacetType Type = FacetType::Render;
};

template<>
struct FacetTrait<NameScopeFacet> {
    static constexpr std::uint32_t Id =
        static_cast<std::uint32_t>(FacetId::NameScope);
    static constexpr FacetType Type = FacetType::Dependency;
};

} // namespace Aero::Core
