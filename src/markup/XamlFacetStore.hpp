#pragma once

#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Meta/TypeRegistry.hpp>
#include "gui/property/EffectiveValueEngine.hpp"
#include "Extensions.hpp"
#include <Aero/Markup/Schema.hpp>
#include <Aero/Version.hpp>

#include <cstdint>

namespace Aero::Markup::Detail {

using XamlInitializationCallback = Base::Result<void> (*)(
    Base::Object& object,
    void* context) noexcept;
using XamlInitializationWithServicesCallback = Base::Result<void> (*)(
    Base::Object& object,
    const ExtensionContext& services,
    void* context) noexcept;
using XamlAbortInitializationCallback = void (*)(
    Base::Object& object,
    void* context) noexcept;
using XamlRegisterNameCallback = Base::Result<void> (*)(
    Base::Object& scopeOwner,
    Base::StringView name,
    Base::Object& object,
    void* context) noexcept;
using XamlAddResourceCallback = Base::Result<void> (*)(
    Base::Object& scopeOwner,
    const Aero::ResourceKey& key,
    const Core::Value& value,
    void* context) noexcept;
using XamlResolveResourceScopeCallback =
    Aero::ResourceDictionary* (*)(
        Base::Object& scopeOwner,
        void* context) noexcept;
using XamlResolveImplicitResourceKeyCallback =
    Base::Result<Aero::ResourceKey> (*)(
        const Base::Object& object,
        void* context) noexcept;
using XamlResolvePropertyTargetCallback =
    Core::DependencyObject* (*)(
        Base::Object& object,
        void* context) noexcept;
using XamlProvideValueCallback =
    Base::Result<ProvidedValue> (*)(
        Base::StringView arguments,
        const ExtensionContext& services,
        void* context) noexcept;

enum class XamlFacetInheritancePolicy : std::uint8_t {
    ExactOnly = 0U,
    NearestBase,
    ComposeBaseToDerived
};

// Compatibility registration DTO. TryAdd() atomically projects this record to
// the narrow capability slots retained by XamlFacetStore.
struct XamlTypeFacet final {
    Core::TypeId type = Core::InvalidTypeId;
    XamlInitializationCallback beginInit = nullptr;
    XamlInitializationCallback endInit = nullptr;
    XamlAbortInitializationCallback abortInit = nullptr;
    void* context = nullptr;
    bool createsNameScope = false;
    bool createsResourceScope = false;
    XamlRegisterNameCallback registerName = nullptr;
    XamlAddResourceCallback addResource = nullptr;
    XamlResolveResourceScopeCallback resolveResourceScope = nullptr;
    XamlInitializationWithServicesCallback endInitWithServices = nullptr;
    bool defersVisualContent = false;
    XamlResolveImplicitResourceKeyCallback resolveImplicitResourceKey =
        nullptr;
    XamlResolvePropertyTargetCallback resolvePropertyTarget = nullptr;
    std::uint32_t abiVersion = XamlFacetAbiVersion;
};

struct XamlLifecycleFacet final {
    inline static constexpr XamlFacetInheritancePolicy InheritancePolicy =
        XamlFacetInheritancePolicy::ComposeBaseToDerived;

    Core::TypeId type = Core::InvalidTypeId;
    XamlInitializationCallback beginInit = nullptr;
    XamlInitializationCallback endInit = nullptr;
    XamlAbortInitializationCallback abortInit = nullptr;
    XamlInitializationWithServicesCallback endInitWithServices = nullptr;
    void* context = nullptr;
    std::uint32_t abiVersion = XamlFacetAbiVersion;
};

struct XamlNameScopeFacet final {
    inline static constexpr XamlFacetInheritancePolicy InheritancePolicy =
        XamlFacetInheritancePolicy::NearestBase;

    Core::TypeId type = Core::InvalidTypeId;
    bool createsNameScope = true;
    XamlRegisterNameCallback registerName = nullptr;
    void* context = nullptr;
    std::uint32_t abiVersion = XamlFacetAbiVersion;
};

struct XamlResourceScopeFacet final {
    inline static constexpr XamlFacetInheritancePolicy InheritancePolicy =
        XamlFacetInheritancePolicy::NearestBase;

    Core::TypeId type = Core::InvalidTypeId;
    bool createsResourceScope = true;
    XamlAddResourceCallback addResource = nullptr;
    XamlResolveResourceScopeCallback resolveResourceScope = nullptr;
    void* context = nullptr;
    std::uint32_t abiVersion = XamlFacetAbiVersion;
};

struct XamlDeferredContentFacet final {
    inline static constexpr XamlFacetInheritancePolicy InheritancePolicy =
        XamlFacetInheritancePolicy::NearestBase;

    Core::TypeId type = Core::InvalidTypeId;
    bool defersVisualContent = true;
    std::uint32_t abiVersion = XamlFacetAbiVersion;
};

struct XamlImplicitResourceKeyFacet final {
    inline static constexpr XamlFacetInheritancePolicy InheritancePolicy =
        XamlFacetInheritancePolicy::NearestBase;

    Core::TypeId type = Core::InvalidTypeId;
    XamlResolveImplicitResourceKeyCallback resolve = nullptr;
    void* context = nullptr;
    std::uint32_t abiVersion = XamlFacetAbiVersion;
};

struct XamlPropertyTargetFacet final {
    inline static constexpr XamlFacetInheritancePolicy InheritancePolicy =
        XamlFacetInheritancePolicy::NearestBase;

    Core::TypeId type = Core::InvalidTypeId;
    XamlResolvePropertyTargetCallback resolve = nullptr;
    void* context = nullptr;
    std::uint32_t abiVersion = XamlFacetAbiVersion;
};

struct XamlMarkupExtensionFacet final {
    inline static constexpr XamlFacetInheritancePolicy InheritancePolicy =
        XamlFacetInheritancePolicy::ExactOnly;

    Core::TypeId type = Core::InvalidTypeId;
    XamlProvideValueCallback provideValue = nullptr;
    void* context = nullptr;
    std::uint32_t abiVersion = XamlFacetAbiVersion;
};

class XamlFacetStore final {
public:
    XamlFacetStore() noexcept = default;

    XamlFacetStore(const XamlFacetStore&) = delete;
    XamlFacetStore& operator=(const XamlFacetStore&) = delete;

    Base::Result<void> TryAdd(
        const XamlTypeFacet& facet,
        const Core::TypeRegistry& descriptors) noexcept;
    Base::Result<void> TryAdd(
        const XamlLifecycleFacet& facet,
        const Core::TypeRegistry& descriptors) noexcept;
    Base::Result<void> TryAdd(
        const XamlNameScopeFacet& facet,
        const Core::TypeRegistry& descriptors) noexcept;
    Base::Result<void> TryAdd(
        const XamlResourceScopeFacet& facet,
        const Core::TypeRegistry& descriptors) noexcept;
    Base::Result<void> TryAdd(
        const XamlDeferredContentFacet& facet,
        const Core::TypeRegistry& descriptors) noexcept;
    Base::Result<void> TryAdd(
        const XamlImplicitResourceKeyFacet& facet,
        const Core::TypeRegistry& descriptors) noexcept;
    Base::Result<void> TryAdd(
        const XamlPropertyTargetFacet& facet,
        const Core::TypeRegistry& descriptors) noexcept;
    Base::Result<void> TryAdd(
        const XamlMarkupExtensionFacet& facet,
        const Core::TypeRegistry& descriptors) noexcept;
    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }

    Base::Result<void> CollectLifecycle(
        Core::TypeId type,
        const Core::TypeRegistry& descriptors,
        Base::Vector<const XamlLifecycleFacet*>& output) const noexcept;
    const XamlLifecycleFacet* FindLifecycle(
        Core::TypeId type,
        const Core::TypeRegistry& descriptors) const noexcept;
    const XamlNameScopeFacet* FindNameScope(
        Core::TypeId type,
        const Core::TypeRegistry& descriptors) const noexcept;
    const XamlResourceScopeFacet* FindResourceScope(
        Core::TypeId type,
        const Core::TypeRegistry& descriptors) const noexcept;
    const XamlDeferredContentFacet* FindDeferredContent(
        Core::TypeId type,
        const Core::TypeRegistry& descriptors) const noexcept;
    const XamlImplicitResourceKeyFacet* FindImplicitResourceKey(
        Core::TypeId type,
        const Core::TypeRegistry& descriptors) const noexcept;
    const XamlPropertyTargetFacet* FindPropertyTarget(
        Core::TypeId type,
        const Core::TypeRegistry& descriptors) const noexcept;
    const XamlMarkupExtensionFacet* FindMarkupExtension(
        Core::TypeId type) const noexcept;

private:
    struct TypeFacets final {
        Core::TypeId type = Core::InvalidTypeId;
        XamlLifecycleFacet lifecycle;
        XamlNameScopeFacet nameScope;
        XamlResourceScopeFacet resourceScope;
        XamlDeferredContentFacet deferredContent;
        XamlImplicitResourceKeyFacet implicitResourceKey;
        XamlPropertyTargetFacet propertyTarget;
        XamlMarkupExtensionFacet markupExtension;
        bool hasLifecycle = false;
        bool hasNameScope = false;
        bool hasResourceScope = false;
        bool hasDeferredContent = false;
        bool hasImplicitResourceKey = false;
        bool hasPropertyTarget = false;
        bool hasMarkupExtension = false;
    };

    using FacetIndex = Base::HashMap<Core::TypeId, std::uint32_t>;

    Base::Vector<TypeFacets> types_;
    FacetIndex index_;
    bool frozen_ = false;

    Base::Result<TypeFacets*> EnsureType(Core::TypeId type) noexcept;
    TypeFacets* FindTypeMutable(Core::TypeId type) noexcept;
    const TypeFacets* FindType(Core::TypeId type) const noexcept;

    const XamlLifecycleFacet* FindLifecycleExact(
        Core::TypeId type) const noexcept;
    const XamlNameScopeFacet* FindNameScopeExact(
        Core::TypeId type) const noexcept;
    const XamlResourceScopeFacet* FindResourceScopeExact(
        Core::TypeId type) const noexcept;
    const XamlDeferredContentFacet* FindDeferredContentExact(
        Core::TypeId type) const noexcept;
    const XamlImplicitResourceKeyFacet* FindImplicitResourceKeyExact(
        Core::TypeId type) const noexcept;
    const XamlPropertyTargetFacet* FindPropertyTargetExact(
        Core::TypeId type) const noexcept;
};

} // namespace Aero::Markup::Detail
