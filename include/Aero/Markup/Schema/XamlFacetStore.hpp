#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/MetadataDescriptors.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Markup/Extensions/XamlExtensionContext.hpp>
#include <Aero/Markup/Schema/XamlResolvedMember.hpp>

namespace Aero::Markup {

using XamlSetMemberCallback = Base::Result<void> (*)(
    Base::Object& object,
    const XamlValue& value,
    void* context) noexcept;
using XamlSetMemberWithServicesCallback = Base::Result<void> (*)(
    Base::Object& object,
    const XamlValue& value,
    const XamlExtensionContext& services,
    void* context) noexcept;
using XamlHandlesMemberCallback = bool (*)(
    const XamlResolvedMember& member,
    void* context) noexcept;
using XamlInitializationCallback = Base::Result<void> (*)(
    Base::Object& object,
    void* context) noexcept;
using XamlInitializationWithServicesCallback = Base::Result<void> (*)(
    Base::Object& object,
    const XamlExtensionContext& services,
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
    Base::StringView key,
    const XamlValue& value,
    void* context) noexcept;
using XamlResolveResourceScopeCallback = ResourceDictionary* (*)(
    Base::Object& scopeOwner,
    void* context) noexcept;
using XamlResolveImplicitResourceKeyCallback = Base::Result<ResourceKey> (*)(
    const Base::Object& object,
    void* context) noexcept;
using XamlResolvePropertyTargetCallback = Core::DependencyObject* (*)(
    Base::Object& object,
    void* context) noexcept;
enum class XamlProvidedValueKind : std::uint8_t {
    Value = 0U,
    Handled,
    Expression
};

using XamlProvidedRollbackCallback = void (*)(
    void* context,
    std::uint64_t token) noexcept;

struct XamlProvidedValue final {
    XamlProvidedValueKind kind = XamlProvidedValueKind::Value;
    XamlValue value;
    Core::EffectiveValueEngine* effectiveValues = nullptr;
    Core::PropertyExpression expression;
    void* rollbackContext = nullptr;
    std::uint64_t rollbackToken = 0U;
    XamlProvidedRollbackCallback rollback = nullptr;

    static XamlProvidedValue FromValue(XamlValue&& provided) noexcept {
        XamlProvidedValue result;
        result.value = static_cast<XamlValue&&>(provided);
        return result;
    }
    static XamlProvidedValue Handled(
        void* context = nullptr,
        std::uint64_t token = 0U,
        XamlProvidedRollbackCallback rollbackCallback = nullptr) noexcept {
        XamlProvidedValue result;
        result.kind = XamlProvidedValueKind::Handled;
        result.rollbackContext = context;
        result.rollbackToken = token;
        result.rollback = rollbackCallback;
        return result;
    }
    static XamlProvidedValue Expression(
        Core::EffectiveValueEngine& engine,
        const Core::PropertyExpression& provided) noexcept {
        XamlProvidedValue result;
        result.kind = XamlProvidedValueKind::Expression;
        result.effectiveValues = &engine;
        result.expression = provided;
        return result;
    }
};

using XamlProvideValueCallback = Base::Result<XamlProvidedValue> (*)(
    Base::StringView arguments,
    const XamlExtensionContext& services,
    void* context) noexcept;

struct XamlMemberFacet final {
    Core::MemberId member = Core::InvalidMemberId;
    XamlMemberWriteMode mode = XamlMemberWriteMode::SetOnce;
    XamlSetMemberCallback set = nullptr;
    void* context = nullptr;
    XamlSetMemberWithServicesCallback setWithServices = nullptr;
    bool acceptsAnyValue = false;
};

struct XamlMemberProviderFacet final {
    XamlHandlesMemberCallback handles = nullptr;
    XamlSetMemberWithServicesCallback set = nullptr;
    void* context = nullptr;
    XamlMemberWriteMode mode = XamlMemberWriteMode::SetOnce;
    bool acceptsAnyValue = false;
};

struct XamlMemberWritePolicy final {
    XamlMemberWriteMode mode = XamlMemberWriteMode::SetOnce;
    bool acceptsAnyValue = false;
    bool writable = false;
};

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
    XamlResolveImplicitResourceKeyCallback resolveImplicitResourceKey = nullptr;
    XamlResolvePropertyTargetCallback resolvePropertyTarget = nullptr;
};

struct XamlMarkupExtensionFacet final {
    Core::TypeId type = Core::InvalidTypeId;
    XamlProvideValueCallback provideValue = nullptr;
    void* context = nullptr;
};

// Compatibility names for the previous schema-owned registration API.
using XamlMemberAdapterRegistration = XamlMemberFacet;
using XamlMemberProviderRegistration = XamlMemberProviderFacet;
using XamlTypeAdapterRegistration = XamlTypeFacet;
using XamlMarkupExtensionRegistration = XamlMarkupExtensionFacet;

// Frozen XAML behavior facets. XamlSchemaContext only resolves metadata and
// delegates behavior lookup here; it no longer owns independent adapter arrays.
class AERO_API XamlFacetStore final {
public:
    XamlFacetStore() noexcept = default;

    XamlFacetStore(const XamlFacetStore&) = delete;
    XamlFacetStore& operator=(const XamlFacetStore&) = delete;

    Base::Result<void> TryAdd(
        const XamlMemberFacet& facet,
        const Core::MetadataDescriptorStore& descriptors) noexcept;
    Base::Result<void> TryAdd(
        const XamlMemberProviderFacet& facet) noexcept;
    Base::Result<void> TryAdd(
        const XamlTypeFacet& facet,
        const Core::MetadataDescriptorStore& descriptors) noexcept;
    Base::Result<void> TryAdd(
        const XamlMarkupExtensionFacet& facet,
        const Core::MetadataDescriptorStore& descriptors) noexcept;
    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }

    const XamlMemberFacet* FindMember(
        Core::MemberId member) const noexcept;
    const XamlMemberProviderFacet* FindMemberProvider(
        const XamlResolvedMember& member) const noexcept;
    const XamlTypeFacet* FindType(
        Core::TypeId type,
        const Core::MetadataDescriptorStore& descriptors) const noexcept;
    const XamlMarkupExtensionFacet* FindMarkupExtension(
        Core::TypeId type) const noexcept;

private:
    Base::Vector<XamlMemberFacet> members_;
    Base::Vector<XamlMemberProviderFacet> memberProviders_;
    Base::Vector<XamlTypeFacet> types_;
    Base::Vector<XamlMarkupExtensionFacet> markupExtensions_;
    bool frozen_ = false;

    const XamlTypeFacet* FindTypeExact(
        Core::TypeId type) const noexcept;
};

} // namespace Aero::Markup
