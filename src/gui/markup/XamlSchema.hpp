#pragma once

#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Value.hpp>
#include <Aero/Version.hpp>

#include "gui/core/ElementTree.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ValueConversion.hpp"

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/HashSet.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Menus.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Markup/MarkupExtension.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Style.hpp>
#include <Aero/UIElement.hpp>
#include "gui/templates/TemplateInstance.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace Aero {
class NameScope;
class DependencyObject;
class ResourceDictionary;
class ResourceKey;
class BindingEngine;
}

namespace Aero::Controls {
class ControlTemplate;
}

namespace Aero::Markup {

class Schema;
class SchemaManifest;
class CompiledDocument;
class NodeReader;
class ObjectWriter;
struct VisualContentPlan;
struct EffectServices;
struct ExtensionServices;

// ===== DeferredContent edges =====
struct DeferredContentEdge {
    Base::Object* owner = nullptr;
    Base::Object* parent = nullptr;
    Base::Ref<Base::Object> child;
    ::Aero::Meta::Registry* metadata = nullptr;
    Meta::MemberId member = Meta::InvalidMemberId;
    bool property = false;
};

struct DeferredBindingEdge {
    Base::Object* owner = nullptr;
    Base::Object* source = nullptr;
    Base::String sourceName;
    Base::String relativeAncestorType;
    std::uint32_t relativeAncestorLevel = 0U;
    ::Aero::DependencyObject* target = nullptr;
    ::Aero::Meta::Registry* metadata = nullptr;
    Meta::DependencyPropertyHandle targetProperty;
    Meta::DependencyPropertyHandle dataContextProperty;
    Base::String path;
    Base::String stringFormat;
    bool bindsToSource = false;
    Data::BindingMode mode =
        Data::BindingMode::Default;
    Meta::UpdateSourceTrigger updateSourceTrigger =
        Meta::UpdateSourceTrigger::PropertyChanged;
    Base::Ref<Data::IValueConverter> converter;
    Meta::PropertyValue converterParameter;
};

class DeferredContentPlan {
public:
    Base::Result<void> Stage(
        Base::Object& owner,
        Base::Object& parent,
        const Base::Ref<Base::Object>& child,
        ::Aero::Meta::Registry& metadata,
        Meta::MemberId member) noexcept;
    Base::Result<void> StageProperty(
        Base::Object& owner,
        Base::Object& parent,
        const Base::Ref<Base::Object>& child,
        ::Aero::Meta::Registry& metadata,
        Meta::MemberId member) noexcept;
    Base::Result<void> CopyForOwner(
        const Base::Object& owner,
        Base::Vector<DeferredContentEdge>& output) const noexcept;
    Base::Result<void> StageBinding(
        Base::Object& owner,
        Base::Object* source,
        Base::StringView sourceName,
        Base::StringView relativeAncestorType,
        std::uint32_t relativeAncestorLevel,
        ::Aero::DependencyObject& target,
        ::Aero::Meta::Registry& metadata,
        Meta::DependencyPropertyHandle targetProperty,
        Meta::DependencyPropertyHandle dataContextProperty,
        Base::StringView path,
        Base::StringView stringFormat,
        Data::BindingMode mode,
        Meta::UpdateSourceTrigger updateSourceTrigger,
        bool bindsToSource,
        const Base::Ref<Data::IValueConverter>& converter,
        const Meta::PropertyValue& converterParameter) noexcept;
    Base::Result<void> CopyBindingsForOwner(
        const Base::Object& owner,
        Base::Vector<DeferredBindingEdge>& output) const noexcept;
    void ReleaseOwner(Base::Object& owner) noexcept;
    void ReleaseAll() noexcept;
    bool Empty() const noexcept {
        return edges_.Empty() && bindings_.Empty();
    }

private:
    Base::Vector<DeferredContentEdge> edges_;
    Base::Vector<DeferredBindingEdge> bindings_;
};

// ============================================================================
// SECTION: ProvidedValue & ExtensionServices Contract
// ============================================================================
inline Base::StringView TrimAscii(Base::StringView value) noexcept {
    return ::Aero::Base::ValueConversion::Trim(value);
}


class DeferredContentPlan;
class Schema;
struct VisualContentPlan;

enum class ProvidedValueKind : std::uint8_t {
    Value = 0U,
    Handled,
    Expression,
    Deferred
};

using ProvidedPrepareCallback =
    Base::Result<void> (*)(
        void* context,
        const Aero::NameScope& names) noexcept;
using ProvidedCommitCallback =
    Base::Result<std::uint64_t> (*)(void* context) noexcept;
using ProvidedRollbackCallback = void (*)(
    void* context,
    std::uint64_t token) noexcept;
using ProvidedCleanupCallback = void (*)(void* context) noexcept;
using ProvidedBindCallback = Base::Result<void> (*)(
    void* context, const EffectServices& services) noexcept;

struct ProvidedValue {
    ProvidedValueKind kind = ProvidedValueKind::Value;
    Meta::Value value;
    Meta::EffectiveValueEngine* effectiveValues = nullptr;
    Meta::PropertyExpression expression;
    void* rollbackContext = nullptr;
    std::uint64_t rollbackToken = 0U;
    ProvidedRollbackCallback rollback = nullptr;
    ProvidedPrepareCallback prepare = nullptr;
    ProvidedCommitCallback commit = nullptr;
    ProvidedCleanupCallback cleanup = nullptr;
    ProvidedBindCallback bind = nullptr;

    static ProvidedValue FromValue(
        Meta::Value&& provided) noexcept {
        ProvidedValue result;
        result.value = static_cast<Meta::Value&&>(provided);
        return result;
    }
    static ProvidedValue Handled(
        void* context = nullptr,
        std::uint64_t token = 0U,
        ProvidedRollbackCallback rollbackCallback = nullptr) noexcept {
        ProvidedValue result;
        result.kind = ProvidedValueKind::Handled;
        result.rollbackContext = context;
        result.rollbackToken = token;
        result.rollback = rollbackCallback;
        return result;
    }
    static ProvidedValue Expression(
        Meta::EffectiveValueEngine& engine,
        const Meta::PropertyExpression& provided) noexcept {
        ProvidedValue result;
        result.kind = ProvidedValueKind::Expression;
        result.effectiveValues = &engine;
        result.expression = provided;
        return result;
    }
    static ProvidedValue Deferred(
        void* context,
        ProvidedCommitCallback commitCallback,
        ProvidedRollbackCallback rollbackCallback,
        ProvidedCleanupCallback cleanupCallback,
        ProvidedPrepareCallback prepareCallback = nullptr,
        ProvidedBindCallback bindCallback = nullptr) noexcept {
        ProvidedValue result;
        result.kind = ProvidedValueKind::Deferred;
        result.rollbackContext = context;
        result.prepare = prepareCallback;
        result.commit = commitCallback;
        result.rollback = rollbackCallback;
        result.cleanup = cleanupCallback;
        result.bind = bindCallback;
        return result;
    }
    void Discard() noexcept {
        if (kind == ProvidedValueKind::Expression &&
            expression.cleanup != nullptr) {
            expression.cleanup(expression.context);
        } else if (kind == ProvidedValueKind::Handled &&
                   rollback != nullptr) {
            rollback(rollbackContext, rollbackToken);
        } else if (kind == ProvidedValueKind::Deferred &&
                   cleanup != nullptr) {
            cleanup(rollbackContext);
        }
        expression = {};
        rollbackContext = nullptr;
        rollbackToken = 0U;
        rollback = nullptr;
        prepare = nullptr;
        commit = nullptr;
        cleanup = nullptr;
        bind = nullptr;
    }
};

class NamespaceScope {
public:
    using LookupCallback = Base::Result<Base::StringView> (*)(void* context, Base::StringView prefix) noexcept;

    NamespaceScope() noexcept = default;
    NamespaceScope(LookupCallback lookup, void* context) noexcept : lookup_(lookup), context_(context) {}

    Base::Result<Base::StringView> Lookup(Base::StringView prefix) const noexcept;
    bool IsAvailable() const noexcept { return lookup_ != nullptr; }

private:
    LookupCallback lookup_ = nullptr;
    void* context_ = nullptr;
};

class ResourceResolver {
public:
    using LookupCallback = Base::Result<Aero::ResourceValue> (*)(void* context, Base::StringView key) noexcept;

    ResourceResolver() noexcept = default;
    ResourceResolver(LookupCallback lookup, void* context) noexcept : lookup_(lookup), context_(context) {}

    Base::Result<Aero::ResourceValue> Lookup(Base::StringView key) const noexcept;
    bool IsAvailable() const noexcept { return lookup_ != nullptr; }

private:
    LookupCallback lookup_ = nullptr;
    void* context_ = nullptr;
};

struct ExtensionServices {
    const Schema* schema = nullptr;
    Base::Object* targetObject = nullptr;
    Meta::TypeId targetObjectType = Meta::InvalidTypeId;
    Meta::MemberId targetMember = Meta::InvalidMemberId;
    Meta::TypeId targetValueType = Meta::InvalidTypeId;
    Base::Object* rootObject = nullptr;
    Base::Object* templatedParent = nullptr;
    const Base::ResourceUri* baseUri = nullptr;
    ::Aero::Diagnostics::SourceSpan source;
    const Aero::NameScope* nameScope = nullptr;
    NamespaceScope namespaces;
    ResourceResolver resources;
    Meta::EffectiveValueEngine* effectiveValues = nullptr;
    Aero::BindingEngine* bindings = nullptr;
    Aero::ResourceDictionary* fallbackResources = nullptr;
    Base::Span<const Aero::ResourceDictionary* const>
        ambientResourceChain;
    VisualContentPlan* visualContent = nullptr;
    Base::Object* deferredContentOwner = nullptr;
    DeferredContentPlan* deferredContent = nullptr;
};

// ============================================================================
// SECTION: XamlFacets Contract
// ============================================================================
using XamlInitializationCallback = Base::Result<void> (*)(
    Base::Object& object,
    void* context) noexcept;
using XamlInitializationWithServicesCallback = Base::Result<void> (*)(
    Base::Object& object,
    const ExtensionServices& services,
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
    const Meta::Value& value,
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
    ::Aero::DependencyObject* (*)(
        Base::Object& object,
        void* context) noexcept;
using XamlProvideValueCallback =
    Base::Result<ProvidedValue> (*)(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;

enum class XamlFacetInheritancePolicy : std::uint8_t {
    ExactOnly = 0U,
    NearestBase,
    ComposeBaseToDerived
};

// Compatibility registration DTO. Add() atomically projects this record to
// the narrow capability slots retained by XamlFacets.
struct XamlTypeFacet {
    Meta::TypeId type = Meta::InvalidTypeId;
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

struct XamlLifecycleFacet {
    inline static constexpr XamlFacetInheritancePolicy InheritancePolicy =
        XamlFacetInheritancePolicy::ComposeBaseToDerived;

    Meta::TypeId type = Meta::InvalidTypeId;
    XamlInitializationCallback beginInit = nullptr;
    XamlInitializationCallback endInit = nullptr;
    XamlAbortInitializationCallback abortInit = nullptr;
    XamlInitializationWithServicesCallback endInitWithServices = nullptr;
    void* context = nullptr;
    std::uint32_t abiVersion = XamlFacetAbiVersion;
};

struct XamlNameScopeFacet {
    inline static constexpr XamlFacetInheritancePolicy InheritancePolicy =
        XamlFacetInheritancePolicy::NearestBase;

    Meta::TypeId type = Meta::InvalidTypeId;
    bool createsNameScope = true;
    XamlRegisterNameCallback registerName = nullptr;
    void* context = nullptr;
    std::uint32_t abiVersion = XamlFacetAbiVersion;
};

struct XamlResourceScopeFacet {
    inline static constexpr XamlFacetInheritancePolicy InheritancePolicy =
        XamlFacetInheritancePolicy::NearestBase;

    Meta::TypeId type = Meta::InvalidTypeId;
    bool createsResourceScope = true;
    XamlAddResourceCallback addResource = nullptr;
    XamlResolveResourceScopeCallback resolveResourceScope = nullptr;
    void* context = nullptr;
    std::uint32_t abiVersion = XamlFacetAbiVersion;
};

struct XamlDeferredContentFacet {
    inline static constexpr XamlFacetInheritancePolicy InheritancePolicy =
        XamlFacetInheritancePolicy::NearestBase;

    Meta::TypeId type = Meta::InvalidTypeId;
    bool defersVisualContent = true;
    std::uint32_t abiVersion = XamlFacetAbiVersion;
};

struct XamlImplicitResourceKeyFacet {
    inline static constexpr XamlFacetInheritancePolicy InheritancePolicy =
        XamlFacetInheritancePolicy::NearestBase;

    Meta::TypeId type = Meta::InvalidTypeId;
    XamlResolveImplicitResourceKeyCallback resolve = nullptr;
    void* context = nullptr;
    std::uint32_t abiVersion = XamlFacetAbiVersion;
};

struct XamlPropertyTargetFacet {
    inline static constexpr XamlFacetInheritancePolicy InheritancePolicy =
        XamlFacetInheritancePolicy::NearestBase;

    Meta::TypeId type = Meta::InvalidTypeId;
    XamlResolvePropertyTargetCallback resolve = nullptr;
    void* context = nullptr;
    std::uint32_t abiVersion = XamlFacetAbiVersion;
};

struct XamlMarkupExtensionFacet {
    inline static constexpr XamlFacetInheritancePolicy InheritancePolicy =
        XamlFacetInheritancePolicy::ExactOnly;

    Meta::TypeId type = Meta::InvalidTypeId;
    XamlProvideValueCallback provideValue = nullptr;
    void* context = nullptr;
    std::uint32_t abiVersion = XamlFacetAbiVersion;
};

class XamlFacets {
public:
    XamlFacets() noexcept = default;

    XamlFacets(const XamlFacets&) = delete;
    XamlFacets& operator=(const XamlFacets&) = delete;

    Base::Result<void> Add(
        const XamlTypeFacet& facet,
        const Meta::TypeRegistry& descriptors) noexcept;
    Base::Result<void> Add(
        const XamlLifecycleFacet& facet,
        const Meta::TypeRegistry& descriptors) noexcept;
    Base::Result<void> Add(
        const XamlNameScopeFacet& facet,
        const Meta::TypeRegistry& descriptors) noexcept;
    Base::Result<void> Add(
        const XamlResourceScopeFacet& facet,
        const Meta::TypeRegistry& descriptors) noexcept;
    Base::Result<void> Add(
        const XamlDeferredContentFacet& facet,
        const Meta::TypeRegistry& descriptors) noexcept;
    Base::Result<void> Add(
        const XamlImplicitResourceKeyFacet& facet,
        const Meta::TypeRegistry& descriptors) noexcept;
    Base::Result<void> Add(
        const XamlPropertyTargetFacet& facet,
        const Meta::TypeRegistry& descriptors) noexcept;
    Base::Result<void> Add(
        const XamlMarkupExtensionFacet& facet,
        const Meta::TypeRegistry& descriptors) noexcept;
    Base::Result<void> Freeze(
        const Meta::TypeRegistry& descriptors) noexcept;

    bool IsFrozen() const noexcept { return frozen_; }

    Base::Span<const std::uint32_t> LifecyclePlan(
        Meta::TypeId type) const noexcept;
    const XamlLifecycleFacet* LifecycleAt(
        std::uint32_t index) const noexcept;
    const XamlLifecycleFacet* FindLifecycle(
        Meta::TypeId type,
        const Meta::TypeRegistry& descriptors) const noexcept;
    const XamlNameScopeFacet* FindNameScope(
        Meta::TypeId type,
        const Meta::TypeRegistry& descriptors) const noexcept;
    const XamlResourceScopeFacet* FindResourceScope(
        Meta::TypeId type,
        const Meta::TypeRegistry& descriptors) const noexcept;
    const XamlDeferredContentFacet* FindDeferredContent(
        Meta::TypeId type,
        const Meta::TypeRegistry& descriptors) const noexcept;
    const XamlImplicitResourceKeyFacet* FindImplicitResourceKey(
        Meta::TypeId type,
        const Meta::TypeRegistry& descriptors) const noexcept;
    const XamlPropertyTargetFacet* FindPropertyTarget(
        Meta::TypeId type,
        const Meta::TypeRegistry& descriptors) const noexcept;
    const XamlMarkupExtensionFacet* FindMarkupExtension(
        Meta::TypeId type) const noexcept;

private:
    enum class FacetKind : std::uint8_t {
        Lifecycle = 0U,
        NameScope,
        ResourceScope,
        DeferredContent,
        ImplicitResourceKey,
        PropertyTarget,
        MarkupExtension,
        Count
    };

    using FacetMask = std::uint16_t;
    inline static constexpr std::uint32_t InvalidFacetIndex = UINT32_MAX;

    struct DraftType {
        Meta::TypeId type = Meta::InvalidTypeId;
        std::uint32_t facets[
            static_cast<std::uint8_t>(FacetKind::Count)] = {
                InvalidFacetIndex,
                InvalidFacetIndex,
                InvalidFacetIndex,
                InvalidFacetIndex,
                InvalidFacetIndex,
                InvalidFacetIndex,
                InvalidFacetIndex};
    };

    // Runtime record. Exact facets are stored as a compact run in facetRefs_;
    // lifecycle inheritance is flattened once at Freeze().
    struct XamlTypePlan {
        Meta::TypeId type = Meta::InvalidTypeId;
        std::uint32_t firstFacetRef = 0U;
        std::uint32_t firstLifecycleRef = 0U;
        FacetMask facetMask = 0U;
        std::uint16_t facetCount = 0U;
        std::uint16_t lifecycleCount = 0U;
        std::uint16_t reserved = 0U;
    };

    static_assert(sizeof(XamlTypePlan) <= 40U,
        "XamlTypePlan must remain compact");
    static_assert(sizeof(std::uint32_t) == 4U,
        "XAML facet references must remain 32-bit");

    using FacetIndex = Base::HashMap<Meta::TypeId, std::uint32_t>;

    Base::Vector<DraftType> drafts_;
    Base::Vector<XamlTypePlan> plans_;
    Base::Vector<std::uint32_t> facetRefs_;
    Base::Vector<std::uint32_t> lifecycleRefs_;
    Base::Vector<XamlLifecycleFacet> lifecycles_;
    Base::Vector<XamlNameScopeFacet> nameScopes_;
    Base::Vector<XamlResourceScopeFacet> resourceScopes_;
    Base::Vector<XamlDeferredContentFacet> deferredContents_;
    Base::Vector<XamlImplicitResourceKeyFacet> implicitResourceKeys_;
    Base::Vector<XamlPropertyTargetFacet> propertyTargets_;
    Base::Vector<XamlMarkupExtensionFacet> markupExtensions_;
    FacetIndex index_;
    bool frozen_ = false;

    static constexpr FacetMask FacetBit(FacetKind kind) noexcept {
        return static_cast<FacetMask>(
            1U << static_cast<std::uint8_t>(kind));
    }
    static std::uint16_t FacetCountBefore(
        FacetMask mask, FacetKind kind) noexcept;
    Base::Result<DraftType*> EnsureType(Meta::TypeId type) noexcept;
    template<class Facet, class ColumnType>
    Base::Result<void> AddFacetCore(
        ColumnType& column,
        FacetKind kind,
        const Facet& facet,
        const Meta::TypeRegistry& descriptors,
        bool isInvalid,
        const char* invalidMessage,
        const char* duplicateMessage) noexcept;
    DraftType* FindDraft(Meta::TypeId type) noexcept;
    const DraftType* FindDraft(Meta::TypeId type) const noexcept;
    const XamlTypePlan* FindPlan(Meta::TypeId type) const noexcept;
    std::uint32_t FindFacetIndex(
        Meta::TypeId type, FacetKind kind) const noexcept;
    Base::Result<void> BuildLifecyclePlans(
        const Meta::TypeRegistry& descriptors) noexcept;

    const XamlLifecycleFacet* FindLifecycleExact(
        Meta::TypeId type) const noexcept;
    const XamlNameScopeFacet* FindNameScopeExact(
        Meta::TypeId type) const noexcept;
    const XamlResourceScopeFacet* FindResourceScopeExact(
        Meta::TypeId type) const noexcept;
    const XamlDeferredContentFacet* FindDeferredContentExact(
        Meta::TypeId type) const noexcept;
    const XamlImplicitResourceKeyFacet* FindImplicitResourceKeyExact(
        Meta::TypeId type) const noexcept;
    const XamlPropertyTargetFacet* FindPropertyTargetExact(
        Meta::TypeId type) const noexcept;
};

// ============================================================================
// SECTION: XamlSchema Core Contract
// ============================================================================
inline constexpr Base::StringView LanguageNamespaceUri() noexcept {
    return Base::StringView("http://schemas.microsoft.com/winfx/2006/xaml");
}

class Schema;
class SchemaManifest;
class CompiledDocument;
class NodeReader;
class XamlFacets;
struct XamlTypeFacet;
struct XamlLifecycleFacet;
struct XamlNameScopeFacet;
struct XamlResourceScopeFacet;
struct XamlDeferredContentFacet;
struct XamlImplicitResourceKeyFacet;
struct XamlPropertyTargetFacet;
struct XamlMarkupExtensionFacet;

enum class XmlTokenKind : std::uint8_t {
    None = 0U,
    StartElement,
    EndElement,
    Text,
    EndOfDocument
};

namespace XmlDiagnosticCodes {
inline constexpr ::Aero::Diagnostics::DiagnosticCode InvalidUtf8 =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 1U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode InputLimitExceeded =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 2U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode UnexpectedEndOfInput =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 3U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode MalformedMarkup =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 4U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode UnsupportedDeclaration =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 5U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode DepthLimitExceeded =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 6U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode AttributeLimitExceeded =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 7U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode NameLimitExceeded =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 8U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode TextLimitExceeded =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 9U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode MismatchedEndElement =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 10U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode MultipleDocumentElements =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 11U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode UnknownEntity =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 12U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode InvalidXmlCharacter =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 13U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode DuplicateAttribute =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 14U);
} // namespace XmlDiagnosticCodes

class XmlAttribute {
public:
    XmlAttribute() noexcept = default;
    XmlAttribute(XmlAttribute&&) noexcept = default;
    XmlAttribute& operator=(XmlAttribute&&) noexcept = default;

    XmlAttribute(const XmlAttribute&) = delete;
    XmlAttribute& operator=(const XmlAttribute&) = delete;

    Base::StringView Name() const noexcept {
        return name_.View();
    }
    Base::StringView Value() const noexcept {
        return value_.View();
    }
    ::Aero::Diagnostics::SourceSpan Source() const noexcept {
        return source_;
    }
    ::Aero::Diagnostics::SourceSpan NameSource() const noexcept {
        return nameSource_;
    }
    ::Aero::Diagnostics::SourceSpan ValueSource() const noexcept {
        return valueSource_;
    }

private:
    friend class Utf8XmlTokenizer;
    friend class ExpatXmlTokenizer;

    Base::String name_;
    Base::String value_;
    ::Aero::Diagnostics::SourceSpan source_;
    ::Aero::Diagnostics::SourceSpan nameSource_;
    ::Aero::Diagnostics::SourceSpan valueSource_;
};

class XmlToken {
public:
    XmlToken() noexcept = default;
    XmlToken(XmlToken&&) noexcept = default;
    XmlToken& operator=(XmlToken&&) noexcept = default;

    XmlToken(const XmlToken&) = delete;
    XmlToken& operator=(const XmlToken&) = delete;

    void Clear() noexcept;

    XmlTokenKind Kind() const noexcept { return kind_; }
    Base::StringView Name() const noexcept {
        return name_.View();
    }
    Base::StringView Text() const noexcept {
        return text_.View();
    }
    Base::Span<const XmlAttribute> Attributes() const noexcept {
        return {attributes_.Data(), attributes_.Size()};
    }
    ::Aero::Diagnostics::SourceSpan Source() const noexcept {
        return source_;
    }
    ::Aero::Diagnostics::SourceSpan NameSource() const noexcept {
        return nameSource_;
    }
    bool IsEmptyElement() const noexcept {
        return emptyElement_;
    }

private:
    friend class Utf8XmlTokenizer;
    friend class ExpatXmlTokenizer;

    XmlTokenKind kind_ = XmlTokenKind::None;
    Base::String name_;
    Base::String text_;
    Base::Vector<XmlAttribute> attributes_;
    ::Aero::Diagnostics::SourceSpan source_;
    ::Aero::Diagnostics::SourceSpan nameSource_;
    bool emptyElement_ = false;
};

class IXmlTokenizer {
public:
    virtual ~IXmlTokenizer() = default;

    virtual Base::Result<void> Reset(
        Base::StringView utf8,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept = 0;
    virtual Base::Result<void> Reset(
        Base::Stream& stream,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept = 0;
    virtual Base::Result<XmlTokenKind> Read(
        XmlToken& token) noexcept = 0;
    virtual std::uint32_t Depth() const noexcept = 0;
};

class Utf8XmlTokenizer
    : public IXmlTokenizer {
public:
    explicit Utf8XmlTokenizer(
        XmlTokenizerLimits limits = {}) noexcept;

    Base::Result<void> Reset(
        Base::StringView utf8,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept override;
    Base::Result<void> Reset(
        Base::Stream& stream,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept override;
    Base::Result<XmlTokenKind> Read(
        XmlToken& token) noexcept override;
    std::uint32_t Depth() const noexcept override {
        return openElements_.Size();
    }

    const XmlTokenizerLimits& Limits() const noexcept {
        return limits_;
    }
    std::uint64_t ByteOffset() const noexcept {
        return offset_;
    }

private:
    XmlTokenizerLimits limits_;
    Base::StringView input_;
    Base::String ownedInput_;
    Base::Vector<Base::String> openElements_;
    Diagnostics::IDiagnosticSink* diagnostics_ = nullptr;
    std::uint32_t offset_ = 0U;
    std::uint32_t line_ = 1U;
    std::uint32_t column_ = 1U;
    bool rootSeen_ = false;
    bool rootClosed_ = false;
    bool endEmitted_ = false;
    bool initialized_ = false;
    bool failed_ = false;

    bool AtEnd() const noexcept;
    bool StartsWith(
        const char* literal,
        std::uint32_t length) const noexcept;
    ::Aero::Diagnostics::SourcePosition Position() const noexcept;
    ::Aero::Diagnostics::SourceSpan SpanFrom(
        ::Aero::Diagnostics::SourcePosition begin) const noexcept;
    std::uint32_t CurrentCodePointLength() const noexcept;
    std::uint32_t CurrentCodePoint() const noexcept;
    void AdvanceCodePoint() noexcept;
    void ConsumeAscii(std::uint32_t count) noexcept;
    bool SkipWhitespace() noexcept;

    Base::Result<XmlTokenKind> ParseStartElement(
        XmlToken& token) noexcept;
    Base::Result<XmlTokenKind> ParseEndElement(
        XmlToken& token) noexcept;
    Base::Result<XmlTokenKind> ParseText(
        XmlToken& token) noexcept;
    Base::Result<XmlTokenKind> ParseCdata(
        XmlToken& token) noexcept;
    Base::Result<void> ParseName(
        Base::String& name,
        ::Aero::Diagnostics::SourceSpan& source) noexcept;
    Base::Result<void> ParseAttributeValue(
        char quote,
        Base::String& value,
        ::Aero::Diagnostics::SourceSpan& source) noexcept;
    Base::Result<void> AppendEntity(
        Base::String& output) noexcept;
    Base::Result<void> AppendCodePoint(
        Base::String& output,
        std::uint32_t codePoint) noexcept;
    Base::Result<void> AppendCurrentCodePoint(
        Base::String& output,
        bool attributeValue = false) noexcept;
    Base::Result<void> SkipComment() noexcept;
    Base::Result<void> SkipProcessingInstruction() noexcept;

    Base::Status Failure(
        Base::ErrorCode error,
        ::Aero::Diagnostics::DiagnosticCode diagnostic,
        Base::StringView message,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
};

class ExpatXmlTokenizer
    : public IXmlTokenizer {
public:
    explicit ExpatXmlTokenizer(
        XmlTokenizerLimits limits = {}) noexcept;
    ~ExpatXmlTokenizer() noexcept override;

    ExpatXmlTokenizer(const ExpatXmlTokenizer&) = delete;
    ExpatXmlTokenizer& operator=(
        const ExpatXmlTokenizer&) = delete;

    Base::Result<void> Reset(
        Base::StringView utf8,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept override;
    Base::Result<void> Reset(
        Base::Stream& stream,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept override;
    Base::Result<XmlTokenKind> Read(
        XmlToken& token) noexcept override;
    std::uint32_t Depth() const noexcept override {
        return depth_;
    }
    const XmlTokenizerLimits& Limits() const noexcept {
        return limits_;
    }

private:
    XmlTokenizerLimits limits_;
    Base::Vector<XmlToken> tokens_;
    Base::Vector<std::uint32_t> tokenDepths_;
    Diagnostics::IDiagnosticSink* diagnostics_ = nullptr;
    void* parser_ = nullptr;
    Base::Status failure_;
    std::uint32_t readIndex_ = 0U;
    std::uint32_t parseDepth_ = 0U;
    std::uint32_t depth_ = 0U;
    bool initialized_ = false;
    Base::Stream* stream_ = nullptr;
    std::uint64_t streamBytes_ = 0U;
    bool streamEof_ = false;
    bool streamMode_ = false;

    void HandleStart(
        const char* name,
        const char** attributes) noexcept;
    void HandleEnd(const char* name) noexcept;
    void HandleText(
        const char* text,
        int length) noexcept;
    void RejectDeclaration() noexcept;
    int RejectExternalEntity() noexcept;
    ::Aero::Diagnostics::SourcePosition Position() const noexcept;
    void Stop(
        Base::Status status,
        ::Aero::Diagnostics::DiagnosticCode diagnostic,
        Base::StringView message) noexcept;
    Base::Result<void> PushToken(
        XmlToken&& token,
        std::uint32_t depth) noexcept;
    Base::Result<void> InitializeParser(
        Diagnostics::IDiagnosticSink* diagnostics) noexcept;
    Base::Result<XmlTokenKind> ReadStream(
        XmlToken& token) noexcept;
};

enum class NodeKind : std::uint8_t {
    None = 0U,
    NamespaceDeclaration,
    StartObject,
    EndObject,
    StartMember,
    EndMember,
    Value,
    EndOfDocument
};

namespace NodeDiagnosticCodes {
inline constexpr ::Aero::Diagnostics::DiagnosticCode UnboundNamespacePrefix =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 101U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode InvalidNamespaceDeclaration =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 102U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode DuplicateNamespacePrefix =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 103U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode InvalidNodeStreamState =
    ::Aero::Diagnostics::MakeDiagnosticCode(
        ::Aero::Diagnostics::DiagnosticDomain::Xaml, 104U);
} // namespace NodeDiagnosticCodes

// Runtime-only replay record. AXB2 continues to persist only MemberId; a
// compatible frozen Registry expands each unique id to this record once during
// binding/deserialization so ObjectWriter does not repeat schema lookup or
// write-policy discovery for every member occurrence.
struct CompiledMemberBinding {
    Meta::MemberId id = Meta::InvalidMemberId;
    Meta::MemberKind kind = Meta::MemberKind::Property;
    Meta::TypeId ownerType = Meta::InvalidTypeId;
    Meta::TypeId valueType = Meta::InvalidTypeId;
    Meta::MetadataTypeKind valueTypeKind =
        Meta::MetadataTypeKind::Object;
    Meta::TypeFlags valueTypeFlags = Meta::TypeFlags::None;
    Meta::PropertyFlags propertyFlags = Meta::PropertyFlags::None;
    Meta::EventFlags eventFlags = Meta::EventFlags::None;
    std::uint8_t writeMode = 0U;
    bool attached = false;
    bool acceptsAnyValue = false;
    bool writable = false;

    bool ValueTypeIsObject() const noexcept {
        return valueTypeKind ==
            Meta::MetadataTypeKind::Object;
    }
    bool ValueTypeIsValueType() const noexcept {
        return valueTypeFlags != Meta::TypeFlags::None &&
            HasTypeFlag(valueTypeFlags, Meta::TypeFlags::ValueType);
    }

    bool IsValid() const noexcept {
        return id != Meta::InvalidMemberId &&
            ownerType != Meta::InvalidTypeId &&
            valueType != Meta::InvalidTypeId;
    }
};

struct CompiledTypeBinding {
    Meta::TypeId id = Meta::InvalidTypeId;
    Meta::MetadataTypeKind kind =
        Meta::MetadataTypeKind::Object;
    Meta::TypeFlags flags = Meta::TypeFlags::None;
    CompiledMemberBinding contentMember;
    bool hasContentMember = false;

    bool HasContentMember() const noexcept {
        return hasContentMember &&
            contentMember.IsValid();
    }
};

class QualifiedName {
public:
    QualifiedName() noexcept = default;
    QualifiedName(QualifiedName&&) noexcept = default;
    QualifiedName& operator=(QualifiedName&&) noexcept = default;

    QualifiedName(const QualifiedName&) = delete;
    QualifiedName& operator=(const QualifiedName&) = delete;

    Base::StringView Prefix() const noexcept {
        return prefix_.View();
    }
    Base::StringView LocalName() const noexcept {
        return localName_.View();
    }
    Base::StringView NamespaceUri() const noexcept {
        return namespaceUri_.View();
    }
    bool HasNamespace() const noexcept {
        return !namespaceUri_.Empty();
    }

private:
    friend class Node;
    friend class NodeReader;
    friend class CompiledDocument;

    void Clear() noexcept;

    Base::String prefix_;
    Base::String localName_;
    Base::String namespaceUri_;
};

class Node {
public:
    Node() noexcept = default;
    Node(Node&&) noexcept = default;
    Node& operator=(Node&&) noexcept = default;

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    void Clear() noexcept;
    static Base::Result<Node> Clone(
        const Node& source) noexcept;

    NodeKind Kind() const noexcept { return kind_; }
    const QualifiedName& Name() const noexcept {
        return name_;
    }
    Base::StringView NamespacePrefix() const noexcept {
        return namespacePrefix_.View();
    }
    Base::StringView NamespaceUri() const noexcept {
        return namespaceUri_.View();
    }
    Base::StringView Value() const noexcept {
        return value_.View();
    }
    ::Aero::Diagnostics::SourceSpan Source() const noexcept {
        return source_;
    }
    bool IsFromAttribute() const noexcept {
        return fromAttribute_;
    }
    // AXB2 binds object and member instructions to the frozen metadata
    // domain at compile time. Invalid ids identify syntax-only instructions
    // such as namespace declarations and XAML directives.
    Meta::TypeId CompiledTypeId() const noexcept {
        return compiledTypeId_;
    }
    bool HasCompiledTypeBinding() const noexcept {
        return compiledTypeBinding_.id !=
            Meta::InvalidTypeId;
    }
    const CompiledTypeBinding& CompiledType() const noexcept {
        return compiledTypeBinding_;
    }
    Meta::MemberId CompiledMemberId() const noexcept {
        return compiledMemberId_;
    }
    bool HasCompiledMemberBinding() const noexcept {
        return compiledMemberBinding_.IsValid();
    }
    const CompiledMemberBinding&
    CompiledMember() const noexcept {
        return compiledMemberBinding_;
    }
    void BindCompiledType(Meta::TypeId type) noexcept {
        compiledTypeId_ = type;
    }
    void BindCompiledType(const CompiledTypeBinding& type) noexcept {
        compiledTypeBinding_ = type;
        compiledTypeId_ = type.id;
    }
    void BindCompiledMember(Meta::MemberId member) noexcept {
        compiledMemberId_ = member;
        compiledMemberBinding_ = {};
    }
    void BindCompiledMember(
        const CompiledMemberBinding& member) noexcept {
        compiledMemberId_ = member.id;
        compiledMemberBinding_ = member;
    }
    bool HasCompiledValue() const noexcept {
        return !compiledValue_.IsUnset();
    }
    const Meta::Value& CompiledValue() const noexcept {
        return compiledValue_;
    }
    void BindCompiledValue(Meta::Value value) noexcept {
        compiledValue_ = std::move(value);
    }

private:
    friend class NodeReader;
    friend class CompiledDocument;

    NodeKind kind_ = NodeKind::None;
    QualifiedName name_;
    Base::String namespacePrefix_;
    Base::String namespaceUri_;
    Base::String value_;
    ::Aero::Diagnostics::SourceSpan source_;
    bool fromAttribute_ = false;
    Meta::TypeId compiledTypeId_ = Meta::InvalidTypeId;
    CompiledTypeBinding compiledTypeBinding_;
    Meta::MemberId compiledMemberId_ = Meta::InvalidMemberId;
    CompiledMemberBinding compiledMemberBinding_;
    Meta::Value compiledValue_;
};

class NodeReader {
public:
    explicit NodeReader(
        IXmlTokenizer& tokenizer,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;

    void Reset() noexcept;
    Base::Result<NodeKind> Read(
        Node& node) noexcept;
    std::uint32_t ObjectDepth() const noexcept {
        return scopes_.Size();
    }

private:
    struct NamespaceBinding {
        NamespaceBinding() noexcept = default;
        NamespaceBinding(NamespaceBinding&&) noexcept = default;
        NamespaceBinding& operator=(NamespaceBinding&&) noexcept = default;
        NamespaceBinding(const NamespaceBinding&) = delete;
        NamespaceBinding& operator=(const NamespaceBinding&) = delete;

        Base::String prefix;
        Base::String uri;
    };

    struct ScopeFrame {
        ScopeFrame() noexcept = default;
        ScopeFrame(ScopeFrame&&) noexcept = default;
        ScopeFrame& operator=(ScopeFrame&&) noexcept = default;
        ScopeFrame(const ScopeFrame&) = delete;
        ScopeFrame& operator=(const ScopeFrame&) = delete;

        std::uint32_t bindingStart = 0U;
        std::uint32_t ignorableNamespaceStart = 0U;
        QualifiedName objectName;
        bool ignored = false;
    };

    IXmlTokenizer* tokenizer_ = nullptr;
    Diagnostics::IDiagnosticSink* diagnostics_ = nullptr;
    XmlToken xmlToken_;
    Base::Vector<Node> pending_;
    Base::Vector<NamespaceBinding> bindings_;
    Base::Vector<Base::String> ignorableNamespaces_;
    Base::Vector<ScopeFrame> scopes_;
    std::uint32_t pendingIndex_ = 0U;
    bool ended_ = false;
    bool failed_ = false;

    Base::Result<void> QueueStartElement(
        const XmlToken& token) noexcept;
    Base::Result<void> QueueEndElement(
        const XmlToken& token) noexcept;
    Base::Result<void> QueueText(
        const XmlToken& token) noexcept;
    Base::Result<void> QueueEndOfDocument(
        const XmlToken& token) noexcept;
    Base::Result<void> QueueNamespaceDeclaration(
        Base::StringView prefix,
        Base::StringView uri,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
    Base::Result<void> QueueObjectNode(
        NodeKind kind,
        const QualifiedName& name,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
    Base::Result<void> QueueMemberNodes(
        const XmlAttribute& attribute) noexcept;
    Base::Result<void> AppendPending(
        Node&& node) noexcept;

    Base::Result<void> AddNamespaceBinding(
        Base::StringView prefix,
        Base::StringView uri,
        std::uint32_t bindingStart,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
    Base::Result<void> AddIgnorableNamespaces(
        Base::StringView prefixes,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
    Base::Result<void> ResolveQualifiedName(
        Base::StringView qualifiedName,
        bool useDefaultNamespace,
        ::Aero::Diagnostics::SourceSpan source,
        QualifiedName& output) noexcept;
    Base::Result<void> CopyQualifiedName(
        const QualifiedName& source,
        QualifiedName& output) noexcept;
    Base::StringView LookupNamespace(
        Base::StringView prefix,
        bool& found) const noexcept;
    bool IsNamespaceDeclaration(
        Base::StringView attributeName,
        Base::StringView& prefix) const noexcept;
    bool IsIgnorableNamespace(
        Base::StringView uri) const noexcept;
    bool IsMarkupCompatibilityIgnorable(
        const XmlAttribute& attribute) const noexcept;
    void PopBindings(std::uint32_t bindingStart) noexcept;
    void PopIgnorableNamespaces(std::uint32_t start) noexcept;

    Base::Result<NodeKind> EmitPending(
        Node& node) noexcept;
    Base::Status Failure(
        Base::ErrorCode error,
        ::Aero::Diagnostics::DiagnosticCode diagnostic,
        Base::StringView message,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
};

struct CompiledCacheIdentity {
    std::uint32_t cacheFormatVersion =
        XamlCompiledCacheFormatVersion;
    std::uint32_t typeIdAlgorithmVersion =
        Meta::TypeIdAlgorithmVersion;
    std::uint32_t metadataSchemaFormatVersion =
        Meta::MetadataSchemaFormatVersion;
    std::uint32_t metadataProgramFormatVersion =
        Meta::MetadataProgramFormatVersion;
    std::uint32_t schemaVersion =
        XamlSchemaAbiVersion;
    Base::HashCode metadataSchemaHash = 0U;
};

enum class CompiledCacheCompatibility : std::uint8_t {
    Compatible = 0U,
    CacheFormatMismatch,
    TypeIdAlgorithmMismatch,
    MetadataSchemaFormatMismatch,
    MetadataProgramFormatMismatch,
    SchemaVersionMismatch,
    MetadataSchemaMismatch
};

Base::Result<CompiledCacheIdentity>
BuildCompiledCacheIdentity(
    const ::Aero::Meta::Registry& domain) noexcept;

CompiledCacheCompatibility
CompareCompiledCacheIdentity(
    const CompiledCacheIdentity& cached,
    const CompiledCacheIdentity& current) noexcept;

Base::Result<void> ValidateCompiledCacheIdentity(
    const CompiledCacheIdentity& cached,
    const ::Aero::Meta::Registry& currentDomain) noexcept;

struct CompiledDocumentSerializeOptions {
    // Source positions are useful for development diagnostics but account for
    // 32 bytes per instruction in the current stable representation. Release
    // assets may omit them without changing object-writer semantics.
    bool includeSourceMap = true;
};

// Immutable replay IR produced from the XML node stream. It removes XML
// tokenization from the load path and is guarded by the same metadata schema
// identity used by persisted compiled-XAML caches.
class CompiledDocument {
public:
    CompiledDocument() noexcept = default;

    CompiledDocument(const CompiledDocument&) = delete;
    CompiledDocument& operator=(const CompiledDocument&) = delete;
    CompiledDocument(CompiledDocument&&) noexcept = default;
    CompiledDocument& operator=(
        CompiledDocument&&) noexcept = default;

    static Base::Result<CompiledDocument> Compile(
        NodeReader& reader,
        const ::Aero::Meta::Registry& domain) noexcept;
    static Base::Result<CompiledDocument> Compile(
        NodeReader& reader,
        const ::Aero::Meta::Registry& domain,
        const Base::ResourceUri& originUri) noexcept;
    static Base::Result<CompiledDocument> Compile(
        Base::Span<const Node> nodes,
        const ::Aero::Meta::Registry& domain,
        const Base::ResourceUri& originUri) noexcept;
    static Base::Result<CompiledDocument> Compile(
        Base::Span<const Node> nodes,
        const Schema& schema,
        const Base::ResourceUri& originUri) noexcept;
    static Base::Result<CompiledDocument> Compile(
        NodeReader& reader,
        const Schema& schema) noexcept;
    static Base::Result<CompiledDocument> Compile(
        NodeReader& reader,
        const Schema& schema,
        const Base::ResourceUri& originUri) noexcept;
    static Base::Result<CompiledDocument> Compile(
        NodeReader& reader,
        const SchemaManifest& manifest) noexcept;
    static Base::Result<CompiledDocument> Compile(
        NodeReader& reader,
        const SchemaManifest& manifest,
        const Base::ResourceUri& originUri) noexcept;
    Base::Result<void> ValidateSchema(
        const Schema& schema) const noexcept;
    Base::Result<void> ValidateSchema(
        const SchemaManifest& manifest) const noexcept;
    Base::Result<Base::Vector<std::uint8_t>>
    Serialize(
        const CompiledDocumentSerializeOptions& options = {}) const noexcept;
    Base::Result<CompiledDocument> Clone() const noexcept;
    static Base::Result<CompiledDocument> Deserialize(
        Base::Span<const std::uint8_t> bytes,
        const ::Aero::Meta::Registry& domain,
        const CompiledDocumentLimits& limits = {}) noexcept;

    const CompiledCacheIdentity& Identity() const noexcept {
        return identity_;
    }
    Base::Span<const Node> Nodes() const noexcept {
        return {nodes_.Data(), nodes_.Size()};
    }
    const Base::ResourceUri& OriginUri() const noexcept {
        return originUri_;
    }
    Base::Span<const Base::ResourceUri> Dependencies() const noexcept {
        return {dependencies_.Data(), dependencies_.Size()};
    }
    Base::Result<void> AddDependency(
        const Base::ResourceUri& dependency) noexcept;
    bool IsValid() const noexcept {
        return !nodes_.Empty() &&
            nodes_.Back().Kind() ==
                NodeKind::EndOfDocument;
    }
    Base::Ref<Base::Object> BytecodeProgram() const noexcept {
        return bytecodeProgram_;
    }
    void SetBytecodeProgram(Base::Ref<Base::Object> program) const noexcept {
        bytecodeProgram_ = std::move(program);
    }

private:
    Base::Result<void> BindSchema(
        const Schema& schema) noexcept;
    Base::Result<void> BindSchema(
        const SchemaManifest& manifest) noexcept;
    static Base::Result<CompiledDocument> CompileWithIdentity(
        NodeReader& reader,
        const CompiledCacheIdentity& identity,
        const Base::ResourceUri& originUri) noexcept;

    CompiledCacheIdentity identity_;
    Base::ResourceUri originUri_;
    Base::Vector<Base::ResourceUri> dependencies_;
    Base::Vector<Node> nodes_;
    mutable Base::Ref<Base::Object> bytecodeProgram_;
};

} // namespace Aero::Markup

// Canonical public schema API.

namespace Aero { class DependencyObject; class ResourceDictionary; class ResourceKey; }
namespace Aero { class ModuleSet; }
namespace Aero::Data { class MultiBinding; }

namespace Aero {
class GuiSchema;
}

namespace Aero::Markup {

struct ExtensionServices;
struct ProvidedValue;
class Loader;
class ObjectWriter;
class SchemaManifest;
struct LoadState;

} // namespace Aero::Markup

namespace Aero::Markup {
class XamlStyleSchemaFacet;

Base::Result<void> PopulateMarkupMetadata(
    ::Aero::Meta::Registration& context) noexcept;
} // namespace Aero::Markup

namespace Aero::Markup {

enum class MemberSyntax : std::uint8_t {
    Attribute = 0U,
    PropertyElement,
    Content
};

enum class MemberWriteMode : std::uint8_t {
    SetOnce = 0U,
    Collection
};

struct ResolvedMember {
    Meta::MemberId id = Meta::InvalidMemberId;
    Meta::MemberKind kind = Meta::MemberKind::Property;
    Meta::TypeId ownerType = Meta::InvalidTypeId;
    Meta::TypeId valueType = Meta::InvalidTypeId;
    Meta::PropertyFlags propertyFlags =
        Meta::PropertyFlags::None;
    Meta::EventFlags eventFlags = Meta::EventFlags::None;
    bool attached = false;

    bool IsValid() const noexcept {
        return id != Meta::InvalidMemberId &&
            ownerType != Meta::InvalidTypeId &&
            valueType != Meta::InvalidTypeId;
    }
};

struct MemberWritePolicy {
    MemberWriteMode mode = MemberWriteMode::SetOnce;
    bool acceptsAnyValue = false;
    bool writable = false;
};

namespace WriterBindingSupport {
Base::Result<ProvidedValue> CreateMultiBindingValueImpl(
    class ::Aero::Data::MultiBinding& binding,
    const struct ExtensionServices& services) noexcept;
}

class Schema;
Base::Result<void> RegisterBuiltinMarkupExtensions(Schema& schema) noexcept;

class Schema {
public:
    Schema(
        ::Aero::Meta::Registry& metadata,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~Schema() noexcept;

    Schema(const Schema&) = delete;
    Schema& operator=(const Schema&) = delete;

    bool IsFrozen() const noexcept { return frozen_; }
    const Meta::TypeRegistry& Types() const noexcept {
        return domain_->Types();
    }
    Base::Result<const Meta::TypeInfo*> ResolveType(
        Base::StringView xamlNamespace,
        Base::StringView localName) const noexcept;
    Base::Result<ResolvedMember> ResolveMember(
        Meta::TypeId targetType,
        const QualifiedName& name,
        MemberSyntax syntax) const noexcept;
    Base::Result<ResolvedMember> ResolveMember(
        Meta::TypeId targetType,
        Meta::MemberId member) const noexcept;
    Base::Result<ResolvedMember> ResolveContentMember(
        Meta::TypeId targetType) const noexcept;
    Base::Result<Meta::Value> ConvertText(
        Meta::TypeId type,
        Base::StringView text,
        const ExtensionServices* services = nullptr) const noexcept;
    MemberWritePolicy ResolveMemberWritePolicy(
        const ResolvedMember& member) const noexcept;


private:
    friend class ::Aero::GuiSchema;
    friend class CompiledDocument;
    friend class Loader;
    friend class ObjectWriter;
    friend class XamlVM;
    friend class SchemaManifest;
    friend class XamlStyleSchemaFacet;
    friend class ResourceExtension;
    friend class XamlTemplateSchemaFacet;
    friend class BindingExtension;
    friend Base::Result<ProvidedValue>
        WriterBindingSupport::CreateMultiBindingValueImpl(
            ::Aero::Data::MultiBinding& binding,
            const ExtensionServices& services) noexcept;
    friend Base::Result<void> RegisterBuiltinMarkupExtensions(Schema& schema) noexcept;
    friend class StaticResourceExtension;
    friend class DynamicResourceExtension;
    friend class TypeExtension;
    friend class LocExtension;
    friend class TemplateBindingExtension;
    friend class StaticExtension;
    friend class ::Aero::ModuleSet;

    Base::Result<void> Freeze() noexcept;
    ::Aero::Meta::Registry* Metadata() const noexcept { return domain_; }
    const ::Aero::Meta::Registry& Domain() const noexcept {
        return *domain_;
    }

    Base::Result<Base::Ref<Base::Object>> CreateObject(
        Meta::TypeId type) const noexcept;
    Base::Result<::Aero::DependencyObject*> ResolvePropertyTarget(
        Base::Object& object) const noexcept;
    Base::Result<void> SetMember(
        Base::Object& object,
        Meta::TypeId objectType,
        const ResolvedMember& member,
        const Meta::Value& value) const noexcept;
    Base::Result<ProvidedValue> ProvideMarkupExtensionValue(
        Meta::TypeId type,
        Base::StringView arguments,
        const ExtensionServices& services) const noexcept;

    Base::Result<void> BeginInit(
        Meta::TypeId type,
        Base::Object& object) const noexcept;
    Base::Result<void> EndInit(
        Meta::TypeId type,
        Base::Object& object,
        const ExtensionServices& services) const noexcept;
    void AbortInit(Meta::TypeId type, Base::Object& object) const noexcept;

    bool CreatesNameScope(Meta::TypeId type) const noexcept;
    bool CreatesResourceScope(Meta::TypeId type) const noexcept;
    bool DefersVisualContent(Meta::TypeId type) const noexcept;
    Base::Result<void> RegisterName(
        Meta::TypeId scopeType,
        Base::Object& scopeOwner,
        Base::StringView name,
        Base::Object& object) const noexcept;
    Base::Result<void> AddResource(
        Meta::TypeId scopeType,
        Base::Object& scopeOwner,
        const Aero::ResourceKey& key,
        const Meta::Value& value) const noexcept;
    Aero::ResourceDictionary* ResolveResourceScope(
        Meta::TypeId scopeType,
        Base::Object& scopeOwner) const noexcept;
    Base::Result<Aero::ResourceKey> ResolveImplicitResourceKey(
        Meta::TypeId type,
        const Base::Object& object) const noexcept;


    Base::IAllocator* allocator_ = nullptr;
    XamlFacets* facets_ = nullptr;
    ::Aero::Meta::Registry* domain_ = nullptr;
    bool frozen_ = false;


    Base::Result<void> AddType(
        const XamlTypeFacet& registration) noexcept;
    Base::Result<void> AddLifecycle(
        const XamlLifecycleFacet& registration) noexcept;
    Base::Result<void> AddNameScope(
        const XamlNameScopeFacet& registration) noexcept;
    Base::Result<void> AddResourceScope(
        const XamlResourceScopeFacet& registration) noexcept;
    Base::Result<void> AddDeferredContent(
        const XamlDeferredContentFacet& registration) noexcept;
    Base::Result<void> AddImplicitResourceKey(
        const XamlImplicitResourceKeyFacet& registration) noexcept;
    Base::Result<void> AddPropertyTarget(
        const XamlPropertyTargetFacet& registration) noexcept;
    Base::Result<void> AddMarkupExtension(
        const XamlMarkupExtensionFacet& registration) noexcept;

    Base::Result<ResolvedMember> ResolvePropertyOrEvent(
        Meta::TypeId targetType,
        Meta::TypeId ownerType,
        Base::StringView memberName,
        MemberSyntax syntax,
        bool ownerWasExplicit) const noexcept;
};

struct SchemaManifestLimits {
    std::uint32_t maxTypes = 100000U;
    std::uint32_t maxMembers = 500000U;
    std::uint32_t maxStringBytes =
        64U * 1024U * 1024U;
};

struct SchemaTypeInfo {
    Meta::TypeId id = Meta::InvalidTypeId;
    Meta::MetadataTypeKind kind =
        Meta::MetadataTypeKind::Object;
    Meta::TypeFlags flags = Meta::TypeFlags::None;
};

class SchemaManifest {
public:
    struct State;
    explicit SchemaManifest(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~SchemaManifest() noexcept;

    SchemaManifest(SchemaManifest&& other) noexcept;
    SchemaManifest& operator=(
        SchemaManifest&& other) noexcept;

    SchemaManifest(const SchemaManifest&) = delete;
    SchemaManifest& operator=(const SchemaManifest&) = delete;

    static Base::Result<SchemaManifest> Capture(
        const Schema& schema,
        Base::IAllocator* allocator = nullptr) noexcept;
    static Base::Result<SchemaManifest> Deserialize(
        Base::Span<const std::uint8_t> bytes,
        const SchemaManifestLimits& limits = {},
        Base::IAllocator* allocator = nullptr) noexcept;

    Base::Result<Base::Vector<std::uint8_t>>
    Serialize() const noexcept;

    bool IsValid() const noexcept;
    std::uint32_t TypeCount() const noexcept;
    std::uint32_t MemberCount() const noexcept;

    const CompiledCacheIdentity& Identity() const noexcept;

    Base::Result<SchemaTypeInfo> ResolveType(
        Base::StringView xamlNamespace,
        Base::StringView localName) const noexcept;
    Base::Result<ResolvedMember> ResolveMember(
        Meta::TypeId targetType,
        const QualifiedName& name,
        MemberSyntax syntax) const noexcept;
    Base::Result<ResolvedMember> ResolveContentMember(
        Meta::TypeId targetType) const noexcept;

private:
    Base::IAllocator* allocator_ = nullptr;
    State* state_ = nullptr;

    explicit SchemaManifest(
        Base::IAllocator& allocator,
        State* state) noexcept;
};

inline constexpr Base::StringView
MarkupMetadataModuleName() noexcept {
    return "Aero.Markup";
}

inline Base::Result<void> RegisterMarkupMetadata(
    ::Aero::Meta::Registry& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 4U;
    const Base::StringView name =
        MarkupMetadataModuleName();
    return domain.RegisterModule({
        Meta::MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &PopulateMarkupMetadata,
        nullptr,
        nullptr});
}

} // namespace Aero::Markup

// Consolidated private Markup runtime and schema contract.

// ===== Loader contract =====
// XamlReader → Loader → ObjectWriter → Schema. Loader owns a short-lived
// LoaderState session; LoadState (≈ ParserContext) is per-Load only.
#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Markup/XamlProvider.hpp>


#include <atomic>

namespace Aero { class ResourceDictionary; }

namespace Aero::Meta {
class DependencyPropertyRegistry;
class EffectiveValueEngine;
}
namespace Aero::Threading { class Dispatcher; }

namespace Aero::Markup {

class Schema;

class EffectLifetime : public Base::Object {
public:
    EffectLifetime() noexcept = default;
    ~EffectLifetime() noexcept override = default;

    bool IsActive() const noexcept {
        return active_.load(std::memory_order_acquire);
    }
    void Invalidate() noexcept {
        active_.store(false, std::memory_order_release);
    }

private:
    std::atomic<bool> active_{true};
};

enum class EffectCommitMode : std::uint8_t {
    Immediate = 0U,
    Deferred
};


struct XamlProviderResolution {
    XamlProvider* provider = nullptr;
    std::uint64_t cacheIdentity = 0U;
};

struct XamlProviderRegistration {
    Base::String scheme;
    Base::String assembly;
    Base::Ref<XamlProvider> provider;
    std::uint64_t identity = 0U;
};

class XamlProviderRegistry {
public:
    explicit XamlProviderRegistry(
        Base::IAllocator* allocator = nullptr) noexcept
        : registrations_(allocator) {}
    XamlProviderRegistry(
        const XamlProviderRegistry* parent,
        Base::IAllocator* allocator) noexcept
        : registrations_(allocator), parent_(parent) {}

    Base::Result<void> Set(
        Base::Ref<XamlProvider> provider,
        Base::StringView scheme = {},
        Base::StringView assembly = {},
        Base::Ref<XamlProvider>* replaced = nullptr) noexcept;
    Base::Result<XamlProviderResolution> ResolveDetailed(
        const Base::ResourceUri& uri) const noexcept;
    Base::Result<XamlProvider*> Resolve(
        const Base::ResourceUri& uri) const noexcept;
    bool Contains(const XamlProvider& provider) const noexcept;

    std::uint32_t ProviderCount() const noexcept {
        return registrations_.Size() +
            (parent_ != nullptr ? parent_->ProviderCount() : 0U);
    }

private:
    Base::Result<XamlProviderResolution> ResolveRoute(
        const Base::ResourceUri& uri,
        bool requireScheme,
        bool requireAssembly) const noexcept;

    Base::Vector<XamlProviderRegistration> registrations_;
    const XamlProviderRegistry* parent_ = nullptr;
};

class EmbeddedXamlProvider
    : public XamlProvider {
public:
    Base::Result<void> Add(
        const Base::ResourceUri& uri,
        Base::Span<const std::uint8_t> bytes,
        std::uint64_t revision = 1U) noexcept;
    Base::Result<void> AddText(
        const Base::ResourceUri& uri,
        Base::StringView text,
        std::uint64_t revision = 1U) noexcept;
    Base::Result<void> Freeze() noexcept;

    Base::Result<::Aero::Markup::StreamResourceInfo> Open(
        const Base::ResourceUri& uri) const noexcept override;
    Base::Result<std::uint64_t> Revision(
        const Base::ResourceUri& uri) const noexcept override;

    bool IsFrozen() const noexcept { return frozen_; }
    std::uint32_t SourceCount() const noexcept {
        return entries_.Size();
    }

private:
    struct Entry {
        Base::ResourceUri uri;
        Base::Vector<std::uint8_t> bytes;
        std::uint64_t revision = 0U;
    };

    Base::Vector<Entry> entries_;
    bool frozen_ = false;
};

class FileXamlProvider
    : public XamlProvider {
public:
    explicit FileXamlProvider(
        std::uint64_t maxFileBytes =
            64ULL * 1024ULL * 1024ULL) noexcept
        : maxFileBytes_(maxFileBytes) {}

    Base::Result<::Aero::Markup::StreamResourceInfo> Open(
        const Base::ResourceUri& uri) const noexcept override;
    Base::Result<std::uint64_t> Revision(
        const Base::ResourceUri& uri) const noexcept override;
private:
    std::uint64_t maxFileBytes_ = 0U;
};

struct DocumentCacheLimits {
    std::uint32_t maxEntries = 256U;
    std::uint64_t maxCompiledBytes =
        64ULL * 1024ULL * 1024ULL;
};

struct DocumentCacheStatistics {
    std::uint32_t entryCount = 0U;
    std::uint64_t compiledBytes = 0U;
    std::uint64_t hitCount = 0U;
    std::uint64_t missCount = 0U;
    std::uint64_t storeCount = 0U;
    std::uint64_t invalidationCount = 0U;
    std::uint64_t evictionCount = 0U;
    std::uint64_t generation = 0U;
};

struct DocumentCacheLookup {
    bool hit = false;
    std::uint64_t sourceRevision = 0U;
    const CompiledDocument* document = nullptr;
};

class DependencyGraph {
public:
    struct State;
    explicit DependencyGraph(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~DependencyGraph() noexcept;

    DependencyGraph(DependencyGraph&& other) noexcept;
    DependencyGraph& operator=(
        DependencyGraph&& other) noexcept;

    DependencyGraph(const DependencyGraph&) = delete;
    DependencyGraph& operator=(const DependencyGraph&) = delete;

    Base::Result<void> Update(
        const Base::ResourceUri& document,
        Base::Span<const Base::ResourceUri> dependencies) noexcept;
    bool Remove(
        const Base::ResourceUri& document) noexcept;
    void Clear() noexcept;

    Base::Result<void> CopyDependencies(
        const Base::ResourceUri& document,
        Base::Vector<Base::ResourceUri>& output) const noexcept;
    Base::Result<void> CopyDependents(
        const Base::ResourceUri& dependency,
        Base::Vector<Base::ResourceUri>& output) const noexcept;
    Base::Result<void> CollectAffected(
        const Base::ResourceUri& changed,
        Base::Vector<Base::ResourceUri>& output) const noexcept;

    std::uint32_t NodeCount() const noexcept;
    std::uint64_t Generation() const noexcept;

private:
    Base::IAllocator* allocator_ = nullptr;
    State* state_ = nullptr;
};

class DocumentCache {
public:
    struct State;
    explicit DocumentCache(
        Base::IAllocator* allocator = nullptr,
        const DocumentCacheLimits& limits = {}) noexcept;
    ~DocumentCache() noexcept;

    DocumentCache(DocumentCache&& other) noexcept;
    DocumentCache& operator=(
        DocumentCache&& other) noexcept;

    DocumentCache(const DocumentCache&) = delete;
    DocumentCache& operator=(const DocumentCache&) = delete;

    Base::Result<DocumentCacheLookup> Lookup(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        const ::Aero::Meta::Registry& domain,
        const CompiledDocumentLimits& limits = {}) noexcept {
        return Lookup(
            uri, sourceRevision, 0U, domain, limits);
    }
    Base::Result<DocumentCacheLookup> Lookup(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        std::uint64_t sourceIdentity,
        const ::Aero::Meta::Registry& domain,
        const CompiledDocumentLimits& limits = {}) noexcept;
    Base::Result<void> Store(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        const CompiledDocument& document,
        Base::Span<const Base::ResourceUri> dependencies) noexcept {
        return Store(
            uri, sourceRevision, 0U,
            document, dependencies);
    }
    Base::Result<void> Store(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        std::uint64_t sourceIdentity,
        const CompiledDocument& document,
        Base::Span<const Base::ResourceUri> dependencies) noexcept;
    Base::Result<void> Store(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        CompiledDocument&& document,
        Base::Span<const Base::ResourceUri> dependencies) noexcept {
        return Store(
            uri, sourceRevision, 0U,
            std::move(document), dependencies);
    }
    Base::Result<void> Store(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        std::uint64_t sourceIdentity,
        CompiledDocument&& document,
        Base::Span<const Base::ResourceUri> dependencies) noexcept;

    Base::Result<std::uint32_t> Invalidate(
        const Base::ResourceUri& uri,
        bool includeDependents = true) noexcept;
    void Clear() noexcept;

    bool Contains(
        const Base::ResourceUri& uri) const noexcept;
    bool GetSourceRevision(
        const Base::ResourceUri& uri,
        std::uint64_t& revision) const noexcept {
        return GetSourceRevision(uri, 0U, revision);
    }
    bool GetSourceRevision(
        const Base::ResourceUri& uri,
        std::uint64_t sourceIdentity,
        std::uint64_t& revision) const noexcept;
    Base::Result<void> CollectAffected(
        const Base::ResourceUri& changed,
        Base::Vector<Base::ResourceUri>& output) const noexcept;

    const DependencyGraph& Dependencies() const noexcept;
    DocumentCacheStatistics Statistics() const noexcept;
    const DocumentCacheLimits& Limits() const noexcept;

private:
    Base::IAllocator* allocator_ = nullptr;
    State* state_ = nullptr;
};

class Loader {
public:
    Loader(
        Schema& schema,
        XamlProviderRegistry& providers,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr,
        Base::IAllocator* allocator = nullptr,
        const LoadState* loadState = nullptr) noexcept;
    ~Loader() noexcept;

    Loader(const Loader&) = delete;
    Loader& operator=(const Loader&) = delete;
    Loader(Loader&&) = delete;
    Loader& operator=(Loader&&) = delete;

    Base::Result<XamlDocument> Load(
        Base::StringView uri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<XamlDocument> Load(
        const Base::ResourceUri& uri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<XamlDocument> Parse(
        Base::StringView text,
        const Base::ResourceUri& baseUri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<XamlDocument> Parse(
        Base::Stream& stream,
        const Base::ResourceUri& baseUri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<XamlDocument> LoadComponent(
        Base::Object& existingRoot,
        Base::StringView uri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<XamlDocument> LoadComponent(
        Base::Object& existingRoot,
        const Base::ResourceUri& uri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<XamlDocument> LoadCompiled(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        const XamlReaderSettings& options = {}) noexcept;

private:
    // Per-Loader URI/provider session storage. Distinct from LoadState
    // (≈ ParserContext), which is supplied per Load and dies with that load.
    struct LoaderState;


    Base::IAllocator* allocator_ = nullptr;
    LoaderState* state_ = nullptr;
};

} // namespace Aero::Markup


// ===== LoaderResult contract =====
// Private transaction result consumed by Loader, XamlDocument, and View.

namespace Aero::Markup {

struct VisualEdge {
    UIElement* parent = nullptr;
    UIElement* child = nullptr;
    ElementAttachment state;
};

} // namespace Aero::Markup

namespace Aero::Markup {

using EffectPrepareCallback = Base::Result<void> (*)(
    void* context,
    const Aero::NameScope& names) noexcept;
using EffectCommitCallback = Base::Result<std::uint64_t> (*)(
    void* context) noexcept;
using EffectRollbackCallback = void (*)(
    void* context,
    std::uint64_t token) noexcept;
using EffectCleanupCallback = void (*)(void* context) noexcept;

struct EffectServices {
    Meta::EffectiveValueEngine* effectiveValues = nullptr;
    Aero::BindingEngine* bindings = nullptr;
    Aero::ResourceDictionary* fallbackResources = nullptr;
    Base::Ref<EffectLifetime> lifetime;
};
using EffectBindCallback = Base::Result<void> (*)(
    void* context, const EffectServices& services) noexcept;

struct VisualContentEdge {
    Base::Ref<Base::Object> parentOwner;
    Base::Ref<Base::Object> childOwner;
    ::Aero::Meta::Registry* metadata = nullptr;
    Meta::MemberId member = Meta::InvalidMemberId;
    bool property = false;
};

// Markup-owned declaration result for visual content. The plan intentionally
// stores only content ownership and UI mount edges; the UI runtime owns
// the actual attach/detach sequence through the owning ElementTree.
struct VisualContentPlan {
    Base::Vector<VisualContentEdge> contentEdges;
    Base::Vector<Aero::Markup::VisualEdge> mountEdges;
    Base::Vector<Aero::Media::Visual*> nodes;

    Base::Result<void> Reserve(
        std::uint32_t contentEdgeCount,
        std::uint32_t mountEdgeCount,
        std::uint32_t nodeCount) noexcept;
    Base::Result<void> AddNode(
        Aero::Media::Visual& node) noexcept;
    void ReleaseContent() noexcept;
    void Clear() noexcept;
    std::uint32_t EdgeCount() const noexcept {
        return mountEdges.Size();
    }
    std::uint32_t NodeCount() const noexcept {
        return nodes.Size();
    }
};


struct CommittedEffect {
    Base::Ref<EffectLifetime> lifetime;
    Meta::EffectiveValueEngine* effectiveValues = nullptr;
    Base::Ref<::Aero::DependencyObject> targetOwner;
    ::Aero::DependencyObject* target = nullptr;
    Meta::DependencyPropertyHandle property;
    Meta::PropertyExpression pendingExpression;
    void* context = nullptr;
    std::uint64_t token = 0U;
    EffectPrepareCallback prepare = nullptr;
    EffectCommitCallback commit = nullptr;
    EffectRollbackCallback rollback = nullptr;
    EffectCleanupCallback cleanup = nullptr;
    EffectBindCallback bind = nullptr;
    bool committed = false;

    Base::Result<void> Bind(
        const EffectServices& services) noexcept {
        if (!lifetime && services.lifetime) lifetime = services.lifetime;
        if (effectiveValues == nullptr && services.effectiveValues != nullptr) {
            effectiveValues = services.effectiveValues;
        }
        return bind != nullptr
            ? bind(context, services)
            : Base::Result<void>();
    }

    Base::Result<void> Prepare(
        const Aero::NameScope& names) noexcept {
        return prepare != nullptr
            ? prepare(context, names)
            : Base::Result<void>();
    }

    Base::Result<void> Commit() noexcept {
        if (committed) return {};
        if (lifetime && !lifetime->IsActive()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML effect runtime is no longer active");
        }
        if (pendingExpression.IsValid()) {
            if (effectiveValues == nullptr || target == nullptr ||
                !property.IsValid()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Deferred XAML property expression is invalid");
            }
            Base::Result<void> installed =
                effectiveValues->SetLocalExpression(
                    *target, property, pendingExpression);
            if (!installed) return installed.GetStatus();
            pendingExpression = {};
            committed = true;
            return {};
        }
        if (commit != nullptr) {
            Base::Result<std::uint64_t> result = commit(context);
            if (!result) return result.GetStatus();
            token = result.Value();
            committed = true;
            return {};
        }
        committed = true;
        return {};
    }

    void Rollback() noexcept {
        const bool runtimeActive = !lifetime || lifetime->IsActive();
        if (committed && runtimeActive) {
            if (effectiveValues != nullptr && target != nullptr) {
                // A committed XAML effect is rolled back only when its
                // document/template lifetime is being discarded. Remove the
                // complete engine record instead of queueing a recomputation
                // for an object that may be released immediately afterwards.
                static_cast<void>(effectiveValues->DetachObject(*target));
            }
            if (rollback != nullptr) {
                rollback(context, token);
            }
        } else if (!committed && pendingExpression.cleanup != nullptr) {
            pendingExpression.cleanup(pendingExpression.context);
        }
        if (cleanup != nullptr) cleanup(context);
        lifetime.Reset();
        effectiveValues = nullptr;
        target = nullptr;
        targetOwner.Reset();
        pendingExpression = {};
        context = nullptr;
        token = 0U;
        prepare = nullptr;
        commit = nullptr;
        rollback = nullptr;
        cleanup = nullptr;
        bind = nullptr;
        committed = false;
    }
};

class CommittedEffectPlan {
public:
    CommittedEffectPlan() noexcept = default;
    ~CommittedEffectPlan() noexcept { Rollback(); }

    CommittedEffectPlan(
        CommittedEffectPlan&& other) noexcept
        : effects_(std::move(other.effects_)) {}
    CommittedEffectPlan& operator=(
        CommittedEffectPlan&& other) noexcept {
        if (this == &other) return *this;
        Rollback();
        effects_ = std::move(other.effects_);
        return *this;
    }

    CommittedEffectPlan(const CommittedEffectPlan&) = delete;
    CommittedEffectPlan& operator=(
        const CommittedEffectPlan&) = delete;

    Base::Result<void> Bind(
        const EffectServices& services) noexcept {
        for (CommittedEffect& effect : effects_) {
            Base::Result<void> bound = effect.Bind(services);
            if (!bound) return bound.GetStatus();
        }
        return {};
    }

    Base::Vector<CommittedEffect>& Items() noexcept { return effects_; }
    const Base::Vector<CommittedEffect>& Items() const noexcept {
        return effects_;
    }
    Base::Result<void> Prepare(
        const Aero::NameScope& names) noexcept {
        for (CommittedEffect& effect : effects_) {
            Base::Result<void> prepared = effect.Prepare(names);
            if (!prepared) return prepared.GetStatus();
        }
        return {};
    }
    Base::Result<void> Commit() noexcept {
        for (std::uint32_t index = 0U; index < effects_.Size(); ++index) {
            Base::Result<void> committed = effects_[index].Commit();
            if (committed) continue;
            for (std::uint32_t rollbackIndex = index;
                 rollbackIndex > 0U; --rollbackIndex) {
                effects_[rollbackIndex - 1U].Rollback();
            }
            return committed.GetStatus();
        }
        return {};
    }
    void Rollback() noexcept {
        for (std::uint32_t index = effects_.Size(); index > 0U; --index) {
            effects_[index - 1U].Rollback();
        }
        effects_.Clear();
    }
    std::uint32_t Size() const noexcept { return effects_.Size(); }

private:
    Base::Vector<CommittedEffect> effects_;
};

// Ownership returned by a successful XAML load. The object writer remains a
// short-lived loading session; mounted runtimes keep names, resources, and the
// visual content plan here instead of reaching back into Markup services.
struct LoaderResult {
    Base::Ref<Base::Object> root;
    ::Aero::Meta::Registry* metadata = nullptr;
    Aero::NameScope names;
    Aero::ResourceDictionary resources;
    VisualContentPlan visualContent;
    CommittedEffectPlan effects;
    Base::ResourceUri canonicalUri;
    Base::Vector<Base::ResourceUri> dependencies;
    Base::Ref<EffectLifetime> runtimeLifetime;
    bool hasDeferredStaticResources = false;

    void Clear() noexcept {
        effects.Rollback();
        root.Reset();
        metadata = nullptr;
        names.Clear();
        resources.Clear();
        visualContent.ReleaseContent();
        visualContent.Clear();
        canonicalUri = {};
        dependencies.Clear();
        runtimeLifetime.Reset();
        hasDeferredStaticResources = false;
    }
};

} // namespace Aero::Markup


// ===== LoadState contract =====





namespace Aero::Markup {

using LoadFinalizeCallback = Base::Result<void> (*)(
    LoaderResult& result,
    void* context) noexcept;

struct LoadState {
    const Aero::ResourceDictionary* resources = nullptr;
    Meta::EffectiveValueEngine* effectiveValues = nullptr;
    Aero::BindingEngine* bindings = nullptr;
    Aero::ResourceDictionary* fallbackResources = nullptr;
    const Base::ResourceUri* baseUri = nullptr;
    Base::Object* templatedParent = nullptr;
    Base::Ref<Base::Object> existingRoot;
    Base::Ref<EffectLifetime> effectLifetime;
    EffectCommitMode effectCommitMode =
        EffectCommitMode::Immediate;
    DocumentCache* documentCache = nullptr;
    ::Aero::Threading::Dispatcher* dispatcher = nullptr;
    Meta::DependencyPropertyRegistry* dependencyProperties = nullptr;
    std::uint32_t maxObjects = UINT32_MAX;
    bool deferUnresolvedStaticResources = false;
    Base::Vector<Node>* recordingNodes = nullptr;
    LoadFinalizeCallback finalize = nullptr;
    void* finalizeContext = nullptr;
};

} // namespace Aero::Markup


// ===== GuiSchema contract =====
#include "gui/core/ModuleSet.hpp"


namespace Aero::Meta { class Registry; class Registration; }


namespace Aero::Markup {
class Schema;
}

namespace Aero {

struct GuiSchemaOptions {
    Base::IAllocator* allocator = nullptr;
};

class GuiSchema {
public:
    struct State;
    explicit GuiSchema(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~GuiSchema() noexcept;

    GuiSchema(const GuiSchema&) = delete;
    GuiSchema& operator=(const GuiSchema&) = delete;

    Base::Result<void> Prepare(
        const ModuleSet& modules) noexcept;
    Base::Result<void> Finalize(
        const GuiSchemaOptions& inputs) noexcept;
    bool IsPrepared() const noexcept;
    bool IsFrozen() const noexcept;
    ::Aero::Meta::Registry& Metadata() noexcept;
    const ::Aero::Meta::Registry& Metadata() const noexcept;
    Markup::Schema& Schema() noexcept;
    const Markup::Schema& Schema() const noexcept;

private:
    Base::IAllocator* allocator_ = nullptr;
    State* state_ = nullptr;
};

} // namespace Aero


// ===== ResourceSupport contract =====
// Private XAML resource-scope registration.


namespace Aero::Markup {

class Schema;

// Installs the ResourceDictionary schema adapters shared by runtime, compiled,
// application and theme XAML.
class ResourceExtension {
public:
    Base::Result<void> Register(
        Schema& schema) noexcept;

private:
    Schema* schema_ = nullptr;
};

} // namespace Aero::Markup


// ===== StaticResourceObject contract =====
#include <Aero/DependencyProperty.hpp>

namespace Aero::Markup {

// Supports the object-element form used by the reference theme, for example
// <StaticResource ResourceKey="Anim.Expand.Vertical.Loaded"/>.
class StaticResourceObject : public ::Aero::DependencyObject {
    AERO_DECLARE_TYPE_NAMED(
        StaticResourceObject,
        ::Aero::DependencyObject,
        "urn:aero",
        "StaticResource")
public:
    StaticResourceObject() noexcept
        : DependencyObject(StaticTypeId()) {}

    Base::StringView ResourceKey() const noexcept {
        return GetValue(ResourceKeyProperty);
    }
    void SetResourceKey(
        Base::StringView value) noexcept {
        SetValue(ResourceKeyProperty, value);
    }

    AERO_DEPENDENCY_PROPERTY(Base::String, ResourceKey);
};

} // namespace Aero::Markup


namespace Aero::Markup {
using ::Aero::Markup::StaticResourceObject;
}


// ===== UiObjectModel contract =====
// Private registration bridge for UI authoring objects.


namespace Aero::Markup {

class Schema;

struct UiObjectModelOptions {
    UiObjectModelOptions() noexcept = default;
    UiObjectModelOptions(
        ::Aero::Meta::Registry* metadata,
        Meta::DependencyPropertyRegistry* dependencyProperties,
        Base::IAllocator* programAllocator = nullptr) noexcept
        : metadata(metadata),
          properties(dependencyProperties),
          allocator(programAllocator) {}

    ::Aero::Meta::Registry* metadata = nullptr;
    Meta::DependencyPropertyRegistry* properties = nullptr;
    Base::IAllocator* allocator = nullptr;
};

// Optional registration override used by schema hosts that expose a custom
// Style/Setter model. Product runtimes use Register(schema), which
// registers Aero's complete Style/Trigger/Template object model.
struct UiObjectModelTypes {
    Meta::TypeId style = Meta::InvalidTypeId;
    Meta::TypeId setter = Meta::InvalidTypeId;
    Meta::TypeId trigger = Meta::InvalidTypeId;
    Meta::DependencyPropertyHandle styleProperty;
    bool includeTemplates = true;
};

// Owns the schema adapters for the UI XAML object model. Parsing,
// compiled XAML, themes, and application resources all register this same
// object model instead of constructing independent Style or Template paths.
class UiObjectModel {
public:
    struct State;
    explicit UiObjectModel(
        const UiObjectModelOptions& options) noexcept;
    ~UiObjectModel() noexcept;

    UiObjectModel(
        const UiObjectModel&) = delete;
    UiObjectModel& operator=(
        const UiObjectModel&) = delete;

    Base::Result<void> Register(
        Schema& schema) noexcept;
    Base::Result<void> Register(
        Schema& schema,
        const UiObjectModelTypes& types) noexcept;

private:
    Base::IAllocator* allocator_ = nullptr;
    State* state_ = nullptr;
    bool optionsValid_ = false;
};

} // namespace Aero::Markup


// ===== UiObjectModelInternal contract =====


#include <Aero/Controls/Control.hpp>

namespace Aero::Markup {

class XamlStyleSchemaFacet {
public:
    explicit XamlStyleSchemaFacet(
        const UiObjectModelOptions& options) noexcept;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId styleType,
        Meta::TypeId setterType,
        Meta::DependencyPropertyHandle styleProperty,
        Meta::TypeId triggerType) noexcept;
private:
    UiObjectModelOptions options_;
    Schema* schema_ = nullptr;
    Meta::TypeId styleType_ = Meta::InvalidTypeId;
    Meta::TypeId setterType_ = Meta::InvalidTypeId;
    Meta::TypeId triggerType_ = Meta::InvalidTypeId;

    Base::Result<void> FinalizeStyle(
        Aero::Style& style) noexcept;
    Base::Result<Meta::PropertyValue> ConvertValueForProperty(
        const Meta::Value& value,
        Meta::TypeId targetType,
        Base::StringView propertyName) const noexcept;

    static Base::Result<void> EndStyleInit(
        Base::Object& object,
        void* context) noexcept;
};

class XamlTemplateSchemaFacet {
public:
    struct State;
    XamlTemplateSchemaFacet(
        ::Aero::Meta::Registry& metadata,
        Meta::DependencyPropertyRegistry& properties,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~XamlTemplateSchemaFacet() noexcept;

    XamlTemplateSchemaFacet(
        const XamlTemplateSchemaFacet&) = delete;
    XamlTemplateSchemaFacet& operator=(
        const XamlTemplateSchemaFacet&) = delete;

    Base::Result<void> Register(
        Schema& schema) noexcept;

private:
    Base::IAllocator* allocator_ = nullptr;
    State* state_ = nullptr;
};

} // namespace Aero::Markup

namespace Aero::Markup {
using ::Aero::Markup::XamlStyleSchemaFacet;
using ::Aero::Markup::XamlTemplateSchemaFacet;
}


// ===== XamlDocumentInternal contract =====



namespace Aero::Markup {
Base::Result<::Aero::Markup::XamlDocument> AdoptXamlDocument(
    ::Aero::Markup::LoaderResult&& result,
    Base::IAllocator& allocator) noexcept;
const ::Aero::Markup::EffectLifetime* XamlDocumentEffectLifetime(
    const ::Aero::Markup::XamlDocument& document) noexcept;
::Aero::Markup::LoaderResult TakeXamlDocument(
    ::Aero::Markup::XamlDocument& document) noexcept;

// ============================================================================
// SECTION: XamlSchemaConventions
// ============================================================================

inline constexpr Base::StringView WpfPresentationNamespace(
    "http://schemas.microsoft.com/winfx/2006/xaml/presentation");
inline constexpr Base::StringView BehaviorsNamespace(
    "http://schemas.microsoft.com/xaml/behaviors");
inline constexpr Base::StringView BlendInteractivityNamespace(
    "http://schemas.microsoft.com/expression/2010/interactivity");
inline constexpr Base::StringView SystemNamespacePrefix(
    "clr-namespace:System");

inline bool MatchesClrNamespacePrefix(
    Base::StringView value,
    Base::StringView prefix) noexcept {
    return value.SizeBytes() >= prefix.SizeBytes() &&
        value.Substr(0U, prefix.SizeBytes()) == prefix;
}

inline bool IsExtensionsClrNamespace(Base::StringView value) noexcept {
    if (value.SizeBytes() < 31 ||
        !MatchesClrNamespacePrefix(value, Base::StringView("clr-namespace:"))) {
        return false;
    }
    return MatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:AeroGUIExtensions")) ||
        MatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:Aero.GUI.Extensions")) ||
        MatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:NoesisGUIExtensions")) ||
        MatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:Noesis.GUI.Extensions"));
}

inline Base::StringView CanonicalXamlNamespace(
    Base::StringView value) noexcept {
    if (value.SizeBytes() < 31) {
        return value;
    }
    return value == WpfPresentationNamespace ||
            value == BehaviorsNamespace ||
            value == BlendInteractivityNamespace ||
            IsExtensionsClrNamespace(value)
        ? Meta::AeroNamespaceUri()
        : value;
}

inline bool IsSystemNamespace(
    Base::StringView value) noexcept {
    return value.SizeBytes() >=
            SystemNamespacePrefix.SizeBytes() &&
        value.Substr(0U, SystemNamespacePrefix.SizeBytes()) ==
            SystemNamespacePrefix;
}

inline Base::StringView CanonicalXamlTypeName(
    Base::StringView value) noexcept {
    switch (value.SizeBytes()) {
    case 8:
        if (value == Base::StringView("Geometry")) {
            return Base::StringView("StreamGeometry");
        }
        break;
    case 10:
        if (value == Base::StringView("WavesBrush")) {
            return Base::StringView("WavesShader");
        }
        break;
    case 15:
        if (value == Base::StringView("MonochromeBrush")) {
            return Base::StringView("MonochromeShader");
        }
        break;
    case 18:
        if (value == Base::StringView("ConicGradientBrush")) {
            return Base::StringView("ConicGradientShader");
        }
        break;
    case 21:
        if (value == Base::StringView("VisualStateTransition")) {
            return Base::StringView("VisualTransition");
        }
        break;
    default:
        break;
    }
    return value;
}

inline bool IsAeroExtensionsFacade(
    Base::StringView xamlNamespace,
    Base::StringView ownerName) noexcept {
    if (!IsExtensionsClrNamespace(xamlNamespace)) {
        return false;
    }
    switch (ownerName.SizeBytes()) {
    case 4:
        return ownerName == Base::StringView("Text") ||
               ownerName == Base::StringView("Path");
    case 5:
        return ownerName == Base::StringView("Brush");
    case 7:
        return ownerName == Base::StringView("Element");
    case 8:
        return ownerName == Base::StringView("RichText");
    default:
        return false;
    }
}

struct ParsedXamlMemberName {
    Base::StringView ownerName;
    Base::StringView memberName;
    Base::StringView ownerNamespace;
    bool hasOwner = false;
    bool isAliasedToBlendMode = false;
};

inline Base::Result<ParsedXamlMemberName> ParseXamlMemberName(
    Base::StringView localName,
    Base::StringView namespaceUri,
    Base::StringView targetXamlNamespace) noexcept {
    if (localName.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML member name cannot be empty");
    }
    std::uint32_t dot = localName.SizeBytes();
    for (std::uint32_t index = 0U; index < localName.SizeBytes(); ++index) {
        if (localName[index] != '.') continue;
        if (dot != localName.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML member contains multiple owner separators");
        }
        dot = index;
    }

    ParsedXamlMemberName result;
    if (dot == localName.SizeBytes()) {
        result.hasOwner = false;
        result.memberName = localName;
        return result;
    }

    if (dot == 0U || dot + 1U >= localName.SizeBytes()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML member owner syntax is invalid");
    }

    result.hasOwner = true;
    result.ownerName = localName.Substr(0U, dot);
    result.memberName = localName.Substr(dot + 1U, localName.SizeBytes() - dot - 1U);
    result.ownerNamespace = namespaceUri.Empty()
        ? targetXamlNamespace : namespaceUri;

    if (IsAeroExtensionsFacade(result.ownerNamespace, result.ownerName)) {
        if (result.ownerName == Base::StringView("Element") &&
            result.memberName == Base::StringView("BlendingMode")) {
            result.hasOwner = false;
            result.memberName = Base::StringView("BlendMode");
            result.isAliasedToBlendMode = true;
        }
    }

    return result;
}

inline bool HasPropertyFlag(
    Meta::PropertyFlags flags,
    Meta::PropertyFlags test) noexcept {
    return (static_cast<std::uint32_t>(flags) &
            static_cast<std::uint32_t>(test)) != 0U;
}

inline bool HasEventFlag(
    Meta::EventFlags flags,
    Meta::EventFlags test) noexcept {
    return (static_cast<std::uint32_t>(flags) &
            static_cast<std::uint32_t>(test)) != 0U;
}

inline ResolvedMember MakeResolvedPropertyMember(
    Meta::MemberId id,
    Meta::TypeId ownerType,
    Meta::TypeId valueType,
    Meta::PropertyFlags flags) noexcept {
    ResolvedMember resolved;
    resolved.id = id;
    resolved.kind = Meta::MemberKind::Property;
    resolved.ownerType = ownerType;
    resolved.valueType = valueType;
    resolved.propertyFlags = flags;
    resolved.attached = HasPropertyFlag(flags, Meta::PropertyFlags::Attached);
    return resolved;
}

inline ResolvedMember MakeResolvedEventMember(
    Meta::MemberId id,
    Meta::TypeId ownerType,
    Meta::TypeId eventArgsType,
    Meta::EventFlags flags) noexcept {
    ResolvedMember resolved;
    resolved.id = id;
    resolved.kind = Meta::MemberKind::Event;
    resolved.ownerType = ownerType;
    resolved.valueType = eventArgsType;
    resolved.eventFlags = flags;
    resolved.attached = HasEventFlag(flags, Meta::EventFlags::Attached);
    return resolved;
}


// ============================================================================
// SECTION: XamlPropertyResolution
// ============================================================================

inline const ::Aero::Meta::DependencyProperty* ResolveXamlDependencyProperty(
    const ::Aero::Meta::DependencyPropertyRegistry& properties,
    ::Aero::Meta::TypeId targetType,
    ::Aero::Base::StringView name) noexcept {
    const ::Aero::Meta::DependencyProperty* property = properties.Find(targetType, name);
    if (property != nullptr) return property;

    if (name == ::Aero::Base::StringView("IsFocused") &&
        properties.Types().IsDerivedFrom(targetType, ::Aero::UIElement::StaticTypeId())) {
        return properties.Find(::Aero::UIElement::IsKeyboardFocusedProperty.Handle());
    }

    if (name == ::Aero::Base::StringView("ContextMenu")) {
        property = properties.Find(
            ::Aero::Controls::ContextMenuService::ContextMenuProperty.Handle());
        if (property != nullptr) return property;
    }

    std::uint32_t separator = UINT32_MAX;
    for (std::uint32_t index = 0U; index < name.SizeBytes(); ++index) {
        if (name[index] == '.') separator = index;
    }
    if (separator == UINT32_MAX || separator == 0U || separator + 1U >= name.SizeBytes()) {
        return nullptr;
    }

    ::Aero::Base::StringView ownerName = name.Substr(0U, separator);
    for (std::uint32_t index = 0U; index < ownerName.SizeBytes(); ++index) {
        if (ownerName[index] == ':') {
            ownerName = ownerName.Substr(index + 1U, ownerName.SizeBytes() - index - 1U);
            break;
        }
    }

    const ::Aero::Meta::TypeInfo* owner = properties.Types().FindType(
        ::Aero::Meta::AeroNamespaceUri(), ownerName);
    if (owner == nullptr && ownerName == ::Aero::Base::StringView("Text")) {
        owner = properties.Types().FindType(
            ::Aero::Meta::AeroNamespaceUri(), "TextProperties");
    }
    const ::Aero::Base::StringView member = name.Substr(
        separator + 1U, name.SizeBytes() - separator - 1U);
    if (owner != nullptr) {
        property = properties.Find(owner->Id(), member);
        if (property != nullptr) return property;
    }

    return properties.Find(targetType, member);
}

inline ::Aero::Base::Result<::Aero::Meta::Value> CoerceXamlValueForProperty(
    ::Aero::Meta::TypeId targetPropertyType,
    const ::Aero::Meta::Value& value) noexcept {
    if (value.Type() == targetPropertyType) {
        return value;
    }

    if ((targetPropertyType == ::Aero::Length::StaticTypeId() ||
         targetPropertyType == ::Aero::Meta::TypeOf<::Aero::Length>() ||
         targetPropertyType == ::Aero::Meta::TypeOf<::Aero::GridLength>()) &&
        (value.Kind() == ::Aero::Meta::ValueKind::SignedInteger ||
         value.Kind() == ::Aero::Meta::ValueKind::UnsignedInteger ||
         value.Kind() == ::Aero::Meta::ValueKind::Double)) {
        double numericVal = 0.0;
        if (value.Kind() == ::Aero::Meta::ValueKind::Double) {
            auto num = ::Aero::Meta::ValueCodec<double>::Decode(value);
            if (num) numericVal = num.Value();
        } else if (value.Kind() == ::Aero::Meta::ValueKind::SignedInteger) {
            auto num = ::Aero::Meta::ValueCodec<std::int64_t>::Decode(value);
            if (num) numericVal = static_cast<double>(num.Value());
        } else if (value.Kind() == ::Aero::Meta::ValueKind::UnsignedInteger) {
            auto num = ::Aero::Meta::ValueCodec<std::uint64_t>::Decode(value);
            if (num) numericVal = static_cast<double>(num.Value());
        }
        if (targetPropertyType == ::Aero::Meta::TypeOf<::Aero::GridLength>()) {
            return ::Aero::Meta::ValueCodec<::Aero::GridLength>::Encode(::Aero::GridLength::Pixel(numericVal));
        }
        return ::Aero::Meta::ValueCodec<::Aero::Length>::Encode(::Aero::Length::Pixels(numericVal));
    }

    if (targetPropertyType == ::Aero::Media::Brush::StaticTypeId() &&
        value.Type() == ::Aero::Meta::TypeOf<::Aero::Base::Color>()) {
        auto color = ::Aero::Meta::ValueCodec<::Aero::Base::Color>::Decode(value);
        if (!color) return color.GetStatus();
        auto brush = ::Aero::Media::MakeSolidColorBrush(color.Value());
        if (!brush) {
            return ::Aero::Base::Status::Failure(::Aero::Base::ErrorCode::OutOfMemory, "Failed to create brush");
        }
        return ::Aero::Meta::Value::FromObject(
            ::Aero::Media::Brush::StaticTypeId(),
            ::Aero::Base::Ref<::Aero::Base::Object>(std::move(brush)));
    }

    if (targetPropertyType == ::Aero::Meta::TypeOf<::Aero::Nullable<bool>>()) {
        if (value.IsNullObject()) {
            return ::Aero::Meta::ValueCodec<::Aero::Nullable<bool>>::Encode(::Aero::Nullable<bool>{});
        }
        if (value.Type() == ::Aero::Meta::TypeOf<bool>() ||
            value.Kind() == ::Aero::Meta::ValueKind::Boolean) {
            auto boolVal = ::Aero::Meta::ValueCodec<bool>::Decode(value);
            if (!boolVal) return boolVal.GetStatus();
            return ::Aero::Meta::ValueCodec<::Aero::Nullable<bool>>::Encode(
                ::Aero::Nullable<bool>(boolVal.Value()));
        }
    }

    if (value.IsNullObject() && value.Type() != targetPropertyType) {
        return ::Aero::Meta::Value::NullObject(targetPropertyType);
    }

    return value;
}

inline ::Aero::Base::Result<::Aero::Meta::Value> CoerceXamlValueForProperty(
    const ::Aero::Meta::DependencyProperty& property,
    const ::Aero::Meta::Value& value,
    const ::Aero::Meta::TypeRegistry& types) noexcept {
    if (property.ValueType() == ::Aero::Meta::TypeOf<::Aero::Base::Color>() &&
        value.Kind() == ::Aero::Meta::ValueKind::Object &&
        types.IsDerivedFrom(value.Type(), ::Aero::Media::Brush::StaticTypeId())) {
        ::Aero::Base::Result<::Aero::Base::Ref<::Aero::Media::Brush>> brush =
            ::Aero::Meta::ValueCodec<::Aero::Base::Ref<::Aero::Media::Brush>>::Decode(value);
        if (!brush) return brush.GetStatus();
        if (brush.Value() == nullptr ||
            brush->RuntimeType() != ::Aero::Media::SolidColorBrush::StaticTypeId()) {
            return ::Aero::Base::Status::Failure(
                ::Aero::Base::ErrorCode::InvalidArgument,
                "Color-backed text property requires SolidColorBrush");
        }
        return ::Aero::Meta::ValueCodec<::Aero::Base::Color>::Encode(
            static_cast<const ::Aero::Media::SolidColorBrush&>(*brush.Value()).GetColor());
    }

    return CoerceXamlValueForProperty(property.ValueType(), value);
}


// ============================================================================
// SECTION: MarkupExtension Providers & Context
// ============================================================================

struct BindingExtensionOptions {
    BindingExtensionOptions() noexcept = default;
    BindingExtensionOptions(
        Aero::BindingEngine* bindingManager,
        Meta::DependencyPropertyHandle dataContext) noexcept
        : bindings(bindingManager),
          dataContextProperty(dataContext) {}

    Aero::BindingEngine* bindings = nullptr;
    Meta::DependencyPropertyHandle dataContextProperty;
};

// Registers a {Binding ElementName=..., Path=..., Mode=...} provider. Explicit
// ElementName wins over DataContext. Paths are compiled to immutable metadata
// plans; DataContext paths are resolved after tree attachment and recompiled
// only when the concrete source type changes.
class BindingExtension {
public:
    explicit BindingExtension(
        const BindingExtensionOptions& options) noexcept;

    BindingExtension(const BindingExtension&) = delete;
    BindingExtension& operator=(const BindingExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId bindingExtensionType) noexcept;
    void SetDataContextProperty(
        Meta::DependencyPropertyHandle property) noexcept {
        options_.dataContextProperty = property;
    }

private:
    BindingExtensionOptions options_;

    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

class DynamicResource {
public:
    static Base::Result<void> Attach(
        Meta::EffectiveValueEngine& effectiveValues,
        Aero::ResourceDictionary& resources,
        ::Aero::DependencyObject& target,
        Meta::DependencyPropertyHandle property,
        Base::StringView key) noexcept;
    static Base::Result<void> Attach(
        Meta::EffectiveValueEngine& effectiveValues,
        Base::Span<
            const Aero::ResourceDictionary* const> resourceChain,
        Aero::ResourceDictionary* fallbackResources,
        ::Aero::DependencyObject& target,
        Meta::DependencyPropertyHandle property,
        Base::StringView key) noexcept;
    static Base::Result<Meta::PropertyExpression> CreateExpression(
        Meta::EffectiveValueEngine& effectiveValues,
        Base::Span<
            const Aero::ResourceDictionary* const> resourceChain,
        Aero::ResourceDictionary* fallbackResources,
        ::Aero::DependencyObject& target,
        Meta::DependencyPropertyHandle property,
        Base::StringView key) noexcept;
};

struct DynamicResourceExtensionOptions {
    DynamicResourceExtensionOptions() noexcept = default;
    DynamicResourceExtensionOptions(
        Meta::EffectiveValueEngine* effectiveValueEngine,
        Aero::ResourceDictionary* resourceDictionary) noexcept
        : effectiveValues(effectiveValueEngine),
          resources(resourceDictionary) {}

    Meta::EffectiveValueEngine* effectiveValues = nullptr;
    Aero::ResourceDictionary* resources = nullptr;
};

class DynamicResourceExtension {
public:
    explicit DynamicResourceExtension(
        const DynamicResourceExtensionOptions& options) noexcept;

    DynamicResourceExtension(const DynamicResourceExtension&) = delete;
    DynamicResourceExtension& operator=(
        const DynamicResourceExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId dynamicResourceExtensionType) noexcept;

private:
    DynamicResourceExtensionOptions options_;

    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

// Mirrors WPF's {StaticResource key}/StaticResourceExtension. Unlike
// DynamicResourceExtension it resolves the resource dictionary key exactly once
// while the XAML tree is authored and returns the value directly; there is no
// expression and no change tracking once the tree is live.
class StaticResourceExtension {
public:
    StaticResourceExtension() noexcept = default;

    StaticResourceExtension(const StaticResourceExtension&) = delete;
    StaticResourceExtension& operator=(
        const StaticResourceExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId staticResourceExtensionType) noexcept;

private:
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

class TypeExtension {
public:
    TypeExtension() noexcept = default;

    TypeExtension(const TypeExtension&) = delete;
    TypeExtension& operator=(const TypeExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId markupExtensionType) noexcept;

private:
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

// Compatibility provider for AeroGUIExtensions.Loc. Its attached Source is
// registered by the markup schema; the provider supplies values to ordinary
// property attributes such as {aero:Loc TitleLabel}.
class LocExtension {
public:
    LocExtension() noexcept = default;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId markupExtensionType) noexcept;

    // Called by the inheritable aero:Loc.Source compatibility property.  The
    // implementation swaps the source XAML dictionary for all Loc values
    // authored in the same loaded visual tree.
    static void OnSourceChanged(
        ::Aero::DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs& args) noexcept;

private:
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

// Records a WPF-style one-way property mapping while a ControlTemplate
// prototype is authored. The prototype keeps no live expression; its immutable
// runtime plan applies the mapping to every instantiated template tree.
class TemplateBindingExtension {
public:
    TemplateBindingExtension() noexcept = default;

    TemplateBindingExtension(
        const TemplateBindingExtension&) = delete;
    TemplateBindingExtension& operator=(
        const TemplateBindingExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId markupExtensionType) noexcept;

private:
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

// Implements the WPF-compatible {x:Static Type.Member} form for registered
// enum values. The returned Meta::Value retains the enum's concrete metadata
// type so AnyValue members (for example discrete object key frames) can defer
// assignment until their target dependency property is known.
class StaticExtension {
public:
    StaticExtension() noexcept = default;

    StaticExtension(const StaticExtension&) = delete;
    StaticExtension& operator=(const StaticExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId markupExtensionType) noexcept;

private:
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};


Base::Result<long double> ReadConstantBindingNumber(
    const Meta::Value& value) noexcept;
Base::Result<Meta::Value> ConvertConstantBindingValue(
    const Meta::Value& value,
    Meta::TypeId targetType) noexcept;
Base::Result<ProvidedValue> CreateMultiBindingValue(
    Data::MultiBinding& binding,
    const ExtensionServices& services) noexcept;
Base::Result<void> CaptureControlTemplateChildName(
    Controls::ControlTemplate& controlTemplate,
    const Aero::NameScope* nameScope,
    Base::Object& target,
    Base::String& storage) noexcept;
Base::Result<Meta::PropertyValue> ConvertDynamicResourceValue(
    const Meta::PropertyValue& value,
    const Meta::DependencyProperty& property) noexcept;
Base::Result<void> RegisterBuiltinMarkupExtensions(
    Schema& schema) noexcept;

inline MarkupExtensionContext MakeMarkupExtensionContext(
    const ExtensionServices& services) noexcept {
    auto typeResolver = [](void* ctx, Base::StringView name) noexcept -> Base::Result<const Meta::TypeInfo*> {
        const auto* s = static_cast<const ExtensionServices*>(ctx);
        if (s == nullptr || s->schema == nullptr || name.Empty()) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState, "ExtensionServices is null or name is empty");
        }
        std::uint32_t colon = name.SizeBytes();
        for (std::uint32_t index = 0U; index < name.SizeBytes(); ++index) {
            if (name[index] != ':') continue;
            if (colon != name.SizeBytes()) {
                return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Type reference contains multiple namespace prefixes");
            }
            colon = index;
        }
        Base::StringView prefix;
        Base::StringView localName = name;
        if (colon != name.SizeBytes()) {
            if (colon == 0U || colon + 1U >= name.SizeBytes()) {
                return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Type reference namespace prefix is malformed");
            }
            prefix = name.Substr(0U, colon);
            localName = name.Substr(colon + 1U, name.SizeBytes() - colon - 1U);
        }
        Base::Result<Base::StringView> uri = s->namespaces.Lookup(prefix);
        if (!uri) return uri.GetStatus();
        return s->schema->ResolveType(uri.Value(), localName);
    };
    return MarkupExtensionContext(
        services.targetObject,
        services.targetMember,
        services.baseUri,
        const_cast<ExtensionServices*>(&services),
        typeResolver);
}


} // namespace Aero::Markup
