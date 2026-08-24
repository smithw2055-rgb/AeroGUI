#pragma once

#include "gui/core/Facet.hpp"
#include <Aero/Base/Object.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Meta.hpp>
#include <Aero/UIElement.hpp>

namespace Aero {
class BindingEngine;
class StyleEngine;
class AnimationEngine;
struct ElementHost;
namespace Shapes { class Path; }
} // namespace Aero

namespace Aero::Core {

// Dependency Property, Data Binding, and Style Engine Facet
class DependencyPropertyFacet : public Facet {
public:
    static constexpr FacetType StaticType = FacetType::Dependency;

    explicit DependencyPropertyFacet(::Aero::Media::Visual& owner) noexcept : owner_(&owner) {}

    ::Aero::Media::Visual& Owner() const noexcept { return *owner_; }

    static BindingEngine* BindingEngineFor(const Aero::UIElement& element) noexcept;
    static StyleEngine* StyleEngineFor(const Aero::UIElement& element) noexcept;
    static AnimationEngine* AnimationEngineFor(const Aero::UIElement& element) noexcept;
    static void* TemplateRuntime(const ::Aero::Media::Visual& visual) noexcept;
    static void* MeshResourcesRuntime(const ::Aero::Media::Visual& visual) noexcept;
    static Base::Object* FindName(
        const Aero::UIElement& element,
        Base::StringView name,
        Meta::TypeId expectedType = Meta::InvalidTypeId) noexcept;

    static void PathInvalidateGeometry(Aero::Shapes::Path& path) noexcept;
    static void PathAttachMeshResources(
        Aero::Shapes::Path& path,
        void* services,
        bool invalidate) noexcept;

    using FreezableVisitor = Base::Result<void> (*)(
        void* context,
        Freezable& child) noexcept;

    static bool HasUnfreezableValueState(
        const DependencyObject& object) noexcept;
    static Base::Result<void> VisitFreezableChildren(
        DependencyObject& object,
        void* context,
        FreezableVisitor visitor) noexcept;
    static Base::Result<void> PrepareConsumerChange(
        DependencyObject& consumer,
        Meta::DependencyPropertyHandle property,
        const Meta::PropertyValue& oldValue,
        const Meta::PropertyValue& newValue) noexcept;
    static void CommitConsumerChange(
        DependencyObject& consumer,
        Meta::DependencyPropertyHandle property,
        const Meta::PropertyValue& oldValue,
        const Meta::PropertyValue& newValue) noexcept;
    static void InvalidateSubProperty(
        DependencyObject& object,
        Meta::DependencyPropertyHandle property) noexcept;

    static Base::Result<void> AttachFreezableConsumer(
        Freezable& value,
        DependencyObject& object,
        Meta::DependencyPropertyHandle property) noexcept;
    static void DetachFreezableConsumer(
        Freezable& value,
        DependencyObject& object,
        Meta::DependencyPropertyHandle property) noexcept;
    static std::uint64_t FreezableRevision(const Freezable& value) noexcept;
    static bool FreezableCheckCore(Freezable& value) noexcept;

private:
    ::Aero::Media::Visual* owner_ = nullptr;
};

template<>
struct FacetTrait<DependencyPropertyFacet> {
    static constexpr std::uint32_t Id = static_cast<std::uint32_t>(FacetId::DependencyProperty);
    static constexpr FacetType Type = FacetType::Dependency;
};

} // namespace Aero::Core
