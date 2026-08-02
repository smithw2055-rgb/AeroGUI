#pragma once
#include "MarkupInternal.hpp"
// Consolidated private Markup writing, facet and template contract.

// ===== Extensions contract =====
#include "gui/ElementInternal.hpp"

// Canonical markup-extension API.

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Value.hpp>
#include <Aero/Data.hpp>
#include <Aero/DependencyProperty.hpp>
#include "gui/PropertyInternal.hpp"
#include <Aero/Markup.hpp>

#include <cstdint>


namespace Aero::Markup {

class DeferredContentPlan;
class Schema;
struct VisualContentPlan;

enum class ProvidedValueKind : std::uint8_t {
    Value = 0U,
    Handled,
    Expression,
    Deferred
};

using ProvidedCommitCallback =
    Base::Result<std::uint64_t> (*)(void* context) noexcept;
using ProvidedRollbackCallback = void (*)(
    void* context,
    std::uint64_t token) noexcept;
using ProvidedCleanupCallback = void (*)(void* context) noexcept;

struct ProvidedValue {
    ProvidedValueKind kind = ProvidedValueKind::Value;
    Meta::Value value;
    Meta::EffectiveValueEngine* effectiveValues = nullptr;
    Meta::PropertyExpression expression;
    void* rollbackContext = nullptr;
    std::uint64_t rollbackToken = 0U;
    ProvidedRollbackCallback rollback = nullptr;
    ProvidedCommitCallback commit = nullptr;
    ProvidedCleanupCallback cleanup = nullptr;

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
        ProvidedCleanupCallback cleanupCallback) noexcept {
        ProvidedValue result;
        result.kind = ProvidedValueKind::Deferred;
        result.rollbackContext = context;
        result.commit = commitCallback;
        result.rollback = rollbackCallback;
        result.cleanup = cleanupCallback;
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
        commit = nullptr;
        cleanup = nullptr;
    }
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
    Aero::Internal::BindingEngine* bindings = nullptr;
    Aero::ResourceDictionary* fallbackResources = nullptr;
    Base::Span<const Aero::ResourceDictionary* const>
        ambientResourceChain;
    VisualContentPlan* visualContent = nullptr;
    Base::Object* deferredContentOwner = nullptr;
    DeferredContentPlan* deferredContent = nullptr;
};

struct BindingExtensionOptions {
    BindingExtensionOptions() noexcept = default;
    BindingExtensionOptions(
        Aero::Internal::BindingEngine* bindingManager,
        Meta::DependencyPropertyHandle dataContext) noexcept
        : bindings(bindingManager),
          dataContextProperty(dataContext) {}

    Aero::Internal::BindingEngine* bindings = nullptr;
    Meta::DependencyPropertyHandle dataContextProperty;
};

// Registers a {Binding ElementName=..., Path=..., Mode=...} provider. Explicit
// ElementName wins over DataContext. Paths are compiled to immutable metadata
// plans; DataContext paths are resolved after tree attachment and recompiled
// only when the concrete source type changes.
class AERO_API BindingExtension {
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

class AERO_API DynamicResource {
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

class AERO_API DynamicResourceExtension {
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

class AERO_API TypeExtension {
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

// Records a WPF-style one-way property mapping while a ControlTemplate
// prototype is authored. The prototype keeps no live expression; its immutable
// runtime plan applies the mapping to every instantiated template tree.
class AERO_API TemplateBindingExtension {
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
class AERO_API StaticExtension {
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

} // namespace Aero::Markup


// ===== DeferredContent contract =====
#include "gui/ElementInternal.hpp"

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include "gui/MetadataInternal.hpp"
#include <Aero/Data.hpp>


namespace Aero::Markup {

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
    ::Aero::DependencyObject* target = nullptr;
    Aero::Internal::BindingEngine* manager = nullptr;
    ::Aero::Meta::Registry* metadata = nullptr;
    Meta::DependencyPropertyHandle targetProperty;
    Meta::DependencyPropertyHandle dataContextProperty;
    Base::String path;
    Base::String stringFormat;
    bool bindsToSource = false;
    Data::BindingMode mode =
        Data::BindingMode::OneWay;
    Meta::UpdateSourceTrigger updateSourceTrigger =
        Meta::UpdateSourceTrigger::PropertyChanged;
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
        ::Aero::DependencyObject& target,
        Aero::Internal::BindingEngine& manager,
        ::Aero::Meta::Registry& metadata,
        Meta::DependencyPropertyHandle targetProperty,
        Meta::DependencyPropertyHandle dataContextProperty,
        Base::StringView path,
        Base::StringView stringFormat,
        Data::BindingMode mode,
        Meta::UpdateSourceTrigger updateSourceTrigger,
        bool bindsToSource) noexcept;
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

} // namespace Aero::Markup


// ===== ObjectWriter contract =====


// Private object materializer used by Loader.

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Diagnostics.hpp>

#include <Aero/Markup.hpp>


#include <cstdint>

namespace Aero {
class UIElement;
class Visual;
}

namespace Aero::Markup {

class CompiledDocument;
class NodeReader;
class NodeCursor;

namespace XamlObjectWriterDiagnosticCodes {
inline constexpr ::Aero::Diagnostics::DiagnosticCode UnknownType =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 201U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode TypeNotConstructible =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 202U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode UnknownMember =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 203U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode InvalidAttachedMember =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 204U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode UnsupportedMember =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 205U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode InvalidValue =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 206U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode InvalidWriterState =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 207U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode MissingContentProperty =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 208U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode DuplicateMemberValue =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 209U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode InitializationFailed =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 210U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode UnexpectedText =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 211U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode TypeMismatch =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 212U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode FactoryFailed =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 213U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode MissingMemberValue =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 214U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode MultipleRootObjects =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 215U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode InvalidDirective =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 216U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode DuplicateName =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 217U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode DuplicateResourceKey =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 218U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode StaticResourceNotFound =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 219U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode MissingResourceScope =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 220U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode NullNotAllowed =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 221U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode InvalidMarkupExtension =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 222U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode NamespaceState =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 223U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode NameRegistrationFailed =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 224U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode ResourceRegistrationFailed =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 225U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode UnknownMarkupExtension =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 226U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode MarkupExtensionFailed =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 227U);
} // namespace XamlObjectWriterDiagnosticCodes

// Immutable writer configuration. Every call creates a fresh one-shot
// private writer state so transaction stacks and document scopes never
// survive a load operation.
class AERO_API ObjectWriter {
public:
    explicit ObjectWriter(
        Schema& schema,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;

    ObjectWriter(const ObjectWriter&) = delete;
    ObjectWriter& operator=(const ObjectWriter&) = delete;

    Base::Result<LoaderResult> LoadDocument(
        NodeReader& reader) noexcept;
    Base::Result<LoaderResult> LoadDocument(
        const CompiledDocument& document) noexcept;

    Markup::Schema& GetSchema() const noexcept {
        return *schema_;
    }
    Diagnostics::IDiagnosticSink* Diagnostics() const noexcept {
        return diagnostics_;
    }

private:
    friend class ObjectBuilder;

    static Base::Result<Aero::Visual*> ResolveVisual(
        Markup::Schema& schema,
        Base::Object& object,
        Meta::TypeId type) noexcept;
    static Base::Result<Aero::UIElement*> ResolveUIElement(
        Markup::Schema& schema,
        Base::Object& object,
        Meta::TypeId type) noexcept;
    static Base::Result<void> StageContent(
        Markup::Schema& schema,
        Base::Object& object,
        const Meta::Value& value,
        const ExtensionServices& services) noexcept;
    Markup::Schema* schema_ = nullptr;
    Diagnostics::IDiagnosticSink* diagnostics_ = nullptr;
};

} // namespace Aero::Markup


// ===== ObjectBuilder contract =====







namespace Aero::Markup {

class ObjectBuilder {
public:
    explicit ObjectBuilder(
        Schema& schema,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    explicit ObjectBuilder(
        const ObjectWriter& writer) noexcept
        : ObjectBuilder(
              writer.GetSchema(),
              writer.Diagnostics()) {}
    ~ObjectBuilder() noexcept;

    ObjectBuilder(const ObjectBuilder&) = delete;
    ObjectBuilder& operator=(const ObjectBuilder&) = delete;

    Base::Result<LoaderResult> Load(
        NodeReader& reader) noexcept;
    Base::Result<LoaderResult> Load(
        NodeReader& reader,
        const LoadState& context) noexcept;
    Base::Result<LoaderResult> Load(
        const CompiledDocument& document) noexcept;
    Base::Result<LoaderResult> Load(
        const CompiledDocument& document,
        const LoadState& context) noexcept;

    bool IsConsumed() const noexcept {
        return consumed_;
    }

    Schema& GetSchema() const noexcept {
        return *schema_;
    }

private:
    static constexpr std::uint32_t InvalidIndex = UINT32_MAX;

    enum class FrameKind : std::uint8_t {
        Object = 0U,
        Member,
        Directive,
        NullObject,
        ValueObject,
        ValueMember
    };

    enum class DirectiveKind : std::uint8_t {
        None = 0U,
        Name,
        Key,
        Class
    };

    enum class MarkupValueKind : std::uint8_t {
        Literal = 0U,
        EscapedLiteral,
        Null,
        StaticResource,
        Extension,
        Invalid
    };

    struct Frame {
        FrameKind kind = FrameKind::Object;
        DirectiveKind directive = DirectiveKind::None;
        std::uint32_t objectIndex = InvalidIndex;
        std::uint32_t targetObjectIndex = InvalidIndex;
        std::uint32_t namespaceBindingStart = InvalidIndex;
        std::uint32_t nameScopeIndex = InvalidIndex;
        std::uint32_t resourceScopeIndex = InvalidIndex;
        ResolvedMember member;
        ::Aero::Diagnostics::SourceSpan source;
        std::uint32_t valuesWritten = 0U;
        bool propertyElement = false;
        bool deferredStaticResource = false;
    };

    struct CreatedObjectRecord {
        CreatedObjectRecord() noexcept = default;

        CreatedObjectRecord(CreatedObjectRecord&&) noexcept = default;
        CreatedObjectRecord& operator=(CreatedObjectRecord&&) noexcept = default;

        CreatedObjectRecord(const CreatedObjectRecord&) = delete;
        CreatedObjectRecord& operator=(const CreatedObjectRecord&) = delete;

        Base::Ref<Base::Object> object;
        Meta::Value value;
        Meta::TypeId type = Meta::InvalidTypeId;
        Base::String name;
        Base::String key;
        bool beginCalled = false;
        bool endCalled = false;
        bool nameRegistered = false;
        bool resourceRegistered = false;
        bool valueElement = false;
    };

    struct AssignmentRecord {
        std::uint32_t objectIndex = InvalidIndex;
        Meta::MemberId member = Meta::InvalidMemberId;
        std::uint32_t count = 0U;
    };

    struct DeferredStaticResourceRecord {
        std::uint32_t targetObjectIndex = InvalidIndex;
        ResolvedMember member;
        Base::String key;
        ::Aero::Diagnostics::SourceSpan source;
    };


    struct NameScopeRecord {
        NameScopeRecord() noexcept = default;

        NameScopeRecord(NameScopeRecord&&) noexcept = default;
        NameScopeRecord& operator=(NameScopeRecord&&) noexcept = default;

        NameScopeRecord(const NameScopeRecord&) = delete;
        NameScopeRecord& operator=(const NameScopeRecord&) = delete;

        std::uint32_t ownerObjectIndex = InvalidIndex;
        Aero::NameScope names;
    };

    struct ResourceScopeRecord {
        ResourceScopeRecord() noexcept = default;

        ResourceScopeRecord(ResourceScopeRecord&&) noexcept = default;
        ResourceScopeRecord& operator=(ResourceScopeRecord&&) noexcept = default;

        ResourceScopeRecord(const ResourceScopeRecord&) = delete;
        ResourceScopeRecord& operator=(const ResourceScopeRecord&) = delete;

        std::uint32_t ownerObjectIndex = InvalidIndex;
        Aero::ResourceDictionary resources;
        const Aero::ResourceDictionary* external = nullptr;
    };

    struct NamespaceBindingRecord {
        NamespaceBindingRecord() noexcept = default;

        NamespaceBindingRecord(NamespaceBindingRecord&&) noexcept = default;
        NamespaceBindingRecord& operator=(NamespaceBindingRecord&&) noexcept = default;

        NamespaceBindingRecord(const NamespaceBindingRecord&) = delete;
        NamespaceBindingRecord& operator=(const NamespaceBindingRecord&) = delete;

        Base::String prefix;
        Base::String uri;
    };

    struct PendingNamespaceRecord {
        PendingNamespaceRecord() noexcept = default;

        PendingNamespaceRecord(PendingNamespaceRecord&&) noexcept = default;
        PendingNamespaceRecord& operator=(PendingNamespaceRecord&&) noexcept = default;

        PendingNamespaceRecord(const PendingNamespaceRecord&) = delete;
        PendingNamespaceRecord& operator=(const PendingNamespaceRecord&) = delete;

        Base::String prefix;
        Base::String uri;
        ::Aero::Diagnostics::SourceSpan source;
    };

    Markup::Schema* schema_ = nullptr;
    Diagnostics::IDiagnosticSink* diagnostics_ = nullptr;
    Base::Vector<Frame> frames_;
    Base::Vector<CreatedObjectRecord> created_;
    Base::Vector<AssignmentRecord> assignments_;
    Base::Vector<DeferredStaticResourceRecord>
        deferredStaticResources_;
    Base::Vector<CommittedEffect> extensionEffects_;
    Base::Vector<NameScopeRecord> nameScopes_;
    Base::Vector<ResourceScopeRecord> resourceScopes_;
    Base::Vector<const Aero::ResourceDictionary*>
        serviceResourceChain_;
    Base::Vector<NamespaceBindingRecord> namespaceBindings_;
    Base::Vector<PendingNamespaceRecord> pendingNamespaces_;
    Aero::NameScope committedNames_;
    Aero::ResourceDictionary committedResources_;
    VisualContentPlan resultVisualContent_;
    DeferredContentPlan deferredContent_;
    Base::Ref<Base::Object> root_;
    std::uint32_t rootObjectIndex_ = InvalidIndex;
    std::uint32_t documentNameScopeIndex_ = InvalidIndex;
    std::uint32_t documentResourceScopeIndex_ = InvalidIndex;
    bool loading_ = false;
    bool ended_ = false;
    bool consumed_ = false;
    bool hasDeferredStaticResources_ = false;
    const LoadState* loadContext_ = nullptr;

    Base::Result<LoaderResult> CompleteLoad(
        Base::Result<Base::Ref<Base::Object>> loaded) noexcept;
    Base::Result<void> ResolveDeferredStaticResources() noexcept;
    Base::Result<Base::Ref<Base::Object>> LoadReaderCore(
        NodeReader& reader) noexcept;
    Base::Result<Base::Ref<Base::Object>> LoadCompiledCore(
        const CompiledDocument& document) noexcept;
    Base::Result<Base::Ref<Base::Object>> LoadCursorCore(
        NodeCursor& cursor) noexcept;
    Base::Result<Base::Ref<Base::Object>> CreateObject(
        Meta::TypeId type) const noexcept;

    Base::Result<void> ProcessNode(
        const Node& node) noexcept;
    Base::Result<void> QueueNamespaceDeclaration(
        const Node& node) noexcept;
    Base::Result<void> StartObject(
        const Node& node) noexcept;
    Base::Result<void> StartValueObject(
        const Node& node,
        std::uint32_t bindingStart,
        Meta::TypeId type) noexcept;
    Base::Result<void> StartNullObject(
        const Node& node,
        std::uint32_t bindingStart) noexcept;
    Base::Result<void> EndObject(
        const Node& node) noexcept;
    Base::Result<void> StartMember(
        const Node& node) noexcept;
    Base::Result<void> StartDirective(
        const Node& node,
        DirectiveKind directive,
        std::uint32_t targetObjectIndex) noexcept;
    Base::Result<void> EndMember(
        const Node& node) noexcept;
    Base::Result<void> WriteText(
        const Node& node) noexcept;
    Base::Result<void> WriteDirectiveText(
        Frame& frame,
        const Node& node) noexcept;

    Base::Result<void> StartPropertyElement(
        const Node& node,
        std::uint32_t targetFrameIndex,
        std::uint32_t bindingStart) noexcept;
    Base::Result<void> CompleteObject(
        const Node& node) noexcept;
    Base::Result<void> CompleteValueObject(
        const Node& node) noexcept;
    Base::Result<void> CompleteNullObject(
        const Node& node) noexcept;
    Base::Result<void> WriteValueToParent(
        Meta::Value&& value,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
    Base::Result<void> WriteObjectToParent(
        std::uint32_t objectIndex,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
    Base::Result<void> WriteObjectToContent(
        std::uint32_t parentObjectIndex,
        std::uint32_t childObjectIndex,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
    Base::Result<void> WriteNullToParent(
        ::Aero::Diagnostics::SourceSpan source) noexcept;
    Base::Result<void> WriteValueToMember(
        Frame& memberFrame,
        Meta::Value&& value,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
    Base::Result<void> WriteProvidedValueToMember(
        Frame& memberFrame,
        ProvidedValue&& value,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
    Base::Result<void> WriteProvidedValue(
        std::uint32_t targetObjectIndex,
        const ResolvedMember& member,
        ProvidedValue&& value,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
    Base::Result<void> WriteValue(
        std::uint32_t targetObjectIndex,
        const ResolvedMember& member,
        Meta::Value&& value,
        ::Aero::Diagnostics::SourceSpan source) noexcept;

    Base::Result<void> RegisterObjectName(
        std::uint32_t objectIndex,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
    Base::Result<bool> RegisterObjectResource(
        std::uint32_t objectIndex,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
    Base::Result<Aero::ResourceValue> LookupResource(
        Base::StringView key) const noexcept;
    Base::Result<void> CreateScopesForObject(
        std::uint32_t objectIndex,
        Frame& frame,
        ::Aero::Diagnostics::SourceSpan source) noexcept;

    Base::Result<void> ActivatePendingNamespaces(
        std::uint32_t& bindingStart) noexcept;
    void PopNamespaceBindings(std::uint32_t bindingStart) noexcept;
    Base::Result<Base::StringView> LookupNamespace(
        Base::StringView prefix) const noexcept;

    ExtensionServices BuildExtensionServices(
        std::uint32_t targetObjectIndex,
        const ResolvedMember& member,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
    const Aero::NameScope*
    FindActiveNameScope() const noexcept;
    Base::Object* FindDeferredContentOwner() const noexcept;
    std::uint32_t FindNameScopeIndexForObject(
        std::uint32_t objectIndex) const noexcept;
    std::uint32_t FindResourceScopeIndexForParent() const noexcept;
    std::uint32_t FindObjectFrameIndex(
        std::uint32_t objectIndex) const noexcept;

    MarkupValueKind ParseMarkupValue(
        Base::StringView text,
        Base::StringView& extensionName,
        Base::StringView& argument) const noexcept;
    Base::Result<ProvidedValue> EvaluateMarkupExtension(
        std::uint32_t targetObjectIndex,
        const ResolvedMember& member,
        Base::StringView extensionName,
        Base::StringView arguments,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
    bool IsXamlDirective(
        const QualifiedName& name,
        Base::StringView localName) const noexcept;
    bool IsXamlNullObject(
        const QualifiedName& name) const noexcept;
    bool HasPropertyElementSyntax(
        const QualifiedName& name) const noexcept;
    bool IsWhitespaceOnly(Base::StringView value) const noexcept;
    AssignmentRecord* FindAssignment(
        std::uint32_t objectIndex,
        Meta::MemberId member) noexcept;

    void CommitDocumentScopes() noexcept;
    void AbortTransaction() noexcept;
    void ClearTransaction() noexcept;
    Base::Status Failure(
        Base::Status status,
        ::Aero::Diagnostics::DiagnosticCode diagnostic,
        Base::StringView message,
        ::Aero::Diagnostics::SourceSpan source) noexcept;

    static Base::Result<Base::StringView>
    NamespaceLookupCallback(
        void* context,
        Base::StringView prefix) noexcept;
    static Base::Result<Aero::ResourceValue>
    ResourceLookupCallback(
        void* context,
        Base::StringView key) noexcept;
};


} // namespace Aero::Markup


// ===== XamlFacets contract =====
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Value.hpp>
#include "gui/PropertyInternal.hpp"


#include <Aero/Version.hpp>

#include <cstdint>

namespace Aero::Internal {

using namespace ::Aero::Markup;

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

} // namespace Aero::Internal

// ===== SchemaInternal contract =====


namespace Aero::Markup::Detail {
using ::Aero::Internal::XamlFacets;
using ::Aero::Internal::XamlMarkupExtensionFacet;
using ::Aero::Internal::XamlTypeFacet;
using ::Aero::Internal::XamlLifecycleFacet;
using ::Aero::Internal::XamlNameScopeFacet;
using ::Aero::Internal::XamlResourceScopeFacet;
using ::Aero::Internal::XamlDeferredContentFacet;
using ::Aero::Internal::XamlImplicitResourceKeyFacet;
using ::Aero::Internal::XamlPropertyTargetFacet;
}


namespace Aero::Markup {

struct Schema::Impl {
    Detail::XamlFacets facets;
};

namespace Detail {

using namespace ::Aero::Internal;

class SchemaPrivate {
public:
    // Compatibility aggregate input. XamlFacets projects it atomically and
    // retains only narrow facet records.
    static Base::Result<void> AddType(
        Schema& schema,
        const XamlTypeFacet& registration) noexcept {
        return schema.impl_->facets.Add(
            registration, schema.Types());
    }

    static Base::Result<void> AddLifecycle(
        Schema& schema,
        const XamlLifecycleFacet& registration) noexcept {
        return schema.impl_->facets.Add(
            registration, schema.Types());
    }

    static Base::Result<void> AddNameScope(
        Schema& schema,
        const XamlNameScopeFacet& registration) noexcept {
        return schema.impl_->facets.Add(
            registration, schema.Types());
    }

    static Base::Result<void> AddResourceScope(
        Schema& schema,
        const XamlResourceScopeFacet& registration) noexcept {
        return schema.impl_->facets.Add(
            registration, schema.Types());
    }

    static Base::Result<void> AddDeferredContent(
        Schema& schema,
        const XamlDeferredContentFacet& registration) noexcept {
        return schema.impl_->facets.Add(
            registration, schema.Types());
    }

    static Base::Result<void> AddImplicitResourceKey(
        Schema& schema,
        const XamlImplicitResourceKeyFacet& registration) noexcept {
        return schema.impl_->facets.Add(
            registration, schema.Types());
    }

    static Base::Result<void> AddPropertyTarget(
        Schema& schema,
        const XamlPropertyTargetFacet& registration) noexcept {
        return schema.impl_->facets.Add(
            registration, schema.Types());
    }

    static Base::Result<void> AddMarkupExtension(
        Schema& schema,
        const XamlMarkupExtensionFacet& registration) noexcept {
        return schema.impl_->facets.Add(
            registration, schema.Types());
    }

    static Base::Result<Meta::Value> ConvertText(
        const Schema& schema,
        Meta::TypeId type,
        Base::StringView text,
        const ExtensionServices* services = nullptr) noexcept {
        return schema.ConvertText(type, text, services);
    }

    static Base::Result<::Aero::DependencyObject*>
    ResolvePropertyTarget(
        const Schema& schema,
        Base::Object& object) noexcept {
        return schema.ResolvePropertyTarget(object);
    }

    static ::Aero::Meta::Registry* Metadata(
        const Schema& schema) noexcept {
        return schema.Metadata();
    }

    static Base::Result<const Meta::TypeInfo*> ResolveType(
        const Schema& schema,
        Base::StringView xamlNamespace,
        Base::StringView localName) noexcept {
        return schema.ResolveType(xamlNamespace, localName);
    }
};

} // namespace Detail

} // namespace Aero::Markup


// ===== TemplateCompiler contract =====
#include "gui/ElementInternal.hpp"

// Private template compiler used by ObjectWriter finalization.



#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Styling.hpp>
#include "../controls/TemplateInternals.hpp"
#include <Aero/Controls/Panels.hpp>
#include "gui/MetadataInternal.hpp"

#include <Aero/Animation.hpp>

#include <cstdint>


namespace Aero::Internal {

using namespace ::Aero::Markup;

class XamlVisualStateObject : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        XamlVisualStateObject,
        Base::Object,
        "urn:aero",
        "VisualState")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::Result<void> SetName(Base::StringView value) noexcept {
        return name_.Assign(value);
    }
    Base::StringView Name() const noexcept {
        return name_.View();
    }
    Base::Result<void> AddSetter(
        const Base::Ref<Base::Object>& value) noexcept {
        return setters_.PushBack(value);
    }
    void ClearSetters() noexcept {
        setters_.Clear();
    }
    Base::Span<const Base::Ref<Base::Object>>
    Setters() const noexcept {
        return {setters_.Data(), setters_.Size()};
    }
    Base::Result<void> SetStoryboard(
        Base::Ref<Media::Animation::Storyboard> value) noexcept {
        if (storyboard_ && value) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "VisualState accepts only one Storyboard");
        }
        storyboard_ = std::move(value);
        return {};
    }
    const Base::Ref<Media::Animation::Storyboard>&
    StoryboardValue() const noexcept {
        return storyboard_;
    }

private:
    Base::String name_;
    Base::Vector<Base::Ref<Base::Object>> setters_;
    Base::Ref<Media::Animation::Storyboard> storyboard_;
};

class XamlVisualTransitionObject
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        XamlVisualTransitionObject,
        Base::Object,
        "urn:aero",
        "VisualTransition")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::StringView From() const noexcept {
        return from_.View();
    }
    Base::StringView To() const noexcept {
        return to_.View();
    }
    Base::StringView GeneratedDuration() const noexcept {
        return generatedDuration_.View();
    }
    Base::Result<void> SetFrom(
        Base::StringView value) noexcept {
        return from_.Assign(value);
    }
    Base::Result<void> SetTo(
        Base::StringView value) noexcept {
        return to_.Assign(value);
    }
    Base::Result<void> SetGeneratedDuration(
        Base::StringView value) noexcept {
        Media::Animation::Storyboard validator;
        validator.SetDuration(value);
        return generatedDuration_.Assign(value);
    }
    Base::Ref<Media::Animation::EasingFunctionBase>
    GeneratedEasingFunction() const noexcept {
        return generatedEasingFunction_;
    }
    Base::Result<void> SetGeneratedEasingFunction(
        Base::Ref<Media::Animation::EasingFunctionBase> value) noexcept {
        generatedEasingFunction_ = std::move(value);
        return {};
    }
    Base::Result<void> SetStoryboard(
        Base::Ref<Media::Animation::Storyboard> value) noexcept {
        if (storyboard_ && value) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "VisualTransition accepts only one Storyboard");
        }
        storyboard_ = std::move(value);
        return {};
    }
    const Base::Ref<Media::Animation::Storyboard>&
    StoryboardValue() const noexcept {
        return storyboard_;
    }

private:
    Base::String from_;
    Base::String to_;
    Base::String generatedDuration_;
    Base::Ref<Media::Animation::EasingFunctionBase>
        generatedEasingFunction_;
    Base::Ref<Media::Animation::Storyboard> storyboard_;
};

class XamlVisualStateGroupObject
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        XamlVisualStateGroupObject,
        Base::Object,
        "urn:aero",
        "VisualStateGroup")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::Result<void> SetName(Base::StringView value) noexcept {
        return name_.Assign(value);
    }
    Base::StringView Name() const noexcept {
        return name_.View();
    }
    Base::Result<void> AddState(
        const Base::Ref<Base::Object>& value) noexcept {
        return states_.PushBack(value);
    }
    void ClearStates() noexcept {
        states_.Clear();
    }
    Base::Span<const Base::Ref<Base::Object>>
    States() const noexcept {
        return {states_.Data(), states_.Size()};
    }
    Base::Result<void> AddTransition(
        const Base::Ref<Base::Object>& value) noexcept {
        return transitions_.PushBack(value);
    }
    void ClearTransitions() noexcept {
        transitions_.Clear();
    }
    Base::Span<const Base::Ref<Base::Object>>
    Transitions() const noexcept {
        return {
            transitions_.Data(),
            transitions_.Size()};
    }

private:
    Base::String name_;
    Base::Vector<Base::Ref<Base::Object>> states_;
    Base::Vector<Base::Ref<Base::Object>> transitions_;
};

// WPF permits VisualStateManager.VisualStateGroups on the root element of a
// ControlTemplate rather than only as a ControlTemplate member. Keep that
// authored collection on the root dependency object until the template is
// compiled into its immutable state program.
class XamlVisualStates
    : public Base::Object {
    AERO_DECLARE_TYPE(XamlVisualStates, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Result<void> Add(
        const Base::Ref<Base::Object>& value) noexcept {
        return groups_.PushBack(value);
    }
    Base::Span<const Base::Ref<Base::Object>> Groups() const noexcept {
        return {groups_.Data(), groups_.Size()};
    }

private:
    Base::Vector<Base::Ref<Base::Object>> groups_;
};

class XamlVisualStateManagerObject
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        XamlVisualStateManagerObject,
        Base::Object,
        "urn:aero",
        "VisualStateManager")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    inline static constexpr Members::AttachedProperty<
        Base::Ref<Base::Object>>
        VisualStateGroupStoreProperty{
            "_VisualStateGroupStore"};
};

struct TemplatePrototypeProperty {
    Meta::DependencyPropertyHandle property;
    Meta::Value value;
    // A dependency-object property participating in a template Binding is
    // cloned as part of the instance graph rather than shared with the
    // authored prototype (for example SolidColorBrush.Color in a DataTemplate).
    std::uint32_t objectNode = UINT32_MAX;
};

struct TemplatePrototypeNode {
    Meta::TypeId type = Meta::InvalidTypeId;
    Base::String name;
    std::uint32_t parent = UINT32_MAX;
    Meta::MemberId contentMember = Meta::InvalidMemberId;
    Base::Vector<TemplatePrototypeProperty> properties;
    Base::Vector<Controls::GridLength> gridColumns;
    Base::Vector<Controls::GridLength> gridRows;
};

struct TemplatePrototypeBinding {
    std::uint32_t target = UINT32_MAX;
    std::uint32_t source = UINT32_MAX;
    Aero::Internal::BindingEngine* manager = nullptr;
    ::Aero::Meta::Registry* metadata = nullptr;
    Meta::DependencyPropertyHandle targetProperty;
    Meta::DependencyPropertyHandle dataContextProperty;
    Base::String path;
    Base::String stringFormat;
    Data::BindingMode mode =
        Data::BindingMode::OneWay;
    Meta::UpdateSourceTrigger updateSourceTrigger =
        Meta::UpdateSourceTrigger::PropertyChanged;
};

struct CompiledTemplateBlueprint {
    ::Aero::Meta::Registry* runtime = nullptr;
    Meta::DependencyPropertyRegistry* properties = nullptr;
    Base::Vector<TemplatePrototypeNode> nodes;
    Base::Vector<TemplatePrototypeBinding> bindings;
    Base::Vector<Base::Ref<Aero::TriggerBase>>
        dataTemplateTriggers;
    // Non-property triggers are retained on the compiled blueprint so every
    // control-template instance can materialize its own sources, name scope,
    // subscriptions, and animation actions.
    Base::Vector<Base::Ref<Aero::TriggerBase>>
        controlTemplateDataTriggers;
    Base::Vector<Base::Ref<Media::Animation::EventTrigger>>
        controlTemplateEventTriggers;
    std::uint32_t contentPresenter = UINT32_MAX;
};

struct CompiledTemplateDefinition {
    Meta::TypeId targetType = Meta::InvalidTypeId;
    CompiledTemplateBlueprint blueprint;
    Base::Vector<Controls::TemplatePropertyTrigger>
        propertyTriggers;
    Base::Vector<Controls::TemplateBindingPlan>
        contentSourceBindings;
    Base::Vector<Controls::VisualStateGroup>
        visualStateGroups;
};

Base::Result<void> BuildCompiledTemplate(
    Controls::TemplateBuilder& context,
    void* factoryContext) noexcept;

Base::Result<Base::Ref<Base::Object>>
BuildCompiledDeferredTemplate(
    const Base::Ref<Base::Object>& payload,
    void* factoryContext) noexcept;

Base::Result<CompiledTemplateBlueprint>
CompileDeferredTemplateBlueprint(
    const Base::Ref<Base::Object>& visualTree,
    const Aero::NameScope* names,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties) noexcept;

Base::Result<CompiledTemplateDefinition>
CompileControlTemplateDefinition(
    Controls::ControlTemplate& controlTemplate,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties) noexcept;

} // namespace Aero::Internal

// Markup implementation sources historically referred to the template
// compiler contracts through Markup::Detail. Keep that source-only bridge
// while the canonical declarations live in Base::Detail; no aliases are
// exposed by installed headers.
namespace Aero::Markup::Detail {
using ::Aero::Internal::CompiledTemplateBlueprint;
using ::Aero::Internal::CompiledTemplateDefinition;
using ::Aero::Internal::BuildCompiledTemplate;
using ::Aero::Internal::BuildCompiledDeferredTemplate;
using ::Aero::Internal::CompileDeferredTemplateBlueprint;
using ::Aero::Internal::CompileControlTemplateDefinition;
}
