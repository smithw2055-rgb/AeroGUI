#pragma once

#include "Extensions.hpp"

#include "DeferredContent.hpp"
#include "LoadContext.hpp"

#include "ObjectWriter.hpp"

namespace Aero::Markup {

class ObjectWriterState final {
public:
    explicit ObjectWriterState(
        Schema& schema,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    explicit ObjectWriterState(
        const ObjectWriter& writer) noexcept
        : ObjectWriterState(
              writer.GetSchema(),
              writer.Diagnostics()) {}
    ~ObjectWriterState() noexcept;

    ObjectWriterState(const ObjectWriterState&) = delete;
    ObjectWriterState& operator=(const ObjectWriterState&) = delete;

    Base::Result<LoaderResult> Load(
        NodeReader& reader) noexcept;
    Base::Result<LoaderResult> Load(
        NodeReader& reader,
        const LoadContext& context) noexcept;
    Base::Result<LoaderResult> Load(
        const CompiledDocument& document) noexcept;
    Base::Result<LoaderResult> Load(
        const CompiledDocument& document,
        const LoadContext& context) noexcept;

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

    struct Frame final {
        FrameKind kind = FrameKind::Object;
        DirectiveKind directive = DirectiveKind::None;
        std::uint32_t objectIndex = InvalidIndex;
        std::uint32_t targetObjectIndex = InvalidIndex;
        std::uint32_t namespaceBindingStart = InvalidIndex;
        std::uint32_t nameScopeIndex = InvalidIndex;
        std::uint32_t resourceScopeIndex = InvalidIndex;
        ResolvedMember member;
        Core::SourceSpan source;
        std::uint32_t valuesWritten = 0U;
        bool propertyElement = false;
        bool deferredStaticResource = false;
    };

    struct CreatedObjectRecord final {
        CreatedObjectRecord() noexcept = default;

        CreatedObjectRecord(CreatedObjectRecord&&) noexcept = default;
        CreatedObjectRecord& operator=(CreatedObjectRecord&&) noexcept = default;

        CreatedObjectRecord(const CreatedObjectRecord&) = delete;
        CreatedObjectRecord& operator=(const CreatedObjectRecord&) = delete;

        Base::Ref<Base::Object> object;
        Core::Value value;
        Core::TypeId type = Core::InvalidTypeId;
        Base::String name;
        Base::String key;
        bool beginCalled = false;
        bool endCalled = false;
        bool nameRegistered = false;
        bool resourceRegistered = false;
        bool valueElement = false;
    };

    struct AssignmentRecord final {
        std::uint32_t objectIndex = InvalidIndex;
        Core::MemberId member = Core::InvalidMemberId;
        std::uint32_t count = 0U;
    };


    struct NameScopeRecord final {
        NameScopeRecord() noexcept = default;

        NameScopeRecord(NameScopeRecord&&) noexcept = default;
        NameScopeRecord& operator=(NameScopeRecord&&) noexcept = default;

        NameScopeRecord(const NameScopeRecord&) = delete;
        NameScopeRecord& operator=(const NameScopeRecord&) = delete;

        std::uint32_t ownerObjectIndex = InvalidIndex;
        Aero::NameScope names;
    };

    struct ResourceScopeRecord final {
        ResourceScopeRecord() noexcept = default;

        ResourceScopeRecord(ResourceScopeRecord&&) noexcept = default;
        ResourceScopeRecord& operator=(ResourceScopeRecord&&) noexcept = default;

        ResourceScopeRecord(const ResourceScopeRecord&) = delete;
        ResourceScopeRecord& operator=(const ResourceScopeRecord&) = delete;

        std::uint32_t ownerObjectIndex = InvalidIndex;
        Aero::ResourceDictionary resources;
        const Aero::ResourceDictionary* external = nullptr;
    };

    struct NamespaceBindingRecord final {
        NamespaceBindingRecord() noexcept = default;

        NamespaceBindingRecord(NamespaceBindingRecord&&) noexcept = default;
        NamespaceBindingRecord& operator=(NamespaceBindingRecord&&) noexcept = default;

        NamespaceBindingRecord(const NamespaceBindingRecord&) = delete;
        NamespaceBindingRecord& operator=(const NamespaceBindingRecord&) = delete;

        Base::String prefix;
        Base::String uri;
    };

    struct PendingNamespaceRecord final {
        PendingNamespaceRecord() noexcept = default;

        PendingNamespaceRecord(PendingNamespaceRecord&&) noexcept = default;
        PendingNamespaceRecord& operator=(PendingNamespaceRecord&&) noexcept = default;

        PendingNamespaceRecord(const PendingNamespaceRecord&) = delete;
        PendingNamespaceRecord& operator=(const PendingNamespaceRecord&) = delete;

        Base::String prefix;
        Base::String uri;
        Core::SourceSpan source;
    };

    Markup::Schema* schema_ = nullptr;
    Core::IDiagnosticSink* diagnostics_ = nullptr;
    Base::Vector<Frame> frames_;
    Base::Vector<CreatedObjectRecord> created_;
    Base::Vector<AssignmentRecord> assignments_;
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
    const LoadContext* loadContext_ = nullptr;

    Base::Result<LoaderResult> CompleteLoad(
        Base::Result<Base::Ref<Base::Object>> loaded) noexcept;
    Base::Result<Base::Ref<Base::Object>> LoadReaderCore(
        NodeReader& reader) noexcept;
    Base::Result<Base::Ref<Base::Object>> LoadCompiledCore(
        const CompiledDocument& document) noexcept;
    Base::Result<Base::Ref<Base::Object>> LoadCursorCore(
        NodeCursor& cursor) noexcept;
    Base::Result<Base::Ref<Base::Object>> CreateObject(
        Core::TypeId type) const noexcept;

    Base::Result<void> ProcessNode(
        const Node& node) noexcept;
    Base::Result<void> QueueNamespaceDeclaration(
        const Node& node) noexcept;
    Base::Result<void> StartObject(
        const Node& node) noexcept;
    Base::Result<void> StartValueObject(
        const Node& node,
        std::uint32_t bindingStart,
        Core::TypeId type) noexcept;
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
        Core::Value&& value,
        Core::SourceSpan source) noexcept;
    Base::Result<void> WriteObjectToParent(
        std::uint32_t objectIndex,
        Core::SourceSpan source) noexcept;
    Base::Result<void> WriteObjectToContent(
        std::uint32_t parentObjectIndex,
        std::uint32_t childObjectIndex,
        Core::SourceSpan source) noexcept;
    Base::Result<void> WriteNullToParent(
        Core::SourceSpan source) noexcept;
    Base::Result<void> WriteValueToMember(
        Frame& memberFrame,
        Core::Value&& value,
        Core::SourceSpan source) noexcept;
    Base::Result<void> WriteProvidedValueToMember(
        Frame& memberFrame,
        ProvidedValue&& value,
        Core::SourceSpan source) noexcept;
    Base::Result<void> WriteProvidedValue(
        std::uint32_t targetObjectIndex,
        const ResolvedMember& member,
        ProvidedValue&& value,
        Core::SourceSpan source) noexcept;
    Base::Result<void> WriteValue(
        std::uint32_t targetObjectIndex,
        const ResolvedMember& member,
        Core::Value&& value,
        Core::SourceSpan source) noexcept;

    Base::Result<void> RegisterObjectName(
        std::uint32_t objectIndex,
        Core::SourceSpan source) noexcept;
    Base::Result<bool> RegisterObjectResource(
        std::uint32_t objectIndex,
        Core::SourceSpan source) noexcept;
    Base::Result<Aero::ResourceValue> LookupResource(
        Base::StringView key) const noexcept;
    Base::Result<void> CreateScopesForObject(
        std::uint32_t objectIndex,
        Frame& frame,
        Core::SourceSpan source) noexcept;

    Base::Result<void> ActivatePendingNamespaces(
        std::uint32_t& bindingStart) noexcept;
    void PopNamespaceBindings(std::uint32_t bindingStart) noexcept;
    Base::Result<Base::StringView> LookupNamespace(
        Base::StringView prefix) const noexcept;

    ExtensionContext BuildServices(
        std::uint32_t targetObjectIndex,
        const ResolvedMember& member,
        Core::SourceSpan source) noexcept;
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
        Core::SourceSpan source) noexcept;
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
        Core::MemberId member) noexcept;

    void CommitDocumentScopes() noexcept;
    void AbortTransaction() noexcept;
    void ClearTransaction() noexcept;
    Base::Status Failure(
        Base::Status status,
        Core::DiagnosticCode diagnostic,
        Base::StringView message,
        Core::SourceSpan source) noexcept;

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
