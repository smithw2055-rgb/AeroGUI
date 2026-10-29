#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Markup/XamlProvider.hpp>
#include <Aero/Markup/MarkupExtension.hpp>
#include <Aero/Gui.hpp>
#include "gui/GuiRuntime.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/markup/XamlSchema.hpp"
#include "gui/markup/XamlBytecode.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include "gui/controls/ItemsContainers.hpp"

#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/HashSet.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Documents/Inlines.hpp>
#include <Aero/Events.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/Resources.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/View.hpp>
#include <Aero/VisualStateManager.hpp>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <new>
#include <utility>

namespace Aero::Markup {

// ============================================================================
// SECTION: ObjectWriter Declaration
// ============================================================================

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

class ObjectWriter {
public:
    explicit ObjectWriter(
        Schema& schema,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    ~ObjectWriter() noexcept;

    ObjectWriter(const ObjectWriter&) = delete;
    ObjectWriter& operator=(const ObjectWriter&) = delete;

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
    Diagnostics::IDiagnosticSink* Diagnostics() const noexcept {
        return diagnostics_;
    }


private:
    static Base::Result<Aero::Media::Visual*> ResolveVisual(
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
        MemberWritePolicy memberPolicy;
        ::Aero::Diagnostics::SourceSpan source;
        std::uint32_t valuesWritten = 0U;
        bool hasMemberPolicy = false;
        bool memberValueTypeIsObject = false;
        bool memberValueTypeIsValueType = false;
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
        ResolvedMember contentMember;
        MemberWritePolicy contentPolicy;
        bool hasContentMember = false;
        bool contentValueTypeIsObject = false;
        bool contentValueTypeIsValueType = false;
        bool deferredStaticResource = false;
    };

    struct AssignmentRecord {
        std::uint32_t objectIndex = InvalidIndex;
        Meta::MemberId member = Meta::InvalidMemberId;
        std::uint32_t count = 0U;
    };

    struct DeferredStaticResourceRecord {
        std::uint32_t targetObjectIndex = InvalidIndex;
        ResolvedMember member;
        MemberWritePolicy policy;
        Base::String key;
        ::Aero::Diagnostics::SourceSpan source;
        bool hasPolicy = false;
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
    Base::Result<void> FinalizeDeferredStylesAndTemplates() noexcept;
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

    struct ResolvedNodeMember {
        ResolvedMember member;
        MemberWritePolicy memberPolicy;
        bool hasCompiledPolicy = false;
        bool memberValueTypeIsObject = false;
        bool memberValueTypeIsValueType = false;
    };

    Base::Result<ResolvedNodeMember> ResolveNodeMember(
        const Node& node,
        Meta::TypeId targetType,
        MemberSyntax syntax) noexcept;

    Base::Result<void> WriteMarkupOrTextToTarget(
        const Node& node,
        std::uint32_t targetObjectIndex,
        const ResolvedMember& member,
        const MemberWritePolicy& policy,
        bool valueTypeIsValueType,
        Frame* memberFrame,
        Frame* targetFrameForDeferred,
        Meta::TypeId targetValueTypeOverride) noexcept;

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
        ::Aero::Diagnostics::SourceSpan source,
        const MemberWritePolicy* policy = nullptr) noexcept;
    Base::Result<void> WriteValue(
        std::uint32_t targetObjectIndex,
        const ResolvedMember& member,
        Meta::Value&& value,
        ::Aero::Diagnostics::SourceSpan source,
        const MemberWritePolicy* policy = nullptr) noexcept;

    Base::Result<void> RegisterObjectName(
        std::uint32_t objectIndex,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
    Base::Result<void> ConnectEvent(
        Frame& memberFrame,
        Base::StringView handlerName,
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


// ============================================================================
// SECTION: WriterSupport & NodeCursor
// ============================================================================

class NodeCursor {
public:
    virtual ~NodeCursor() = default;
    virtual Base::Result<const Node*> Read(
        Node& scratch) noexcept = 0;
};

namespace WriterSupport {

inline constexpr Base::StringView MessageSchemaNotReady(
    "XAML object writer requires a frozen schema context");
inline constexpr Base::StringView MessageUnknownType(
    "XAML object element does not resolve to a registered type");
inline constexpr Base::StringView MessageTypeNotConstructible(
    "XAML object element resolves to a non-constructible type");
inline constexpr Base::StringView MessageUnknownMember(
    "XAML member does not resolve on the target object type");
inline constexpr Base::StringView MessageInvalidAttachedMember(
    "XAML qualified member is not valid for this target object");
inline constexpr Base::StringView MessageUnsupportedMember(
    "XAML member has no supported object-writer adapter");
inline constexpr Base::StringView MessageInvalidValue(
    "XAML value conversion or member assignment failed");
inline constexpr Base::StringView MessageInvalidWriterState(
    "XAML node sequence is invalid for the object writer");
inline constexpr Base::StringView MessageMissingContentProperty(
    "XAML child object requires a registered content property");
inline constexpr Base::StringView MessageDuplicateMemberValue(
    "XAML member is assigned more than once");
inline constexpr Base::StringView MessageInitializationFailed(
    "XAML object initialization callback failed");
inline constexpr Base::StringView MessageUnexpectedText(
    "XAML text is not valid in the current object context");
inline constexpr Base::StringView MessageTypeMismatch(
    "XAML object or scalar value is not assignable to the target member");
inline constexpr Base::StringView MessageFactoryFailed(
    "XAML object factory failed");
inline constexpr Base::StringView MessageMissingMemberValue(
    "XAML member scope does not contain a value");
inline constexpr Base::StringView MessageMultipleRoots(
    "XAML document contains more than one root object");
inline constexpr Base::StringView MessageInvalidDirective(
    "XAML language directive is unsupported or used in an invalid context");
inline constexpr Base::StringView MessageDuplicateName(
    "x:Name is duplicated in the active XAML name scope");
inline constexpr Base::StringView MessageDuplicateResourceKey(
    "x:Key is duplicated in the active XAML resource scope");
inline constexpr Base::StringView MessageStaticResourceNotFound(
    "StaticResource key is not available; forward references are not supported");
inline constexpr Base::StringView MessageMissingResourceScope(
    "x:Key requires an enclosing XAML resource scope");
inline constexpr Base::StringView MessageNullNotAllowed(
    "x:Null is not valid for this XAML value or document root");
inline constexpr Base::StringView MessageInvalidMarkupExtension(
    "XAML markup-extension text is malformed or unsupported");
inline constexpr Base::StringView MessageNamespaceState(
    "XAML namespace declaration state is invalid");
inline constexpr Base::StringView MessageNameRegistrationFailed(
    "XAML name registration callback failed");
inline constexpr Base::StringView MessageResourceRegistrationFailed(
    "XAML resource registration callback failed");
inline constexpr Base::StringView MessageUnknownMarkupExtension(
    "XAML markup-extension type or provider is not registered");
inline constexpr Base::StringView MessageMarkupExtensionFailed(
    "XAML markup-extension value provider failed");

inline constexpr Base::StringView XmlPrefix("xml");
inline constexpr Base::StringView XmlNamespaceUri(
    "http://www.w3.org/XML/1998/namespace");
inline constexpr Base::StringView DirectiveName("Name");
inline constexpr Base::StringView DirectiveKey("Key");
inline constexpr Base::StringView DirectiveClass("Class");
inline constexpr Base::StringView DirectiveNull("Null");
inline constexpr Base::StringView NullMarkup("x:Null");
inline constexpr Base::StringView StaticResourceMarkup("StaticResource");

class StreamingXamlNodeCursor final : public NodeCursor {
public:
    explicit StreamingXamlNodeCursor(NodeReader& reader) noexcept
        : reader_(&reader) {}

    Base::Result<const Node*> Read(Node& scratch) noexcept override {
        Base::Result<NodeKind> read = reader_->Read(scratch);
        return read
            ? Base::Result<const Node*>(&scratch)
            : Base::Result<const Node*>(read.GetStatus());
    }

private:
    NodeReader* reader_ = nullptr;
};

class CompiledXamlNodeCursor final : public NodeCursor {
public:
    explicit CompiledXamlNodeCursor(
        const CompiledDocument& document) noexcept
        : nodes_(document.Nodes()) {}

    Base::Result<const Node*> Read(Node&) noexcept override {
        if (index_ >= nodes_.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Compiled XAML node stream ended unexpectedly");
        }
        return &nodes_[index_++];
    }

private:
    Base::Span<const Node> nodes_;
    std::uint32_t index_ = 0U;
};


inline Base::Status InvalidStateStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        MessageInvalidWriterState.Data());
}

inline Base::Status SessionConsumedStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "XAML load session is single use");
}

inline bool IsAsciiWhitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

inline bool HasTypeFlag(Meta::TypeFlags value, Meta::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Result<Base::String> StaticResourceNotFoundMessage(
    Base::StringView key) noexcept;

ResolvedMember ResolveCompiledMember(
    const CompiledMemberBinding& binding) noexcept;

MemberWritePolicy ResolveCompiledMemberPolicy(
    const CompiledMemberBinding& binding) noexcept;

bool IsCompiledMemberCompatible(
    const Schema& schema,
    Meta::TypeId targetType,
    const ResolvedMember& member) noexcept;

} // namespace WriterSupport


} // namespace Aero::Markup

// ============================================================================
// SECTION: ObjectWriter Implementation
// ============================================================================



// ===== XamlObjectWriter.cpp =====






















// ServiceProvider.hpp inlined into MarkupExtensionContract.hpp




// ===== ObjectWriter / DeferredContentPlan =====



namespace Aero::Markup {

using namespace WriterSupport;

namespace {



Base::Status InvalidContent(const char* message) noexcept {

    return Base::Status::Failure(

        Base::ErrorCode::InvalidArgument, message);

}



Base::Status InvalidContentState(const char* message) noexcept {

    return Base::Status::Failure(

        Base::ErrorCode::InvalidState, message);

}



} // namespace



Base::Result<void> DeferredContentPlan::Stage(

    Base::Object& owner,

    Base::Object& parent,

    const Base::Ref<Base::Object>& child,

    ::Aero::Meta::Registry& metadata,

    Meta::MemberId member) noexcept {

    if (!child || member == Meta::InvalidMemberId ||

        !metadata.IsReady()) {

        return InvalidContentState(

            "Deferred XAML content edge is invalid");

    }

    edges_.PushBack({

            &owner,

            &parent,

            child,

            &metadata,

            member,

            false});

    Base::Result<void> written =

        metadata.WriteContent(parent, member, child);

    if (!written) {

        edges_.PopBack();

        return written.GetStatus();

    }

    return {};

}



Base::Result<void> DeferredContentPlan::StageProperty(

    Base::Object& owner,

    Base::Object& parent,

    const Base::Ref<Base::Object>& child,

    ::Aero::Meta::Registry& metadata,

    Meta::MemberId member) noexcept {

    if (!child ||

        member == Meta::InvalidMemberId) {

        return InvalidContentState(

            "Deferred XAML structural property edge is invalid");

    }

    const Meta::PropertyInfo* property =

        metadata.Types().FindProperty(member);

    if (property == nullptr) {

        return InvalidContent(

            "Deferred XAML structural property was not found");

    }

    edges_.PushBack({

            &owner,

            &parent,

            child,

            &metadata,

            member,

            true});

    const Meta::Value value =

        Meta::Value::FromObject(

            property->ValueType(), child);

    Base::Result<void> written =

        metadata.SetProperty(

            parent, member, value);

    if (!written) {

        edges_.PopBack();

        return written.GetStatus();

    }

    return {};

}



Base::Result<void> DeferredContentPlan::CopyForOwner(

    const Base::Object& owner,

    Base::Vector<DeferredContentEdge>& output) const noexcept {

    output.Clear();

    for (const DeferredContentEdge& edge : edges_) {

        if (edge.owner != &owner) continue;

        output.PushBack(edge);

    }

    return {};

}



Base::Result<void> DeferredContentPlan::StageBinding(

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

    const Meta::PropertyValue& converterParameter) noexcept {

    if (!targetProperty.IsValid() ||

        (path.Empty() && !bindsToSource) ||

        !metadata.IsReady()) {

        return InvalidContentState(

            "Deferred XAML Binding declaration is invalid");

    }

    DeferredBindingEdge edge;

    edge.owner = &owner;

    edge.source = source;

    Base::Result<void> sourceAssigned =

        edge.sourceName.Assign(sourceName);

    if (!sourceAssigned) return sourceAssigned.GetStatus();

    sourceAssigned = edge.relativeAncestorType.Assign(

        relativeAncestorType);

    if (!sourceAssigned) return sourceAssigned.GetStatus();

    edge.relativeAncestorLevel = relativeAncestorLevel;

    edge.target = &target;

    edge.metadata = &metadata;

    edge.targetProperty = targetProperty;

    edge.dataContextProperty = dataContextProperty;

    edge.mode = mode;

    edge.bindsToSource = bindsToSource;

    edge.updateSourceTrigger = updateSourceTrigger;

    edge.converter = converter;

    edge.converterParameter = converterParameter;

    Base::Result<void> assigned =

        edge.path.Assign(path);

    if (!assigned) return assigned.GetStatus();

    assigned = edge.stringFormat.Assign(

        stringFormat);

    if (!assigned) return assigned.GetStatus();

    bindings_.PushBack(std::move(edge));

    return {};

}



Base::Result<void>

DeferredContentPlan::CopyBindingsForOwner(

    const Base::Object& owner,

    Base::Vector<DeferredBindingEdge>& output) const noexcept {

    output.Clear();

    for (const DeferredBindingEdge& edge : bindings_) {

        if (edge.owner != &owner) continue;

        output.PushBack(edge);

    }

    return {};

}



void DeferredContentPlan::ReleaseOwner(

    Base::Object& owner) noexcept {

    for (std::uint32_t index = 0U;

         index < edges_.Size();

         ++index) {

        DeferredContentEdge& edge = edges_[index];

        if (edge.owner != &owner) continue;

        bool firstForParent = true;

        for (std::uint32_t earlier = 0U;

             earlier < index;

             ++earlier) {

            firstForParent =

                firstForParent &&

                (edges_[earlier].owner != &owner ||

                 edges_[earlier].parent !=

                     edge.parent ||

                 (edge.property &&

                  edges_[earlier].member !=

                      edge.member));

        }

        if (firstForParent &&

            edge.parent != nullptr &&

            edge.metadata != nullptr) {

            if (edge.property) {

                const Meta::PropertyInfo* property =

                    edge.metadata->Types().

                        FindProperty(edge.member);

                if (property != nullptr) {

                    (void)edge.metadata->SetProperty(

                        *edge.parent,

                        edge.member,

                        Meta::Value::NullObject(

                            property->ValueType()));

                }

            } else {

                (void)edge.metadata->ClearContent(

                    *edge.parent,

                    edge.member);

            }

        }

    }



    std::uint32_t output = 0U;

    for (std::uint32_t index = 0U;

         index < edges_.Size();

         ++index) {

        DeferredContentEdge& edge = edges_[index];

        if (edge.owner == &owner) continue;

        if (output != index) {

            edges_[output] = std::move(edge);

        }

        ++output;

    }

    (void)edges_.Resize(output);



    output = 0U;

    for (std::uint32_t index = 0U;

         index < bindings_.Size();

         ++index) {

        DeferredBindingEdge& edge = bindings_[index];

        if (edge.owner == &owner) continue;

        if (output != index) {

            bindings_[output] = std::move(edge);

        }

        ++output;

    }

    (void)bindings_.Resize(output);

}



void DeferredContentPlan::ReleaseAll() noexcept {

    while (!edges_.Empty() || !bindings_.Empty()) {

        Base::Object* owner = !edges_.Empty()

            ? edges_.Front().owner

            : bindings_.Front().owner;

        if (owner == nullptr) {

            edges_.Clear();

            return;

        }

        ReleaseOwner(*owner);

    }

}





Base::Result<Aero::Media::Visual*> ObjectWriter::ResolveVisual(

    ::Aero::Markup::Schema& schema,

    Base::Object& object,

    Meta::TypeId type) noexcept {

    if (!schema.Types().IsDerivedFrom(object.RuntimeType(), type) ||

        !schema.Types().IsDerivedFrom(

            type, Aero::Media::Visual::StaticTypeId())) {

        return InvalidContent(

            "XAML object metadata is not compatible with Visual");

    }

    return static_cast<Aero::Media::Visual*>(&object);

}



Base::Result<Aero::UIElement*> ObjectWriter::ResolveUIElement(

    ::Aero::Markup::Schema& schema,

    Base::Object& object,

    Meta::TypeId type) noexcept {

    Base::Result<Aero::Media::Visual*> visual =

        ResolveVisual(schema, object, type);

    if (!visual) return visual.GetStatus();

    Aero::UIElement* element =

        ::Aero::TryCast<::Aero::UIElement>(visual.Value());

    if (element == nullptr) {

        return InvalidContent("XAML object is not a UIElement");

    }

    return element;

}



Base::Result<void> ObjectWriter::StageContent(

    ::Aero::Markup::Schema& schema,

    Base::Object& object,

    const Meta::Value& value,

    const ExtensionServices& services) noexcept {

    VisualContentPlan* plan = services.visualContent;

    if (services.targetObject != &object ||

        value.Kind() != Meta::ValueKind::Object ||

        value.IsNullObject() || !value.AsObject()) {

        return InvalidContentState(

            "XAML content requires a non-null object");

    }



    ::Aero::Meta::Registry* metadata = schema.Metadata();

    if (metadata == nullptr) {

        return InvalidContentState(

            "XAML content metadata is unavailable");

    }

    Base::Result<Meta::ContentInfo> contentResult =

        metadata->GetContentInfo(services.targetMember);

    const Meta::PropertyInfo* property =

        schema.Types().FindProperty(

            services.targetMember);

    const bool attachedMember =

        property != nullptr &&

        (static_cast<std::uint32_t>(property->Flags()) &

         static_cast<std::uint32_t>(

             Meta::PropertyFlags::Attached)) != 0U;

    const bool structuralProperty =

        !contentResult &&

        property != nullptr &&

        (static_cast<std::uint32_t>(

             property->Flags()) &

         static_cast<std::uint32_t>(

             Meta::PropertyFlags::Structural)) !=

            0U &&

        (static_cast<std::uint32_t>(

             property->Flags()) &

         static_cast<std::uint32_t>(

             Meta::PropertyFlags::Collection)) ==

            0U &&

        metadata->CanWriteProperty(

            services.targetMember);

    if (!contentResult && !structuralProperty) {

        return InvalidContent(

            "XAML content target has no content metadata");

    }

    if (!structuralProperty) {

        const Meta::ContentInfo& content =

            contentResult.Value();

        if (!content.writable ||

            !content.clearable ||

            (!attachedMember &&

             !schema.Types().IsDerivedFrom(

                 services.targetObjectType,

                 content.ownerType)) ||

            (services.deferredContentOwner == nullptr &&

             !content.IsVisual())) {

            return InvalidContent(

                "XAML content target has no compatible content facet");

        }

    }



    Base::Object* childObject = value.AsObject().Get();

    const bool childIsDependencyObject =

        schema.Types().IsDerivedFrom(

            childObject->RuntimeType(),

            Aero::DependencyObject::StaticTypeId());



    // A deferred template owns a visual root without itself being a ::Aero::Media::Visual.

    // Commit that root through the template's content accessor; descendant

    // visual edges are staged below once their actual visual parent exists.

    if (services.deferredContentOwner == &object &&

        !schema.Types().IsDerivedFrom(

            services.targetObjectType,

            Aero::Media::Visual::StaticTypeId())) {

        if (structuralProperty) {

            return InvalidContent(

                "A non-visual template root cannot use a visual structural property");

        }

        return metadata->WriteContent(

            object,

            services.targetMember,

            value.AsObject());

    }



    if (services.deferredContentOwner != nullptr) {

        const bool parentIsVisual =

            schema.Types().IsDerivedFrom(

                object.RuntimeType(),

                Aero::Media::Visual::StaticTypeId());

        const bool parentIsFreezable =

            schema.Types().IsDerivedFrom(

                object.RuntimeType(),

                Aero::Freezable::StaticTypeId());

        const bool parentIsTimeline =

            schema.Types().IsDerivedFrom(

                object.RuntimeType(),

                Media::Animation::Timeline::StaticTypeId());

        const bool deferParent =

            services.deferredContentOwner == &object ||

            parentIsVisual ||

            (parentIsFreezable && !parentIsTimeline);

        if (!deferParent) {

            // DataTemplate.Resources Storyboards and Trigger.EnterActions are

            // live shared objects. Visual-tree Freezables (TransformGroup)

            // still clone through the deferred graph.

            return metadata->WriteContent(

                object,

                services.targetMember,

                value.AsObject());

        }

        if (services.deferredContent == nullptr ||

            !childIsDependencyObject) {

            return InvalidContentState(

                "Deferred XAML content requires a DependencyObject child");

        }

        return structuralProperty

            ? services.deferredContent->

                  StageProperty(

                      *services.deferredContentOwner,

                      object,

                      value.AsObject(),

                      *metadata,

                      services.targetMember)

            : services.deferredContent->Stage(

                  *services.deferredContentOwner,

                  object,

                  value.AsObject(),

                  *metadata,

                  services.targetMember);

    }



    if (plan == nullptr) {

        return InvalidContentState(

            "XAML visual content plan is unavailable");

    }

    Base::Result<Aero::UIElement*> childResult =

        ResolveUIElement(schema, *childObject, value.Type());

    if (!childResult) return childResult.GetStatus();

    Base::Result<Aero::UIElement*> parentResult =

        ResolveUIElement(

            schema, object, services.targetObjectType);

    if (!parentResult) return parentResult.GetStatus();



    Base::Result<void> reserved = plan->Reserve(

        plan->contentEdges.Size() + 1U,

        plan->mountEdges.Size() + 1U,

        plan->nodes.Size() + 2U);

    if (!reserved) return reserved.GetStatus();



    Base::Result<Aero::Media::Visual*> parentNode =

        ResolveVisual(

            schema, object, services.targetObjectType);

    if (!parentNode) return parentNode.GetStatus();

    Base::Result<Aero::Media::Visual*> childNode =

        ResolveVisual(schema, *childObject, value.Type());

    if (!childNode) return childNode.GetStatus();



    Base::Result<void> parentAdded =

        plan->AddNode(*parentNode.Value());

    if (!parentAdded) return parentAdded.GetStatus();

    Base::Result<void> childAdded =

        plan->AddNode(*childNode.Value());

    if (!childAdded) return childAdded.GetStatus();



    Base::Ref<Base::Object> parentOwner =

        Base::Ref<Base::Object>::FromBorrowed(object);

    plan->contentEdges.PushBack({

            std::move(parentOwner), value.AsObject(),

            metadata, services.targetMember,

            structuralProperty});



    plan->mountEdges.PushBack({

        parentResult.Value(), childResult.Value(), {}});



    Base::Result<void> written =

        structuralProperty

        ? metadata->SetProperty(

              object,

              services.targetMember,

              value)

        : metadata->WriteContent(

              object,

              services.targetMember,

              value.AsObject());

    if (!written) {

        plan->mountEdges.PopBack();

        plan->contentEdges.PopBack();

        return written.GetStatus();

    }

    return {};

}



} // namespace Aero::Markup



// ===== XamlObjectWriterState (writer helpers) =====







namespace Aero::Markup {

using namespace WriterSupport;

namespace WriterSupport {



Base::Result<Base::String> StaticResourceNotFoundMessage(

    Base::StringView key) noexcept {

    Base::String message;

    Base::Result<void> appended = message.Assign(

        "StaticResource key '");

    if (appended) appended = message.Append(key);

    if (appended) appended = message.Append(

        "' is not available; forward references are not supported");

    return appended

        ? Base::Result<Base::String>(std::move(message))

        : Base::Result<Base::String>(appended.GetStatus());

}



ResolvedMember ResolveCompiledMember(

    const CompiledMemberBinding& binding) noexcept {

    ResolvedMember member;

    member.id = binding.id;

    member.kind = binding.kind;

    member.ownerType = binding.ownerType;

    member.valueType = binding.valueType;

    member.propertyFlags = binding.propertyFlags;

    member.eventFlags = binding.eventFlags;

    member.attached = binding.attached;

    return member;

}



MemberWritePolicy ResolveCompiledMemberPolicy(

    const CompiledMemberBinding& binding) noexcept {

    MemberWritePolicy policy;

    policy.mode = binding.writeMode ==

            static_cast<std::uint8_t>(

                MemberWriteMode::Collection)

        ? MemberWriteMode::Collection

        : MemberWriteMode::SetOnce;

    if (binding.id ==

        VisualStateManager::VisualStateGroupsProperty.Handle().value) {

        policy.mode = MemberWriteMode::Collection;

    }

    policy.acceptsAnyValue =

        binding.acceptsAnyValue;

    policy.writable = binding.writable;

    return policy;

}



bool IsCompiledMemberCompatible(

    const Schema& schema,

    Meta::TypeId targetType,

    const ResolvedMember& member) noexcept {

    return member.attached ||

        schema.Types().IsDerivedFrom(

            targetType, member.ownerType);

}



} // namespace WriterSupport



// ===== Scopes =====



namespace {



constexpr const char* MessageNamespaceUnavailable =

    "XAML namespace scope is not available";

constexpr const char* MessageResourceResolverUnavailable =

    "XAML resource resolver is not available";



} // namespace



Base::Result<Base::StringView> NamespaceScope::Lookup(

    Base::StringView prefix) const noexcept {

    if (lookup_ == nullptr) {

        return Base::Status::Failure(

            Base::ErrorCode::NotInitialized,

            MessageNamespaceUnavailable);

    }

    return lookup_(context_, prefix);

}



Base::Result<Aero::ResourceValue> ResourceResolver::Lookup(

    Base::StringView key) const noexcept {

    if (lookup_ == nullptr) {

        return Base::Status::Failure(

            Base::ErrorCode::NotInitialized,

            MessageResourceResolverUnavailable);

    }

    return lookup_(context_, key);

}



} // namespace Aero::Markup



// ===== XamlObjectWriterBuilderLoad.cpp =====






// ===== ObjectWriter core / node stack =====



namespace Aero::Markup {

using namespace WriterSupport;

// ===== ObjectWriter load lifecycle (ctor/Load/Complete/Create) =====



ObjectWriter::ObjectWriter(

    ::Aero::Markup::Schema& schema,

    Diagnostics::IDiagnosticSink* diagnostics) noexcept

    : schema_(&schema),

      diagnostics_(diagnostics),

      frames_(),

      created_(),

      assignments_(),

      deferredStaticResources_(),

      nameScopes_(),

      resourceScopes_(),

      serviceResourceChain_(),

      namespaceBindings_(),

      pendingNamespaces_(),

      committedNames_(),

      committedResources_(),

      resultVisualContent_() {}



ObjectWriter::~ObjectWriter() noexcept {

    AbortTransaction();

}



Base::Result<LoaderResult> ObjectWriter::Load(

    NodeReader& reader) noexcept {

    const LoadState* previous = loadContext_;

    loadContext_ = nullptr;

    Base::Result<Base::Ref<Base::Object>> loaded =

        LoadReaderCore(reader);

    Base::Result<LoaderResult> result =

        CompleteLoad(std::move(loaded));

    loadContext_ = previous;

    return result;

}



Base::Result<LoaderResult> ObjectWriter::Load(

    NodeReader& reader,

    const LoadState& context) noexcept {

    const LoadState* previous = loadContext_;

    loadContext_ = &context;

    Base::Result<Base::Ref<Base::Object>> loaded =

        LoadReaderCore(reader);

    Base::Result<LoaderResult> result =

        CompleteLoad(std::move(loaded));

    loadContext_ = previous;

    return result;

}



Base::Result<LoaderResult> ObjectWriter::Load(

    const CompiledDocument& document) noexcept {

    const LoadState* previous = loadContext_;

    loadContext_ = nullptr;

    Base::Result<Base::Ref<Base::Object>> loaded =

        LoadCompiledCore(document);

    Base::Result<LoaderResult> result =

        CompleteLoad(std::move(loaded));

    loadContext_ = previous;

    return result;

}



Base::Result<LoaderResult> ObjectWriter::Load(

    const CompiledDocument& document,

    const LoadState& context) noexcept {

    const LoadState* previous = loadContext_;

    loadContext_ = &context;

    Base::Result<Base::Ref<Base::Object>> loaded =

        LoadCompiledCore(document);

    Base::Result<LoaderResult> result =

        CompleteLoad(std::move(loaded));

    loadContext_ = previous;

    return result;

}



Base::Result<LoaderResult> ObjectWriter::CompleteLoad(

    Base::Result<Base::Ref<Base::Object>> loaded) noexcept {

    if (!loaded) return loaded.GetStatus();

    LoaderResult result;

    result.root = std::move(loaded).Value();

    result.metadata = schema_->Metadata();

    result.names = std::move(committedNames_);

    result.resources = std::move(committedResources_);

    result.visualContent = std::move(resultVisualContent_);

    result.effects.Items() = std::move(extensionEffects_);

    result.hasDeferredStaticResources = hasDeferredStaticResources_;

    if (loadContext_ != nullptr) {

        result.runtimeLifetime = loadContext_->effectLifetime;

    }

    if (!deferredContent_.Empty()) {

        result.Clear();

        AbortTransaction();

        return Base::Status::Failure(

            Base::ErrorCode::InvalidState,

            "Deferred XAML content was not finalized");

    }

    if (loadContext_ != nullptr &&

        loadContext_->finalize != nullptr) {

        Base::Result<void> finalized =

            loadContext_->finalize(

                result,

                loadContext_->finalizeContext);

        if (!finalized) {

            const Base::Status status =

                finalized.GetStatus();

            result.Clear();

            AbortTransaction();

            return status;

        }

    }

    // Source-backed merged dictionaries are committed by the loader finalizer.

    // Resolve queued StaticResource references only after that transaction so

    // sibling theme dictionaries participate in WPF resource lookup order.

    if (hasDeferredStaticResources_) {

        Base::Result<void> resolved =

            ResolveDeferredStaticResources();

        if (!resolved) {

            const Base::Status status = resolved.GetStatus();

            result.Clear();

            AbortTransaction();

            return status;

        }

    }

    {

        Base::Result<void> finalizedStyles = FinalizeDeferredStylesAndTemplates();

        if (!finalizedStyles) {

            const Base::Status status = finalizedStyles.GetStatus();

            result.Clear();

            AbortTransaction();

            return status;

        }

    }

    result.hasDeferredStaticResources =

        hasDeferredStaticResources_;

    Base::Result<void> prepared =

        result.effects.Prepare(result.names);

    if (!prepared) {

        const Base::Status status = prepared.GetStatus();

        result.Clear();

        AbortTransaction();

        return status;

    }

    ClearTransaction();

    return result;

}



Base::Result<void> ObjectWriter::ResolveDeferredStaticResources() noexcept {

    for (DeferredStaticResourceRecord& deferred :

         deferredStaticResources_) {

        Base::Result<Aero::ResourceValue> resource =

            LookupResource(deferred.key.View());

        if (!resource) {

            Base::Result<Base::String> message =

                StaticResourceNotFoundMessage(

                    deferred.key.View());

            if (!message) return message.GetStatus();

            return Failure(

                resource.GetStatus(),

                XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,

                message.Value().View(),

                deferred.source);

        }

        Base::Result<void> written = WriteValue(

            deferred.targetObjectIndex,

            deferred.member,

            std::move(resource).Value(),

            deferred.source,

            deferred.hasPolicy

                ? &deferred.policy

                : nullptr);

        if (!written) return written.GetStatus();

    }

    deferredStaticResources_.Clear();

    hasDeferredStaticResources_ = false;

    return {};

}



Base::Result<void> ObjectWriter::FinalizeDeferredStylesAndTemplates() noexcept {
    for (std::uint32_t index = 0U; index < created_.Size(); ++index) {
        CreatedObjectRecord& record = created_[index];
        if (!record.endCalled || !record.object) {
            continue;
        }
        if (record.type == Aero::Style::StaticTypeId()) {
            auto& style = static_cast<Aero::Style&>(*record.object);
            if (style.GetIsSealed()) {
                continue;
            }
            Base::Result<void> endResult = schema_->EndInit(
                record.type,
                *record.object,
                BuildExtensionServices(index, {}, {}));
            if (!endResult) {
                return endResult.GetStatus();
            }
        } else if (record.type == Aero::Controls::ControlTemplate::StaticTypeId()) {
            auto& controlTemplate =
                static_cast<Aero::Controls::ControlTemplate&>(*record.object);
            if (controlTemplate.GetIsSealed()) {
                continue;
            }
            Base::Result<void> endResult = schema_->EndInit(
                record.type,
                *record.object,
                BuildExtensionServices(index, {}, {}));
            if (!endResult) {
                return endResult.GetStatus();
            }
        }
    }
    return {};
}



Base::Result<Base::Ref<Base::Object>> ObjectWriter::LoadReaderCore(

    NodeReader& reader) noexcept {

    StreamingXamlNodeCursor cursor(reader);

    return LoadCursorCore(cursor);

}



Base::Result<Base::Ref<Base::Object>> ObjectWriter::LoadCursorCore(

    NodeCursor& cursor) noexcept {

    if (consumed_ || loading_) return SessionConsumedStatus();

    consumed_ = true;



    AbortTransaction();

    committedNames_.Clear();

    committedResources_.Clear();

    resultVisualContent_.ReleaseContent();

    resultVisualContent_.Clear();

    if (!schema_->IsFrozen()) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::InvalidState,

                MessageSchemaNotReady.Data()),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageSchemaNotReady,

            {});

    }



    loading_ = true;

    Node node;

    while (!ended_) {

        Base::Result<const Node*> readResult =

            cursor.Read(node);

        if (!readResult) {

            const Base::Status status = readResult.GetStatus();

            AbortTransaction();

            loading_ = false;

            return status;

        }



        const Node* current = readResult.Value();

        if (current == nullptr) {

            const Base::Status status = Failure(

                InvalidStateStatus(),

                XamlObjectWriterDiagnosticCodes::InvalidWriterState,

                MessageInvalidWriterState,

                {});

            AbortTransaction();

            loading_ = false;

            return status;

        }

        if (loadContext_ != nullptr &&

            loadContext_->recordingNodes != nullptr) {

            Base::Result<Node> cloned = Node::Clone(*current);

            if (!cloned) {

                const Base::Status status = cloned.GetStatus();

                AbortTransaction();

                loading_ = false;

                return status;

            }

            loadContext_->recordingNodes->PushBack(

                std::move(cloned).Value());

        }

        Base::Result<void> processResult =

            ProcessNode(*current);

        if (!processResult) {

            const Base::Status status = processResult.GetStatus();

            AbortTransaction();

            loading_ = false;

            return status;

        }

    }



    if (!frames_.Empty() || !root_ || !pendingNamespaces_.Empty() ||

        !namespaceBindings_.Empty()) {

        const Base::Status status = Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            node.Source());

        AbortTransaction();

        loading_ = false;

        return status;

    }



    CommitDocumentScopes();

    Base::Ref<Base::Object> result = std::move(root_);

    loading_ = false;

    return result;

}



Base::Result<Base::Ref<Base::Object>> ObjectWriter::LoadCompiledCore(

    const CompiledDocument& document) noexcept {

    if (consumed_ || loading_) return SessionConsumedStatus();

    if (!schema_->IsFrozen() || !document.IsValid()) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::InvalidState,

                MessageSchemaNotReady.Data()),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageSchemaNotReady,

            {});

    }

    Base::Result<void> compatible =

        ValidateCompiledCacheIdentity(

            document.Identity(), schema_->Domain());

    if (!compatible) return compatible.GetStatus();

    // Step 5: High-speed XamlVM Bytecode Document Execution pipeline
    if (loadContext_ == nullptr || loadContext_->recordingNodes == nullptr) {
        Base::Result<const XamlBytecodeProgram*> bytecodeReady = EnsureDocumentBytecode(
            document, *schema_);
        if (bytecodeReady && bytecodeReady.Value() != nullptr && bytecodeReady.Value()->IsValid()) {
            const XamlBytecodeProgram& program = *bytecodeReady.Value();
            XamlDocumentExecutionContext vmContext;
            vmContext.schema = schema_;
            vmContext.loadContext = loadContext_;
            vmContext.runtime = schema_->Metadata();

            XamlDocumentExecutionResult vmResult;
            Base::Result<void> executed = XamlVM::ExecuteDocument(
                program, vmContext, vmResult);
            if (executed && vmResult.root) {
                consumed_ = true;
                root_ = std::move(vmResult.root);
                committedNames_ = std::move(vmResult.names);
                committedResources_ = std::move(vmResult.resources);
                resultVisualContent_ = std::move(vmResult.visualContent);
                Base::Ref<Base::Object> result = std::move(root_);
                return result;
            }
        }
    }

    CompiledXamlNodeCursor cursor(document);

    return LoadCursorCore(cursor);

}



Base::Result<Base::Ref<Base::Object>> ObjectWriter::CreateObject(

    Meta::TypeId type) const noexcept {

    if (loadContext_ != nullptr &&

        created_.Size() >= loadContext_->maxObjects) {

        return Base::Status::Failure(

            Base::ErrorCode::OutOfRange,

            "XAML object count exceeds configured limits");

    }

    if (loadContext_ != nullptr &&

        loadContext_->existingRoot &&

        rootObjectIndex_ == InvalidIndex &&

        frames_.Empty()) {

        const Meta::TypeId actual =

            loadContext_->existingRoot->RuntimeType();

        if (actual == Meta::InvalidTypeId ||

            !schema_->Types().IsAssignableFrom(type, actual)) {

            return Base::Status::Failure(

                Base::ErrorCode::InvalidArgument,

                "LoadComponent root type is incompatible with XAML root type");

        }

        return loadContext_->existingRoot;

    }

    Base::Result<Base::Ref<Base::Object>> created = schema_->CreateObject(type);



    return created;

}



} // namespace Aero::Markup



// ===== XamlObjectWriterBuilderNodes.cpp =====





// ===== ObjectWriter core / node stack =====



namespace Aero::Markup {

using namespace WriterSupport;

// ===== ObjectWriter node dispatch (Process/Start/End object/member) =====



Base::Result<void> ObjectWriter::ProcessNode(

    const Node& node) noexcept {

    switch (node.Kind()) {

    case NodeKind::NamespaceDeclaration:

        return QueueNamespaceDeclaration(node);

    case NodeKind::StartObject:

        return StartObject(node);

    case NodeKind::EndObject:

        return EndObject(node);

    case NodeKind::StartMember:

        return StartMember(node);

    case NodeKind::EndMember:

        return EndMember(node);

    case NodeKind::Value:

        return WriteText(node);

    case NodeKind::EndOfDocument:

        if (!frames_.Empty() || !root_ || !pendingNamespaces_.Empty() ||

            !namespaceBindings_.Empty()) {

            return Failure(

                InvalidStateStatus(),

                XamlObjectWriterDiagnosticCodes::InvalidWriterState,

                MessageInvalidWriterState,

                node.Source());

        }

        ended_ = true;

        return {};

    case NodeKind::None:

        break;

    }



    return Failure(

        InvalidStateStatus(),

        XamlObjectWriterDiagnosticCodes::InvalidWriterState,

        MessageInvalidWriterState,

        node.Source());

}



Base::Result<void> ObjectWriter::QueueNamespaceDeclaration(

    const Node& node) noexcept {

    if (node.NamespaceUri().Empty()) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::ValidationFailed,

                MessageNamespaceState.Data()),

            XamlObjectWriterDiagnosticCodes::NamespaceState,

            MessageNamespaceState,

            node.Source());

    }



    PendingNamespaceRecord record;

    Base::Result<void> prefixResult = record.prefix.AssignUnchecked(

        node.NamespacePrefix());

    if (!prefixResult) {

        return prefixResult.GetStatus();

    }

    Base::Result<void> uriResult = record.uri.AssignUnchecked(

        node.NamespaceUri());

    if (!uriResult) {

        return uriResult.GetStatus();

    }

    record.source = node.Source();

    pendingNamespaces_.PushBack(std::move(record));

    return {};

}

Base::Result<void> ObjectWriter::StartObject(

    const Node& node) noexcept {



    if (frames_.Empty() && root_) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::AlreadyExists,

                MessageMultipleRoots.Data()),

            XamlObjectWriterDiagnosticCodes::MultipleRootObjects,

            MessageMultipleRoots,

            node.Source());

    }



    std::uint32_t bindingStart = InvalidIndex;

    Base::Result<void> namespaceResult =

        ActivatePendingNamespaces(bindingStart);

    if (!namespaceResult) {

        return Failure(

            namespaceResult.GetStatus(),

            XamlObjectWriterDiagnosticCodes::NamespaceState,

            MessageNamespaceState,

            node.Source());

    }



    if (!frames_.Empty() &&

        frames_.Back().kind == FrameKind::Object &&

        (node.CompiledMemberId() != Meta::InvalidMemberId ||

         HasPropertyElementSyntax(node.Name()))) {

        return StartPropertyElement(

            node,

            frames_.Size() - 1U,

            bindingStart);

    }



    if (IsXamlNullObject(node.Name())) {

        return StartNullObject(node, bindingStart);

    }



    Meta::TypeId typeId = Meta::InvalidTypeId;

    Meta::TypeFlags typeFlags = Meta::TypeFlags::None;

    const CompiledTypeBinding* compiledType = nullptr;

    Base::Status typeStatus;

    if (node.HasCompiledTypeBinding()) {

        const CompiledTypeBinding& binding = node.CompiledType();

        compiledType = &binding;

        typeId = binding.id;

        typeFlags = binding.flags;

    } else if (node.CompiledTypeId() != Meta::InvalidTypeId) {

        const Meta::TypeInfo* type =

            schema_->Types().FindType(node.CompiledTypeId());

        if (type != nullptr) {

            typeId = type->Id();

            typeFlags = type->Flags();

        } else {

            typeStatus = Base::Status::Failure(

                Base::ErrorCode::NotFound,

                "AXB2 type id is absent from the frozen schema");

        }

    } else {

        Base::Result<const Meta::TypeInfo*> typeResult =

            schema_->ResolveType(

                node.Name().NamespaceUri(),

                node.Name().LocalName());

        if (typeResult) {

            typeId = typeResult->Id();

            typeFlags = typeResult->Flags();

        } else {

            typeStatus = typeResult.GetStatus();

        }

    }

    if (typeId == Meta::InvalidTypeId) {

        return Failure(

            typeStatus,

            XamlObjectWriterDiagnosticCodes::UnknownType,

            MessageUnknownType,

            node.Source());

    }



    if (HasTypeFlag(typeFlags, Meta::TypeFlags::ValueType)) {

        return StartValueObject(node, bindingStart, typeId);

    }

    Base::Result<Base::Ref<Base::Object>> createResult =

        CreateObject(typeId);

    if (!createResult) {

        const bool nonConstructible =

            createResult.GetStatus().code == Base::ErrorCode::Unsupported;

        // WPF permits abstract object types with a text converter to be used

        // as value elements. ImageSource is the canonical example:

        // <ImageSource>Images/Atlas.png</ImageSource> materializes a

        // BitmapImage through the registered ImageSource converter. Defer

        // construction until the element text is seen; conversion will still

        // reject abstract types that have no converter.

        if (nonConstructible &&

            HasTypeFlag(typeFlags, Meta::TypeFlags::Abstract) &&

            !frames_.Empty()) {

            return StartValueObject(node, bindingStart, typeId);

        }

        return Failure(

            createResult.GetStatus(),

            nonConstructible

                ? XamlObjectWriterDiagnosticCodes::TypeNotConstructible

                : XamlObjectWriterDiagnosticCodes::FactoryFailed,

            nonConstructible

                ? MessageTypeNotConstructible

                : MessageFactoryFailed,

            node.Source());

    }



    CreatedObjectRecord record;

    record.object = std::move(createResult).Value();

    record.type = typeId;

    if (loadContext_ != nullptr && loadContext_->existingRoot &&

        rootObjectIndex_ == InvalidIndex && frames_.Empty() &&

        record.object.Get() == loadContext_->existingRoot.Get()) {

        const Meta::TypeId runtimeType = record.object->RuntimeType();

        if (runtimeType == Meta::InvalidTypeId) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::InvalidArgument,

                    "Existing XAML root has no runtime type"),

                XamlObjectWriterDiagnosticCodes::TypeMismatch,

                MessageTypeMismatch,

                node.Source());

        }

        record.type = runtimeType;

    }

    if (compiledType != nullptr &&

        compiledType->HasContentMember()) {

        record.contentMember =

            ResolveCompiledMember(compiledType->contentMember);

        record.contentPolicy =

            ResolveCompiledMemberPolicy(

                compiledType->contentMember);

        record.hasContentMember = true;

        record.contentValueTypeIsObject =

            compiledType->contentMember.ValueTypeIsObject();

        record.contentValueTypeIsValueType =

            compiledType->contentMember.ValueTypeIsValueType();

    } else if (schema_ != nullptr) {

        Base::Result<ResolvedMember> content =

            schema_->ResolveContentMember(typeId);

        if (content) {

            record.contentMember = content.Value();

            record.contentPolicy =

                schema_->ResolveMemberWritePolicy(

                    record.contentMember);

            record.hasContentMember = true;

            if (const Meta::TypeInfo* valueType =

                    schema_->Types().FindType(

                        record.contentMember.valueType)) {

                record.contentValueTypeIsObject =

                    valueType->Kind() ==

                        Meta::MetadataTypeKind::Object;

                record.contentValueTypeIsValueType =

                    valueType->Kind() !=

                        Meta::MetadataTypeKind::Object &&

                    HasTypeFlag(

                        valueType->Flags(),

                        Meta::TypeFlags::ValueType);

            }

        }

    }

    const std::uint32_t objectIndex = created_.Size();

    created_.PushBack(std::move(record));



    if (rootObjectIndex_ == InvalidIndex) {

        rootObjectIndex_ = objectIndex;

    }



    CreatedObjectRecord& stored = created_[objectIndex];

    stored.beginCalled = true;

    Base::Result<void> beginResult = schema_->BeginInit(

        stored.type,

        *stored.object);

    if (!beginResult) {

        return Failure(

            beginResult.GetStatus(),

            XamlObjectWriterDiagnosticCodes::InitializationFailed,

            MessageInitializationFailed,

            node.Source());

    }



    Frame frame;

    frame.kind = FrameKind::Object;

    frame.objectIndex = objectIndex;

    frame.namespaceBindingStart = bindingStart;

    frame.source = node.Source();

    Base::Result<void> scopeResult = CreateScopesForObject(

        objectIndex,

        frame,

        node.Source());

    if (!scopeResult) {

        return scopeResult.GetStatus();

    }



    frames_.PushBack(frame);

    return {};

}



Base::Result<void> ObjectWriter::StartValueObject(

    const Node& node,

    std::uint32_t bindingStart,

    Meta::TypeId type) noexcept {

    if (frames_.Empty() ||

        (frames_.Back().kind != FrameKind::Object &&

         frames_.Back().kind != FrameKind::Member)) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::ValidationFailed,

                MessageTypeMismatch.Data()),

            XamlObjectWriterDiagnosticCodes::TypeMismatch,

            MessageTypeMismatch,

            node.Source());

    }



    CreatedObjectRecord record;

    record.type = type;

    record.valueElement = true;

    const std::uint32_t objectIndex = created_.Size();

    created_.PushBack(

        std::move(record));



    Frame frame;

    frame.kind = FrameKind::ValueObject;

    frame.objectIndex = objectIndex;

    frame.namespaceBindingStart = bindingStart;

    frame.source = node.Source();

    frames_.PushBack(frame);

    return {};

}



Base::Result<void> ObjectWriter::StartNullObject(

    const Node& node,

    std::uint32_t bindingStart) noexcept {

    if (frames_.Empty() ||

        (frames_.Back().kind != FrameKind::Member &&

         frames_.Back().kind != FrameKind::Object)) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::ValidationFailed,

                MessageNullNotAllowed.Data()),

            XamlObjectWriterDiagnosticCodes::NullNotAllowed,

            MessageNullNotAllowed,

            node.Source());

    }



    Frame frame;

    frame.kind = FrameKind::NullObject;

    frame.namespaceBindingStart = bindingStart;

    frame.source = node.Source();

    frames_.PushBack(frame);

    return {};

}



Base::Result<void> ObjectWriter::EndObject(

    const Node& node) noexcept {

    if (frames_.Empty()) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            node.Source());

    }



    Frame& frame = frames_.Back();

    if (frame.kind == FrameKind::ValueObject) {

        return CompleteValueObject(node);

    }

    if (frame.kind == FrameKind::NullObject) {

        return CompleteNullObject(node);

    }

    if (frame.kind == FrameKind::Member) {

        if (!frame.propertyElement) {

            return Failure(

                InvalidStateStatus(),

                XamlObjectWriterDiagnosticCodes::InvalidWriterState,

                MessageInvalidWriterState,

                node.Source());

        }

        if (frame.valuesWritten == 0U) {

            const Meta::TypeInfo* valueType =

                schema_->Types().FindType(

                    frame.member.valueType);

            // WPF permits an empty property element for reference-valued

            // properties. It means "leave the property's current/default

            // value in place", which is required by empty

            // Application.Resources declarations.

            if (valueType == nullptr ||

                valueType->Kind() !=

                    Meta::MetadataTypeKind::Object) {

                return Failure(

                    Base::Status::Failure(

                        Base::ErrorCode::ValidationFailed,

                        MessageMissingMemberValue.Data()),

                    XamlObjectWriterDiagnosticCodes::MissingMemberValue,

                    MessageMissingMemberValue,

                    node.Source());

            }

        }

        const std::uint32_t bindingStart = frame.namespaceBindingStart;

        frames_.PopBack();

        PopNamespaceBindings(bindingStart);

        return {};

    }

    if (frame.kind != FrameKind::Object) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            node.Source());

    }



    return CompleteObject(node);

}

Base::Result<ObjectWriter::ResolvedNodeMember> ObjectWriter::ResolveNodeMember(
    const Node& node,
    Meta::TypeId targetType,
    MemberSyntax syntax) noexcept {
    ResolvedNodeMember result;
    if (node.HasCompiledMemberBinding()) {
        result.member = ResolveCompiledMember(
            node.CompiledMember());
        result.memberPolicy = ResolveCompiledMemberPolicy(
            node.CompiledMember());
        result.hasCompiledPolicy = true;
        result.memberValueTypeIsObject =
            node.CompiledMember().ValueTypeIsObject();
        result.memberValueTypeIsValueType =
            node.CompiledMember().ValueTypeIsValueType();
        if (!IsCompiledMemberCompatible(
                *schema_,
                targetType,
                result.member)) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    MessageInvalidAttachedMember.Data()),
                XamlObjectWriterDiagnosticCodes::
                    InvalidAttachedMember,
                MessageInvalidAttachedMember,
                node.Source());
        }
    } else {
        Base::Result<ResolvedMember> memberResult =
            node.CompiledMemberId() != Meta::InvalidMemberId
            ? schema_->ResolveMember(
                  targetType,
                  node.CompiledMemberId())
            : schema_->ResolveMember(
                  targetType,
                  node.Name(),
                  syntax);
        if (!memberResult) {
            const bool notFound =
                memberResult.GetStatus().code ==
                    Base::ErrorCode::NotFound;
            return Failure(
                memberResult.GetStatus(),
                notFound
                    ? XamlObjectWriterDiagnosticCodes::UnknownMember
                    : XamlObjectWriterDiagnosticCodes::
                        InvalidAttachedMember,
                notFound
                    ? MessageUnknownMember
                    : MessageInvalidAttachedMember,
                node.Source());
        }
        result.member = memberResult.Value();
        result.memberPolicy =
            schema_->ResolveMemberWritePolicy(result.member);
        if (const Meta::TypeInfo* valueType =
                schema_->Types().FindType(
                    result.member.valueType)) {
            result.memberValueTypeIsObject =
                valueType->Kind() ==
                    Meta::MetadataTypeKind::Object;
            result.memberValueTypeIsValueType =
                HasTypeFlag(
                    valueType->Flags(),
                    Meta::TypeFlags::ValueType);
        }
    }
    return result;
}

Base::Result<void> ObjectWriter::StartMember(

    const Node& node) noexcept {

    if (frames_.Empty() ||

        (frames_.Back().kind != FrameKind::Object &&

         frames_.Back().kind != FrameKind::ValueObject)) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            node.Source());

    }



    const Frame& objectFrame = frames_.Back();

    if (objectFrame.objectIndex >= created_.Size()) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            node.Source());

    }



    if (objectFrame.kind == FrameKind::ValueObject) {

        if (node.Name().NamespaceUri() == LanguageNamespaceUri()) {

            if (IsXamlDirective(node.Name(), DirectiveKey)) {

                return StartDirective(

                    node, DirectiveKind::Key, objectFrame.objectIndex);

            }

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::Unsupported,

                    MessageInvalidDirective.Data()),

                XamlObjectWriterDiagnosticCodes::InvalidDirective,

                MessageInvalidDirective,

                node.Source());

        }

        if (!node.IsFromAttribute() ||

            node.Name().LocalName() != Base::StringView("Value")) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::NotFound,

                    MessageUnknownMember.Data()),

                XamlObjectWriterDiagnosticCodes::UnknownMember,

                MessageUnknownMember,

                node.Source());

        }

        Frame frame;

        frame.kind = FrameKind::ValueMember;

        frame.targetObjectIndex = objectFrame.objectIndex;

        frame.source = node.Source();

        frames_.PushBack(frame);

        return {};

    }



    if (node.IsFromAttribute() &&

        node.Name().LocalName() == DirectiveName) {

        return StartDirective(

            node,

            DirectiveKind::Name,

            objectFrame.objectIndex);

    }



    if (node.Name().NamespaceUri() == LanguageNamespaceUri()) {

        if (IsXamlDirective(node.Name(), DirectiveName)) {

            return StartDirective(

                node,

                DirectiveKind::Name,

                objectFrame.objectIndex);

        }

        if (IsXamlDirective(node.Name(), DirectiveKey)) {

            return StartDirective(

                node,

                DirectiveKind::Key,

                objectFrame.objectIndex);

        }

        if (IsXamlDirective(node.Name(), DirectiveClass)) {

            return StartDirective(

                node,

                DirectiveKind::Class,

                objectFrame.objectIndex);

        }

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::Unsupported,

                MessageInvalidDirective.Data()),

            XamlObjectWriterDiagnosticCodes::InvalidDirective,

            MessageInvalidDirective,

            node.Source());

    }



    Base::Result<ResolvedNodeMember> resolvedNodeMember =
        ResolveNodeMember(node, created_[objectFrame.objectIndex].type, MemberSyntax::Attribute);
    if (!resolvedNodeMember) return resolvedNodeMember.GetStatus();

    const ResolvedMember member = resolvedNodeMember.Value().member;
    const MemberWritePolicy memberPolicy = resolvedNodeMember.Value().memberPolicy;
    const bool hasCompiledPolicy = resolvedNodeMember.Value().hasCompiledPolicy;
    const bool memberValueTypeIsObject = resolvedNodeMember.Value().memberValueTypeIsObject;
    const bool memberValueTypeIsValueType = resolvedNodeMember.Value().memberValueTypeIsValueType;



    const bool eventAttribute =

        member.kind == Meta::MemberKind::Event &&

        node.IsFromAttribute();

    if (!eventAttribute &&

        (member.kind != Meta::MemberKind::Property ||

         !memberPolicy.writable)) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::Unsupported,

                MessageUnsupportedMember.Data()),

            XamlObjectWriterDiagnosticCodes::UnsupportedMember,

            MessageUnsupportedMember,

            node.Source());

    }



    Frame frame;

    frame.kind = FrameKind::Member;

    frame.targetObjectIndex = objectFrame.objectIndex;

    frame.member = member;

    frame.memberPolicy = memberPolicy;

    frame.hasMemberPolicy =

        hasCompiledPolicy;

    frame.memberValueTypeIsObject =

        memberValueTypeIsObject;

    frame.memberValueTypeIsValueType =

        memberValueTypeIsValueType;

    frame.source = node.Source();

    frame.propertyElement = false;

    frames_.PushBack(frame);

    return {};

}



Base::Result<void> ObjectWriter::StartDirective(

    const Node& node,

    DirectiveKind directive,

    std::uint32_t targetObjectIndex) noexcept {

    if (!node.IsFromAttribute() || targetObjectIndex >= created_.Size()) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::ValidationFailed,

                MessageInvalidDirective.Data()),

            XamlObjectWriterDiagnosticCodes::InvalidDirective,

            MessageInvalidDirective,

            node.Source());

    }



    const CreatedObjectRecord& record = created_[targetObjectIndex];

    if ((directive == DirectiveKind::Name &&

         (!record.name.Empty() || record.nameRegistered)) ||

        (directive == DirectiveKind::Key &&

         (!record.key.Empty() || record.resourceRegistered))) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::AlreadyExists,

                MessageInvalidDirective.Data()),

            XamlObjectWriterDiagnosticCodes::InvalidDirective,

            MessageInvalidDirective,

            node.Source());

    }



    Frame frame;

    frame.kind = FrameKind::Directive;

    frame.directive = directive;

    frame.targetObjectIndex = targetObjectIndex;

    frame.source = node.Source();

    frames_.PushBack(frame);

    return {};

}



Base::Result<void> ObjectWriter::EndMember(

    const Node& node) noexcept {

    if (frames_.Empty()) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            node.Source());

    }



    const Frame& frame = frames_.Back();

    if (frame.kind == FrameKind::ValueMember) {

        if (frame.valuesWritten != 1U) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::ValidationFailed,

                    MessageMissingMemberValue.Data()),

                XamlObjectWriterDiagnosticCodes::MissingMemberValue,

                MessageMissingMemberValue,

                node.Source());

        }

        frames_.PopBack();

        return {};

    }

    if (frame.kind == FrameKind::Directive) {

        if (frame.valuesWritten != 1U) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::ValidationFailed,

                    MessageMissingMemberValue.Data()),

                XamlObjectWriterDiagnosticCodes::MissingMemberValue,

                MessageMissingMemberValue,

                node.Source());

        }

        frames_.PopBack();

        return {};

    }



    if (frame.kind != FrameKind::Member || frame.propertyElement) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            node.Source());

    }

    if (frame.valuesWritten == 0U && !frame.deferredStaticResource) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::ValidationFailed,

                MessageMissingMemberValue.Data()),

            XamlObjectWriterDiagnosticCodes::MissingMemberValue,

            MessageMissingMemberValue,

            node.Source());

    }

    frames_.PopBack();

    return {};

}



} // namespace Aero::Markup



// ===== XamlObjectWriterBuilderValues.cpp =====






// ===== ObjectWriter core / node stack =====



namespace Aero::Markup {

using namespace WriterSupport;

// ===== ObjectWriter markup-extension and text values =====



ObjectWriter::MarkupValueKind ObjectWriter::ParseMarkupValue(

    Base::StringView text,

    Base::StringView& extensionName,

    Base::StringView& argument) const noexcept {

    extensionName = {};

    argument = {};

    const Base::StringView value = TrimAscii(text);

    if (value.Empty() || value[0] != '{') {

        return MarkupValueKind::Literal;

    }

    if (value.SizeBytes() >= 2U && value[1] == '}') {

        argument = value.Substr(2U, value.SizeBytes() - 2U);

        return MarkupValueKind::EscapedLiteral;

    }

    if (value.SizeBytes() < 2U ||

        value[value.SizeBytes() - 1U] != '}') {

        return MarkupValueKind::Invalid;

    }



    const Base::StringView inner = TrimAscii(value.Substr(

        1U,

        value.SizeBytes() - 2U));

    if (inner == NullMarkup) {

        return MarkupValueKind::Null;

    }



    std::uint32_t nestedDepth = 0U;

    char quote = '\0';

    for (char character : inner) {

        if (quote != '\0') {

            if (character == quote) quote = '\0';

            continue;

        }

        if (character == '\'' || character == '"') {

            quote = character;

        } else if (character == '{') {

            ++nestedDepth;

        } else if (character == '}') {

            if (nestedDepth == 0U) {

                return MarkupValueKind::Invalid;

            }

            --nestedDepth;

        }

    }

    if (nestedDepth != 0U || quote != '\0') {

        return MarkupValueKind::Invalid;

    }



    std::uint32_t nameEnd = 0U;

    while (nameEnd < inner.SizeBytes() &&

           !IsAsciiWhitespace(inner[nameEnd]) && inner[nameEnd] != ',') {

        ++nameEnd;

    }

    if (nameEnd == 0U) {

        return MarkupValueKind::Invalid;

    }

    extensionName = inner.Substr(0U, nameEnd);



    argument = TrimAscii(inner.Substr(nameEnd, inner.SizeBytes() - nameEnd));

    if (!argument.Empty() && argument[0] == ',') {

        argument = TrimAscii(argument.Substr(1U, argument.SizeBytes() - 1U));

    }

    if (extensionName == StaticResourceMarkup) {

        if (argument.Empty()) {

            return MarkupValueKind::Invalid;

        }

        constexpr Base::StringView resourceKeyPrefix("ResourceKey=");

        if (argument.SizeBytes() > resourceKeyPrefix.SizeBytes() &&

            argument.Substr(0U, resourceKeyPrefix.SizeBytes()) ==

                resourceKeyPrefix) {

            argument = TrimAscii(argument.Substr(

                resourceKeyPrefix.SizeBytes(),

                argument.SizeBytes() - resourceKeyPrefix.SizeBytes()));

            if (argument.Empty()) return MarkupValueKind::Invalid;

        }

        return MarkupValueKind::StaticResource;

    }

    return MarkupValueKind::Extension;

}



Base::Result<ProvidedValue> ObjectWriter::EvaluateMarkupExtension(

    std::uint32_t targetObjectIndex,

    const ResolvedMember& member,

    Base::StringView extensionName,

    Base::StringView arguments,

    ::Aero::Diagnostics::SourceSpan source) noexcept {

    std::uint32_t colon = extensionName.SizeBytes();

    for (std::uint32_t index = 0U;

         index < extensionName.SizeBytes();

         ++index) {

        if (extensionName[index] != ':') {

            continue;

        }

        if (colon != extensionName.SizeBytes()) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::ValidationFailed,

                    MessageInvalidMarkupExtension.Data()),

                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,

                MessageInvalidMarkupExtension,

                source);

        }

        colon = index;

    }



    Base::StringView prefix;

    Base::StringView localName = extensionName;

    if (colon != extensionName.SizeBytes()) {

        if (colon == 0U || colon + 1U >= extensionName.SizeBytes()) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::ValidationFailed,

                    MessageInvalidMarkupExtension.Data()),

                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,

                MessageInvalidMarkupExtension,

                source);

        }

        prefix = extensionName.Substr(0U, colon);

        localName = extensionName.Substr(

            colon + 1U,

            extensionName.SizeBytes() - colon - 1U);

    }



    Base::Result<Base::StringView> namespaceResult = LookupNamespace(prefix);

    if (!namespaceResult) {

        return Failure(

            namespaceResult.GetStatus(),

            XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension,

            MessageUnknownMarkupExtension,

            source);

    }

    Base::Result<const Meta::TypeInfo*> typeResult =
        schema_->ResolveType(
            namespaceResult.Value(),
            localName);
    if (!typeResult) {
        char buffer[128];
        if (localName.SizeBytes() + 9U < sizeof(buffer) &&
            (localName.SizeBytes() < 9U ||
             localName.Substr(localName.SizeBytes() - 9U, 9U) != Base::StringView("Extension"))) {
            std::memcpy(buffer, localName.Data(), localName.SizeBytes());
            std::memcpy(buffer + localName.SizeBytes(), "Extension", 9U);
            typeResult = schema_->ResolveType(
                namespaceResult.Value(),
                Base::StringView(buffer, localName.SizeBytes() + 9U));
        }
    }
    if (!typeResult) {
        return Failure(
            typeResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension,
            MessageUnknownMarkupExtension,
            source);
    }



    const ExtensionServices services = BuildExtensionServices(

        targetObjectIndex,

        member,

        source);

    Base::Result<ProvidedValue> provided =

        schema_->ProvideMarkupExtensionValue(

            typeResult->Id(),

            arguments,

            services);

    if (!provided) {

        const bool missing =

            provided.GetStatus().code == Base::ErrorCode::Unsupported ||

            provided.GetStatus().code == Base::ErrorCode::NotFound;

        return Failure(

            provided.GetStatus(),

            missing

                ? XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension

                : XamlObjectWriterDiagnosticCodes::MarkupExtensionFailed,

            missing

                ? MessageUnknownMarkupExtension

                : MessageMarkupExtensionFailed,

            source);

    }

    return provided;

}



// ===== ObjectWriter property / content apply =====

Base::Result<void> ObjectWriter::WriteMarkupOrTextToTarget(
    const Node& node,
    std::uint32_t targetObjectIndex,
    const ResolvedMember& member,
    const MemberWritePolicy& policy,
    bool valueTypeIsValueType,
    Frame* memberFrame,
    Frame* targetFrameForDeferred,
    Meta::TypeId targetValueTypeOverride) noexcept {
    Base::StringView extensionName;
    Base::StringView argument;
    const MarkupValueKind markup = ParseMarkupValue(
        node.Value(), extensionName, argument);

    if (markup == MarkupValueKind::Invalid) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageInvalidMarkupExtension.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
            MessageInvalidMarkupExtension,
            node.Source());
    }

    auto dispatchValue = [&](Meta::Value&& val) -> Base::Result<void> {
        if (memberFrame != nullptr) {
            return WriteValueToMember(*memberFrame, std::move(val), node.Source());
        }
        return WriteValue(targetObjectIndex, member, std::move(val), node.Source(), &policy);
    };

    auto dispatchProvided = [&](ProvidedValue&& prov) -> Base::Result<void> {
        if (memberFrame != nullptr) {
            return WriteProvidedValueToMember(*memberFrame, std::move(prov), node.Source());
        }
        return WriteProvidedValue(targetObjectIndex, member, std::move(prov), node.Source(), &policy);
    };

    if (markup == MarkupValueKind::Null) {
        if (memberFrame != nullptr) {
            if (targetObjectIndex < created_.Size() &&
                member.id == Controls::Primitives::ToggleButton::IsCheckedProperty.Handle().value &&
                schema_->Types().IsDerivedFrom(
                    created_[targetObjectIndex].type,
                    Controls::Primitives::ToggleButton::StaticTypeId())) {
                Base::Result<Meta::Value> nullable =
                    Meta::ValueCodec<Nullable<bool>>::Encode(Nullable<bool>{});
                if (!nullable) return nullable.GetStatus();
                return dispatchValue(std::move(nullable).Value());
            }
            if (member.valueType == Meta::TypeOf<Base::String>()) {
                Base::Result<Meta::Value> empty =
                    Meta::Value::TryFromString(Meta::TypeOf<Base::String>(), {});
                if (!empty) return empty.GetStatus();
                return dispatchValue(std::move(empty).Value());
            }
            if (valueTypeIsValueType && !policy.acceptsAnyValue) {
                return Failure(
                    Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        MessageNullNotAllowed.Data()),
                    XamlObjectWriterDiagnosticCodes::NullNotAllowed,
                    MessageNullNotAllowed,
                    node.Source());
            }
        } else {
            if (valueTypeIsValueType) {
                return Failure(
                    Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        MessageNullNotAllowed.Data()),
                    XamlObjectWriterDiagnosticCodes::NullNotAllowed,
                    MessageNullNotAllowed,
                    node.Source());
            }
        }
        Meta::Value value = Meta::Value::NullObject(member.valueType);
        return dispatchValue(std::move(value));
    }

    if (markup == MarkupValueKind::StaticResource) {
        Base::Result<Aero::ResourceValue> resource = LookupResource(argument);
        if (!resource) {
            if (loadContext_ != nullptr &&
                loadContext_->deferUnresolvedStaticResources) {
                if (targetFrameForDeferred != nullptr) {
                    targetFrameForDeferred->deferredStaticResource = true;
                }
                hasDeferredStaticResources_ = true;
                if (targetObjectIndex < created_.Size()) {
                    created_[targetObjectIndex].deferredStaticResource = true;
                }
                DeferredStaticResourceRecord deferred;
                deferred.targetObjectIndex = targetObjectIndex;
                deferred.member = member;
                deferred.policy = policy;
                deferred.hasPolicy = true;
                deferred.source = node.Source();
                Base::Result<void> key = deferred.key.Assign(argument);
                if (!key) return key.GetStatus();
                deferredStaticResources_.PushBack(std::move(deferred));
                return {};
            }
            Base::Result<Base::String> message =
                StaticResourceNotFoundMessage(argument);
            if (!message) return message.GetStatus();
            return Failure(
                resource.GetStatus(),
                XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,
                message.Value().View(),
                node.Source());
        }
        return dispatchValue(std::move(resource).Value());
    }

    if (markup == MarkupValueKind::Extension) {
        Base::Result<ProvidedValue> value = EvaluateMarkupExtension(
            targetObjectIndex,
            member,
            extensionName,
            argument,
            node.Source());
        if (!value) return value.GetStatus();
        return dispatchProvided(std::move(value).Value());
    }

    const ExtensionServices services = BuildExtensionServices(
        targetObjectIndex,
        member,
        node.Source());

    const Meta::TypeId effectiveValueType =
        targetValueTypeOverride != Meta::InvalidTypeId
            ? targetValueTypeOverride
            : member.valueType;

    Base::Result<Meta::Value> convertResult = schema_->ConvertText(
        effectiveValueType,
        markup == MarkupValueKind::EscapedLiteral ? argument : node.Value(),
        &services);
    if (!convertResult) {
        return Failure(
            convertResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidValue,
            MessageInvalidValue,
            node.Source());
    }
    return dispatchValue(std::move(convertResult).Value());
}

Base::Result<void> ObjectWriter::WriteText(

    const Node& node) noexcept {

    if (!node.HasCompiledValue() &&

        !node.IsFromAttribute() &&

        IsWhitespaceOnly(node.Value())) {

        // WPF TextBlock/Span mixed content collapses XML whitespace between

        // inlines to a single space so sibling Runs do not mash together

        // ("Release, Press or Hover" not "Release,PressorHover").

        if (frames_.Empty()) {

            return {};

        }

        Frame& spaceFrame = frames_.Back();

        std::uint32_t objectIndex = InvalidIndex;

        if (spaceFrame.kind == FrameKind::Member ||

            spaceFrame.kind == FrameKind::ValueMember) {

            objectIndex = spaceFrame.targetObjectIndex;

        } else if (spaceFrame.kind == FrameKind::Object ||

                   spaceFrame.kind == FrameKind::ValueObject) {

            objectIndex = spaceFrame.objectIndex;

        }

        if (objectIndex >= created_.Size() ||

            !created_[objectIndex].object) {

            return {};

        }

        const Meta::TypeId hostType = created_[objectIndex].type;

        const bool inlineHost =

            schema_->Types().IsDerivedFrom(

                hostType, Controls::TextBlock::StaticTypeId()) ||

            schema_->Types().IsDerivedFrom(

                hostType, Documents::Span::StaticTypeId());

        if (!inlineHost) {

            return {};

        }

        std::uint32_t inlineCount = 0U;

        Base::Object& host = *created_[objectIndex].object;

        if (schema_->Types().IsDerivedFrom(

                hostType, Controls::TextBlock::StaticTypeId())) {

            inlineCount = static_cast<Controls::TextBlock&>(host)

                .GetInlineCount();

        } else {

            inlineCount = static_cast<Documents::Span&>(host)

                .GetInlines().GetCount();

        }

        if (inlineCount == 0U) {

            return {};

        }

        Base::Result<Meta::Value> space =

            Meta::Value::TryFromString(

                Meta::TypeOf<Base::String>(),

                Base::StringView(" "));

        if (!space) {

            return space.GetStatus();

        }

        if (spaceFrame.kind == FrameKind::Member) {

            return WriteValueToMember(

                spaceFrame, std::move(space).Value(), node.Source());

        }

        if (spaceFrame.kind == FrameKind::Object &&

            created_[objectIndex].hasContentMember) {

            return WriteValue(

                objectIndex,

                created_[objectIndex].contentMember,

                std::move(space).Value(),

                node.Source(),

                &created_[objectIndex].contentPolicy);

        }

        return {};

    }

    if (frames_.Empty()) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::ValidationFailed,

                MessageUnexpectedText.Data()),

            XamlObjectWriterDiagnosticCodes::UnexpectedText,

            MessageUnexpectedText,

            node.Source());

    }



    Frame& frame = frames_.Back();

    if (node.HasCompiledValue()) {

        Meta::Value value = node.CompiledValue();

        if (frame.kind == FrameKind::ValueMember ||

            frame.kind == FrameKind::ValueObject) {

            const std::uint32_t objectIndex =

                frame.kind == FrameKind::ValueMember

                    ? frame.targetObjectIndex

                    : frame.objectIndex;

            if (objectIndex >= created_.Size() ||

                !created_[objectIndex].valueElement ||

                !created_[objectIndex].value.IsUnset() ||

                frame.valuesWritten != 0U) {

                return Failure(

                    Base::Status::Failure(

                        Base::ErrorCode::AlreadyExists,

                        MessageDuplicateMemberValue.Data()),

                    XamlObjectWriterDiagnosticCodes::

                        DuplicateMemberValue,

                    MessageDuplicateMemberValue,

                    node.Source());

            }

            created_[objectIndex].value = std::move(value);

            ++frame.valuesWritten;

            return {};

        }

        if (frame.kind == FrameKind::Member) {

            return WriteValueToMember(

                frame, std::move(value), node.Source());

        }

        if (frame.kind == FrameKind::Object &&

            frame.objectIndex < created_.Size()) {

            const CreatedObjectRecord& object =

                created_[frame.objectIndex];

            if (!object.hasContentMember) {

                return Failure(

                    Base::Status::Failure(

                        Base::ErrorCode::NotFound,

                        MessageMissingContentProperty.Data()),

                    XamlObjectWriterDiagnosticCodes::

                        MissingContentProperty,

                    MessageMissingContentProperty,

                    node.Source());

            }

            return WriteValue(

                frame.objectIndex,

                object.contentMember,

                std::move(value),

                node.Source(),

                &object.contentPolicy);

        }

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            node.Source());

    }

    if (frame.kind == FrameKind::Directive) {

        return WriteDirectiveText(frame, node);

    }



    if (frame.kind == FrameKind::ValueMember ||

        frame.kind == FrameKind::ValueObject) {

        const std::uint32_t objectIndex =

            frame.kind == FrameKind::ValueMember

                ? frame.targetObjectIndex

                : frame.objectIndex;

        if (objectIndex >= created_.Size() ||

            !created_[objectIndex].valueElement ||

            !created_[objectIndex].value.IsUnset() ||

            frame.valuesWritten != 0U) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::AlreadyExists,

                    MessageDuplicateMemberValue.Data()),

                XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,

                MessageDuplicateMemberValue,

                node.Source());

        }

        Base::StringView extensionName;

        Base::StringView argument;

        const MarkupValueKind markup = ParseMarkupValue(

            node.Value(), extensionName, argument);

        if (markup != MarkupValueKind::Literal &&

            markup != MarkupValueKind::EscapedLiteral) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::ValidationFailed,

                    MessageInvalidMarkupExtension.Data()),

                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,

                MessageInvalidMarkupExtension,

                node.Source());

        }

        ResolvedMember valueMember;

        valueMember.valueType = created_[objectIndex].type;

        const ExtensionServices services = BuildExtensionServices(

            objectIndex,

            valueMember,

            node.Source());

        Base::Result<Meta::Value> converted = schema_->ConvertText(

            created_[objectIndex].type,

            markup == MarkupValueKind::EscapedLiteral

                ? argument : node.Value(),

            &services);

        if (!converted) {

            return Failure(

                converted.GetStatus(),

                XamlObjectWriterDiagnosticCodes::InvalidValue,

                MessageInvalidValue,

                node.Source());

        }

        created_[objectIndex].value = std::move(converted).Value();

        ++frame.valuesWritten;

        return {};

    }



    if (frame.kind == FrameKind::Member) {

        if (frame.member.kind == Meta::MemberKind::Event) {

            if (frame.valuesWritten != 0U ||

                node.HasCompiledValue()) {

                return Failure(

                    Base::Status::Failure(

                        Base::ErrorCode::AlreadyExists,

                        MessageDuplicateMemberValue.Data()),

                    XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,

                    MessageDuplicateMemberValue,

                    node.Source());

            }

            Base::Result<void> connected = ConnectEvent(

                frame, TrimAscii(node.Value()), node.Source());

            if (!connected) return connected.GetStatus();

            ++frame.valuesWritten;

            return {};

        }

        const MemberWritePolicy policy =
            frame.hasMemberPolicy
            ? frame.memberPolicy
            : schema_->ResolveMemberWritePolicy(frame.member);
        return WriteMarkupOrTextToTarget(
            node,
            frame.targetObjectIndex,
            frame.member,
            policy,
            frame.memberValueTypeIsValueType,
            &frame,
            &frame,
            Meta::InvalidTypeId);
    }



    if (frame.kind != FrameKind::Object ||

        frame.objectIndex >= created_.Size()) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::ValidationFailed,

                MessageUnexpectedText.Data()),

            XamlObjectWriterDiagnosticCodes::UnexpectedText,

            MessageUnexpectedText,

            node.Source());

    }



    const CreatedObjectRecord& contentOwner =

        created_[frame.objectIndex];

    if (!contentOwner.hasContentMember) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::NotFound,

                MessageUnexpectedText.Data()),

            XamlObjectWriterDiagnosticCodes::UnexpectedText,

            MessageUnexpectedText,

            node.Source());

    }

    const ResolvedMember& contentMember =

        contentOwner.contentMember;



    const bool itemsControlScalarContent =
        schema_->Types().IsDerivedFrom(
            contentOwner.type,
            Controls::ItemsControl::StaticTypeId()) &&
        contentOwner.hasContentMember &&
        contentOwner.contentMember.id == contentMember.id;

    return WriteMarkupOrTextToTarget(
        node,
        frame.objectIndex,
        contentMember,
        contentOwner.contentPolicy,
        contentOwner.contentValueTypeIsValueType,
        nullptr,
        &frame,
        itemsControlScalarContent ? Meta::TypeOf<Base::String>() : Meta::InvalidTypeId);
}



Base::Result<void> ObjectWriter::WriteDirectiveText(

    Frame& frame,

    const Node& node) noexcept {

    if (frame.targetObjectIndex >= created_.Size() ||

        frame.valuesWritten != 0U || !node.IsFromAttribute()) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::ValidationFailed,

                MessageInvalidDirective.Data()),

            XamlObjectWriterDiagnosticCodes::InvalidDirective,

            MessageInvalidDirective,

            node.Source());

    }



    CreatedObjectRecord& object = created_[frame.targetObjectIndex];

    if (frame.directive == DirectiveKind::Name) {

        const Meta::PropertyInfo* nameProperty =

            schema_->Metadata()->Types().FindProperty(

                object.type, Base::StringView("Name"), false);

        const bool validName =

            Aero::NameScope::IsValidName(node.Value());

        if (!validName && nameProperty == nullptr) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::InvalidArgument,

                    MessageInvalidDirective.Data()),

                XamlObjectWriterDiagnosticCodes::InvalidDirective,

                MessageInvalidDirective,

                node.Source());

        }

        if (validName) {

            Base::Result<void> assignResult = object.name.Assign(node.Value());

            if (!assignResult) {

                return assignResult.GetStatus();

            }

            Base::Result<void> registerResult = RegisterObjectName(

                frame.targetObjectIndex,

                node.Source());

            if (!registerResult) {

                return registerResult.GetStatus();

            }

        }

        // VisualState and related non-visual authoring objects expose an

        // ordinary Name property in addition to participating in x:Name

        // scopes. Types such as Scoreboard's Game also expose Name as a

        // display string that is not a runtime name (spaces are allowed).

        if (nameProperty != nullptr) {

            Base::Result<Meta::Value> nameValue =

                schema_->ConvertText(

                    nameProperty->ValueType(), node.Value(), nullptr);

            if (!nameValue) return nameValue.GetStatus();

            ResolvedMember nameMember;

            nameMember.id = nameProperty->Id();

            nameMember.kind = Meta::MemberKind::Property;

            nameMember.ownerType = nameProperty->OwnerType();

            nameMember.valueType = nameProperty->ValueType();

            nameMember.propertyFlags = nameProperty->Flags();

            Base::Result<void> assignedName = WriteValue(

                frame.targetObjectIndex, nameMember,

                std::move(nameValue).Value(), node.Source());

            if (!assignedName) return assignedName.GetStatus();

        }

    } else if (frame.directive == DirectiveKind::Key) {

        if (node.Value().Empty()) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::InvalidArgument,

                    MessageInvalidDirective.Data()),

                XamlObjectWriterDiagnosticCodes::InvalidDirective,

                MessageInvalidDirective,

                node.Source());

        }

        Base::Result<void> assignResult = object.key.Assign(node.Value());

        if (!assignResult) {

            return assignResult.GetStatus();

        }

    } else if (frame.directive == DirectiveKind::Class) {

        if (node.Value().Empty() ||

            frame.targetObjectIndex != rootObjectIndex_) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::InvalidArgument,

                    MessageInvalidDirective.Data()),

                XamlObjectWriterDiagnosticCodes::InvalidDirective,

                MessageInvalidDirective,

                node.Source());

        }

        const Base::StringView className = TrimAscii(node.Value());

        std::uint32_t separator = className.SizeBytes();

        for (std::uint32_t index = 0U;

             index < className.SizeBytes(); ++index) {

            if (className[index] == '.') separator = index;

        }

        if (separator == 0U ||

            separator + 1U >= className.SizeBytes()) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::ValidationFailed,

                    "x:Class must use Namespace.Type syntax"),

                XamlObjectWriterDiagnosticCodes::InvalidDirective,

                MessageInvalidDirective,

                node.Source());

        }

        Base::String xamlNamespace;

        Base::Result<void> namespaceAssigned =

            xamlNamespace.Assign("clr-namespace:");

        if (namespaceAssigned) {

            namespaceAssigned = xamlNamespace.Append(

                className.Substr(0U, separator));

        }

        if (!namespaceAssigned) return namespaceAssigned.GetStatus();

        const Base::StringView localName = className.Substr(

            separator + 1U,

            className.SizeBytes() - separator - 1U);

        Base::Result<const Meta::TypeInfo*> classType =

            schema_->ResolveType(xamlNamespace.View(), localName);

        if (!classType) {

            if (loadContext_ != nullptr && loadContext_->existingRoot) {

                return Failure(

                    classType.GetStatus(),

                    XamlObjectWriterDiagnosticCodes::UnknownType,

                    MessageUnknownType,

                    node.Source());

            }

            if (classType.GetStatus().code ==

                Base::ErrorCode::NotFound) {

                // A pure-XAML host may intentionally omit the code-behind

                // class. Keep the authored root type in that case while

                // activating a registered derived class when one exists.

                frame.valuesWritten = 1U;

                return {};

            }

            return classType.GetStatus();

        }

        if (!schema_->Types().IsDerivedFrom(

                classType->Id(), object.type)) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::InvalidArgument,

                    "x:Class type does not derive from the authored root type"),

                XamlObjectWriterDiagnosticCodes::TypeMismatch,

                MessageTypeMismatch,

                node.Source());

        }

        if (loadContext_ != nullptr && loadContext_->existingRoot &&

            frame.targetObjectIndex == rootObjectIndex_ &&

            classType->Id() !=

                loadContext_->existingRoot->RuntimeType()) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::InvalidArgument,

                    "x:Class does not match the existing root runtime type"),

                XamlObjectWriterDiagnosticCodes::TypeMismatch,

                MessageTypeMismatch,

                node.Source());

        }

        if (classType->Id() != object.type) {

            Base::Result<Base::Ref<Base::Object>> replacement =

                CreateObject(classType->Id());

            if (!replacement) {

                return Failure(

                    replacement.GetStatus(),

                    XamlObjectWriterDiagnosticCodes::FactoryFailed,

                    MessageFactoryFailed,

                    node.Source());

            }

            Base::Result<void> initialized = schema_->BeginInit(

                classType->Id(), *replacement.Value());

            if (!initialized) return initialized.GetStatus();

            if (object.beginCalled && object.object) {

                schema_->AbortInit(object.type, *object.object);

            }

            object.object = std::move(replacement).Value();

            object.type = classType->Id();

            object.beginCalled = true;

            object.endCalled = false;

            object.hasContentMember = false;

            object.contentMember = {};

            object.contentPolicy = {};

            object.contentValueTypeIsObject = false;

            object.contentValueTypeIsValueType = false;

            Base::Result<ResolvedMember> content =

                schema_->ResolveContentMember(object.type);

            if (content) {

                object.contentMember = content.Value();

                object.contentPolicy =

                    schema_->ResolveMemberWritePolicy(

                        object.contentMember);

                object.hasContentMember = true;

                if (const Meta::TypeInfo* valueType =

                        schema_->Types().FindType(

                            object.contentMember.valueType)) {

                    object.contentValueTypeIsObject =

                        valueType->Kind() ==

                            Meta::MetadataTypeKind::Object;

                    object.contentValueTypeIsValueType =

                        valueType->Kind() !=

                            Meta::MetadataTypeKind::Object &&

                        HasTypeFlag(

                            valueType->Flags(),

                            Meta::TypeFlags::ValueType);

                }

            } else if (content.GetStatus().code !=

                       Base::ErrorCode::NotFound) {

                return content.GetStatus();

            }

            const std::uint32_t objectFrame =

                FindObjectFrameIndex(frame.targetObjectIndex);

            if (objectFrame != InvalidIndex &&

                frames_[objectFrame].resourceScopeIndex != InvalidIndex &&

                frames_[objectFrame].resourceScopeIndex <

                    resourceScopes_.Size()) {

                resourceScopes_[

                    frames_[objectFrame].resourceScopeIndex].external =

                        schema_->ResolveResourceScope(

                            object.type, *object.object);

            }

        }

    } else {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidDirective,

            MessageInvalidDirective,

            node.Source());

    }



    frame.valuesWritten = 1U;

    return {};

}



} // namespace Aero::Markup



// ===== XamlObjectWriterBuilderWrite.cpp =====







// ===== ObjectWriter core / node stack =====



namespace Aero::Markup {

using namespace WriterSupport;

// ===== ObjectWriter completion and parent writes =====



Base::Result<void> ObjectWriter::StartPropertyElement(

    const Node& node,

    std::uint32_t targetFrameIndex,

    std::uint32_t bindingStart) noexcept {

    if (targetFrameIndex >= frames_.Size() ||

        frames_[targetFrameIndex].kind != FrameKind::Object ||

        frames_[targetFrameIndex].objectIndex >= created_.Size()) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            node.Source());

    }



    const std::uint32_t targetObjectIndex =

        frames_[targetFrameIndex].objectIndex;

    Base::Result<ResolvedNodeMember> resolvedNodeMember =
        ResolveNodeMember(node, created_[targetObjectIndex].type, MemberSyntax::PropertyElement);
    if (!resolvedNodeMember) return resolvedNodeMember.GetStatus();

    const ResolvedMember member = resolvedNodeMember.Value().member;
    const MemberWritePolicy memberPolicy = resolvedNodeMember.Value().memberPolicy;
    const bool hasCompiledPolicy = resolvedNodeMember.Value().hasCompiledPolicy;
    const bool memberValueTypeIsObject = resolvedNodeMember.Value().memberValueTypeIsObject;
    const bool memberValueTypeIsValueType = resolvedNodeMember.Value().memberValueTypeIsValueType;



    const bool resourceEntries =

        created_[targetObjectIndex].type ==

            Aero::ResourceDictionary::StaticTypeId() &&

        created_[targetObjectIndex].hasContentMember &&

        created_[targetObjectIndex].contentMember.id == member.id;

    if (member.kind != Meta::MemberKind::Property ||

        (!memberPolicy.writable && !resourceEntries)) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::Unsupported,

                MessageUnsupportedMember.Data()),

            XamlObjectWriterDiagnosticCodes::UnsupportedMember,

            MessageUnsupportedMember,

            node.Source());

    }



    Frame frame;

    frame.kind = FrameKind::Member;

    frame.targetObjectIndex = targetObjectIndex;

    frame.namespaceBindingStart = bindingStart;

    frame.member = member;

    frame.memberPolicy = memberPolicy;

    frame.hasMemberPolicy =

        hasCompiledPolicy;

    frame.memberValueTypeIsObject = memberValueTypeIsObject;

    frame.memberValueTypeIsValueType = memberValueTypeIsValueType;

    frame.source = node.Source();

    frame.propertyElement = true;

    frames_.PushBack(frame);

    return {};

}



Base::Result<void> ObjectWriter::CompleteObject(

    const Node& node) noexcept {

    if (frames_.Empty() || frames_.Back().kind != FrameKind::Object) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            node.Source());

    }



    const Frame frame = frames_.Back();

    const std::uint32_t objectIndex = frame.objectIndex;

    if (objectIndex >= created_.Size()) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            node.Source());

    }



    CreatedObjectRecord& record = created_[objectIndex];

    if (record.endCalled) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            node.Source());

    }



    if (record.type ==

        StaticResourceObject::StaticTypeId()) {

        const auto& extension =

            static_cast<const StaticResourceObject&>(

                *record.object);

        if (extension.ResourceKey().Empty()) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::ValidationFailed,

                    "StaticResource ResourceKey is empty"),

                XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,

                MessageStaticResourceNotFound,

                node.Source());

        }

        Base::Result<Aero::ResourceValue> resource =

            LookupResource(extension.ResourceKey());

        if (!resource) {

            if (loadContext_ != nullptr &&

                loadContext_->deferUnresolvedStaticResources) {

                frames_.PopBack();

                PopNamespaceBindings(frame.namespaceBindingStart);

                if (!frames_.Empty()) {

                    Frame& parent = frames_.Back();

                    parent.deferredStaticResource = true;

                    DeferredStaticResourceRecord deferred;

                    if (parent.kind == FrameKind::Member) {

                        deferred.targetObjectIndex =

                            parent.targetObjectIndex;

                        deferred.member = parent.member;

                        deferred.policy = parent.hasMemberPolicy

                            ? parent.memberPolicy

                            : schema_->ResolveMemberWritePolicy(

                                parent.member);

                        deferred.hasPolicy = true;

                    } else if (parent.kind == FrameKind::Object &&

                               parent.objectIndex < created_.Size()) {

                        const CreatedObjectRecord& contentOwner =

                            created_[parent.objectIndex];

                        if (!contentOwner.hasContentMember) {

                            return Base::Status::Failure(

                                Base::ErrorCode::NotFound,

                                MessageMissingContentProperty.Data());

                        }

                        deferred.targetObjectIndex =

                            parent.objectIndex;

                        deferred.member =

                            contentOwner.contentMember;

                        deferred.policy =

                            contentOwner.contentPolicy;

                        deferred.hasPolicy = true;

                    } else {

                        return Base::Status::Failure(

                            Base::ErrorCode::InvalidState,

                            "StaticResource parent frame is invalid");

                    }

                    if (deferred.targetObjectIndex < created_.Size()) {

                        created_[deferred.targetObjectIndex]

                            .deferredStaticResource = true;

                    }

                    deferred.source = node.Source();

                    Base::Result<void> key = deferred.key.Assign(

                        extension.ResourceKey());

                    if (!key) return key.GetStatus();

                    deferredStaticResources_.PushBack(

                            std::move(deferred));

                }

                hasDeferredStaticResources_ = true;

                return {};

            }

            Base::Result<Base::String> message =

                StaticResourceNotFoundMessage(extension.ResourceKey());

            if (!message) return message.GetStatus();

            return Failure(

                resource.GetStatus(),

                XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,

                message.Value().View(),

                node.Source());

        }

        frames_.PopBack();

        PopNamespaceBindings(frame.namespaceBindingStart);

        return WriteValueToParent(

            std::move(resource).Value(), node.Source());

    }



    if (!record.name.Empty() && !record.nameRegistered) {

        Base::Result<void> nameResult = RegisterObjectName(

            objectIndex,

            node.Source());

        if (!nameResult) {

            return nameResult.GetStatus();

        }

    }



    // BasedOn="{StaticResource ...}" is queued when the referenced key is

    // not yet visible (merged Source dictionaries commit after EndObject).

    // Sealing now would reject the deferred BasedOn write. Applies to Style

    // and to ControlTemplate (template inheritance).

    if (!(record.deferredStaticResource &&

          (record.type == Aero::Style::StaticTypeId() ||

           record.type == Aero::Controls::ControlTemplate::StaticTypeId()))) {

    Base::Result<void> endResult = schema_->EndInit(

        record.type,

        *record.object,

        BuildExtensionServices(

            objectIndex,

            {},

            node.Source()));

    if (!endResult) {

        return Failure(

            endResult.GetStatus(),

            XamlObjectWriterDiagnosticCodes::InitializationFailed,

            MessageInitializationFailed,

            node.Source());

    }

    }

    record.endCalled = true;



    frames_.PopBack();

    PopNamespaceBindings(frame.namespaceBindingStart);



    Base::Result<bool> resourceResult = RegisterObjectResource(

        objectIndex,

        node.Source());

    if (!resourceResult) {

        return resourceResult.GetStatus();

    }

    if (resourceResult.Value()) {

        return {};

    }

    return WriteObjectToParent(objectIndex, node.Source());

}



Base::Result<void> ObjectWriter::CompleteValueObject(

    const Node& node) noexcept {

    if (frames_.Empty() ||

        frames_.Back().kind != FrameKind::ValueObject) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            node.Source());

    }

    const Frame frame = frames_.Back();

    if (frame.objectIndex >= created_.Size() ||

        created_[frame.objectIndex].value.IsUnset()) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::ValidationFailed,

                MessageMissingMemberValue.Data()),

            XamlObjectWriterDiagnosticCodes::MissingMemberValue,

            MessageMissingMemberValue,

            node.Source());

    }

    frames_.PopBack();

    PopNamespaceBindings(frame.namespaceBindingStart);

    Base::Result<bool> resource = RegisterObjectResource(

        frame.objectIndex, node.Source());

    if (!resource) return resource.GetStatus();

    if (resource.Value()) return {};

    return WriteValueToParent(

        std::move(created_[frame.objectIndex].value),

        node.Source());

}



Base::Result<void> ObjectWriter::CompleteNullObject(

    const Node& node) noexcept {

    if (frames_.Empty() || frames_.Back().kind != FrameKind::NullObject) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            node.Source());

    }

    const std::uint32_t bindingStart =

        frames_.Back().namespaceBindingStart;

    frames_.PopBack();

    PopNamespaceBindings(bindingStart);

    return WriteNullToParent(node.Source());

}



Base::Result<void> ObjectWriter::WriteValueToParent(

    Meta::Value&& value,

    ::Aero::Diagnostics::SourceSpan source) noexcept {

    if (frames_.Empty()) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::ValidationFailed,

                MessageTypeMismatch.Data()),

            XamlObjectWriterDiagnosticCodes::TypeMismatch,

            MessageTypeMismatch,

            source);

    }

    Frame& parent = frames_.Back();

    if (parent.kind == FrameKind::Member) {

        return WriteValueToMember(parent, std::move(value), source);

    }

    if (parent.kind != FrameKind::Object ||

        parent.objectIndex >= created_.Size()) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            source);

    }

    const CreatedObjectRecord& parentRecord = created_[parent.objectIndex];

    if (parentRecord.hasContentMember) {

        return WriteValue(

            parent.objectIndex,

            parentRecord.contentMember,

            std::move(value),

            source);

    }



    Base::Result<ResolvedMember> content =

        schema_->ResolveContentMember(

            parentRecord.type);

    if (!content) {

        return Failure(

            content.GetStatus(),

            XamlObjectWriterDiagnosticCodes::MissingContentProperty,

            MessageMissingContentProperty,

            source);

    }

    return WriteValue(

        parent.objectIndex,

        content.Value(),

        std::move(value),

        source);

}



Base::Result<void> ObjectWriter::WriteObjectToParent(

    std::uint32_t objectIndex,

    ::Aero::Diagnostics::SourceSpan source) noexcept {

    if (objectIndex >= created_.Size()) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            source);

    }



    if (frames_.Empty()) {

        if (root_) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::AlreadyExists,

                    MessageMultipleRoots.Data()),

                XamlObjectWriterDiagnosticCodes::MultipleRootObjects,

                MessageMultipleRoots,

                source);

        }

        root_ = created_[objectIndex].object;

        return {};

    }



    Frame& parent = frames_.Back();

    if (parent.kind == FrameKind::Member) {

        Meta::Value value = Meta::Value::FromObject(

            created_[objectIndex].type,

            created_[objectIndex].object);

        return WriteValueToMember(parent, std::move(value), source);

    }

    if (parent.kind != FrameKind::Object) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            source);

    }



    return WriteObjectToContent(

        parent.objectIndex,

        objectIndex,

        source);

}



Base::Result<void> ObjectWriter::WriteObjectToContent(

    std::uint32_t parentObjectIndex,

    std::uint32_t childObjectIndex,

    ::Aero::Diagnostics::SourceSpan source) noexcept {

    if (parentObjectIndex >= created_.Size() ||

        childObjectIndex >= created_.Size()) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            source);

    }



    const CreatedObjectRecord& contentOwner =

        created_[parentObjectIndex];



    if (!contentOwner.hasContentMember) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::NotFound,

                MessageMissingContentProperty.Data()),

            XamlObjectWriterDiagnosticCodes::MissingContentProperty,

            MessageMissingContentProperty,

            source);

    }



    Meta::Value value = Meta::Value::FromObject(

        created_[childObjectIndex].type,

        created_[childObjectIndex].object);

    return WriteValue(

        parentObjectIndex,

        contentOwner.contentMember,

        std::move(value),

        source,

        &contentOwner.contentPolicy);

}



Base::Result<void> ObjectWriter::WriteNullToParent(

    ::Aero::Diagnostics::SourceSpan source) noexcept {

    if (frames_.Empty()) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::ValidationFailed,

                MessageNullNotAllowed.Data()),

            XamlObjectWriterDiagnosticCodes::NullNotAllowed,

            MessageNullNotAllowed,

            source);

    }



    Frame& parent = frames_.Back();

    if (parent.kind == FrameKind::Member) {

        if (parent.targetObjectIndex < created_.Size() &&

            parent.member.id ==

                Controls::Primitives::ToggleButton::

                    IsCheckedProperty.Handle().value &&

            schema_->Types().IsDerivedFrom(

                created_[parent.targetObjectIndex].type,

                Controls::Primitives::ToggleButton::

                    StaticTypeId())) {

            Base::Result<Meta::Value> nullable =

                Meta::ValueCodec<Nullable<bool>>::Encode(

                    Nullable<bool>{});

            if (!nullable) return nullable.GetStatus();

            Base::Result<void> written = WriteValueToMember(

                parent, std::move(nullable).Value(), source);

            if (!written) return written.GetStatus();

            return {};

        }

        const MemberWritePolicy policy =

            parent.hasMemberPolicy

            ? parent.memberPolicy

            : schema_->ResolveMemberWritePolicy(

                  parent.member);

        const bool acceptsAnyValue =

            policy.acceptsAnyValue;

        if (parent.member.valueType == Meta::TypeOf<Base::String>()) {

            Base::Result<Meta::Value> empty =

                Meta::Value::TryFromString(

                    Meta::TypeOf<Base::String>(), {});

            if (!empty) return empty.GetStatus();

            return WriteValueToMember(

                parent, std::move(empty).Value(), source);

        }

        if (parent.memberValueTypeIsValueType &&

            !acceptsAnyValue) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::ValidationFailed,

                    MessageNullNotAllowed.Data()),

                XamlObjectWriterDiagnosticCodes::NullNotAllowed,

                MessageNullNotAllowed,

                source);

        }

        Meta::Value value = Meta::Value::NullObject(parent.member.valueType);

        return WriteValueToMember(parent, std::move(value), source);

    }

    if (parent.kind != FrameKind::Object ||

        parent.objectIndex >= created_.Size()) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            source);

    }



    const CreatedObjectRecord& contentOwner =

        created_[parent.objectIndex];

    if (!contentOwner.hasContentMember ||

        contentOwner.contentValueTypeIsValueType) {

        return Failure(

            Base::Status::Failure(

                contentOwner.hasContentMember

                    ? Base::ErrorCode::ValidationFailed

                    : Base::ErrorCode::NotFound,

                MessageNullNotAllowed.Data()),

            XamlObjectWriterDiagnosticCodes::NullNotAllowed,

            MessageNullNotAllowed,

            source);

    }



    Meta::Value value = Meta::Value::NullObject(

        contentOwner.contentMember.valueType);

    return WriteValue(

        parent.objectIndex,

        contentOwner.contentMember,

        std::move(value),

        source,

        &contentOwner.contentPolicy);

}



Base::Result<void> ObjectWriter::WriteValueToMember(

    Frame& memberFrame,

    Meta::Value&& value,

    ::Aero::Diagnostics::SourceSpan source) noexcept {

    Base::Result<void> result = WriteValue(

        memberFrame.targetObjectIndex,

        memberFrame.member,

        std::move(value),

        source,

        memberFrame.hasMemberPolicy

            ? &memberFrame.memberPolicy

            : nullptr);

    if (!result) {

        return result.GetStatus();

    }

    if (memberFrame.valuesWritten == UINT32_MAX) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::OutOfRange,

                MessageDuplicateMemberValue.Data()),

            XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,

            MessageDuplicateMemberValue,

            source);

    }

    ++memberFrame.valuesWritten;

    return {};

}



Base::Result<void> ObjectWriter::WriteProvidedValueToMember(

    Frame& memberFrame,

    ProvidedValue&& provided,

    ::Aero::Diagnostics::SourceSpan source) noexcept {

    Base::Result<void> result = WriteProvidedValue(

        memberFrame.targetObjectIndex,

        memberFrame.member,

        std::move(provided),

        source,

        memberFrame.hasMemberPolicy

            ? &memberFrame.memberPolicy

            : nullptr);

    if (!result) return result.GetStatus();

    if (memberFrame.valuesWritten == UINT32_MAX) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::OutOfRange,

                MessageDuplicateMemberValue.Data()),

            XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,

            MessageDuplicateMemberValue,

            source);

    }

    ++memberFrame.valuesWritten;

    return {};

}



Base::Result<void> ObjectWriter::WriteProvidedValue(

    std::uint32_t targetObjectIndex,

    const ResolvedMember& member,

    ProvidedValue&& provided,

    ::Aero::Diagnostics::SourceSpan source,

    const MemberWritePolicy* compiledPolicy) noexcept {

    if (provided.kind == ProvidedValueKind::Value) {

        return WriteValue(

            targetObjectIndex,

            member,

            std::move(provided.value),

            source,

            compiledPolicy);

    }

    if (targetObjectIndex >= created_.Size()) {

        provided.Discard();

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            source);

    }



    const MemberWritePolicy policy =

        compiledPolicy != nullptr

        ? *compiledPolicy

        : schema_->ResolveMemberWritePolicy(member);

    AssignmentRecord* assignment = FindAssignment(

        targetObjectIndex, member.id);

    if (!policy.writable ||

        (assignment != nullptr && assignment->count != 0U &&

         policy.mode == MemberWriteMode::SetOnce)) {

        provided.Discard();

        return Failure(

            Base::Status::Failure(

                policy.writable

                    ? Base::ErrorCode::AlreadyExists

                    : Base::ErrorCode::Unsupported,

                policy.writable

                    ? MessageDuplicateMemberValue.Data()

                    : MessageUnsupportedMember.Data()),

            policy.writable

                ? XamlObjectWriterDiagnosticCodes::DuplicateMemberValue

                : XamlObjectWriterDiagnosticCodes::UnsupportedMember,

            policy.writable

                ? MessageDuplicateMemberValue

                : MessageUnsupportedMember,

            source);

    }

    if (assignment == nullptr) {

        assignments_.PushBack({

            targetObjectIndex, member.id, 0U});

        assignment = &assignments_.Back();

    }



    CommittedEffect effect;

    if (loadContext_ != nullptr) {

        effect.lifetime = loadContext_->effectLifetime;

    }

    if (provided.kind == ProvidedValueKind::Expression) {

        if (provided.effectiveValues == nullptr ||

            !provided.expression.IsValid()) {

            provided.Discard();

            return Base::Status::Failure(

                Base::ErrorCode::InvalidState,

                "Markup extension returned an invalid property expression");

        }

        Base::Result<::Aero::DependencyObject*> target =

            schema_->ResolvePropertyTarget(

                *created_[targetObjectIndex].object);

        if (!target) {

            provided.Discard();

            return target.GetStatus();

        }

        effect.effectiveValues = provided.effectiveValues;

        effect.targetOwner =

            Base::Ref<::Aero::DependencyObject>::TryFromBorrowed(

                *target.Value());

        if (!effect.targetOwner) {

            provided.Discard();

            return Base::Status::Failure(

                Base::ErrorCode::InvalidState,

                "Markup expression target is not reference-counted");

        }

        effect.target = effect.targetOwner.Get();

        effect.property = Meta::DependencyPropertyHandle{member.id};

        effect.pendingExpression = provided.expression;

        provided.expression = {};

    } else if (provided.kind == ProvidedValueKind::Handled) {

        effect.context = provided.rollbackContext;

        effect.token = provided.rollbackToken;

        effect.rollback = provided.rollback;

        effect.committed = true;

        provided.rollbackContext = nullptr;

        provided.rollback = nullptr;

    } else if (provided.kind == ProvidedValueKind::Deferred) {

        effect.context = provided.rollbackContext;

        effect.prepare = provided.prepare;

        effect.commit = provided.commit;

        effect.rollback = provided.rollback;

        effect.cleanup = provided.cleanup;

        effect.bind = provided.bind;

        provided.rollbackContext = nullptr;

        provided.prepare = nullptr;

        provided.commit = nullptr;

        provided.rollback = nullptr;

        provided.cleanup = nullptr;

        provided.bind = nullptr;

    } else {

        provided.Discard();

        return Base::Status::Failure(

            Base::ErrorCode::InvalidState,

            "Markup extension returned an unknown value kind");

    }



    const bool immediate = loadContext_ == nullptr ||

        loadContext_->effectCommitMode == EffectCommitMode::Immediate;

    if (immediate && !effect.committed) {

        Base::Result<void> committed = effect.Commit();

        if (!committed) {

            effect.Rollback();

            return committed.GetStatus();

        }

    }

    extensionEffects_.PushBack(std::move(effect));

    if (assignment->count == UINT32_MAX) {

        return Base::Status::Failure(

            Base::ErrorCode::OutOfRange,

            MessageDuplicateMemberValue.Data());

    }

    ++assignment->count;

    return {};

}



Base::Result<void> ObjectWriter::WriteValue(

    std::uint32_t targetObjectIndex,

    const ResolvedMember& member,

    Meta::Value&& value,

    ::Aero::Diagnostics::SourceSpan source,

    const MemberWritePolicy* compiledPolicy) noexcept {

    if (targetObjectIndex >= created_.Size()) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            source);

    }

    if (value.Kind() == Meta::ValueKind::Object &&

        !value.IsNullObject() &&

        value.AsObject()) {

        if (MarkupExtension* extension =
                ::Aero::TryCast<MarkupExtension>(value.AsObject().Get())) {
            const ExtensionServices services =
                BuildExtensionServices(
                    targetObjectIndex,
                    member,
                    source);
            MarkupExtensionContext context = MakeMarkupExtensionContext(services);
            Base::Result<Meta::Value> provided = extension->ProvideValue(context);
            if (!provided) {
                return Failure(
                    provided.GetStatus(),
                    XamlObjectWriterDiagnosticCodes::MarkupExtensionFailed,
                    MessageMarkupExtensionFailed,
                    source);
            }
            value = std::move(provided).Value();
        }

    }

    if (value.Kind() == Meta::ValueKind::Object &&

        !value.IsNullObject() &&

        value.AsObject() &&

        value.AsObject()->RuntimeType() ==

            Data::MultiBinding::StaticTypeId()) {

        const ExtensionServices services =

            BuildExtensionServices(

                targetObjectIndex,

                member,

                source);

        Base::Result<ProvidedValue> provided =

            CreateMultiBindingValue(

                static_cast<Data::MultiBinding&>(

                    *value.AsObject()),

                services);

        if (!provided) {

            return Failure(

                provided.GetStatus(),

                XamlObjectWriterDiagnosticCodes::InvalidValue,

                MessageInvalidValue,

                source);

        }

        return WriteProvidedValue(

            targetObjectIndex,

            member,

            std::move(provided).Value(),

            source,

            compiledPolicy);

    }





    const MemberWritePolicy policy =

        compiledPolicy != nullptr

        ? *compiledPolicy

        : schema_->ResolveMemberWritePolicy(member);

    if (!policy.writable) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::Unsupported,

                MessageUnsupportedMember.Data()),

            XamlObjectWriterDiagnosticCodes::UnsupportedMember,

            MessageUnsupportedMember,

            source);

    }



    AssignmentRecord* assignment = FindAssignment(

        targetObjectIndex,

        member.id);

    if (assignment != nullptr && assignment->count != 0U &&

        policy.mode == MemberWriteMode::SetOnce) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::AlreadyExists,

                MessageDuplicateMemberValue.Data()),

            XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,

            MessageDuplicateMemberValue,

            source);

    }



    if (assignment == nullptr) {

        assignments_.PushBack({

            targetObjectIndex,

            member.id,

            0U});

        assignment = &assignments_.Back();

    }



    CreatedObjectRecord& targetRecord =

        created_[targetObjectIndex];

    if (value.Kind() != Meta::ValueKind::Object &&

        (member.valueType == Meta::TypeOf<Base::Object>() ||

         (targetRecord.hasContentMember &&

          targetRecord.contentMember.id == member.id &&

          schema_->Types().IsDerivedFrom(

              targetRecord.type,

              Controls::ItemsControl::StaticTypeId())))) {

        Base::Result<Base::Ref<

            Controls::BoxedItemValue>> boxed =

                Base::MakeRef<Controls::BoxedItemValue>(

                    value);

        if (!boxed) return boxed.GetStatus();

        value = Meta::Value::FromObject(

            member.valueType,

            Base::Ref<Base::Object>(

                std::move(boxed).Value()));

    }



    const ExtensionServices services = BuildExtensionServices(

        targetObjectIndex,

        member,

        source);

    Base::Result<Meta::Value> coerced =
        CoerceXamlValueForProperty(member.valueType, std::move(value));
    if (!coerced) {
        return Failure(
            coerced.GetStatus(),
            XamlObjectWriterDiagnosticCodes::TypeMismatch,
            MessageTypeMismatch,
            source);
    }
    value = std::move(coerced).Value();

    Base::Result<Meta::ContentInfo> content =

        schema_->Metadata()->GetContentInfo(member.id);

    const bool hasWritableContent =

        content && content.Value().writable;

    const bool hasVisualContent =

        hasWritableContent && content.Value().IsVisual();

    const Meta::PropertyInfo* memberProperty =

        schema_->Types().FindProperty(member.id);

    const auto isUIElementValue =

        [this](const Meta::Value& candidate) noexcept {

            return candidate.Kind() == Meta::ValueKind::Object &&

                !candidate.IsNullObject() &&

                candidate.AsObject() &&

                schema_->Types().IsDerivedFrom(

                    candidate.AsObject()->RuntimeType(),

                    Aero::UIElement::StaticTypeId());

        };

    const bool hasVisualStructuralProperty =

        !hasWritableContent &&

        memberProperty != nullptr &&

        (static_cast<std::uint32_t>(

             memberProperty->Flags()) &

         static_cast<std::uint32_t>(

             Meta::PropertyFlags::Structural)) !=

            0U &&

        (static_cast<std::uint32_t>(

             memberProperty->Flags()) &

         static_cast<std::uint32_t>(

             Meta::PropertyFlags::Collection)) ==

            0U &&

        schema_->Metadata()->

            CanWriteProperty(member.id);

    const bool isDependencyObjectValue =

        value.Kind() == Meta::ValueKind::Object &&

        !value.IsNullObject() && value.AsObject() &&

        schema_->Types().IsDerivedFrom(

            value.AsObject()->RuntimeType(),

            Aero::DependencyObject::StaticTypeId());

    const bool stagesVisualContent =

        (hasVisualContent ||

         hasVisualStructuralProperty) &&

        isUIElementValue(value);

    const bool parentIsVisual =

        schema_->Types().IsDerivedFrom(

            created_[targetObjectIndex].type,

            Aero::Media::Visual::StaticTypeId());

    const bool parentIsFreezable =

        schema_->Types().IsDerivedFrom(

            created_[targetObjectIndex].type,

            Aero::Freezable::StaticTypeId());

    const bool parentIsTimeline =

        schema_->Types().IsDerivedFrom(

            created_[targetObjectIndex].type,

            Media::Animation::Timeline::StaticTypeId());

    const bool stagesDeferredObjectContent =

        services.deferredContentOwner != nullptr &&

        (hasWritableContent || hasVisualStructuralProperty) &&

        isDependencyObjectValue &&

        (services.deferredContentOwner ==

             created_[targetObjectIndex].object.Get() ||

         parentIsVisual ||

         (parentIsFreezable && !parentIsTimeline));



    Base::Result<void> setResult =

        (stagesVisualContent || stagesDeferredObjectContent)

        ? ObjectWriter::StageContent(

              *schema_,

              *created_[targetObjectIndex].object,

              value,

              services)

        : schema_->SetMember(

              *created_[targetObjectIndex].object,

              created_[targetObjectIndex].type,

              member,

              value);

    if (setResult &&

        hasVisualContent &&

        !stagesVisualContent &&

        schema_->Metadata()->CanReadProperty(member.id)) {

        Base::Result<Meta::Value> materialized =

            schema_->Metadata()->GetProperty(

                *created_[targetObjectIndex].object,

                member.id);

        if (materialized &&

            isUIElementValue(materialized.Value())) {

            setResult = ObjectWriter::StageContent(

                *schema_,

                *created_[targetObjectIndex].object,

                materialized.Value(),

                services);

        }

    }

    if (!setResult) {

        ::Aero::Diagnostics::DiagnosticCode code =

            XamlObjectWriterDiagnosticCodes::InvalidValue;

        Base::StringView message = MessageInvalidValue;

        if (setResult.GetStatus().code == Base::ErrorCode::Unsupported) {

            code = XamlObjectWriterDiagnosticCodes::UnsupportedMember;

            message = MessageUnsupportedMember;

        } else if (setResult.GetStatus().code ==

            Base::ErrorCode::InvalidArgument) {

            code = XamlObjectWriterDiagnosticCodes::TypeMismatch;

            message = MessageTypeMismatch;

        }

        return Failure(setResult.GetStatus(), code, message, source);

    }



    if (assignment->count == UINT32_MAX) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::OutOfRange,

                MessageDuplicateMemberValue.Data()),

            XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,

            MessageDuplicateMemberValue,

            source);

    }

    ++assignment->count;

    return {};

}



} // namespace Aero::Markup



// ===== XamlObjectWriterNameScope.cpp =====







// ===== ObjectWriter name-scope / deferrals =====



namespace Aero::Markup {

using namespace WriterSupport;



Base::Result<void> ObjectWriter::RegisterObjectName(

    std::uint32_t objectIndex,

    ::Aero::Diagnostics::SourceSpan source) noexcept {

    if (objectIndex >= created_.Size()) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            source);

    }



    CreatedObjectRecord& object = created_[objectIndex];

    if (object.name.Empty() || object.nameRegistered) {

        return {};

    }



    const std::uint32_t scopeIndex =

        FindNameScopeIndexForObject(objectIndex);

    if (scopeIndex == InvalidIndex || scopeIndex >= nameScopes_.Size()) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            source);

    }



    NameScopeRecord& scope = nameScopes_[scopeIndex];
    if (scope.names.Find(object.name.View()) != nullptr) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                MessageDuplicateName.Data()),
            XamlObjectWriterDiagnosticCodes::DuplicateName,
            MessageDuplicateName,
            source);
    }
    scope.names.Register(
        object.name.View(),
        *object.object);



    if (scope.ownerObjectIndex >= created_.Size()) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            source);

    }

    CreatedObjectRecord& owner = created_[scope.ownerObjectIndex];

    Base::Result<void> callbackResult = schema_->RegisterName(

        owner.type,

        *owner.object,

        object.name.View(),

        *object.object);

    if (!callbackResult) {

        return Failure(

            callbackResult.GetStatus(),

            XamlObjectWriterDiagnosticCodes::NameRegistrationFailed,

            MessageNameRegistrationFailed,

            source);

    }



    object.nameRegistered = true;

    return {};

}



Base::Result<void> ObjectWriter::ConnectEvent(

    Frame& memberFrame,

    Base::StringView handlerName,

    ::Aero::Diagnostics::SourceSpan source) noexcept {

    if (handlerName.Empty() ||

        memberFrame.targetObjectIndex >= created_.Size() ||

        rootObjectIndex_ >= created_.Size() ||

        memberFrame.member.kind != Meta::MemberKind::Event) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::InvalidArgument,

                "XAML event attribute requires a handler name"),

            XamlObjectWriterDiagnosticCodes::InvalidValue,

            MessageInvalidValue,

            source);

    }



    CreatedObjectRecord& eventSource =

        created_[memberFrame.targetObjectIndex];

    CreatedObjectRecord& codeBehind =

        created_[rootObjectIndex_];

    if (!eventSource.object || !codeBehind.object) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            source);

    }



    Meta::EventHandlerThunk thunk =

        schema_->Metadata()->Types().FindEventHandler(

            codeBehind.type,

            handlerName);

    if (thunk == nullptr) {

        // LoadComponentInto a real code-behind instance must resolve the

        // handler. A pure-XAML host (x:Class omitted or unregistered) stores

        // Click="OnFoo" the same way CommandBinding Executed stores a name:

        // do not abort document construction.

        if (loadContext_ != nullptr && loadContext_->existingRoot) {

            return Failure(

                Base::Status::Failure(

                    Base::ErrorCode::NotFound,

                    "XAML event handler is not registered on the x:Class type"),

                XamlObjectWriterDiagnosticCodes::UnknownMember,

                MessageUnknownMember,

                source);

        }

        return {};

    }



    Base::Result<Base::Ref<XamlEventConnection>> connection =
        Base::MakeRef<XamlEventConnection>(
            Base::WeakRef<Base::Object>(codeBehind.object),
            thunk);

    if (!connection) return connection.GetStatus();

    Base::Delegate<void(Base::Object*, RoutedEventArgs&)> handler(

        XamlEventInvoker{std::move(connection).Value()});

    const RoutedEventHandle routedEvent{memberFrame.member.id};



    Base::Result<void> connected;

    if (schema_->Types().IsDerivedFrom(

            eventSource.type, UIElement::StaticTypeId())) {

        static_cast<UIElement*>(

            eventSource.object.Get())->AddHandler(

                routedEvent, handler);

    } else if (schema_->Types().IsDerivedFrom(

                   eventSource.type,

                   ContentElement::StaticTypeId())) {

        static_cast<ContentElement*>(

            eventSource.object.Get())->AddHandler(

                routedEvent, handler);

    } else {

        connected = Base::Status::Failure(

            Base::ErrorCode::Unsupported,

            "XAML event source does not support routed handlers");

    }

    return connected

        ? Base::Result<void>{}

        : Base::Result<void>(Failure(

              connected.GetStatus(),

              XamlObjectWriterDiagnosticCodes::UnsupportedMember,

              MessageUnsupportedMember,

              source));

}



Base::Result<bool> ObjectWriter::RegisterObjectResource(

    std::uint32_t objectIndex,

    ::Aero::Diagnostics::SourceSpan source) noexcept {

    if (objectIndex >= created_.Size()) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            source);

    }



    CreatedObjectRecord& object = created_[objectIndex];

    if (object.resourceRegistered) {

        return true;

    }



    bool dictionaryContent = false;

    if (!frames_.Empty()) {

        const Frame& parent = frames_.Back();

        std::uint32_t parentObjectIndex = InvalidIndex;

        Meta::MemberId targetMember = Meta::InvalidMemberId;

        if (parent.kind == FrameKind::Object) {

            parentObjectIndex = parent.objectIndex;

        } else if (parent.kind == FrameKind::Member) {

            parentObjectIndex = parent.targetObjectIndex;

            targetMember = parent.member.id;

        }

        if (parentObjectIndex < created_.Size() &&

            created_[parentObjectIndex].type ==

                Aero::ResourceDictionary::StaticTypeId()) {

            const CreatedObjectRecord& dictionary =

                created_[parentObjectIndex];

            dictionaryContent =

                dictionary.hasContentMember &&

                (targetMember == Meta::InvalidMemberId ||

                 targetMember ==

                     dictionary.contentMember.id);

        } else if (

            parentObjectIndex < created_.Size() &&

            targetMember != Meta::InvalidMemberId &&

            object.type !=

                Aero::ResourceDictionary::

                    StaticTypeId() &&

            schema_->CreatesResourceScope(

                created_[parentObjectIndex].type)) {

            dictionaryContent =

                parent.kind == FrameKind::Member &&

                parent.member.valueType ==

                    Aero::ResourceDictionary::

                        StaticTypeId();

        }

    }



    const bool explicitKey = !object.key.Empty();

    if (!dictionaryContent) {

        if (!explicitKey) return false;

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::InvalidState,

                MessageInvalidDirective.Data()),

            XamlObjectWriterDiagnosticCodes::InvalidDirective,

            MessageInvalidDirective,

            source);

    }



    Base::Result<Aero::ResourceKey> resourceKey =

        explicitKey

        ? Aero::ResourceKey::FromString(

              object.key.View())

        : (object.valueElement || !object.object

            ? Base::Result<Aero::ResourceKey>(

                  Base::Status::Failure(

                      Base::ErrorCode::ValidationFailed,

                      "Unkeyed ResourceDictionary entry must be an object"))

            : schema_->ResolveImplicitResourceKey(

                  object.type,

                  *object.object));

    if (!resourceKey) {

        return Failure(

            resourceKey.GetStatus(),

            XamlObjectWriterDiagnosticCodes::ResourceRegistrationFailed,

            MessageResourceRegistrationFailed,

            source);

    }



    const std::uint32_t scopeIndex = FindResourceScopeIndexForParent();

    if (scopeIndex == InvalidIndex || scopeIndex >= resourceScopes_.Size()) {

        return Failure(

            Base::Status::Failure(

                Base::ErrorCode::NotFound,

                MessageMissingResourceScope.Data()),

            XamlObjectWriterDiagnosticCodes::MissingResourceScope,

            MessageMissingResourceScope,

            source);

    }



    ResourceScopeRecord& scope = resourceScopes_[scopeIndex];

    Meta::Value resourceValue = object.valueElement

        ? object.value

        : Meta::Value::FromObject(object.type, object.object);

    if (scope.resources.Contains(resourceKey.Value())) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                MessageDuplicateResourceKey.Data()),
            XamlObjectWriterDiagnosticCodes::DuplicateResourceKey,
            MessageDuplicateResourceKey,
            source);
    }

    scope.resources.Add(
        resourceKey.Value(),
        resourceValue,
        source);



    if (scope.ownerObjectIndex >= created_.Size()) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            source);

    }

    CreatedObjectRecord& owner = created_[scope.ownerObjectIndex];

    Base::Result<void> callbackResult = schema_->AddResource(

        owner.type,

        *owner.object,

        resourceKey.Value(),

        resourceValue);

    if (!callbackResult) {

        return Failure(

            callbackResult.GetStatus(),

            XamlObjectWriterDiagnosticCodes::ResourceRegistrationFailed,

            MessageResourceRegistrationFailed,

            source);

    }



    if (!explicitKey &&

        object.type == Aero::Style::StaticTypeId() &&

        object.object) {

        const auto& style = static_cast<const Aero::Style&>(

            *object.object);

        const Meta::TypeInfo* targetType =

            schema_->Types().FindType(

                style.GetTargetType());

        if (targetType != nullptr &&

            !targetType->Name().Empty()) {

            Base::String alias;

            Base::Result<void> named =

                alias.Assign("Style.");

            if (named) {

                named = alias.Append(

                    targetType->Name());

            }

            if (!named) return named.GetStatus();

            if (!scope.resources.Contains(alias.View())) {

                Base::Result<Aero::ResourceKey> aliasKey =

                    Aero::ResourceKey::FromString(

                        alias.View());

                if (!aliasKey) return aliasKey.GetStatus();

                scope.resources.Add(
                    aliasKey.Value(),
                    resourceValue,
                    source);

                Base::Result<void> callbackAlias =

                    schema_->AddResource(

                        owner.type,

                        *owner.object,

                        aliasKey.Value(),

                        resourceValue);

                if (!callbackAlias) {

                    return Failure(

                        callbackAlias.GetStatus(),

                        XamlObjectWriterDiagnosticCodes::

                            ResourceRegistrationFailed,

                        MessageResourceRegistrationFailed,

                        source);

                }

            }

        }

    }



    object.resourceRegistered = true;

    return true;

}



Base::Result<Aero::ResourceValue> ObjectWriter::LookupResource(

    Base::StringView key) const noexcept {

    constexpr Base::StringView PresentationNamespace(

        "http://schemas.microsoft.com/winfx/2006/xaml/presentation");



    Aero::ResourceKey candidates[4];

    std::uint32_t candidateCount = 0U;

    auto addCandidate = [&](Aero::ResourceKey candidate) noexcept {

        if (!candidate.IsValid() || candidateCount >= 4U) {

            return;

        }

        for (std::uint32_t index = 0U; index < candidateCount; ++index) {

            if (candidates[index] == candidate) {

                return;

            }

        }

        candidates[candidateCount++] = std::move(candidate);

    };

    auto addStringKey = [&](Base::StringView text) noexcept {

        if (text.Empty()) {

            return;

        }

        Base::Result<Aero::ResourceKey> parsed =

            Aero::ResourceKey::FromString(text);

        if (parsed) {

            addCandidate(std::move(parsed).Value());

        }

    };

    auto addTypeCandidates = [&](

        Base::StringView localName,

        Meta::TypeId type) noexcept {

        if (type != Meta::InvalidTypeId) {

            addCandidate(Aero::ResourceKey::FromType(type));

        }

        if (!localName.Empty()) {

            Base::String alias;

            if (alias.Assign("Style.") && alias.Append(localName)) {

                addStringKey(alias.View());

            }

            addStringKey(localName);

        }

    };

    auto splitTypeName = [&](Base::StringView typeName)

        -> Base::Result<std::pair<Base::StringView, Base::StringView>> {

        std::uint32_t colon = typeName.SizeBytes();

        for (std::uint32_t index = 0U;

             index < typeName.SizeBytes();

             ++index) {

            if (typeName[index] != ':') continue;

            if (colon != typeName.SizeBytes()) {

                return Base::Status::Failure(

                    Base::ErrorCode::InvalidArgument,

                    "StaticResource x:Type key contains multiple namespace prefixes");

            }

            colon = index;

        }

        Base::StringView prefix;

        Base::StringView localName = typeName;

        if (colon != typeName.SizeBytes()) {

            if (colon == 0U || colon + 1U >= typeName.SizeBytes()) {

                return Base::Status::Failure(

                    Base::ErrorCode::InvalidArgument,

                    "StaticResource x:Type key namespace prefix is malformed");

            }

            prefix = typeName.Substr(0U, colon);

            localName = typeName.Substr(

                colon + 1U,

                typeName.SizeBytes() - colon - 1U);

        }

        return std::make_pair(prefix, localName);

    };

    auto resolveTypeId = [&](Base::StringView prefix, Base::StringView localName)

        -> Base::Result<Meta::TypeId> {

        Base::StringView namespaceUri = PresentationNamespace;

        Base::Result<Base::StringView> bound = LookupNamespace(prefix);

        if (bound) {

            namespaceUri = bound.Value();

        } else if (!prefix.Empty()) {

            return bound.GetStatus();

        }

        Base::Result<const Meta::TypeInfo*> type =

            schema_->ResolveType(namespaceUri, localName);

        if (!type && prefix.Empty() &&

            namespaceUri != PresentationNamespace) {

            type = schema_->ResolveType(PresentationNamespace, localName);

        }

        if (!type) {

            return type.GetStatus();

        }

        return type->Id();

    };

    auto looksLikeTypeName = [&](Base::StringView text) noexcept -> bool {

        if (text.Empty() || text[0] == '{') {

            return false;

        }

        bool sawColon = false;

        for (std::uint32_t index = 0U; index < text.SizeBytes(); ++index) {

            const char character = text[index];

            if (character == ':') {

                if (sawColon || index == 0U ||

                    index + 1U >= text.SizeBytes()) {

                    return false;

                }

                sawColon = true;

                continue;

            }

            const bool letter =

                (character >= 'A' && character <= 'Z') ||

                (character >= 'a' && character <= 'z') ||

                character == '_';

            const bool digit =

                character >= '0' && character <= '9';

            if (!(letter || (index > 0U && digit))) {

                return false;

            }

        }

        return true;

    };



    Base::StringView extensionName;

    Base::StringView typeArgument;

    const MarkupValueKind markup =

        ParseMarkupValue(key, extensionName, typeArgument);

    if (markup == MarkupValueKind::Extension &&

        extensionName == Base::StringView("x:Type")) {

        Base::Result<std::pair<Base::StringView, Base::StringView>> parts =

            splitTypeName(typeArgument);

        if (!parts) {

            return parts.GetStatus();

        }

        Base::Result<Meta::TypeId> typeId =

            resolveTypeId(parts.Value().first, parts.Value().second);

        if (typeId) {

            addTypeCandidates(parts.Value().second, typeId.Value());

        }

        addStringKey(key);

    } else {

        addStringKey(key);

        if (looksLikeTypeName(key)) {

            Base::Result<std::pair<Base::StringView, Base::StringView>> parts =

                splitTypeName(key);

            if (parts) {

                Base::Result<Meta::TypeId> typeId =

                    resolveTypeId(parts.Value().first, parts.Value().second);

                if (typeId) {

                    addTypeCandidates(parts.Value().second, typeId.Value());

                }

            }

        }

    }

    if (candidateCount == 0U) {

        addStringKey(key);

        if (candidateCount == 0U) {

            return Base::Status::Failure(

                Base::ErrorCode::InvalidArgument,

                "String resource key cannot be empty");

        }

    }



    auto lookupOne = [&](const Aero::ResourceKey& resourceKey)

        -> Base::Result<Aero::ResourceValue> {

        for (std::uint32_t index = frames_.Size(); index > 0U; --index) {

            const Frame& frame = frames_[index - 1U];

            if (frame.kind != FrameKind::Object ||

                frame.resourceScopeIndex == InvalidIndex ||

                frame.resourceScopeIndex >= resourceScopes_.Size()) {

                continue;

            }

            Aero::ResourceValue value =

                (resourceScopes_[frame.resourceScopeIndex].external != nullptr

                    ? resourceScopes_[frame.resourceScopeIndex].external

                    : &resourceScopes_[frame.resourceScopeIndex].resources)

                    ->Lookup(resourceKey);

            if (value.Kind() != Meta::ValueKind::Unset) {

                return value;

            }

        }

        if (Base::Object* deferredOwner = FindDeferredContentOwner()) {

            Aero::ResourceDictionary* templateResources = nullptr;

            const Meta::TypeId ownerType = deferredOwner->RuntimeType();

            if (ownerType == ::Aero::DataTemplate::StaticTypeId() ||

                ownerType == ::Aero::HierarchicalDataTemplate::StaticTypeId()) {

                templateResources =

                    &static_cast<::Aero::DataTemplate&>(

                        *deferredOwner).GetResources();

            } else if (

                ownerType == Controls::ItemsPanelTemplate::StaticTypeId()) {

                templateResources =

                    &static_cast<Controls::ItemsPanelTemplate&>(

                        *deferredOwner).GetResources();

            } else if (

                ownerType == Controls::ControlTemplate::StaticTypeId()) {

                templateResources =

                    &static_cast<Controls::ControlTemplate&>(

                        *deferredOwner).GetResources();

            }

            if (templateResources != nullptr) {

                Aero::ResourceValue value =

                    templateResources->Lookup(resourceKey);

                if (value.Kind() != Meta::ValueKind::Unset) return value;

            }

        }

        // WPF: a later merged dictionary's StaticResource sees keys from earlier

        // application merged dictionaries AND from nested MergedDictionaries, even

        // while this document still has an object/member frame on the stack.

        for (std::uint32_t index = resourceScopes_.Size();

             index > 0U; --index) {

            const ResourceScopeRecord& scope =

                resourceScopes_[index - 1U];

            const Aero::ResourceDictionary* dictionary =

                scope.external != nullptr

                    ? scope.external

                    : &scope.resources;

            Aero::ResourceValue value =

                dictionary->Lookup(resourceKey);

            if (value.Kind() != Meta::ValueKind::Unset) return value;

        }

        {

            Aero::ResourceValue committed =

                committedResources_.Lookup(resourceKey);

            if (committed.Kind() != Meta::ValueKind::Unset) return committed;

        }

        if (loadContext_ != nullptr && loadContext_->resources != nullptr) {

            Aero::ResourceValue value =

                loadContext_->resources->Lookup(resourceKey);

            if (value.Kind() != Meta::ValueKind::Unset) {

                return value;

            }

        }

        if (loadContext_ != nullptr &&

            loadContext_->fallbackResources != nullptr &&

            loadContext_->fallbackResources != loadContext_->resources) {

            Aero::ResourceValue value =

                loadContext_->fallbackResources->Lookup(resourceKey);

            if (value.Kind() != Meta::ValueKind::Unset) {

                return value;

            }

        }

        return Base::Status::Failure(

            Base::ErrorCode::NotFound,

            MessageStaticResourceNotFound.Data());

    };



    Base::Result<Aero::ResourceValue> last =

        Base::Status::Failure(

            Base::ErrorCode::NotFound,

            MessageStaticResourceNotFound.Data());

    for (std::uint32_t index = 0U; index < candidateCount; ++index) {

        Base::Result<Aero::ResourceValue> value =

            lookupOne(candidates[index]);

        if (value) {

            return value;

        }

        if (value.GetStatus().code != Base::ErrorCode::NotFound) {

            return value.GetStatus();

        }

        last = std::move(value);

    }

    return last;

}



Base::Result<void> ObjectWriter::CreateScopesForObject(

    std::uint32_t objectIndex,

    Frame& frame,

    ::Aero::Diagnostics::SourceSpan source) noexcept {

    if (objectIndex >= created_.Size()) {

        return Failure(

            InvalidStateStatus(),

            XamlObjectWriterDiagnosticCodes::InvalidWriterState,

            MessageInvalidWriterState,

            source);

    }



    const bool documentRoot = objectIndex == rootObjectIndex_;

    if (documentRoot || schema_->CreatesNameScope(created_[objectIndex].type)) {

        NameScopeRecord scope;

        scope.ownerObjectIndex = objectIndex;

        const std::uint32_t index = nameScopes_.Size();

        nameScopes_.PushBack(std::move(scope));

        frame.nameScopeIndex = index;

        if (documentRoot) {

            documentNameScopeIndex_ = index;

        }

    }



    if (documentRoot ||

        schema_->CreatesResourceScope(created_[objectIndex].type)) {

        ResourceScopeRecord scope;

        scope.ownerObjectIndex = objectIndex;

        scope.external = schema_->ResolveResourceScope(

            created_[objectIndex].type,

            *created_[objectIndex].object);

        const std::uint32_t index = resourceScopes_.Size();

        resourceScopes_.PushBack(std::move(scope));

        frame.resourceScopeIndex = index;

        if (documentRoot) {

            documentResourceScopeIndex_ = index;

        }

    }

    return {};

}



Base::Result<void> ObjectWriter::ActivatePendingNamespaces(

    std::uint32_t& bindingStart) noexcept {

    bindingStart = namespaceBindings_.Size();

    for (PendingNamespaceRecord& pending : pendingNamespaces_) {

        NamespaceBindingRecord binding;

        Base::Result<void> prefixResult = binding.prefix.AssignUnchecked(

            pending.prefix.View());

        if (!prefixResult) {

            return prefixResult.GetStatus();

        }

        Base::Result<void> uriResult = binding.uri.AssignUnchecked(

            pending.uri.View());

        if (!uriResult) {

            return uriResult.GetStatus();

        }

        namespaceBindings_.PushBack(std::move(binding));

    }

    pendingNamespaces_.Clear();

    return {};

}



void ObjectWriter::PopNamespaceBindings(

    std::uint32_t bindingStart) noexcept {

    if (bindingStart == InvalidIndex) {

        return;

    }

    while (namespaceBindings_.Size() > bindingStart) {

        namespaceBindings_.PopBack();

    }

}



Base::Result<Base::StringView> ObjectWriter::LookupNamespace(

    Base::StringView prefix) const noexcept {

    if (prefix == XmlPrefix) {

        return XmlNamespaceUri;

    }

    for (std::uint32_t index = namespaceBindings_.Size();

         index > 0U;

         --index) {

        const NamespaceBindingRecord& binding =

            namespaceBindings_[index - 1U];

        if (binding.prefix.View() == prefix) {

            return binding.uri.View();

        }

    }

    return Base::Status::Failure(

        Base::ErrorCode::NotFound,

        "XAML namespace prefix is not bound in the active scope");

}



ExtensionServices ObjectWriter::BuildExtensionServices(

    std::uint32_t targetObjectIndex,

    const ResolvedMember& member,

    ::Aero::Diagnostics::SourceSpan source) noexcept {

    ExtensionServices services;

    services.schema = schema_;

    if (targetObjectIndex < created_.Size()) {

        services.targetObject = created_[targetObjectIndex].object.Get();

        services.targetObjectType = created_[targetObjectIndex].type;

    }

    services.targetMember = member.id;

    services.targetValueType = member.valueType;

    if (rootObjectIndex_ < created_.Size()) {

        services.rootObject = created_[rootObjectIndex_].object.Get();

    }

    if (loadContext_ != nullptr) {

        services.templatedParent = loadContext_->templatedParent;

        services.baseUri = loadContext_->baseUri;

        services.effectiveValues = loadContext_->effectiveValues;

        services.bindings = loadContext_->bindings;

        services.fallbackResources = loadContext_->fallbackResources;

    }

    services.source = source;

    services.nameScope = FindActiveNameScope();

    services.namespaces = NamespaceScope(

        &ObjectWriter::NamespaceLookupCallback,

        this);

    services.resources = ResourceResolver(

        &ObjectWriter::ResourceLookupCallback,

        this);

    serviceResourceChain_.Clear();

    for (std::uint32_t index = frames_.Size();

         index > 0U; --index) {

        const Frame& frame = frames_[index - 1U];

        if (frame.kind != FrameKind::Object ||

            frame.resourceScopeIndex == InvalidIndex ||

            frame.resourceScopeIndex >= resourceScopes_.Size()) {

            continue;

        }

        const Aero::ResourceDictionary* dictionary =

            resourceScopes_[frame.resourceScopeIndex].external;

        if (dictionary == nullptr) {

            dictionary =

                &resourceScopes_[

                    frame.resourceScopeIndex].resources;

        }

        bool duplicate = false;

        for (const Aero::ResourceDictionary* existing :

             serviceResourceChain_) {

            if (existing == dictionary) {

                duplicate = true;

                break;

            }

        }

        if (!duplicate) {

            serviceResourceChain_.PushBack(dictionary);

        }

    }

    if (loadContext_ != nullptr &&

        loadContext_->resources != nullptr) {

        bool duplicate = false;

        for (const Aero::ResourceDictionary* existing :

             serviceResourceChain_) {

            if (existing == loadContext_->resources) {

                duplicate = true;

                break;

            }

        }

        if (!duplicate) {

            serviceResourceChain_.PushBack(

                    loadContext_->resources);

        }

    }

    services.ambientResourceChain = {

        serviceResourceChain_.Data(),

        serviceResourceChain_.Size()};

    services.visualContent = &resultVisualContent_;

    services.deferredContentOwner =

        FindDeferredContentOwner();

    services.deferredContent =

        &deferredContent_;

    return services;

}



const Aero::NameScope*

ObjectWriter::FindActiveNameScope() const noexcept {

    for (std::uint32_t index = frames_.Size(); index > 0U; --index) {

        const Frame& frame = frames_[index - 1U];

        if (frame.kind == FrameKind::Object &&

            frame.nameScopeIndex != InvalidIndex &&

            frame.nameScopeIndex < nameScopes_.Size()) {

            return &nameScopes_[frame.nameScopeIndex].names;

        }

    }

    return nullptr;

}



Base::Object*

ObjectWriter::FindDeferredContentOwner() const noexcept {

    for (std::uint32_t index = frames_.Size();

         index > 0U;

         --index) {

        const Frame& frame = frames_[index - 1U];

        if (frame.kind != FrameKind::Object ||

            frame.objectIndex >= created_.Size()) {

            continue;

        }

        const CreatedObjectRecord& record =

            created_[frame.objectIndex];

        if (schema_->DefersVisualContent(

                record.type)) {

            return record.object.Get();

        }

    }

    return nullptr;

}



std::uint32_t ObjectWriter::FindNameScopeIndexForObject(

    std::uint32_t objectIndex) const noexcept {

    const std::uint32_t objectFrame = FindObjectFrameIndex(objectIndex);

    if (objectFrame == InvalidIndex) {

        return InvalidIndex;

    }

    for (std::uint32_t index = objectFrame + 1U; index > 0U; --index) {

        const Frame& frame = frames_[index - 1U];

        if (frame.kind != FrameKind::Object ||

            frame.nameScopeIndex == InvalidIndex) {

            continue;

        }

        // UserControl/Page own a nested NameScope for names inside their

        // content. Their own x:Name belongs to the enclosing namescope so

        // sibling ElementName bindings (MultiBinding ElementName=BgR) resolve.

        if (frame.objectIndex == objectIndex &&

            frame.nameScopeIndex != documentNameScopeIndex_) {

            continue;

        }

        return frame.nameScopeIndex;

    }

    return documentNameScopeIndex_;

}



std::uint32_t ObjectWriter::FindResourceScopeIndexForParent() const noexcept {

    for (std::uint32_t index = frames_.Size(); index > 0U; --index) {

        const Frame& frame = frames_[index - 1U];

        if (frame.kind == FrameKind::Object &&

            frame.resourceScopeIndex != InvalidIndex) {

            return frame.resourceScopeIndex;

        }

    }

    return InvalidIndex;

}



std::uint32_t ObjectWriter::FindObjectFrameIndex(

    std::uint32_t objectIndex) const noexcept {

    for (std::uint32_t index = frames_.Size(); index > 0U; --index) {

        const Frame& frame = frames_[index - 1U];

        if (frame.kind == FrameKind::Object &&

            frame.objectIndex == objectIndex) {

            return index - 1U;

        }

    }

    return InvalidIndex;

}



bool ObjectWriter::IsXamlDirective(

    const QualifiedName& name,

    Base::StringView localName) const noexcept {

    return name.NamespaceUri() == LanguageNamespaceUri() &&

        name.LocalName() == localName;

}



bool ObjectWriter::IsXamlNullObject(

    const QualifiedName& name) const noexcept {

    return IsXamlDirective(name, DirectiveNull);

}



bool ObjectWriter::HasPropertyElementSyntax(

    const QualifiedName& name) const noexcept {

    const Base::StringView local = name.LocalName();

    return local.Data() != nullptr &&

        std::memchr(local.Data(), '.', local.SizeBytes()) != nullptr;

}



bool ObjectWriter::IsWhitespaceOnly(

    Base::StringView value) const noexcept {

    for (char character : value) {

        if (!IsAsciiWhitespace(character)) {

            return false;

        }

    }

    return true;

}



ObjectWriter::AssignmentRecord* ObjectWriter::FindAssignment(

    std::uint32_t objectIndex,

    Meta::MemberId member) noexcept {

    for (AssignmentRecord& assignment : assignments_) {

        if (assignment.objectIndex == objectIndex &&

            assignment.member == member) {

            return &assignment;

        }

    }

    return nullptr;

}



void ObjectWriter::CommitDocumentScopes() noexcept {

    committedNames_.Clear();

    committedResources_.Clear();

    if (documentNameScopeIndex_ < nameScopes_.Size()) {

        committedNames_ = std::move(

            nameScopes_[documentNameScopeIndex_].names);

    }

    if (documentResourceScopeIndex_ < resourceScopes_.Size()) {

        committedResources_ = std::move(

            resourceScopes_[documentResourceScopeIndex_].resources);

    }

}



void ObjectWriter::AbortTransaction() noexcept {

    root_.Reset();

    resultVisualContent_.ReleaseContent();

    resultVisualContent_.Clear();

    deferredContent_.ReleaseAll();

    for (std::uint32_t index = extensionEffects_.Size();

         index > 0U; --index) {

        extensionEffects_[index - 1U].Rollback();

    }

    extensionEffects_.Clear();

    for (std::uint32_t index = created_.Size(); index > 0U; --index) {

        CreatedObjectRecord& record = created_[index - 1U];

        if (record.beginCalled && record.object) {

            schema_->AbortInit(record.type, *record.object);

        }

    }

    ClearTransaction();

}



void ObjectWriter::ClearTransaction() noexcept {

    deferredContent_.ReleaseAll();

    frames_.Clear();

    assignments_.Clear();

    deferredStaticResources_.Clear();

    extensionEffects_.Clear();

    nameScopes_.Clear();

    resourceScopes_.Clear();

    serviceResourceChain_.Clear();

    namespaceBindings_.Clear();

    pendingNamespaces_.Clear();

    created_.Clear();

    root_.Reset();

    rootObjectIndex_ = InvalidIndex;

    documentNameScopeIndex_ = InvalidIndex;

    documentResourceScopeIndex_ = InvalidIndex;

    ended_ = false;

    hasDeferredStaticResources_ = false;

}



Base::Status ObjectWriter::Failure(

    Base::Status status,

    ::Aero::Diagnostics::DiagnosticCode diagnostic,

    Base::StringView message,

    ::Aero::Diagnostics::SourceSpan source) noexcept {

    if (diagnostics_ != nullptr) {

        Base::Result<::Aero::Diagnostics::Diagnostic> item = ::Aero::Diagnostics::Diagnostic::Create(

            diagnostic,

            ::Aero::Diagnostics::DiagnosticSeverity::Error,

            message,

            source,

            ::Aero::Diagnostics::InvalidDiagnosticObjectId,

            Meta::InvalidMemberId);

        if (!item) {

            return item.GetStatus();

        }

        Base::Result<void> reportResult = diagnostics_->Report(

            std::move(item).Value());

        if (!reportResult) {

            return reportResult.GetStatus();

        }

    }

    return status;

}



Base::Result<Base::StringView> ObjectWriter::NamespaceLookupCallback(

    void* context,

    Base::StringView prefix) noexcept {

    if (context == nullptr) {

        return Base::Status::Failure(

            Base::ErrorCode::InvalidArgument,

            MessageNamespaceState.Data());

    }

    return static_cast<ObjectWriter*>(context)->LookupNamespace(prefix);

}



Base::Result<Aero::ResourceValue> ObjectWriter::ResourceLookupCallback(

    void* context,

    Base::StringView key) noexcept {

    if (context == nullptr) {

        return Base::Status::Failure(

            Base::ErrorCode::InvalidArgument,

            MessageStaticResourceNotFound.Data());

    }

    return static_cast<ObjectWriter*>(context)->LookupResource(key);

}



} // namespace Aero::Markup


// ============================================================================
// SECTION: ObjectLoader Implementation
// ============================================================================



// ===== LoaderResult =====



namespace Aero::Markup {

Base::Result<void> VisualContentPlan::Reserve(
    std::uint32_t contentEdgeCount,
    std::uint32_t mountEdgeCount,
    std::uint32_t nodeCount) noexcept {
    contentEdges.Reserve(contentEdgeCount);
    mountEdges.Reserve(mountEdgeCount);
    nodes.Reserve(nodeCount);
    return {};
}

Base::Result<void> VisualContentPlan::AddNode(
    Aero::Media::Visual& node) noexcept {
    for (Aero::Media::Visual* existing : nodes) {
        if (existing == &node) return {};
    }
    nodes.PushBack(&node);
    return {};
}

void VisualContentPlan::ReleaseContent() noexcept {
    for (std::uint32_t index = 0U; index < contentEdges.Size(); ++index) {
        VisualContentEdge& edge = contentEdges[index];
        bool firstForParent = true;
        for (std::uint32_t prior = 0U; prior < index; ++prior) {
            if (contentEdges[prior].parentOwner.Get() ==
                    edge.parentOwner.Get() &&
                (!edge.property ||
                 contentEdges[prior].member ==
                     edge.member)) {
                firstForParent = false;
                break;
            }
        }
        if (firstForParent && edge.metadata != nullptr && edge.parentOwner) {
            if (edge.property) {
                const Meta::PropertyInfo* property =
                    edge.metadata->Types().
                        FindProperty(edge.member);
                if (property != nullptr) {
                    (void)edge.metadata->SetProperty(
                        *edge.parentOwner.Get(),
                        edge.member,
                        Meta::Value::NullObject(
                            property->ValueType()));
                }
            } else {
                (void)edge.metadata->ClearContent(
                    *edge.parentOwner.Get(),
                    edge.member);
            }
        }
    }
}

void VisualContentPlan::Clear() noexcept {
    contentEdges.Clear();
    mountEdges.Clear();
    nodes.Clear();
}

} // namespace Aero::Markup


// ===== Loader =====












namespace Aero::Markup {
namespace LoaderDiagnosticCodes {
inline constexpr ::Aero::Diagnostics::DiagnosticCode InvalidUri =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 301U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode XamlProviderNotFound =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 302U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode SourceLoadFailed =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 303U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode SourceRejected =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 304U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode RecursiveLoad =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 305U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode LoadComponentTypeMismatch =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 306U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode ResourceDependencyFailed =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 307U);
} // namespace LoaderDiagnosticCodes

struct Loader::LoaderState {
    LoaderState(
        Schema& schema,
        XamlProviderRegistry& providers,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr,
        const LoadState* loadState = nullptr) noexcept
        : schema_(&schema),
          providers_(&providers),
          diagnostics_(diagnostics),
          loadState_(loadState) {}

    struct Operation;

    Schema* schema_ = nullptr;
    XamlProviderRegistry* providers_ = nullptr;
    Diagnostics::IDiagnosticSink* diagnostics_ = nullptr;
    const LoadState* loadState_ = nullptr;
};

using Aero::ResourceDictionary;

namespace {

class MemoryStream : public Base::Stream {
public:
    explicit MemoryStream(
        Base::Span<const std::uint8_t> bytes) noexcept
        : bytes_(bytes) {}

    bool CanRead() const noexcept override { return true; }

    Base::Result<std::uint32_t> Read(
        Base::Span<std::uint8_t> destination) noexcept override {
        const std::uint32_t available = bytes_.Size() - position_;
        const std::uint32_t count =
            std::min(available, destination.Size());
        if (count != 0U) {
            std::memcpy(
                destination.Data(),
                bytes_.Data() + position_,
                count);
            position_ += count;
        }
        return count;
    }

    bool CanSeek() const noexcept override { return true; }
    Base::Result<std::uint64_t> Position() const noexcept override {
        return static_cast<std::uint64_t>(position_);
    }
    Base::Result<std::uint64_t> Length() const noexcept override {
        return static_cast<std::uint64_t>(bytes_.Size());
    }
    Base::Result<std::uint64_t> Seek(
        std::int64_t offset,
        Base::SeekOrigin origin) noexcept override {
        const std::int64_t base = origin == Base::SeekOrigin::Begin
            ? 0
            : origin == Base::SeekOrigin::Current
                ? static_cast<std::int64_t>(position_)
                : static_cast<std::int64_t>(bytes_.Size());
        const std::int64_t next = base + offset;
        if (next < 0 || static_cast<std::uint64_t>(next) > bytes_.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Memory stream seek is outside its bounds");
        }
        position_ = static_cast<std::uint32_t>(next);
        return static_cast<std::uint64_t>(position_);
    }

private:
    Base::Span<const std::uint8_t> bytes_;
    std::uint32_t position_ = 0U;
};

class FileStream : public Base::Stream {
public:
    FileStream(std::FILE* file, std::uint64_t length) noexcept
        : file_(file), length_(length) {}
    ~FileStream() noexcept override {
        if (file_ != nullptr) {
            std::fclose(file_);
            file_ = nullptr;
        }
    }

    bool CanRead() const noexcept override { return file_ != nullptr; }
    Base::Result<std::uint32_t> Read(
        Base::Span<std::uint8_t> destination) noexcept override {
        if (file_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "File stream is closed");
        }
        if (destination.Empty()) return std::uint32_t{0U};
        const std::size_t count = std::fread(
            destination.Data(), 1U, destination.Size(), file_);
        if (count == 0U && std::ferror(file_) != 0) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "File stream read failed");
        }
        return static_cast<std::uint32_t>(count);
    }
    bool CanSeek() const noexcept override { return file_ != nullptr; }
    Base::Result<std::uint64_t> Position() const noexcept override {
        if (file_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "File stream is closed");
        }
        const long position = std::ftell(file_);
        if (position < 0L) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "File stream position could not be read");
        }
        return static_cast<std::uint64_t>(position);
    }
    Base::Result<std::uint64_t> Length() const noexcept override {
        return length_;
    }
    Base::Result<std::uint64_t> Seek(
        std::int64_t offset,
        Base::SeekOrigin origin) noexcept override {
        if (file_ == nullptr ||
            offset < static_cast<std::int64_t>(std::numeric_limits<long>::min()) ||
            offset > static_cast<std::int64_t>(std::numeric_limits<long>::max())) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "File stream seek is outside its bounds");
        }
        const int whence = origin == Base::SeekOrigin::Begin
            ? SEEK_SET
            : origin == Base::SeekOrigin::Current
                ? SEEK_CUR
                : SEEK_END;
        if (std::fseek(file_, static_cast<long>(offset), whence) != 0) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "File stream seek failed");
        }
        return Position();
    }

private:
    std::FILE* file_ = nullptr;
    std::uint64_t length_ = 0U;
};

class HashingStream : public Base::Stream {
public:
    explicit HashingStream(Base::Stream& source) noexcept
        : source_(&source) {}

    bool CanRead() const noexcept override {
        return source_ != nullptr && source_->CanRead();
    }
    Base::Result<std::uint32_t> Read(
        Base::Span<std::uint8_t> destination) noexcept override {
        if (source_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Hashing stream is not initialized");
        }
        Base::Result<std::uint32_t> result =
            source_->Read(destination);
        if (!result || result.Value() == 0U) return result;
        constexpr Base::HashCode Prime = UINT64_C(1099511628211);
        for (std::uint32_t index = 0U;
             index < result.Value(); ++index) {
            hash_ ^= static_cast<Base::HashCode>(
                destination[index]);
            hash_ *= Prime;
        }
        size_ += result.Value();
        return result;
    }
    bool CanSeek() const noexcept override { return false; }
    Base::HashCode Hash() const noexcept {
        return Base::MixHash64(hash_ ^ size_);
    }

private:
    Base::Stream* source_ = nullptr;
    Base::HashCode hash_ =
        UINT64_C(14695981039346656037) ^
        Base::MixHash64(0U);
    std::uint64_t size_ = 0U;
};

char ToLowerAscii(char value) noexcept {
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value - 'A' + 'a')
        : value;
}

Base::Result<void> AssignLowerAscii(
    Base::String& output,
    Base::StringView value) noexcept {
    Base::String replacement(&output.Allocator());
    replacement.Reserve(value.SizeBytes());
    for (char character : value) {
        const char lower = ToLowerAscii(character);
        Base::Result<void> append =
            replacement.AppendUnchecked(
                Base::StringView(&lower, 1U));
        if (!append) {
            return append.GetStatus();
        }
    }
    output = std::move(replacement);
    return {};
}

bool RegistrationMatches(
    const XamlProviderRegistration& registration,
    const Base::ResourceUri& uri,
    bool requireScheme,
    bool requireAssembly) noexcept {
    const bool schemeMatches = requireScheme
        ? !registration.scheme.Empty() &&
            registration.scheme.View() == uri.Scheme()
        : registration.scheme.Empty();
    const bool assemblyMatches = requireAssembly
        ? !registration.assembly.Empty() &&
            registration.assembly.View() == uri.Assembly()
        : registration.assembly.Empty();
    return schemeMatches && assemblyMatches;
}

Base::Result<::Aero::Markup::StreamResourceInfo> CreateMemoryResource(
    const Base::ResourceUri& uri,
    Base::Span<const std::uint8_t> bytes,
    std::uint64_t revision) noexcept {
    Base::Result<Base::Ref<MemoryStream>> stream =
        Base::MakeRef<MemoryStream>(bytes);
    if (!stream) return stream.GetStatus();
    ::Aero::Markup::StreamResourceInfo result;
    result.uri = uri;
    result.stream = std::move(stream).Value();
    result.revision = revision;
    return result;
}

Base::Result<Base::ResourceUri> ResolveRequestedUri(
    Base::StringView uri,
    const Base::ResourceUri& baseUri) noexcept {
    // WPF component URIs beginning with '/' are assembly resources, not
    // filesystem-rooted paths. Resolving them against a file-backed App.xaml
    // would otherwise incorrectly manufacture a file: URI.
    if (uri.SizeBytes() > 0U && uri[0] == '/') {
        for (std::uint32_t index = 1U;
             index + 10U <= uri.SizeBytes(); ++index) {
            if (uri.Substr(index, 10U) ==
                Base::StringView(";component")) {
                return Base::ResourceUri::Parse(uri);
            }
        }
    }
    if (!baseUri.Empty()) {
        return Base::ResourceUri::Resolve(
            baseUri, uri);
    }
    return Base::ResourceUri::Parse(uri);
}

} // namespace

Base::Result<void> XamlProviderRegistry::Set(
    Base::Ref<XamlProvider> provider,
    Base::StringView scheme,
    Base::StringView assembly,
    Base::Ref<XamlProvider>* replaced) noexcept {
    if (!provider) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Markup XAML provider is required");
    }
    XamlProviderRegistration registration;
    Base::Result<void> schemeResult =
        AssignLowerAscii(registration.scheme, scheme);
    if (!schemeResult) {
        return schemeResult.GetStatus();
    }
    Base::Result<void> assemblyResult =
        registration.assembly.Assign(assembly);
    if (!assemblyResult) {
        return assemblyResult.GetStatus();
    }
    registration.provider = std::move(provider);
    for (const XamlProviderRegistration& existing : registrations_) {
        if (existing.provider.Get() == registration.provider.Get()) {
            registration.identity = existing.identity;
            break;
        }
    }
    static std::atomic<std::uint64_t> nextIdentity{1U};
    if (registration.identity == 0U) {
        registration.identity =
            nextIdentity.fetch_add(1U, std::memory_order_relaxed);
        if (registration.identity == 0U) {
            registration.identity =
                nextIdentity.fetch_add(1U, std::memory_order_relaxed);
        }
    }

    for (XamlProviderRegistration& existing : registrations_) {
        if (existing.scheme.View() == registration.scheme.View() &&
            existing.assembly.View() ==
                registration.assembly.View()) {
            if (replaced != nullptr) {
                *replaced = std::move(existing.provider);
            }
            existing = std::move(registration);
            return {};
        }
    }
    registrations_.PushBack(
        std::move(registration));
    return {};
}

Base::Result<XamlProviderResolution>
XamlProviderRegistry::ResolveRoute(
    const Base::ResourceUri& uri,
    bool requireScheme,
    bool requireAssembly) const noexcept {
    for (const XamlProviderRegistration& registration : registrations_) {
        if (!RegistrationMatches(
                registration, uri, requireScheme, requireAssembly)) {
            continue;
        }
        XamlProviderResolution result;
        result.provider = registration.provider.Get();
        result.cacheIdentity = Base::MixHash64(
            registration.identity ^
            Base::DefaultHash<Base::StringView>{}(
                registration.scheme.View()) ^
            Base::DefaultHash<Base::StringView>{}(
                registration.assembly.View(), UINT64_C(0xA3E0)));
        return result;
    }
    if (parent_ != nullptr) {
        Base::Result<XamlProviderResolution> inherited =
            parent_->ResolveRoute(uri, requireScheme, requireAssembly);
        if (inherited) return inherited;
        if (inherited.GetStatus().code != Base::ErrorCode::NotFound) {
            return inherited.GetStatus();
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "No XAML source provider matches this route shape");
}

Base::Result<XamlProviderResolution>
XamlProviderRegistry::ResolveDetailed(
    const Base::ResourceUri& uri) const noexcept {
    const struct Route {
        bool scheme;
        bool assembly;
    } routes[] = {
        {true, true},
        {true, false},
        {false, true},
        {false, false}};

    for (const Route route : routes) {
        if ((route.scheme && uri.Scheme().Empty()) ||
            (route.assembly && uri.Assembly().Empty())) {
            continue;
        }
        Base::Result<XamlProviderResolution> resolved =
            ResolveRoute(uri, route.scheme, route.assembly);
        if (resolved) return resolved;
        if (resolved.GetStatus().code != Base::ErrorCode::NotFound) {
            return resolved.GetStatus();
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "No XAML source provider matches the resource URI");
}

Base::Result<XamlProvider*>
XamlProviderRegistry::Resolve(
    const Base::ResourceUri& uri) const noexcept {
    Base::Result<XamlProviderResolution> resolved =
        ResolveDetailed(uri);
    return resolved
        ? Base::Result<XamlProvider*>(resolved.Value().provider)
        : Base::Result<XamlProvider*>(resolved.GetStatus());
}

bool XamlProviderRegistry::Contains(
    const XamlProvider& provider) const noexcept {
    for (const XamlProviderRegistration& registration : registrations_) {
        if (registration.provider.Get() == &provider) return true;
    }
    return false;
}

Base::Result<void> EmbeddedXamlProvider::Add(
    const Base::ResourceUri& uri,
    Base::Span<const std::uint8_t> bytes,
    std::uint64_t revision) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Embedded XAML source provider is frozen");
    }
    if (uri.Empty() || bytes.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Embedded XAML source registration is invalid");
    }
    for (const Entry& entry : entries_) {
        if (entry.uri == uri) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Embedded XAML source URI is already registered");
        }
    }

    Entry entry;
    entry.uri = uri;
    entry.bytes.Append(bytes);
    entry.revision = revision;
    entries_.PushBack(std::move(entry));
    return {};
}

Base::Result<void> EmbeddedXamlProvider::AddText(
    const Base::ResourceUri& uri,
    Base::StringView text,
    std::uint64_t revision) noexcept {
    return Add(
        uri,
        Base::Span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(text.Data()),
            text.SizeBytes()),
        revision);
}

Base::Result<void> EmbeddedXamlProvider::Freeze() noexcept {
    frozen_ = true;
    return {};
}

Base::Result<::Aero::Markup::StreamResourceInfo>
EmbeddedXamlProvider::Open(
    const Base::ResourceUri& uri) const noexcept {
    for (const Entry& entry : entries_) {
        if (entry.uri == uri) {
            return CreateMemoryResource(
                entry.uri,
                entry.bytes.AsSpan(),
                entry.revision);
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "Embedded XAML source was not found");
}

Base::Result<std::uint64_t> EmbeddedXamlProvider::Revision(
    const Base::ResourceUri& uri) const noexcept {
    for (const Entry& entry : entries_) {
        if (entry.uri == uri) return entry.revision;
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "Embedded XAML source was not found");
}

Base::Result<std::uint64_t> FileXamlProvider::Revision(
    const Base::ResourceUri& uri) const noexcept {
    if ((!uri.Scheme().Empty() &&
         uri.Scheme() != Base::StringView("file")) ||
        uri.Path().Empty() || maxFileBytes_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "File XAML source URI is invalid");
    }
    Base::String path;
    Base::Result<void> assigned = path.Assign(uri.Path());
    if (!assigned) return assigned.GetStatus();
    std::error_code error;
    const std::filesystem::path filePath(path.CStr());
    const std::uintmax_t size =
        std::filesystem::file_size(filePath, error);
    if (error || size > maxFileBytes_ || size > UINT32_MAX) {
        return Base::Status::Failure(
            error ? Base::ErrorCode::NotFound : Base::ErrorCode::OutOfRange,
            "XAML source file revision could not be read");
    }
    const auto writeTime =
        std::filesystem::last_write_time(filePath, error);
    if (error) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML source file timestamp could not be read");
    }
    const std::uint64_t ticks = static_cast<std::uint64_t>(
        writeTime.time_since_epoch().count());
    return Base::MixHash64(
        static_cast<std::uint64_t>(size) ^ Base::MixHash64(ticks));
}

Base::Result<::Aero::Markup::StreamResourceInfo>
FileXamlProvider::Open(
    const Base::ResourceUri& uri) const noexcept {
    if ((!uri.Scheme().Empty() &&
         uri.Scheme() != Base::StringView("file")) ||
        uri.Path().Empty() ||
        maxFileBytes_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "File XAML source URI is invalid");
    }

    Base::String path;
    Base::Result<void> assigned = path.Assign(uri.Path());
    if (!assigned) {
        return assigned.GetStatus();
    }
    std::FILE* file = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&file, path.CStr(), "rb") != 0) {
        file = nullptr;
    }
#else
    file = std::fopen(path.CStr(), "rb");
#endif
    if (file == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML source file could not be opened");
    }
    if (std::fseek(file, 0L, SEEK_END) != 0) {
        std::fclose(file);
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "XAML source file size could not be read");
    }
    const long length = std::ftell(file);
    if (length < 0 ||
        static_cast<std::uint64_t>(length) > maxFileBytes_ ||
        static_cast<std::uint64_t>(length) > UINT32_MAX) {
        std::fclose(file);
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "XAML source file exceeds configured limits");
    }
    if (std::fseek(file, 0L, SEEK_SET) != 0) {
        std::fclose(file);
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "XAML source file could not be rewound");
    }

    Base::Result<Base::Ref<FileStream>> stream =
        Base::MakeRef<FileStream>(
            file, static_cast<std::uint64_t>(length));
    if (!stream) {
        std::fclose(file);
        return stream.GetStatus();
    }
    ::Aero::Markup::StreamResourceInfo source;
    source.uri = uri;
    source.stream = std::move(stream).Value();
    Base::Result<std::uint64_t> revision = Revision(uri);
    source.revision = revision
        ? revision.Value()
        : 0U;
    return source;
}

struct Loader::LoaderState::Operation {
    struct FinalizeState {
        Operation* operation = nullptr;
        const XamlReaderSettings* options = nullptr;
        const Base::ResourceUri* origin = nullptr;
        const CompiledDocument* compiled = nullptr;
    };

    struct PendingResourceMerge {
        ResourceDictionary target;
        ResourceDictionary source;
    };

    Operation(
        Schema& schema,
        XamlProviderRegistry& providers,
        Diagnostics::IDiagnosticSink* diagnostics,
        const LoadState* loadState) noexcept
        : schema_(&schema),
          providers_(&providers),
          diagnostics_(diagnostics),
          loadState_(loadState) {}

    const LoadState& LoadContext() const noexcept {
        static const LoadState empty;
        return loadState_ != nullptr ? *loadState_ : empty;
    }

    Base::Result<LoaderResult> LoadCore(
        const Base::ResourceUri& uri,
        const XamlReaderSettings& options,
        const Base::Ref<Base::Object>& existingRoot) noexcept;
    Base::Result<LoaderResult> ParseCore(
        Base::StringView text,
        const Base::ResourceUri& baseUri,
        const XamlReaderSettings& options,
        const Base::Ref<Base::Object>& existingRoot,
        bool deferUnresolvedStaticResources = false) noexcept;
    Base::Result<LoaderResult> ParseStreamCore(
        Base::Stream& stream,
        const Base::ResourceUri& baseUri,
        const XamlReaderSettings& options,
        const Base::Ref<Base::Object>& existingRoot,
        bool deferUnresolvedStaticResources = false,
        Base::Vector<Node>* recordingNodes = nullptr) noexcept;
    Base::Result<LoaderResult> LoadCompiled(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        const XamlReaderSettings& options) noexcept;
    Base::Result<LoaderResult> LoadCompiledDocument(
        const CompiledDocument& document,
        const Base::ResourceUri& originUri,
        const XamlReaderSettings& options,
        const Base::Ref<Base::Object>& existingRoot) noexcept;
    Base::Result<void> ResolveResourceDependencies(
        LoaderResult& result,
        const XamlReaderSettings& options) noexcept;
    Base::Result<void> ResolveDictionaryDependencies(
        ResourceDictionary& dictionary,
        LoaderResult& owner,
        const XamlReaderSettings& options,
        std::uint32_t& resourceCount,
        Base::Vector<PendingResourceMerge>& pending) noexcept;
    Base::Result<void> CommitResourceDependencies(
        Base::Vector<PendingResourceMerge>& pending) noexcept;
    Base::Result<void> AppendDependencies(
        LoaderResult& destination,
        const LoaderResult& source,
        const XamlReaderSettings& options) noexcept;
    Base::Result<void> AppendDependency(
        LoaderResult& destination,
        const Base::ResourceUri& dependency,
        const XamlReaderSettings& options) noexcept;
    Base::Result<void> FinalizeResult(
        LoaderResult& result,
        const XamlReaderSettings& options,
        const Base::ResourceUri& origin,
        const CompiledDocument* compiled) noexcept;
    static Base::Result<void> FinalizeLoad(
        LoaderResult& result,
        void* context) noexcept;
    Base::Result<void> ValidateOptions(
        const XamlReaderSettings& options) const noexcept;
    Base::Result<void> CheckPolicy(
        const Base::ResourceUri& uri,
        const XamlReaderSettings& options) noexcept;
    bool IsLoading(const Base::ResourceUri& uri) const noexcept;
    Base::Status Failure(
        Base::Status status,
        ::Aero::Diagnostics::DiagnosticCode code,
        Base::StringView message) noexcept;

    Schema* schema_ = nullptr;
    XamlProviderRegistry* providers_ = nullptr;
    Diagnostics::IDiagnosticSink* diagnostics_ = nullptr;
    const LoadState* loadState_ = nullptr;
    Base::Vector<Base::ResourceUri> loadStack_;
};

Base::Result<LoaderResult>
Loader::LoaderState::Operation::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    const XamlReaderSettings& options) noexcept {
    Base::Result<void> validOptions =
        ValidateOptions(options);
    if (!validOptions) {
        return validOptions.GetStatus();
    }
    if (bytes.Size() > options.limits.maxSourceBytes) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Compiled XAML source exceeds configured limits"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "Compiled XAML source exceeds configured limits"));
    }
    Base::Result<CompiledDocument> document =
        CompiledDocument::Deserialize(
            bytes,
            schema_->Domain(),
            options.limits.compiled);
    if (!document) {
        const Base::Status status = document.GetStatus();
        if (!originUri.Empty() &&
            (status.code == Base::ErrorCode::Unsupported ||
             status.code == Base::ErrorCode::ValidationFailed)) {
            // A compatible source is authoritative when the cache identity no
            // longer matches this runtime. Hosts may persist a replacement
            // cache after this successful source load.
            return LoadCore(originUri, options, {});
        }
        return status;
    }

    return LoadCompiledDocument(
        document.Value(), originUri, options, {});
}

Base::Result<LoaderResult>
Loader::LoaderState::Operation::LoadCompiledDocument(
    const CompiledDocument& document,
    const Base::ResourceUri& originUri,
    const XamlReaderSettings& options,
    const Base::Ref<Base::Object>& existingRoot) noexcept {
    const LoadState& loadState = LoadContext();
    LoadState context;
    context.resources = loadState.resources;
    context.effectiveValues = loadState.effectiveValues;
    context.bindings = loadState.bindings;
    context.fallbackResources = loadState.fallbackResources;
    context.baseUri = &originUri;
    context.templatedParent = loadState.templatedParent;
    context.existingRoot = existingRoot;
    context.effectLifetime = loadState.effectLifetime;
    context.effectCommitMode = loadState.effectCommitMode;
    context.maxObjects = options.limits.maxObjects;
    // Source-backed compiled documents need the same two-phase resource
    // resolution as streamed documents. Merged ResourceDictionary sources
    // are committed by the load finalizer before queued StaticResource
    // references are written.
    context.deferUnresolvedStaticResources = true;
    FinalizeState finalize{
        this, &options, &originUri, &document};
    context.finalize = &FinalizeLoad;
    context.finalizeContext = &finalize;
    ObjectWriter writer(*schema_, diagnostics_);
    Base::Result<LoaderResult> loaded =
        loadState.dispatcher != nullptr &&
        loadState.dependencyProperties != nullptr
        ? [&]() noexcept -> Base::Result<LoaderResult> {
              Meta::ObjectFactoryScope services(
                  *loadState.dispatcher,
                  *loadState.dependencyProperties,
                  schema_->Metadata());
              return writer.Load(document, context);
          }()
        : [&]() noexcept {
              return writer.Load(document, context);
          }();
    if (!loaded) return loaded.GetStatus();
    return std::move(loaded).Value();
}

Base::Result<LoaderResult> Loader::LoaderState::Operation::LoadCore(
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options,
    const Base::Ref<Base::Object>& existingRoot) noexcept {
    const LoadState& loadState = LoadContext();
    Base::Result<void> validOptions =
        ValidateOptions(options);
    if (!validOptions) {
        return validOptions.GetStatus();
    }
    Base::Result<void> policy = CheckPolicy(uri, options);
    if (!policy) {
        return policy.GetStatus();
    }
    if (IsLoading(uri)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "Recursive XAML source load was detected"),
            LoaderDiagnosticCodes::RecursiveLoad,
            Base::StringView(
                "Recursive XAML source load was detected"));
    }
    if (loadStack_.Size() >=
        options.limits.maxDependencyDepth) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML dependency depth exceeds configured limits"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML dependency depth exceeds configured limits"));
    }

    Base::Result<XamlProviderResolution> provider =
        providers_->ResolveDetailed(uri);
    if (!provider) {
        return Failure(
            provider.GetStatus(),
            LoaderDiagnosticCodes::XamlProviderNotFound,
            Base::StringView(
                "No XAML source provider matches the resource URI"));
    }
    loadStack_.PushBack(uri);

    if (loadState.documentCache != nullptr) {
        Base::Result<std::uint64_t> probedRevision =
            provider.Value().provider->Revision(uri);
        if (probedRevision && probedRevision.Value() != 0U) {
            Base::Result<DocumentCacheLookup> cached =
                loadState.documentCache->Lookup(
                    uri,
                    probedRevision.Value(),
                    provider.Value().cacheIdentity,
                    schema_->Domain(),
                    options.limits.compiled);
            if (cached && cached.Value().hit && cached.Value().document != nullptr) {
                Base::Result<LoaderResult> loaded =
                    LoadCompiledDocument(
                        *cached.Value().document,
                        uri,
                        options,
                        existingRoot);
                loadStack_.PopBack();
                return loaded;
            }
        }
    }

    Base::Result<::Aero::Markup::StreamResourceInfo> source =
        provider.Value().provider->Open(uri);
    if (!source) {
        loadStack_.PopBack();
        return Failure(
            source.GetStatus(),
            LoaderDiagnosticCodes::SourceLoadFailed,
            Base::StringView("XAML source could not be loaded"));
    }
    ::Aero::Markup::StreamResourceInfo sourceInfo =
        std::move(source).Value();
    if (!sourceInfo.stream ||
        !sourceInfo.stream->CanRead()) {
        loadStack_.PopBack();
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML source stream is invalid"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML source stream could not be read"));
    }

    const Base::ResourceUri& origin =
        sourceInfo.uri.Empty()
        ? uri
        : sourceInfo.uri;
    HashingStream hashing(*sourceInfo.stream);
    Base::Vector<Node> recordedNodes;
    Base::Result<LoaderResult> loaded = ParseStreamCore(
        hashing,
        origin,
        options,
        existingRoot,
        true,
        loadState.documentCache != nullptr
            ? &recordedNodes
            : nullptr);
    if (sourceInfo.revision == 0U) {
        sourceInfo.revision = hashing.Hash();
    }
    if (loaded && loadState.documentCache != nullptr &&
        !recordedNodes.Empty()) {
        Base::Result<CompiledDocument> compiled =
            CompiledDocument::Compile(
                {recordedNodes.Data(), recordedNodes.Size()},
                *schema_,
                origin);
        if (compiled) {
            for (const Base::ResourceUri& dependency :
                 loaded.Value().dependencies) {
                Base::Result<void> added =
                    compiled.Value().AddDependency(dependency);
                if (!added) {
                    compiled = added.GetStatus();
                    break;
                }
            }
        }
        if (compiled) {
            static_cast<void>(loadState.documentCache->Store(
                uri,
                sourceInfo.revision,
                provider.Value().cacheIdentity,
                std::move(compiled).Value(),
                {loaded.Value().dependencies.Data(),
                 loaded.Value().dependencies.Size()}));
        }
    }
    loadStack_.PopBack();
    return loaded;
}

Base::Result<LoaderResult> Loader::LoaderState::Operation::ParseCore(
    Base::StringView text,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options,
    const Base::Ref<Base::Object>& existingRoot,
    bool deferUnresolvedStaticResources) noexcept {
    Base::Result<void> validOptions =
        ValidateOptions(options);
    if (!validOptions) {
        return validOptions.GetStatus();
    }
    if (text.SizeBytes() >
        options.limits.maxSourceBytes) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML source exceeds configured limits"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML source exceeds configured limits"));
    }

    const Base::Span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(text.Data()),
        text.SizeBytes()};
    Base::Result<Base::Ref<MemoryStream>> stream =
        Base::MakeRef<MemoryStream>(bytes);
    if (!stream) return stream.GetStatus();
    return ParseStreamCore(
        *stream.Value(),
        baseUri,
        options,
        existingRoot,
        deferUnresolvedStaticResources);
}

Base::Result<LoaderResult> Loader::LoaderState::Operation::ParseStreamCore(
    Base::Stream& stream,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options,
    const Base::Ref<Base::Object>& existingRoot,
    bool deferUnresolvedStaticResources,
    Base::Vector<Node>* recordingNodes) noexcept {
    const LoadState& loadState = LoadContext();
    Base::Result<void> validOptions =
        ValidateOptions(options);
    if (!validOptions) {
        return validOptions.GetStatus();
    }

#if AERO_WITH_EXPAT
    ExpatXmlTokenizer tokenizer(options.limits.xml);
#else
    Utf8XmlTokenizer tokenizer(options.limits.xml);
#endif
    Base::Result<void> reset =
        tokenizer.Reset(stream, diagnostics_);
    if (!reset) return reset.GetStatus();

    NodeReader reader(tokenizer, diagnostics_);
    LoadState context;
    context.resources = loadState.resources;
    context.effectiveValues = loadState.effectiveValues;
    context.bindings = loadState.bindings;
    context.fallbackResources = loadState.fallbackResources;
    context.baseUri = &baseUri;
    context.templatedParent = loadState.templatedParent;
    context.existingRoot = existingRoot;
    context.effectLifetime = loadState.effectLifetime;
    context.effectCommitMode = loadState.effectCommitMode;
    context.maxObjects = options.limits.maxObjects;
    context.deferUnresolvedStaticResources =
        deferUnresolvedStaticResources;
    context.recordingNodes = recordingNodes;
    FinalizeState finalize{
        this, &options, &baseUri, nullptr};
    context.finalize = &FinalizeLoad;
    context.finalizeContext = &finalize;
    ObjectWriter writer(*schema_, diagnostics_);
    Base::Result<LoaderResult> loaded =
        loadState.dispatcher != nullptr &&
        loadState.dependencyProperties != nullptr
        ? [&]() noexcept -> Base::Result<LoaderResult> {
              Meta::ObjectFactoryScope services(
                  *loadState.dispatcher,
                  *loadState.dependencyProperties,
                  schema_->Metadata());
              return writer.Load(reader, context);
          }()
        : [&]() noexcept -> Base::Result<LoaderResult> {
              return writer.Load(reader, context);
          }();
    if (!loaded) return loaded.GetStatus();
    return std::move(loaded).Value();
}

Base::Result<void> Loader::LoaderState::Operation::FinalizeLoad(
    LoaderResult& result,
    void* context) noexcept {
    auto* finalize =
        static_cast<FinalizeState*>(context);
    if (finalize == nullptr ||
        finalize->operation == nullptr ||
        finalize->options == nullptr ||
        finalize->origin == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML load finalization context is invalid");
    }
    return finalize->operation->FinalizeResult(
        result,
        *finalize->options,
        *finalize->origin,
        finalize->compiled);
}

Base::Result<void> Loader::LoaderState::Operation::FinalizeResult(
    LoaderResult& result,
    const XamlReaderSettings& options,
    const Base::ResourceUri& origin,
    const CompiledDocument* compiled) noexcept {
    const Base::ResourceUri& effectiveOrigin =
        compiled != nullptr && origin.Empty()
        ? compiled->OriginUri()
        : origin;
    result.canonicalUri = effectiveOrigin;
    if (compiled != nullptr) {
        for (const Base::ResourceUri& dependency :
             compiled->Dependencies()) {
            Base::Result<void> appended =
                AppendDependency(
                    result, dependency, options);
            if (!appended) return appended.GetStatus();
        }
    }
    Base::Result<void> originDependency =
        AppendDependency(
            result, effectiveOrigin, options);
    if (!originDependency) {
        return originDependency.GetStatus();
    }
    return ResolveResourceDependencies(
        result, options);
}

Base::Result<void>
Loader::LoaderState::Operation::ResolveResourceDependencies(
    LoaderResult& result,
    const XamlReaderSettings& options) noexcept {
    std::uint32_t resourceCount = 0U;
    Base::Vector<PendingResourceMerge> pending;

    auto resolveDictionary = [&](ResourceDictionary& dictionary)
        noexcept -> Base::Result<void> {
        if (dictionary.Size() == 0U &&
            dictionary.MergedDictionaryCount() == 0U &&
            dictionary.GetSource().Empty()) {
            return {};
        }
        return ResolveDictionaryDependencies(
            dictionary,
            result,
            options,
            resourceCount,
            pending);
    };

    Base::Result<void> resolved = resolveDictionary(result.resources);
    if (!resolved) return resolved.GetStatus();

    ResourceDictionary* rootResources = nullptr;
    if (result.root) {
        rootResources = schema_->ResolveResourceScope(result.root->RuntimeType(), *result.root);
        if (rootResources != nullptr) {
            resolved = resolveDictionary(*rootResources);
            if (!resolved) return resolved.GetStatus();
        }
    }

    for (Aero::Media::Visual* visual : result.visualContent.nodes) {
        if (visual == nullptr ||
            (result.root && visual == result.root.Get())) {
            continue;
        }
        ResourceDictionary* resources = schema_->ResolveResourceScope(visual->RuntimeType(), *visual);
        if (resources == nullptr || resources == rootResources) continue;
        resolved = resolveDictionary(*resources);
        if (!resolved) return resolved.GetStatus();
    }
    return CommitResourceDependencies(pending);
}

Base::Result<void>
Loader::LoaderState::Operation::ResolveDictionaryDependencies(
    ResourceDictionary& dictionary,
    LoaderResult& owner,
    const XamlReaderSettings& options,
    std::uint32_t& resourceCount,
    Base::Vector<PendingResourceMerge>& pending) noexcept {
    if (resourceCount >
            options.limits.maxResources ||
        dictionary.Size() >
            options.limits.maxResources - resourceCount) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML resource count exceeds configured limits"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML resource count exceeds configured limits"));
    }
    resourceCount += dictionary.Size();

    for (std::uint32_t index = 0U;
         index < dictionary.Size();
         ++index) {
        Base::Result<Aero::ResourceEntrySnapshot>
            entry = dictionary.EntryAt(index);
        if (!entry) return entry.GetStatus();
        const Meta::Value& value = entry.Value().value;
        if (value.Kind() != Meta::ValueKind::Object ||
            value.IsNullObject() ||
            !value.AsObject()) {
            continue;
        }
        Base::Object& object = *value.AsObject();
        ResourceDictionary* nested = schema_->ResolveResourceScope(object.RuntimeType(), object);
        if (nested == nullptr ||
            (nested->Size() == 0U &&
             nested->MergedDictionaryCount() == 0U &&
             nested->GetSource().Empty())) {
            continue;
        }
        Base::Result<void> resolved =
            ResolveDictionaryDependencies(
                *nested,
                owner,
                options,
                resourceCount,
                pending);
        if (!resolved) return resolved.GetStatus();
    }

    const std::uint32_t mergedCount =
        dictionary.MergedDictionaryCount();
    for (std::uint32_t index = 0U;
         index < mergedCount;
         ++index) {
        ResourceDictionary merged =
            dictionary.MergedDictionaryAt(index);
        Base::Result<void> resolved =
            ResolveDictionaryDependencies(
                merged,
                owner,
                options,
                resourceCount,
                pending);
        if (!resolved) return resolved.GetStatus();
    }

    const Base::ResourceUri source =
        dictionary.GetSource();
    if (source.Empty()) return {};
    if ((!owner.canonicalUri.Empty() &&
         source == owner.canonicalUri) ||
        IsLoading(source)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "Recursive ResourceDictionary Source was detected"),
            LoaderDiagnosticCodes::RecursiveLoad,
            Base::StringView(
                "Recursive ResourceDictionary Source was detected"));
    }

    // Merged dictionaries are evaluated in declaration order. Supply already
    // discovered siblings as ambient resources while loading the next source,
    // so WPF-style DynamicResource values in styles can resolve against an
    // earlier palette or brush dictionary.
    ResourceDictionary ambientResources;
    // Already-loaded sibling Source dictionaries first so a later merge
    // (Resources.xaml) can StaticResource keys defined in nested merges of
    // an earlier sibling (Colors.Dark inside Brushes.DarkBlue).
    for (PendingResourceMerge& discovered : pending) {
        ambientResources.AddMerged(discovered.source);
    }
    if (dictionary.Size() > 0U ||
        dictionary.MergedDictionaryCount() > 0U) {
        ambientResources.AddMerged(dictionary);
    }
    const LoadState& loadState = LoadContext();
    LoadState resourceContext = loadState;
    resourceContext.resources = &ambientResources;
    resourceContext.fallbackResources = &ambientResources;
    const LoadState* previousLoadState = loadState_;
    loadState_ = &resourceContext;
    Base::Result<LoaderResult> loaded =
        LoadCore(source, options, {});
    loadState_ = previousLoadState;
    if (!loaded) {
        return Failure(
            loaded.GetStatus(),
            LoaderDiagnosticCodes::ResourceDependencyFailed,
            Base::StringView(
                "ResourceDictionary Source could not be loaded"));
    }
    if (!loaded.Value().root ||
        loaded.Value().root->RuntimeType() !=
            ResourceDictionary::StaticTypeId()) {
        loaded.Value().Clear();
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "ResourceDictionary Source root has an incompatible type"),
            LoaderDiagnosticCodes::ResourceDependencyFailed,
            Base::StringView(
                "ResourceDictionary Source root must be ResourceDictionary"));
    }
    auto& sourceDictionary =
        static_cast<ResourceDictionary&>(
            *loaded.Value().root);
    Base::Result<void> dependencies =
        AppendDependencies(owner, loaded.Value(), options);
    if (!dependencies) {
        loaded.Value().Clear();
        return dependencies.GetStatus();
    }
    PendingResourceMerge merge;
    Base::Result<ResourceDictionary> target =
        dictionary.Share();
    if (!target) {
        loaded.Value().Clear();
        return target.GetStatus();
    }
    merge.target = std::move(target).Value();
    merge.source = std::move(sourceDictionary);
    pending.PushBack(std::move(merge));
    loaded.Value().Clear();
    return {};
}

Base::Result<void>
Loader::LoaderState::Operation::CommitResourceDependencies(
    Base::Vector<PendingResourceMerge>& pending) noexcept {
    for (PendingResourceMerge& merge : pending) {
        merge.target.AddMerged(merge.source);
    }
    return {};
}

Base::Result<void> Loader::LoaderState::Operation::AppendDependencies(
    LoaderResult& destination,
    const LoaderResult& source,
    const XamlReaderSettings& options) noexcept {
    for (const Base::ResourceUri& dependency :
         source.dependencies) {
        Base::Result<void> appended =
            AppendDependency(
                destination, dependency, options);
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> Loader::LoaderState::Operation::AppendDependency(
    LoaderResult& destination,
    const Base::ResourceUri& dependency,
    const XamlReaderSettings& options) noexcept {
    if (dependency.Empty()) return {};
    for (const Base::ResourceUri& existing :
         destination.dependencies) {
        if (existing == dependency) return {};
    }
    if (destination.dependencies.Size() >=
        options.limits.compiled.maxDependencies) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML dependency count exceeds configured limits"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML dependency count exceeds configured limits"));
    }
    destination.dependencies.PushBack(
        dependency);
    return {};
}

Base::Result<void> Loader::LoaderState::Operation::ValidateOptions(
    const XamlReaderSettings& options) const noexcept {
    const LoadState& loadState = LoadContext();
    if (schema_ == nullptr || providers_ == nullptr ||
        !schema_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML loader requires a frozen schema and provider registry");
    }
    if (options.limits.maxSourceBytes == 0U ||
        options.limits.maxObjects == 0U ||
        options.limits.maxResources == 0U ||
        options.limits.maxDependencyDepth == 0U ||
        options.limits.xml.maxInputBytes == 0U ||
        options.limits.xml.maxDepth == 0U ||
        options.limits.compiled.maxNodes == 0U ||
        options.limits.compiled.maxStringBytes == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML load limits must be positive");
    }
    if ((loadState.dispatcher == nullptr) !=
        (loadState.dependencyProperties == nullptr)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML object factory require dispatcher and property metadata");
    }
    return {};
}

Base::Result<void> Loader::LoaderState::Operation::CheckPolicy(
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    if (uri.Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML resource URI cannot be empty"),
            LoaderDiagnosticCodes::InvalidUri,
            Base::StringView("XAML resource URI cannot be empty"));
    }
    if (uri.IsNetwork() &&
        !options.policy.allowNetwork) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Network XAML sources are disabled by policy"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "Network XAML sources are disabled by policy"));
    }
    if (uri.Scheme() == Base::StringView("file") &&
        !options.policy.allowFile) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "File XAML sources are disabled by policy"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "File XAML sources are disabled by policy"));
    }
    if (uri.Scheme() == Base::StringView("pack") &&
        !options.policy.allowPackApplication) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Pack application XAML sources are disabled by policy"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "Pack application XAML sources are disabled by policy"));
    }
    return {};
}

bool Loader::LoaderState::Operation::IsLoading(
    const Base::ResourceUri& uri) const noexcept {
    for (const Base::ResourceUri& active : loadStack_) {
        if (active == uri) {
            return true;
        }
    }
    return false;
}

Base::Status Loader::LoaderState::Operation::Failure(
    Base::Status status,
    ::Aero::Diagnostics::DiagnosticCode code,
    Base::StringView message) noexcept {
    if (diagnostics_ != nullptr) {
        Base::Result<::Aero::Diagnostics::Diagnostic> diagnostic =
            ::Aero::Diagnostics::Diagnostic::Create(
                code,
                ::Aero::Diagnostics::DiagnosticSeverity::Error,
                message);
        if (diagnostic) {
            if (status.message != nullptr &&
                status.message[0] != '\0') {
                const Base::StringView detail(
                    status.message,
                    static_cast<std::uint32_t>(
                        std::strlen(status.message)));
                if (detail != message) {
                    diagnostic.Value().AddNote(detail);
                }
            }
            diagnostics_->Report(
                std::move(diagnostic).Value());
        }
    }
    return status;
}

} // namespace Aero::Markup

namespace Aero::Markup {

namespace {

Base::Result<XamlDocument> AdoptResult(
    Base::Result<LoaderResult>&& loaded,
    Base::IAllocator& allocator) noexcept {
    if (!loaded) return loaded.GetStatus();
    Base::Result<XamlDocument> document =
        ::Aero::Markup::AdoptXamlDocument(
            std::move(loaded).Value(), allocator);
    return document;
}

} // namespace

Loader::Loader(
    Schema& schema,
    XamlProviderRegistry& providers,
    Diagnostics::IDiagnosticSink* diagnostics,
    Base::IAllocator* allocator,
    const LoadState* loadState) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    static_cast<void>(Base::AllocateObject(
        *allocator_, Base::MemoryTag::Markup, state_,
        schema, providers, diagnostics, loadState));
}

Loader::~Loader() noexcept {
    if (state_ == nullptr) return;
    Base::FreeObject(*allocator_, Base::MemoryTag::Markup, state_);
    state_ = nullptr;
}

Base::Result<XamlDocument> Loader::Load(
    Base::StringView uri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    LoaderState::Operation operation(
        *state_->schema_, *state_->providers_,
        state_->diagnostics_, state_->loadState_);
    Base::Result<Base::ResourceUri> resolved =
        ResolveRequestedUri(uri, {});
    if (!resolved) {
        return AdoptResult(
            operation.Failure(
                resolved.GetStatus(),
                LoaderDiagnosticCodes::InvalidUri,
                Base::StringView("XAML resource URI is invalid")),
            *allocator_);
    }
    return AdoptResult(
        operation.LoadCore(resolved.Value(), options, {}),
        *allocator_);
}

Base::Result<XamlDocument> Loader::Load(
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    LoaderState::Operation operation(
        *state_->schema_, *state_->providers_,
        state_->diagnostics_, state_->loadState_);
    return AdoptResult(
        operation.LoadCore(uri, options, {}), *allocator_);
}

Base::Result<XamlDocument> Loader::Parse(
    Base::StringView text,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    LoaderState::Operation operation(
        *state_->schema_, *state_->providers_,
        state_->diagnostics_, state_->loadState_);
    return AdoptResult(
        operation.ParseCore(text, baseUri, options, {}, true),
        *allocator_);
}

Base::Result<XamlDocument> Loader::Parse(
    Base::Stream& stream,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    LoaderState::Operation operation(
        *state_->schema_, *state_->providers_,
        state_->diagnostics_, state_->loadState_);
    return AdoptResult(
        operation.ParseStreamCore(
            stream, baseUri, options, {}, true),
        *allocator_);
}

Base::Result<XamlDocument> Loader::LoadComponent(
    Base::Object& existingRoot,
    Base::StringView uri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    LoaderState::Operation operation(
        *state_->schema_, *state_->providers_,
        state_->diagnostics_, state_->loadState_);
    Base::Result<Base::ResourceUri> resolved =
        ResolveRequestedUri(uri, {});
    if (!resolved) {
        return AdoptResult(
            operation.Failure(
                resolved.GetStatus(),
                LoaderDiagnosticCodes::InvalidUri,
                Base::StringView("XAML component URI is invalid")),
            *allocator_);
    }
    Base::Ref<Base::Object> retained =
        Base::Ref<Base::Object>::TryFromBorrowed(existingRoot);
    if (!retained) {
        return AdoptResult(
            operation.Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "LoadComponent requires a managed root object"),
                LoaderDiagnosticCodes::LoadComponentTypeMismatch,
                Base::StringView(
                    "XAML component root cannot be retained")),
            *allocator_);
    }
    return AdoptResult(
        operation.LoadCore(resolved.Value(), options, retained),
        *allocator_);
}

Base::Result<XamlDocument> Loader::LoadComponent(
    Base::Object& existingRoot,
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    LoaderState::Operation operation(
        *state_->schema_, *state_->providers_,
        state_->diagnostics_, state_->loadState_);
    Base::Ref<Base::Object> retained =
        Base::Ref<Base::Object>::TryFromBorrowed(existingRoot);
    if (!retained) {
        return AdoptResult(
            operation.Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "LoadComponent requires a managed root object"),
                LoaderDiagnosticCodes::LoadComponentTypeMismatch,
                Base::StringView(
                    "XAML component root cannot be retained")),
            *allocator_);
    }
    return AdoptResult(
        operation.LoadCore(uri, options, retained), *allocator_);
}
Base::Result<XamlDocument> Loader::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    LoaderState::Operation operation(
        *state_->schema_, *state_->providers_,
        state_->diagnostics_, state_->loadState_);
    return AdoptResult(
        operation.LoadCompiled(bytes, originUri, options),
        *allocator_);
}

} // namespace Aero::Markup



// ============================================================================
// SECTION: XamlReader / XamlLoader / DocumentCache / Coordinator Implementation
// ============================================================================




// ============================================================================
// SECTION: XamlLoader / CompiledCache / DocumentState
// ============================================================================
// Consolidated implementation. Keep sections ordered by dependency.


// ===== CompiledCache =====


namespace Aero::Markup {

Base::Result<CompiledCacheIdentity> BuildCompiledCacheIdentity(
    const ::Aero::Meta::Registry& domain) noexcept {
    Base::Result<Base::HashCode> hash = domain.ComputeSchemaHash();
    if (!hash) return hash.GetStatus();

    CompiledCacheIdentity identity;
    identity.metadataSchemaHash = hash.Value();
    return identity;
}

CompiledCacheCompatibility CompareCompiledCacheIdentity(
    const CompiledCacheIdentity& cached,
    const CompiledCacheIdentity& current) noexcept {
    if (cached.cacheFormatVersion != current.cacheFormatVersion) {
        return CompiledCacheCompatibility::CacheFormatMismatch;
    }
    if (cached.typeIdAlgorithmVersion != current.typeIdAlgorithmVersion) {
        return CompiledCacheCompatibility::TypeIdAlgorithmMismatch;
    }
    if (cached.metadataSchemaFormatVersion !=
        current.metadataSchemaFormatVersion) {
        return CompiledCacheCompatibility::MetadataSchemaFormatMismatch;
    }
    if (cached.metadataProgramFormatVersion !=
        current.metadataProgramFormatVersion) {
        return CompiledCacheCompatibility::MetadataProgramFormatMismatch;
    }
    if (cached.schemaVersion != current.schemaVersion) {
        return CompiledCacheCompatibility::SchemaVersionMismatch;
    }
    if (cached.metadataSchemaHash != current.metadataSchemaHash) {
        return CompiledCacheCompatibility::MetadataSchemaMismatch;
    }
    return CompiledCacheCompatibility::Compatible;
}

Base::Result<void> ValidateCompiledCacheIdentity(
    const CompiledCacheIdentity& cached,
    const ::Aero::Meta::Registry& currentDomain) noexcept {
    Base::Result<CompiledCacheIdentity> current =
        BuildCompiledCacheIdentity(currentDomain);
    if (!current) return current.GetStatus();

    switch (CompareCompiledCacheIdentity(cached, current.Value())) {
    case CompiledCacheCompatibility::Compatible:
        return {};
    case CompiledCacheCompatibility::CacheFormatMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML cache format version is incompatible");
    case CompiledCacheCompatibility::TypeIdAlgorithmMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML TypeId algorithm version is incompatible");
    case CompiledCacheCompatibility::MetadataSchemaFormatMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML metadata descriptor format is incompatible");
    case CompiledCacheCompatibility::MetadataProgramFormatMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML metadata facet format is incompatible");
    case CompiledCacheCompatibility::SchemaVersionMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML schema ABI version is incompatible");
    case CompiledCacheCompatibility::MetadataSchemaMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Compiled XAML metadata schema hash does not match the runtime");
    }
    return Base::Status::Failure(
        Base::ErrorCode::InternalError,
        "Compiled XAML compatibility result is invalid");
}

} // namespace Aero::Markup




// ===== Resources =====





namespace Aero::Markup {
namespace {

using namespace Aero::Meta;
using namespace Aero::Threading;


Base::Status InvalidResource(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

Base::Result<void> AddResource(
    Base::Object& scopeOwner,
    const ResourceKey& key,
    const Meta::Value& value,
    void*) noexcept {
    if (scopeOwner.RuntimeType() !=
            ResourceDictionary::StaticTypeId()) {
        return InvalidResource(
            "XAML resource scope is not a ResourceDictionary");
    }
    static_cast<ResourceDictionary&>(scopeOwner)
        .Add(key, value);
    return {};
}

Base::Result<void> AddFrameworkResource(
    Base::Object& scopeOwner,
    const ResourceKey& key,
    const Meta::Value& value,
    void*) noexcept {
    auto* element =
        ::Aero::TryCast<::Aero::FrameworkElement>(&scopeOwner);
    if (element == nullptr) {
        return InvalidResource(
            "XAML resource scope is not a FrameworkElement");
    }
    element->GetResources().Add(key, value);
    return {};
}

ResourceDictionary* ResolveDictionaryScope(
    Base::Object& scopeOwner,
    void*) noexcept {
    return scopeOwner.RuntimeType() ==
            ResourceDictionary::StaticTypeId()
        ? &static_cast<ResourceDictionary&>(
              scopeOwner)
        : nullptr;
}

ResourceDictionary* ResolveFrameworkScope(
    Base::Object& scopeOwner,
    void*) noexcept {
    ::Aero::Media::Visual& visual =
        static_cast<::Aero::Media::Visual&>(scopeOwner);
    FrameworkElement* element =
        ::Aero::TryCast<::Aero::FrameworkElement>(&(visual));
    return element != nullptr
        ? &element->GetResources()
        : nullptr;
}

Base::Result<void> RegisterFrameworkName(
    Base::Object& scopeOwner,
    Base::StringView name,
    Base::Object& object,
    void*) noexcept {
    FrameworkElement* element =
        ::Aero::TryCast<::Aero::FrameworkElement>(&scopeOwner);
    if (element == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML name scope is not a FrameworkElement");
    }
    element->RegisterName(name, object);
    return {};
}

} // namespace

Base::Result<void> ResourceExtension::Register(
    Schema& schema) noexcept {
    if (schema.IsFrozen() || schema_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML resource extension registration is invalid");
    }
    const PropertyInfo* source =
        schema.Types().FindProperty(
            ResourceDictionary::StaticTypeId(),
            Base::StringView("Source"),
            false);
    const PropertyInfo* merged =
        schema.Types().FindProperty(
            ResourceDictionary::StaticTypeId(),
            Base::StringView("MergedDictionaries"),
            false);
    const PropertyInfo* entries =
        schema.Types().FindProperty(
            ResourceDictionary::StaticTypeId(),
            Base::StringView("Entries"),
            false);
    if (source == nullptr || merged == nullptr ||
        entries == nullptr ||
        source->ValueType() !=
            TypeOf<Base::ResourceUri>() ||
        merged->ValueType() !=
            ResourceDictionary::StaticTypeId()) {
        return InvalidResource(
            "ResourceDictionary XAML metadata is incomplete");
    }

    schema_ = &schema;
    Base::Result<void> status =
        schema.AddResourceScope({
            ResourceDictionary::StaticTypeId(),
            true,
            &AddResource,
            &ResolveDictionaryScope,
            nullptr});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = schema.AddResourceScope({
        FrameworkElement::StaticTypeId(),
        true,
        &AddFrameworkResource,
        &ResolveFrameworkScope,
        nullptr});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = schema.AddNameScope({
        FrameworkElement::StaticTypeId(),
        false,
        &RegisterFrameworkName,
        nullptr});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = schema.AddNameScope({
        ::Aero::Controls::UserControl::StaticTypeId(),
        true,
        &RegisterFrameworkName,
        nullptr});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    return {};
}

} // namespace Aero::Markup


// ===== XamlDocument =====






namespace Aero::Markup {

struct XamlDocumentState {
    explicit XamlDocumentState(LoaderResult&& value) noexcept
        : result(std::move(value)) {}

    static Base::Result<XamlDocument> Adopt(
        LoaderResult&& result,
        Base::IAllocator& allocator) noexcept;
    static LoaderResult Take(
        XamlDocument& document) noexcept;
    static const EffectLifetime* RuntimeLifetime(
        const XamlDocument& document) noexcept;

    LoaderResult result;
};

XamlDocument::~XamlDocument() noexcept {
    Reset();
}

XamlDocument::XamlDocument(XamlDocument&& other) noexcept
    : allocator_(other.allocator_), state_(other.state_) {
    other.allocator_ = nullptr;
    other.state_ = nullptr;
}

XamlDocument& XamlDocument::operator=(XamlDocument&& other) noexcept {
    if (this == &other) return *this;
    Reset();
    allocator_ = other.allocator_;
    state_ = other.state_;
    other.allocator_ = nullptr;
    other.state_ = nullptr;
    return *this;
}

} // namespace Aero::Markup

namespace Aero::Markup {

Base::Result<::Aero::Markup::XamlDocument> AdoptXamlDocument(
    ::Aero::Markup::LoaderResult&& result,
    Base::IAllocator& allocator) noexcept {
    return ::Aero::Markup::XamlDocumentState::Adopt(
        std::move(result), allocator);
}

} // namespace Aero::Markup

namespace Aero::Markup {

Base::Result<XamlDocument> XamlDocumentState::Adopt(
    LoaderResult&& result,
    Base::IAllocator& allocator) noexcept {
    if (!result.root) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "UI document requires a loaded root object");
    }
    void* memory = allocator.Allocate({
        sizeof(XamlDocumentState),
        alignof(XamlDocumentState),
        Base::MemoryTag::Markup});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "UI document allocation failed");
    }
    XamlDocument document;
    document.allocator_ = &allocator;
    document.state_ =
        new (memory) XamlDocumentState(std::move(result));
    return document;
}

} // namespace Aero::Markup

namespace Aero::Markup {

bool XamlDocument::IsValid() const noexcept {
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr && state->result.root;
}

const Base::Ref<Base::Object>& XamlDocument::Root() const noexcept {
    static const Base::Ref<Base::Object> empty;
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr ? state->result.root : empty;
}

Base::Object* XamlDocument::RootObject(
    Meta::TypeId expectedType) noexcept {
    auto* state = static_cast<XamlDocumentState*>(state_);
    if (state == nullptr || !state->result.root) return nullptr;
    Base::Object* root = state->result.root.Get();
    if (expectedType == Meta::InvalidTypeId) return root;
    const Meta::Registry* metadata = state->result.metadata;
    return metadata != nullptr && metadata->Types().IsDerivedFrom(
        root->RuntimeType(), expectedType)
        ? root
        : nullptr;
}

Base::Object* XamlDocument::FindName(
    Base::StringView name,
    Meta::TypeId expectedType) noexcept {
    auto* state = static_cast<XamlDocumentState*>(state_);
    if (state == nullptr || name.Empty()) return nullptr;
    Base::Object* object = state->result.names.Find(name);
    if (object == nullptr || expectedType == Meta::InvalidTypeId) {
        return object;
    }
    const Meta::Registry* metadata = state->result.metadata;
    return metadata != nullptr && metadata->Types().IsDerivedFrom(
        object->RuntimeType(), expectedType)
        ? object
        : nullptr;
}

std::uint32_t XamlDocument::NamedObjectCount() const noexcept {
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr ? state->result.names.Size() : 0U;
}

Aero::ResourceDictionary* XamlDocument::Resources() noexcept {
    auto* state = static_cast<XamlDocumentState*>(state_);
    return state != nullptr ? &state->result.resources : nullptr;
}

const Aero::ResourceDictionary* XamlDocument::Resources() const noexcept {
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr ? &state->result.resources : nullptr;
}

const Base::ResourceUri& XamlDocument::CanonicalUri() const noexcept {
    static const Base::ResourceUri empty;
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr ? state->result.canonicalUri : empty;
}

Base::Span<const Base::ResourceUri> XamlDocument::Dependencies() const noexcept {
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr
        ? Base::Span<const Base::ResourceUri>{
              state->result.dependencies.Data(),
              state->result.dependencies.Size()}
        : Base::Span<const Base::ResourceUri>{};
}

} // namespace Aero::Markup

namespace Aero::Markup {

const EffectLifetime*
XamlDocumentState::RuntimeLifetime(
    const XamlDocument& document) noexcept {
    const auto* state =
        static_cast<const XamlDocumentState*>(document.state_);
    return state != nullptr
        ? state->result.runtimeLifetime.Get()
        : nullptr;
}

LoaderResult XamlDocumentState::Take(
    XamlDocument& document) noexcept {
    auto* state = static_cast<XamlDocumentState*>(document.state_);
    if (state == nullptr) {
        LoaderResult empty;
        return empty;
    }
    LoaderResult result = std::move(state->result);
    document.Reset();
    return result;
}

} // namespace Aero::Markup

namespace Aero::Markup {

const ::Aero::Markup::EffectLifetime*
XamlDocumentEffectLifetime(
    const ::Aero::Markup::XamlDocument& document) noexcept {
    return ::Aero::Markup::XamlDocumentState::RuntimeLifetime(document);
}

::Aero::Markup::LoaderResult TakeXamlDocument(
    ::Aero::Markup::XamlDocument& document) noexcept {
    return ::Aero::Markup::XamlDocumentState::Take(document);
}

} // namespace Aero::Markup

namespace Aero::Markup {

void XamlDocument::Reset() noexcept {
    auto* state = static_cast<XamlDocumentState*>(state_);
    if (state == nullptr) return;
    state->result.Clear();
    state->~XamlDocumentState();
    allocator_->Deallocate(
        state,
        sizeof(XamlDocumentState),
        alignof(XamlDocumentState),
        Base::MemoryTag::Markup);
    state_ = nullptr;
    allocator_ = nullptr;
}

} // namespace Aero::Markup

// ============================================================================
// SECTION: XamlProvider / Adapter
// ============================================================================

namespace Aero::Markup {

void XamlProvider::AddChangedHandler(
    const XamlProviderChangedHandler& handler) noexcept {
    changed_.Add(handler);
}

bool XamlProvider::RemoveChangedHandler(
    const XamlProviderChangedHandler& handler) noexcept {
    return changed_.Remove(handler);
}

void XamlProvider::RaiseChanged(
    const Base::ResourceUri& uri) noexcept {
    if (changed_) changed_(uri);
}

Base::Result<StreamResourceInfo>
XamlProviderAdapter::Open(
    const Base::ResourceUri& uri) const noexcept {
    if (open_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML provider has no open callback");
    }
    return open_(uri, context_);
}

Base::Result<std::uint64_t>
XamlProviderAdapter::Revision(
    const Base::ResourceUri& uri) const noexcept {
    return revision_ != nullptr
        ? revision_(uri, context_)
        : XamlProvider::Revision(uri);
}

} // namespace Aero::Markup

// ============================================================================
// SECTION: XamlDocumentCache / DependencyGraph
// ============================================================================


// ===== DocumentCache =====






namespace Aero::Markup {
namespace {

Base::Result<Base::String> MakeKey(
    const Base::ResourceUri& uri,
    Base::IAllocator& allocator) noexcept {
    if (uri.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML cache URI cannot be empty");
    }
    Base::String key(&allocator);
    Base::Result<void> assigned = key.Assign(uri.Canonical());
    if (!assigned) return assigned.GetStatus();
    return key;
}

bool ContainsKey(
    const Base::Vector<Base::String>& values,
    Base::StringView key) noexcept {
    for (const Base::String& value : values) {
        if (value.View() == key) return true;
    }
    return false;
}

void RemoveKey(
    Base::Vector<Base::String>& values,
    Base::StringView key) noexcept {
    for (std::uint32_t index = 0U; index < values.Size(); ++index) {
        if (values[index].View() != key) continue;
        if (index + 1U != values.Size()) {
            values[index] = std::move(values.Back());
        }
        values.PopBack();
        return;
    }
}

} // namespace

struct DependencyGraph::State {
    struct Node {
        explicit Node(Base::IAllocator& allocator) noexcept
            : dependencies(&allocator), dependents(&allocator) {}

        Base::ResourceUri uri;
        Base::Vector<Base::String> dependencies;
        Base::Vector<Base::String> dependents;
    };

    explicit State(Base::IAllocator& allocator) noexcept
        : allocator(&allocator), nodes(&allocator) {}

    Base::Result<Node*> EnsureNode(
        const Base::ResourceUri& uri) noexcept {
        Base::Result<Base::String> key =
            MakeKey(uri, *allocator);
        if (!key) return key.GetStatus();
        Node* current = nodes.Find(key.Value());
        if (current != nullptr) return current;
        Node node(*allocator);
        node.uri = uri;
        typename Base::HashMap<Base::String, Node>::InsertResult
            inserted = nodes.Insert(
                std::move(key).Value(), std::move(node));
        return &inserted.entry->Value();
    }

    Base::IAllocator* allocator = nullptr;
    Base::HashMap<Base::String, Node> nodes;
    std::uint64_t generation = 0U;
};

static_assert(
    sizeof(DependencyGraph::State) <= 2048,
    "DependencyGraph inline state storage is too small");
static_assert(
    alignof(DependencyGraph::State) <= alignof(std::max_align_t),
    "DependencyGraph inline state alignment is insufficient");

DependencyGraph::DependencyGraph(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    static_cast<void>(Base::AllocateObject(
        *allocator_, Base::MemoryTag::Markup, state_, *allocator_));
}

DependencyGraph::~DependencyGraph() noexcept {
    if (state_ == nullptr) return;
    Base::FreeObject(*allocator_, Base::MemoryTag::Markup, state_);
    state_ = nullptr;
}

DependencyGraph::DependencyGraph(
    DependencyGraph&& other) noexcept
    : allocator_(other.allocator_),
      state_(other.state_) {
    other.state_ = nullptr;
    other.allocator_ = nullptr;
}

DependencyGraph& DependencyGraph::operator=(
    DependencyGraph&& other) noexcept {
    if (this == &other) return *this;
    if (state_ != nullptr) {
        Base::FreeObject(*allocator_, Base::MemoryTag::Markup, state_);
    }
    allocator_ = other.allocator_;
    state_ = other.state_;
    other.state_ = nullptr;
    other.allocator_ = nullptr;
    return *this;
}

Base::Result<void> DependencyGraph::Update(
    const Base::ResourceUri& document,
    Base::Span<const Base::ResourceUri> dependencies) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML dependency graph is unavailable");
    }
    Base::Result<Base::String> documentKey =
        MakeKey(document, *allocator_);
    if (!documentKey) return documentKey.GetStatus();
    Base::Result<DependencyGraph::State::Node*> documentNode =
        state_->EnsureNode(document);
    if (!documentNode) return documentNode.GetStatus();

    Base::Vector<Base::String> newDependencies(allocator_);
    newDependencies.Reserve(dependencies.Size());
    for (const Base::ResourceUri& dependencyUri : dependencies) {
        if (dependencyUri.Empty() || dependencyUri == document) continue;
        Base::Result<Base::String> dependencyKey =
            MakeKey(dependencyUri, *allocator_);
        if (!dependencyKey) return dependencyKey.GetStatus();
        if (ContainsKey(newDependencies, dependencyKey.Value().View())) {
            continue;
        }
        Base::Result<DependencyGraph::State::Node*> dependencyNode =
            state_->EnsureNode(dependencyUri);
        if (!dependencyNode) return dependencyNode.GetStatus();
        newDependencies.PushBack(
            std::move(dependencyKey).Value());
    }

    // Prepare reverse-edge key ownership and vector capacity before mutating
    // any edge. No hash-map insertions occur after this point, so node
    // references remain stable even when EnsureNode() previously rehashed.
    Base::Vector<Base::String> reverseKeys(allocator_);
    reverseKeys.Reserve(newDependencies.Size());
    for (const Base::String& dependencyKey : newDependencies) {
        DependencyGraph::State::Node* dependency = state_->nodes.Find(dependencyKey);
        if (dependency == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML dependency graph lost a prepared node");
        }
        dependency->dependents.Reserve(
                dependency->dependents.Size() + 1U);
        Base::String reverseKey(allocator_);
        Base::Result<void> copied = reverseKey.Assign(
            documentKey.Value().View());
        if (!copied) return copied.GetStatus();
        reverseKeys.PushBack(
            std::move(reverseKey));
    }

    DependencyGraph::State::Node* node = state_->nodes.Find(documentKey.Value());
    if (node == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML dependency graph lost the document node");
    }
    for (const Base::String& oldDependency : node->dependencies) {
        DependencyGraph::State::Node* dependency = state_->nodes.Find(oldDependency);
        if (dependency == nullptr) continue;
        RemoveKey(
            dependency->dependents,
            documentKey.Value().View());
        if (!ContainsKey(newDependencies, oldDependency.View()) &&
            dependency->dependencies.Empty() &&
            dependency->dependents.Empty()) {
            state_->nodes.Erase(oldDependency);
        }
    }
    node->dependencies = std::move(newDependencies);
    for (std::uint32_t index = 0U;
         index < node->dependencies.Size();
         ++index) {
        DependencyGraph::State::Node* dependency =
            state_->nodes.Find(node->dependencies[index]);
        if (dependency == nullptr) continue;
        if (ContainsKey(
                dependency->dependents,
                documentKey.Value().View())) {
            continue;
        }
        dependency->dependents.PushBack(
                std::move(reverseKeys[index]));
    }
    ++state_->generation;
    return {};
}

bool DependencyGraph::Remove(
    const Base::ResourceUri& document) noexcept {
    if (state_ == nullptr || document.Empty()) return false;
    Base::Result<Base::String> key =
        MakeKey(document, *allocator_);
    if (!key) return false;
    DependencyGraph::State::Node* node = state_->nodes.Find(key.Value());
    if (node == nullptr) return false;

    Base::Vector<Base::String> previousDependencies(allocator_);
    previousDependencies.Append(
            node->dependencies.AsSpan());
    node->dependencies.Clear();
    for (const Base::String& dependencyKey : previousDependencies) {
        DependencyGraph::State::Node* dependency = state_->nodes.Find(dependencyKey);
        if (dependency == nullptr) continue;
        RemoveKey(dependency->dependents, key.Value().View());
        if (dependency->dependencies.Empty() &&
            dependency->dependents.Empty()) {
            state_->nodes.Erase(dependencyKey);
        }
    }
    if (node->dependents.Empty()) {
        state_->nodes.Erase(key.Value());
    }
    ++state_->generation;
    return true;
}

void DependencyGraph::Clear() noexcept {
    if (state_ == nullptr) return;
    state_->nodes.Clear();
    ++state_->generation;
}

Base::Result<void> DependencyGraph::CopyDependencies(
    const Base::ResourceUri& document,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    output.Clear();
    if (state_ == nullptr || document.Empty()) return {};
    Base::Result<Base::String> key =
        MakeKey(document, *allocator_);
    if (!key) return key.GetStatus();
    const DependencyGraph::State::Node* node = state_->nodes.Find(key.Value());
    if (node == nullptr) return {};
    output.Reserve(node->dependencies.Size());
    for (const Base::String& dependencyKey : node->dependencies) {
        const DependencyGraph::State::Node* dependency = state_->nodes.Find(dependencyKey);
        if (dependency == nullptr) continue;
        output.PushBack(dependency->uri);
    }
    return {};
}

Base::Result<void> DependencyGraph::CopyDependents(
    const Base::ResourceUri& dependency,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    output.Clear();
    if (state_ == nullptr || dependency.Empty()) return {};
    Base::Result<Base::String> key =
        MakeKey(dependency, *allocator_);
    if (!key) return key.GetStatus();
    const DependencyGraph::State::Node* node = state_->nodes.Find(key.Value());
    if (node == nullptr) return {};
    output.Reserve(node->dependents.Size());
    for (const Base::String& dependentKey : node->dependents) {
        const DependencyGraph::State::Node* dependent = state_->nodes.Find(dependentKey);
        if (dependent == nullptr) continue;
        output.PushBack(dependent->uri);
    }
    return {};
}

Base::Result<void> DependencyGraph::CollectAffected(
    const Base::ResourceUri& changed,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    output.Clear();
    if (state_ == nullptr || changed.Empty()) return {};

    Base::Vector<Base::String> queue(allocator_);
    Base::HashSet<Base::String> visited(allocator_);
    Base::Result<Base::String> changedKey =
        MakeKey(changed, *allocator_);
    if (!changedKey) return changedKey.GetStatus();
    queue.PushBack(changedKey.Value());

    std::uint32_t cursor = 0U;
    while (cursor < queue.Size()) {
        Base::String key = queue[cursor++];
        typename Base::HashSet<Base::String>::InsertResult
            inserted = visited.Insert(key);
        if (!inserted.inserted) continue;

        const DependencyGraph::State::Node* node = state_->nodes.Find(key);
        Base::ResourceUri uri = node != nullptr
            ? node->uri
            : changed;
        output.PushBack(uri);
        if (node == nullptr) continue;
        for (const Base::String& dependent : node->dependents) {
            if (visited.Contains(dependent)) continue;
            queue.PushBack(dependent);
        }
    }
    return {};
}

std::uint32_t DependencyGraph::NodeCount() const noexcept {
    return state_ != nullptr ? state_->nodes.Size() : 0U;
}

std::uint64_t DependencyGraph::Generation() const noexcept {
    return state_ != nullptr ? state_->generation : 0U;
}

struct DocumentCache::State {
    struct Entry {
        explicit Entry(Base::IAllocator&) noexcept {}

        Base::ResourceUri uri;
        std::uint64_t entrySize = 0U;
        CompiledDocument document;
        std::uint64_t sourceRevision = 0U;
        std::uint64_t sourceIdentity = 0U;
        std::uint64_t lastAccess = 0U;
    };

    State(
        Base::IAllocator& allocator,
        const DocumentCacheLimits& valueLimits) noexcept
        : allocator(&allocator),
          entries(&allocator),
          graph(&allocator),
          limits(valueLimits) {}

    bool EraseEntry(
        const Base::ResourceUri& uri,
        bool eviction) noexcept {
        Base::Result<Base::String> key = MakeKey(uri, *allocator);
        if (!key) return false;
        Entry* entry = entries.Find(key.Value());
        if (entry == nullptr) return false;
        compiledBytes -= entry->entrySize;
        entries.Erase(key.Value());
        graph.Remove(uri);
        if (eviction) ++evictions;
        else ++invalidations;
        ++generation;
        return true;
    }

    void EvictToLimits() noexcept {
        while ((limits.maxEntries != 0U &&
                entries.Size() > limits.maxEntries) ||
               (limits.maxCompiledBytes != 0U &&
                compiledBytes > limits.maxCompiledBytes)) {
            const typename Base::HashMap<Base::String, Entry>::Entry*
                oldest = nullptr;
            for (const auto& current : entries) {
                if (oldest == nullptr ||
                    current.Value().lastAccess <
                        oldest->Value().lastAccess) {
                    oldest = &current;
                }
            }
            if (oldest == nullptr) break;
            Base::ResourceUri uri = oldest->Value().uri;
            if (!EraseEntry(uri, true)) break;
        }
    }

    Base::IAllocator* allocator = nullptr;
    Base::HashMap<Base::String, Entry> entries;
    DependencyGraph graph;
    DocumentCacheLimits limits;
    std::uint64_t compiledBytes = 0U;
    std::uint64_t accessSequence = 0U;
    std::uint64_t hits = 0U;
    std::uint64_t misses = 0U;
    std::uint64_t stores = 0U;
    std::uint64_t invalidations = 0U;
    std::uint64_t evictions = 0U;
    std::uint64_t generation = 0U;
};

DocumentCache::DocumentCache(
    Base::IAllocator* allocator,
    const DocumentCacheLimits& limits) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    static_cast<void>(Base::AllocateObject(
        *allocator_, Base::MemoryTag::Markup, state_, *allocator_, limits));
}

DocumentCache::~DocumentCache() noexcept {
    if (state_ == nullptr) return;
    Base::FreeObject(*allocator_, Base::MemoryTag::Markup, state_);
    state_ = nullptr;
}

DocumentCache::DocumentCache(
    DocumentCache&& other) noexcept
    : allocator_(other.allocator_),
      state_(other.state_) {
    other.state_ = nullptr;
    other.allocator_ = nullptr;
}

DocumentCache& DocumentCache::operator=(
    DocumentCache&& other) noexcept {
    if (this == &other) return *this;
    if (state_ != nullptr) {
        Base::FreeObject(*allocator_, Base::MemoryTag::Markup, state_);
    }
    allocator_ = other.allocator_;
    state_ = other.state_;
    other.state_ = nullptr;
    other.allocator_ = nullptr;
    return *this;
}

Base::Result<DocumentCacheLookup> DocumentCache::Lookup(
    const Base::ResourceUri& uri,
    std::uint64_t sourceRevision,
    std::uint64_t sourceIdentity,
    const ::Aero::Meta::Registry& domain,
    const CompiledDocumentLimits& limits) noexcept {
    DocumentCacheLookup result;
    (void)domain;
    (void)limits;
    if (state_ == nullptr || uri.Empty()) return result;
    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    if (!key) return key.GetStatus();
    DocumentCache::State::Entry* entry = state_->entries.Find(key.Value());
    if (entry == nullptr) {
        ++state_->misses;
        return result;
    }
    if (entry->sourceRevision != sourceRevision ||
        entry->sourceIdentity != sourceIdentity) {
        ++state_->misses;
        Base::Result<std::uint32_t> invalidated =
            Invalidate(uri, true);
        if (!invalidated) return invalidated.GetStatus();
        return result;
    }

    if (!entry->document.IsValid()) {
        ++state_->misses;
        return result;
    }

    entry->lastAccess = ++state_->accessSequence;
    ++state_->hits;
    result.hit = true;
    result.sourceRevision = entry->sourceRevision;
    result.document = &entry->document;
    return result;
}

namespace {

template<class TDoc>
Base::Result<void> StoreInternal(
    DocumentCache::State* state,
    Base::IAllocator* allocator,
    const Base::ResourceUri& uri,
    std::uint64_t sourceRevision,
    std::uint64_t sourceIdentity,
    TDoc&& document,
    Base::Span<const Base::ResourceUri> dependencies) noexcept {
    if (state == nullptr || uri.Empty() || !document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML document cache entry is invalid");
    }
    CompiledDocument doc;
    if constexpr (std::is_rvalue_reference_v<TDoc&&>) {
        doc = std::move(document);
    } else {
        Base::Result<CompiledDocument> cloned = document.Clone();
        if (!cloned) return cloned.GetStatus();
        doc = std::move(cloned).Value();
    }

    const std::uint64_t serializedSize =
        static_cast<std::uint64_t>(doc.Nodes().Size()) * sizeof(Node) + sizeof(CompiledDocument);

    Base::Result<Base::String> key = MakeKey(uri, *allocator);
    if (!key) return key.GetStatus();
    DocumentCache::State::Entry* existing = state->entries.Find(key.Value());
    if (existing != nullptr) {
        state->compiledBytes -= existing->entrySize;
        existing->uri = uri;
        existing->entrySize = serializedSize;
        existing->document = std::move(doc);
        existing->sourceRevision = sourceRevision;
        existing->sourceIdentity = sourceIdentity;
        existing->lastAccess = ++state->accessSequence;
        state->compiledBytes += existing->entrySize;
    } else {
        DocumentCache::State::Entry entry(*allocator);
        entry.uri = uri;
        entry.entrySize = serializedSize;
        entry.document = std::move(doc);
        entry.sourceRevision = sourceRevision;
        entry.sourceIdentity = sourceIdentity;
        entry.lastAccess = ++state->accessSequence;
        state->compiledBytes += entry.entrySize;
        typename Base::HashMap<Base::String, DocumentCache::State::Entry>::InsertResult
            inserted = state->entries.Insert(
                std::move(key).Value(), std::move(entry));
        if (!inserted.inserted) {
            state->compiledBytes -= serializedSize;
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Document cache entry already exists");
        }
    }
    Base::Result<void> graph =
        state->graph.Update(uri, dependencies);
    if (!graph) {
        state->EraseEntry(uri, false);
        return graph.GetStatus();
    }
    ++state->stores;
    ++state->generation;
    state->EvictToLimits();
    return {};
}

} // namespace

Base::Result<void> DocumentCache::Store(
    const Base::ResourceUri& uri,
    std::uint64_t sourceRevision,
    std::uint64_t sourceIdentity,
    const CompiledDocument& document,
    Base::Span<const Base::ResourceUri> dependencies) noexcept {
    return StoreInternal(state_, allocator_, uri, sourceRevision, sourceIdentity, document, dependencies);
}

Base::Result<void> DocumentCache::Store(
    const Base::ResourceUri& uri,
    std::uint64_t sourceRevision,
    std::uint64_t sourceIdentity,
    CompiledDocument&& document,
    Base::Span<const Base::ResourceUri> dependencies) noexcept {
    return StoreInternal(state_, allocator_, uri, sourceRevision, sourceIdentity, std::move(document), dependencies);
}

Base::Result<std::uint32_t> DocumentCache::Invalidate(
    const Base::ResourceUri& uri,
    bool includeDependents) noexcept {
    if (state_ == nullptr || uri.Empty()) return 0U;
    Base::Vector<Base::ResourceUri> affected(allocator_);
    if (includeDependents) {
        Base::Result<void> collected =
            state_->graph.CollectAffected(uri, affected);
        if (!collected) return collected.GetStatus();
    } else {
        affected.PushBack(uri);
    }
    std::uint32_t count = 0U;
    for (std::uint32_t index = affected.Size();
         index > 0U;
         --index) {
        const Base::ResourceUri& affectedUri = affected[index - 1U];
        if (state_->EraseEntry(affectedUri, false)) {
            ++count;
        } else {
            static_cast<void>(state_->graph.Remove(affectedUri));
        }
    }
    return count;
}

void DocumentCache::Clear() noexcept {
    if (state_ == nullptr) return;
    state_->entries.Clear();
    state_->graph.Clear();
    state_->compiledBytes = 0U;
    ++state_->generation;
}

bool DocumentCache::Contains(
    const Base::ResourceUri& uri) const noexcept {
    if (state_ == nullptr || uri.Empty()) return false;
    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    return key && state_->entries.Contains(key.Value());
}

bool DocumentCache::GetSourceRevision(
    const Base::ResourceUri& uri,
    std::uint64_t sourceIdentity,
    std::uint64_t& revision) const noexcept {
    revision = 0U;
    if (state_ == nullptr || uri.Empty()) return false;
    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    if (!key) return false;
    const DocumentCache::State::Entry* entry = state_->entries.Find(key.Value());
    if (entry == nullptr || entry->sourceIdentity != sourceIdentity) {
        return false;
    }
    revision = entry->sourceRevision;
    return true;
}

Base::Result<void> DocumentCache::CollectAffected(
    const Base::ResourceUri& changed,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    return state_ != nullptr
        ? state_->graph.CollectAffected(changed, output)
        : Base::Result<void>{};
}

const DependencyGraph& DocumentCache::Dependencies() const noexcept {
    static const DependencyGraph empty;
    return state_ != nullptr ? state_->graph : empty;
}

DocumentCacheStatistics DocumentCache::Statistics() const noexcept {
    DocumentCacheStatistics result;
    if (state_ == nullptr) return result;
    result.entryCount = state_->entries.Size();
    result.compiledBytes = state_->compiledBytes;
    result.hitCount = state_->hits;
    result.missCount = state_->misses;
    result.storeCount = state_->stores;
    result.invalidationCount = state_->invalidations;
    result.evictionCount = state_->evictions;
    result.generation = state_->generation;
    return result;
}

const DocumentCacheLimits& DocumentCache::Limits() const noexcept {
    static const DocumentCacheLimits empty{};
    return state_ != nullptr ? state_->limits : empty;
}

} // namespace Aero::Markup



// ============================================================================
// SECTION: ReloadCoordinator
// ============================================================================





namespace Aero::Markup {

struct ReloadCoordinatorState final {
    struct RevisionRecord  {
        Base::ResourceUri uri;
        std::uint64_t revision = 0U;
    };

    ReloadCoordinatorState(
        View& valueView,
        Base::IAllocator& valueAllocator,
        GuiRuntime* valueGui) noexcept
        : view(&valueView),
          allocator(&valueAllocator),
          gui(valueGui),
          revisions(&valueAllocator) {}

    Base::Result<std::uint64_t> ReadRevision(
        const Base::ResourceUri& uri) noexcept {
        if (view == nullptr || gui == nullptr || uri.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML reload source is unavailable");
        }
        std::uint64_t sourceIdentity = 0U;
        std::uint64_t revision = 0U;
        Base::Result<void> queried = gui->QuerySource(
            uri, sourceIdentity, revision);
        return queried
            ? Base::Result<std::uint64_t>(revision)
            : Base::Result<std::uint64_t>(queried.GetStatus());
    }

    static bool HasTracked(
        const Base::Vector<RevisionRecord>& records,
        const Base::ResourceUri& uri) noexcept {
        for (const RevisionRecord& record : records) {
            if (record.uri == uri) return true;
        }
        return false;
    }

    Base::Result<void> AddTracked(
        Base::Vector<RevisionRecord>& records,
        const Base::ResourceUri& uri) noexcept {
        if (uri.Empty() || HasTracked(records, uri)) return {};
        if (view == nullptr || gui == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML reload View is unavailable");
        }
        std::uint64_t sourceIdentity = 0U;
        std::uint64_t currentRevision = 0U;
        Base::Result<void> queried = gui->QuerySource(
            uri, sourceIdentity, currentRevision);
        if (!queried) return queried.GetStatus();
        std::uint64_t revision = currentRevision;
        static_cast<void>(gui->TryGetCachedRevision(
            uri, sourceIdentity, revision));
        RevisionRecord record;
        record.uri = uri;
        record.revision = revision;
        records.PushBack(std::move(record));
        return {};
    }

    Base::Result<void> BuildTrackedSources(
        const XamlDocument& document,
        Base::ResourceUri& resolvedRoot,
        Base::Vector<RevisionRecord>& output) noexcept {
        output.Clear();
        if (view == nullptr || !document.IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML reload document is unavailable");
        }
        if (!document.CanonicalUri().Empty()) {
            resolvedRoot = document.CanonicalUri();
        }
        if (resolvedRoot.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML reload root URI is empty");
        }
        output.Reserve(1U + document.Dependencies().Size());
        Base::Result<void> root = AddTracked(output, resolvedRoot);
        if (!root) return root.GetStatus();
        for (const Base::ResourceUri& dependency : document.Dependencies()) {
            Base::Result<void> tracked = AddTracked(output, dependency);
            if (!tracked) return tracked.GetStatus();
        }
        return {};
    }

    Base::Result<ReloadResult> ReloadFor(
        const Base::ResourceUri& changed,
        Diagnostics::IDiagnosticSink* diagnostics) noexcept {
        if (!active || view == nullptr || view->GetContent() == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML reload coordinator is not active");
        }
        if (gui == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML reload Gui state is unavailable");
        }
        Base::Result<std::uint32_t> invalidated =
            gui->Invalidate(changed, true);
        if (!invalidated) return invalidated.GetStatus();
        const std::uint32_t invalidatedCount = invalidated.Value();

        XamlReader reader(view->GetGui());
        Base::Result<XamlDocument> replacement =
            reader.Load(rootUri.Canonical(), {}, diagnostics);
        if (!replacement) return replacement.GetStatus();
        Base::ResourceUri replacementRoot = rootUri;
        Base::Vector<RevisionRecord> replacementRevisions(allocator);
        Base::Result<void> tracked = BuildTrackedSources(
            replacement.Value(), replacementRoot, replacementRevisions);
        if (!tracked) return tracked.GetStatus();
        Aero::Size targetSize = availableSize;
        if ((targetSize.width <= 0.0 || targetSize.height <= 0.0) &&
            view != nullptr) {
            targetSize = view->GetSize();
        }
        if (!view->SetContent(
            std::move(replacement).Value(), targetSize)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState, "SetContent failed");
        }
        rootUri = replacementRoot;
        revisions = std::move(replacementRevisions);

        ReloadResult result;
        result.changed = true;
        result.reloaded = true;
        result.invalidatedDocuments = invalidatedCount;
        result.generation = ++generation;
        result.changedUri = changed;
        return result;
    }

    View* view = nullptr;
    Base::IAllocator* allocator = nullptr;
    GuiRuntime* gui = nullptr;
    Base::ResourceUri rootUri;
    Aero::Size availableSize;
    Base::Vector<RevisionRecord> revisions;
    std::uint64_t consumedXamlChangeGeneration = 0U;
    std::uint64_t generation = 0U;
    bool active = false;
};

ReloadCoordinator::ReloadCoordinator(
    View& view,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    Gui& gui = view.GetGui();
    GuiRuntime* guiState = gui.state_.Get();
    ReloadCoordinatorState* state = nullptr;
    static_cast<void>(Base::AllocateObject(
        *allocator_, Base::MemoryTag::Markup, state, view, *allocator_, guiState));
    state_ = state;
}

ReloadCoordinator::~ReloadCoordinator() noexcept {
    if (state_ == nullptr) return;
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    Base::FreeObject(*allocator_, Base::MemoryTag::Markup, state);
    state_ = nullptr;
}

ReloadCoordinator::ReloadCoordinator(
    ReloadCoordinator&& other) noexcept
    : allocator_(other.allocator_), state_(other.state_) {
    other.allocator_ = nullptr;
    other.state_ = nullptr;
}

ReloadCoordinator& ReloadCoordinator::operator=(
    ReloadCoordinator&& other) noexcept {
    if (this == &other) return *this;
    if (state_ != nullptr) {
        auto* state = static_cast<ReloadCoordinatorState*>(state_);
        Base::FreeObject(*allocator_, Base::MemoryTag::Markup, state);
    }
    allocator_ = other.allocator_;
    state_ = other.state_;
    other.allocator_ = nullptr;
    other.state_ = nullptr;
    return *this;
}

Base::Result<void> ReloadCoordinator::Start(
    Base::StringView rootUri,
    Aero::Size availableSize,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    if (state == nullptr || state->view == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "XAML reload requires a View");
    }
    if (state->active || state->view->GetContent() != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML reload requires an unmounted View");
    }
    Base::Result<Base::ResourceUri> parsed =
        Base::ResourceUri::Parse(rootUri);
    if (!parsed) return parsed.GetStatus();
    XamlReader reader(state->view->GetGui());
    Base::Result<XamlDocument> document =
        reader.Load(rootUri, {}, diagnostics);
    if (!document) return document.GetStatus();
    Base::ResourceUri resolvedRoot = parsed.Value();
    Base::Vector<ReloadCoordinatorState::RevisionRecord> revisions(
        state->allocator);
    Base::Result<void> tracked = state->BuildTrackedSources(
        document.Value(), resolvedRoot, revisions);
    if (!tracked) return tracked.GetStatus();
    if (!state->view->SetContent(
        std::move(document).Value(), availableSize)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState, "SetContent failed");
    }

    state->rootUri = resolvedRoot;
    state->revisions = std::move(revisions);
    state->availableSize = availableSize;
    state->consumedXamlChangeGeneration = state->gui != nullptr
        ? state->gui->xamlChangeGeneration : 0U;
    state->active = true;
    return {};
}

void ReloadCoordinator::Stop() noexcept {
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    if (state == nullptr) return;
    state->active = false;
    state->revisions.Clear();
    state->rootUri = {};
}

Base::Result<ReloadResult> ReloadCoordinator::Poll(
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    ReloadResult noChange;
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    if (state == nullptr || !state->active) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML reload coordinator is not active");
    }
    noChange.generation = state->generation;
    if (state->gui != nullptr &&
        state->gui->xamlChangeGeneration >
            state->consumedXamlChangeGeneration) {
        const std::uint64_t latest = state->gui->xamlChangeGeneration;
        if (state->gui->xamlChangesLost) {
            state->consumedXamlChangeGeneration = latest;
            return state->ReloadFor(state->rootUri, diagnostics);
        }
        for (const XamlProviderChangeRecord& change :
             state->gui->xamlChanges) {
            if (change.generation <=
                    state->consumedXamlChangeGeneration) {
                continue;
            }
            state->consumedXamlChangeGeneration = change.generation;
            if (change.uri.Empty()) {
                return state->ReloadFor(state->rootUri, diagnostics);
            }
            for (const ReloadCoordinatorState::RevisionRecord& record :
                 state->revisions) {
                if (record.uri == change.uri) {
                    return state->ReloadFor(change.uri, diagnostics);
                }
            }
        }
        state->consumedXamlChangeGeneration = latest;
    }
    for (const ReloadCoordinatorState::RevisionRecord& record : state->revisions) {
        Base::Result<std::uint64_t> current =
            state->ReadRevision(record.uri);
        if (!current) return current.GetStatus();
        if (current.Value() == record.revision) continue;
        return state->ReloadFor(record.uri, diagnostics);
    }
    return noChange;
}

Base::Result<ReloadResult> ReloadCoordinator::Reload(
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    if (state == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML reload coordinator is unavailable");
    }
    return state->ReloadFor(state->rootUri, diagnostics);
}

Base::Result<ReloadResult>
ReloadCoordinator::NotifySourceChanged(
    const Base::ResourceUri& changedUri,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    if (state == nullptr || changedUri.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML reload changed URI is invalid");
    }
    return state->ReloadFor(changedUri, diagnostics);
}

bool ReloadCoordinator::IsActive() const noexcept {
    const auto* state = static_cast<const ReloadCoordinatorState*>(state_);
    return state != nullptr && state->active;
}

const Base::ResourceUri& ReloadCoordinator::RootUri() const noexcept {
    static const Base::ResourceUri empty;
    const auto* state = static_cast<const ReloadCoordinatorState*>(state_);
    return state != nullptr ? state->rootUri : empty;
}

std::uint64_t ReloadCoordinator::Generation() const noexcept {
    const auto* state = static_cast<const ReloadCoordinatorState*>(state_);
    return state != nullptr ? state->generation : 0U;
}

std::uint32_t ReloadCoordinator::TrackedSourceCount() const noexcept {
    const auto* state = static_cast<const ReloadCoordinatorState*>(state_);
    return state != nullptr ? state->revisions.Size() : 0U;
}

} // namespace Aero::Markup

// ============================================================================
// SECTION: XamlReader entry points
// ============================================================================


namespace Aero::Markup {
namespace {

struct XamlLoadScope {
    XamlLoadScope(
        Threading::Dispatcher& dispatcher,
        GuiSchema& schema,
        DocumentCache& documents) noexcept
        : factory(
              dispatcher,
              (
                  schema.Metadata()).DependencyProperties(),
              schema.Metadata()) {
        load.documentCache = &documents;
        load.dispatcher = &dispatcher;
        load.dependencyProperties =
            &(
                schema.Metadata()).DependencyProperties();
        load.effectCommitMode = EffectCommitMode::Deferred;
    }

    Meta::ObjectFactoryScope factory;
    LoadState load;
};

} // namespace

Base::Result<XamlDocument> XamlReader::Load(
    Base::StringView uri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    if (gui_ == nullptr || !gui_->IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Gui must be initialized before XAML loading");
    }
    GuiRuntime& state = *gui_->state_;
    XamlLoadScope scope(state.dispatcher, state.schema, state.documents);
    return state.Load(
        &scope.load, uri, settings, diagnostics);
}

Base::Result<XamlDocument> XamlReader::Load(
    Base::StringView uri,
    Aero::ResourceDictionary& resources,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    if (gui_ == nullptr || !gui_->IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Gui must be initialized before XAML loading");
    }
    GuiRuntime& state = *gui_->state_;
    XamlLoadScope scope(state.dispatcher, state.schema, state.documents);
    scope.load.resources = &resources;
    scope.load.fallbackResources = &resources;
    return state.Load(
        &scope.load, uri, settings, diagnostics);
}

Base::Result<XamlDocument> XamlReader::Load(
    Base::Stream& source,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    if (gui_ == nullptr || !gui_->IsInitialized()) {
        return Base::Status::Failure(Base::ErrorCode::NotInitialized,
            "Gui must be initialized before XAML loading");
    }
    GuiRuntime& state = *gui_->state_;
    XamlLoadScope scope(state.dispatcher, state.schema, state.documents);
    return state.Parse(
        &scope.load, source, baseUri, settings, diagnostics);
}

Base::Result<XamlDocument> XamlReader::LoadComponentCore(
    Base::StringView uri,
    Meta::TypeId expectedRoot,
    ResourceDictionary* resources,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    Base::Result<XamlDocument> loaded = resources != nullptr
        ? Load(uri, *resources, settings, diagnostics)
        : Load(uri, settings, diagnostics);
    if (!loaded) return loaded.GetStatus();
    const Base::Ref<Base::Object>& root = loaded.Value().Root();
    if (!root || gui_ == nullptr || expectedRoot == Meta::InvalidTypeId) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "XAML component root is incompatible with the requested type");
    }
    const GuiRuntime& state = *gui_->state_;
    if (root->RuntimeType() != expectedRoot &&
        !state.schema.Metadata().Types().IsDerivedFrom(
            root->RuntimeType(), expectedRoot)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "XAML component root is incompatible with the requested type");
    }
    return std::move(loaded).Value();
}

Base::Result<XamlDocument> XamlReader::LoadComponentInto(
    Base::Ref<Base::Object> existingRoot,
    Base::StringView uri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics,
    ResourceDictionary* resources) noexcept {
    if (gui_ == nullptr || !gui_->IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Gui must be initialized before XAML component loading");
    }
    if (!existingRoot) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML component requires an existing root object");
    }
    GuiRuntime& state = *gui_->state_;
    XamlLoadScope scope(state.dispatcher, state.schema, state.documents);
    if (resources != nullptr) {
        // Ambient lookup only. `resources` is the document dictionary; wiring
        // Application.Resources there lets LoadComponent mutate/seal app-wide
        // styles and drops Button/Thumb hover storyboards.
        scope.load.fallbackResources = resources;
    }
    Base::Result<XamlDocument> loaded = state.LoadComponentInto(
        &scope.load,
        *existingRoot,
        uri,
        settings,
        diagnostics);
    if (!loaded) return loaded.GetStatus();
    if (loaded.Value().Root().Get() != existingRoot.Get()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML component replaced its existing root object");
    }
    return std::move(loaded).Value();
}

Base::Result<XamlDocument> XamlReader::Parse(
    Base::StringView source,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    if (gui_ == nullptr || !gui_->IsInitialized()) {
        return Base::Status::Failure(Base::ErrorCode::NotInitialized,
            "Gui must be initialized before XAML parsing");
    }
    GuiRuntime& state = *gui_->state_;
    XamlLoadScope scope(state.dispatcher, state.schema, state.documents);
    return state.Parse(
        &scope.load, source, baseUri, settings, diagnostics);
}

Base::Result<XamlDocument> XamlReader::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri) noexcept {
    if (gui_ == nullptr || !gui_->IsInitialized()) {
        return Base::Status::Failure(Base::ErrorCode::NotInitialized,
            "Gui must be initialized before compiled XAML loading");
    }
    GuiRuntime& state = *gui_->state_;
    XamlLoadScope scope(state.dispatcher, state.schema, state.documents);
    return state.LoadCompiled(
        &scope.load, bytes, originUri, XamlReaderSettings{});
}

} // namespace Aero::Markup