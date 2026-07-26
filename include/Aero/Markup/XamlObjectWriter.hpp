#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Markup/XamlNamesResources.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

#include <cstdint>

namespace Aero::Markup {

class XamlCompiledDocument;
struct XamlLoadContext;

namespace XamlObjectWriterDiagnosticCodes {
inline constexpr Core::DiagnosticCode UnknownType =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 201U);
inline constexpr Core::DiagnosticCode TypeNotConstructible =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 202U);
inline constexpr Core::DiagnosticCode UnknownMember =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 203U);
inline constexpr Core::DiagnosticCode InvalidAttachedMember =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 204U);
inline constexpr Core::DiagnosticCode UnsupportedMember =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 205U);
inline constexpr Core::DiagnosticCode InvalidValue =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 206U);
inline constexpr Core::DiagnosticCode InvalidWriterState =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 207U);
inline constexpr Core::DiagnosticCode MissingContentProperty =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 208U);
inline constexpr Core::DiagnosticCode DuplicateMemberValue =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 209U);
inline constexpr Core::DiagnosticCode InitializationFailed =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 210U);
inline constexpr Core::DiagnosticCode UnexpectedText =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 211U);
inline constexpr Core::DiagnosticCode TypeMismatch =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 212U);
inline constexpr Core::DiagnosticCode FactoryFailed =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 213U);
inline constexpr Core::DiagnosticCode MissingMemberValue =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 214U);
inline constexpr Core::DiagnosticCode MultipleRootObjects =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 215U);
inline constexpr Core::DiagnosticCode InvalidDirective =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 216U);
inline constexpr Core::DiagnosticCode DuplicateName =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 217U);
inline constexpr Core::DiagnosticCode DuplicateResourceKey =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 218U);
inline constexpr Core::DiagnosticCode StaticResourceNotFound =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 219U);
inline constexpr Core::DiagnosticCode MissingResourceScope =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 220U);
inline constexpr Core::DiagnosticCode NullNotAllowed =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 221U);
inline constexpr Core::DiagnosticCode InvalidMarkupExtension =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 222U);
inline constexpr Core::DiagnosticCode NamespaceState =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 223U);
inline constexpr Core::DiagnosticCode NameRegistrationFailed =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 224U);
inline constexpr Core::DiagnosticCode ResourceRegistrationFailed =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 225U);
inline constexpr Core::DiagnosticCode UnknownMarkupExtension =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 226U);
inline constexpr Core::DiagnosticCode MarkupExtensionFailed =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 227U);
} // namespace XamlObjectWriterDiagnosticCodes

class AERO_API XamlObjectWriter final {
public:
    explicit XamlObjectWriter(
        XamlSchemaContext& schema,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    ~XamlObjectWriter() noexcept;

    XamlObjectWriter(const XamlObjectWriter&) = delete;
    XamlObjectWriter& operator=(const XamlObjectWriter&) = delete;

    Base::Result<Base::Ref<Base::Object>> Load(
        XamlNodeReader& reader) noexcept;
    Base::Result<Base::Ref<Base::Object>> Load(
        XamlNodeReader& reader,
        const XamlLoadContext& context) noexcept;
    Base::Result<Base::Ref<Base::Object>> Load(
        const XamlCompiledDocument& document) noexcept;
    Base::Result<Base::Ref<Base::Object>> Load(
        const XamlCompiledDocument& document,
        const XamlLoadContext& context) noexcept;
    void Reset() noexcept;

    const NameScope& DocumentNameScope() const noexcept {
        return committedNames_;
    }
    const ResourceDictionary& DocumentResources() const noexcept {
        return committedResources_;
    }

private:
    static constexpr std::uint32_t InvalidIndex = UINT32_MAX;

    enum class FrameKind : std::uint8_t {
        Object = 0U,
        Member,
        Directive,
        NullObject
    };

    enum class DirectiveKind : std::uint8_t {
        None = 0U,
        Name,
        Key
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
        XamlResolvedMember member;
        Core::SourceSpan source;
        std::uint32_t valuesWritten = 0U;
        bool propertyElement = false;
    };

    struct CreatedObjectRecord final {
        CreatedObjectRecord() noexcept = default;

        CreatedObjectRecord(CreatedObjectRecord&&) noexcept = default;
        CreatedObjectRecord& operator=(CreatedObjectRecord&&) noexcept = default;

        CreatedObjectRecord(const CreatedObjectRecord&) = delete;
        CreatedObjectRecord& operator=(const CreatedObjectRecord&) = delete;

        Base::Ref<Base::Object> object;
        Core::TypeId type = Core::InvalidTypeId;
        Base::String name;
        Base::String key;
        bool beginCalled = false;
        bool endCalled = false;
        bool nameRegistered = false;
        bool resourceRegistered = false;
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
        NameScope names;
    };

    struct ResourceScopeRecord final {
        ResourceScopeRecord() noexcept = default;

        ResourceScopeRecord(ResourceScopeRecord&&) noexcept = default;
        ResourceScopeRecord& operator=(ResourceScopeRecord&&) noexcept = default;

        ResourceScopeRecord(const ResourceScopeRecord&) = delete;
        ResourceScopeRecord& operator=(const ResourceScopeRecord&) = delete;

        std::uint32_t ownerObjectIndex = InvalidIndex;
        ResourceDictionary resources;
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

    XamlSchemaContext* schema_ = nullptr;
    Core::IDiagnosticSink* diagnostics_ = nullptr;
    Base::Vector<Frame> frames_;
    Base::Vector<CreatedObjectRecord> created_;
    Base::Vector<AssignmentRecord> assignments_;
    Base::Vector<NameScopeRecord> nameScopes_;
    Base::Vector<ResourceScopeRecord> resourceScopes_;
    Base::Vector<NamespaceBindingRecord> namespaceBindings_;
    Base::Vector<PendingNamespaceRecord> pendingNamespaces_;
    NameScope committedNames_;
    ResourceDictionary committedResources_;
    Base::Ref<Base::Object> root_;
    std::uint32_t rootObjectIndex_ = InvalidIndex;
    std::uint32_t documentNameScopeIndex_ = InvalidIndex;
    std::uint32_t documentResourceScopeIndex_ = InvalidIndex;
    bool loading_ = false;
    bool ended_ = false;
    const XamlLoadContext* loadContext_ = nullptr;

    Base::Result<Base::Ref<Base::Object>> LoadReaderCore(
        XamlNodeReader& reader) noexcept;
    Base::Result<Base::Ref<Base::Object>> LoadCompiledCore(
        const XamlCompiledDocument& document) noexcept;
    Base::Result<Base::Ref<Base::Object>> CreateObject(
        Core::TypeId type) const noexcept;

    Base::Result<void> ProcessNode(
        const XamlNode& node) noexcept;
    Base::Result<void> QueueNamespaceDeclaration(
        const XamlNode& node) noexcept;
    Base::Result<void> StartObject(
        const XamlNode& node) noexcept;
    Base::Result<void> StartNullObject(
        const XamlNode& node,
        std::uint32_t bindingStart) noexcept;
    Base::Result<void> EndObject(
        const XamlNode& node) noexcept;
    Base::Result<void> StartMember(
        const XamlNode& node) noexcept;
    Base::Result<void> StartDirective(
        const XamlNode& node,
        DirectiveKind directive,
        std::uint32_t targetObjectIndex) noexcept;
    Base::Result<void> EndMember(
        const XamlNode& node) noexcept;
    Base::Result<void> WriteText(
        const XamlNode& node) noexcept;
    Base::Result<void> WriteDirectiveText(
        Frame& frame,
        const XamlNode& node) noexcept;

    Base::Result<void> StartPropertyElement(
        const XamlNode& node,
        std::uint32_t targetFrameIndex,
        std::uint32_t bindingStart) noexcept;
    Base::Result<void> CompleteObject(
        const XamlNode& node) noexcept;
    Base::Result<void> CompleteNullObject(
        const XamlNode& node) noexcept;
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
        XamlValue&& value,
        Core::SourceSpan source) noexcept;
    Base::Result<void> WriteValue(
        std::uint32_t targetObjectIndex,
        const XamlResolvedMember& member,
        XamlValue&& value,
        Core::SourceSpan source) noexcept;

    Base::Result<void> RegisterObjectName(
        std::uint32_t objectIndex,
        Core::SourceSpan source) noexcept;
    Base::Result<bool> RegisterObjectResource(
        std::uint32_t objectIndex,
        Core::SourceSpan source) noexcept;
    Base::Result<XamlResourceValue> LookupResource(
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

    XamlServiceProvider BuildServices(
        std::uint32_t targetObjectIndex,
        const XamlResolvedMember& member,
        Core::SourceSpan source) noexcept;
    const NameScope* FindActiveNameScope() const noexcept;
    std::uint32_t FindNameScopeIndexForObject(
        std::uint32_t objectIndex) const noexcept;
    std::uint32_t FindResourceScopeIndexForParent() const noexcept;
    std::uint32_t FindObjectFrameIndex(
        std::uint32_t objectIndex) const noexcept;

    MarkupValueKind ParseMarkupValue(
        Base::StringView text,
        Base::StringView& extensionName,
        Base::StringView& argument) const noexcept;
    Base::Result<XamlValue> EvaluateMarkupExtension(
        std::uint32_t targetObjectIndex,
        const XamlResolvedMember& member,
        Base::StringView extensionName,
        Base::StringView arguments,
        Core::SourceSpan source) noexcept;
    bool IsXamlDirective(
        const XamlQualifiedName& name,
        Base::StringView localName) const noexcept;
    bool IsXamlNullObject(
        const XamlQualifiedName& name) const noexcept;
    bool HasPropertyElementSyntax(
        const XamlQualifiedName& name) const noexcept;
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
    static Base::Result<XamlResourceValue>
    ResourceLookupCallback(
        void* context,
        Base::StringView key) noexcept;
};

} // namespace Aero::Markup
