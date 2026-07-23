#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Core/MetadataDomain.hpp>
#include <Aero/Core/MetadataRuntime.hpp>
#include <Aero/Markup/XamlNamesResources.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <cstdint>
#include <cstdio>
#include <memory>
#include <utility>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Markup;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

struct Fixture;
Fixture* gFixture = nullptr;
TypeId gRootType = InvalidTypeId;
TypeId gLeafType = InvalidTypeId;

class DirectiveNode final : public Object {
public:
    DirectiveNode(TypeId type, bool root) noexcept
        : type_(type), root_(root) {
        ++liveCount_;
    }

    ~DirectiveNode() override {
        --liveCount_;
    }

    StringView Title() const noexcept {
        return title_.View();
    }
    const Ref<Object>& Reference() const noexcept {
        return reference_;
    }
    const Ref<Object>& Optional() const noexcept {
        return optional_;
    }
    Span<const Ref<Object>> Children() const noexcept {
        return {children_.Data(), children_.Size()};
    }
    const NameScope& Names() const noexcept {
        return names_;
    }
    const ResourceDictionary& Resources() const noexcept {
        return resources_;
    }
    bool ServiceChecked() const noexcept {
        return serviceChecked_;
    }
    bool IsRoot() const noexcept { return root_; }
    TypeId RuntimeType() const noexcept override { return type_; }

    static void ResetCounters() noexcept {
        liveCount_ = 0U;
        abortCount_ = 0U;
    }
    static std::uint32_t LiveCount() noexcept {
        return liveCount_;
    }
    static std::uint32_t AbortCount() noexcept {
        return abortCount_;
    }

private:
    friend Result<void> SetTitle(Object&, const XamlValue&, void*) noexcept;
    friend Result<void> SetReference(Object&, const XamlValue&, void*) noexcept;
    friend Result<void> SetOptional(Object&, const XamlValue&, void*) noexcept;
    friend Result<void> AddChild(Object&, const XamlValue&, void*) noexcept;
    friend Result<void> SetProbe(
        Object&,
        const XamlValue&,
        const XamlServiceProvider&,
        void*) noexcept;
    friend Result<void> RegisterName(
        Object&,
        StringView,
        Object&,
        void*) noexcept;
    friend Result<void> AddResource(
        Object&,
        StringView,
        TypeId,
        const Ref<Object>&,
        void*) noexcept;
    friend Result<void> BeginNode(Object&, void*) noexcept;
    friend Result<void> EndNode(Object&, void*) noexcept;
    friend void AbortNode(Object&, void*) noexcept;

    TypeId type_ = InvalidTypeId;
    String title_;
    Ref<Object> reference_;
    Ref<Object> optional_;
    Vector<Ref<Object>> children_;
    NameScope names_;
    ResourceDictionary resources_;
    bool root_ = false;
    bool begun_ = false;
    bool ended_ = false;
    bool aborted_ = false;
    bool serviceChecked_ = false;

    static std::uint32_t liveCount_;
    static std::uint32_t abortCount_;
};

std::uint32_t DirectiveNode::liveCount_ = 0U;
std::uint32_t DirectiveNode::abortCount_ = 0U;

Result<void> SetProbe(
    Object& object,
    const XamlValue& value,
    const XamlServiceProvider& services,
    void* context) noexcept;
Result<XamlValue> ProvideEcho(
    StringView arguments,
    const XamlServiceProvider& services,
    void* context) noexcept;

Result<Ref<Object>> MakeRoot() noexcept {
    Result<Ref<DirectiveNode>> created = MakeRef<DirectiveNode>(gRootType, true);
    if (!created) {
        return created.GetStatus();
    }
    Ref<DirectiveNode> typed = std::move(created).Value();
    Ref<Object> result(std::move(typed));
    return result;
}

Result<Ref<Object>> MakeLeaf() noexcept {
    Result<Ref<DirectiveNode>> created = MakeRef<DirectiveNode>(gLeafType, false);
    if (!created) {
        return created.GetStatus();
    }
    Ref<DirectiveNode> typed = std::move(created).Value();
    Ref<Object> result(std::move(typed));
    return result;
}

Result<void> SetTitle(
    Object& object,
    const XamlValue& value,
    void*) noexcept {
    if (value.Kind() != XamlValueKind::String) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Title expects a string");
    }
    return static_cast<DirectiveNode&>(object).title_.TryAssign(
        value.AsString());
}

Result<void> SetReference(
    Object& object,
    const XamlValue& value,
    void*) noexcept {
    if (value.Kind() != XamlValueKind::Object) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Reference expects an object");
    }
    static_cast<DirectiveNode&>(object).reference_ = value.AsObject();
    return {};
}

Result<void> SetOptional(
    Object& object,
    const XamlValue& value,
    void*) noexcept {
    if (value.Kind() != XamlValueKind::Object) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Optional expects an object");
    }
    static_cast<DirectiveNode&>(object).optional_ = value.AsObject();
    return {};
}

Result<void> AddChild(
    Object& object,
    const XamlValue& value,
    void*) noexcept {
    if (value.Kind() != XamlValueKind::Object || !value.AsObject()) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Children expects a non-null object");
    }
    return static_cast<DirectiveNode&>(object).children_.TryPushBack(
        value.AsObject());
}

Result<void> BeginNode(Object& object, void*) noexcept {
    DirectiveNode& node = static_cast<DirectiveNode&>(object);
    if (node.begun_) {
        return Status::Failure(
            ErrorCode::InvalidState,
            "BeginInit was called twice");
    }
    node.begun_ = true;
    return {};
}

Result<void> EndNode(Object& object, void*) noexcept {
    DirectiveNode& node = static_cast<DirectiveNode&>(object);
    if (!node.begun_ || node.ended_) {
        return Status::Failure(
            ErrorCode::InvalidState,
            "EndInit was called in an invalid state");
    }
    node.ended_ = true;
    return {};
}

void AbortNode(Object& object, void*) noexcept {
    DirectiveNode& node = static_cast<DirectiveNode&>(object);
    if (!node.aborted_) {
        node.aborted_ = true;
        ++DirectiveNode::abortCount_;
    }
}

Result<void> RegisterName(
    Object& scopeOwner,
    StringView name,
    Object& object,
    void*) noexcept {
    return static_cast<DirectiveNode&>(scopeOwner).names_.TryRegister(
        name,
        object);
}

Result<void> AddResource(
    Object& scopeOwner,
    StringView key,
    TypeId valueType,
    const Ref<Object>& value,
    void*) noexcept {
    return static_cast<DirectiveNode&>(scopeOwner).resources_.TryAdd(
        key,
        valueType,
        value);
}

struct Fixture final {
    MetadataDomain metadata;
    std::unique_ptr<MetadataRuntime> runtime;
    std::unique_ptr<XamlSchemaContext> schema;

    TypeId objectType = InvalidTypeId;
    TypeId stringType = InvalidTypeId;
    TypeId nodeType = InvalidTypeId;
    TypeId rootType = InvalidTypeId;
    TypeId leafType = InvalidTypeId;
    TypeId echoExtensionType = InvalidTypeId;
    TypeId noProviderExtensionType = InvalidTypeId;

    MemberId title = InvalidMemberId;
    MemberId reference = InvalidMemberId;
    MemberId optional = InvalidMemberId;
    MemberId probe = InvalidMemberId;
    MemberId children = InvalidMemberId;

    static Result<void> RegisterModule(
        MetaRegistrationContext& context,
        void* userContext) noexcept {
        return static_cast<Fixture*>(userContext)->RegisterMetadata(context);
    }

    Result<void> RegisterMetadata(MetaRegistrationContext& context) noexcept {
        MetadataRegistrationTypes types = context.Types();
        const StringView ns("urn:directives");
        const TypeRegistration registrations[] = {
            {ns, StringView("Object"), InvalidTypeId, TypeFlags::None, nullptr},
            {ns, StringView("String"), InvalidTypeId,
             TypeFlags::ValueType | TypeFlags::Sealed, nullptr},
            {ns, StringView("Node"), objectType, TypeFlags::Abstract, nullptr},
            {ns, StringView("Root"), nodeType, TypeFlags::None, &MakeRoot},
            {ns, StringView("Leaf"), nodeType, TypeFlags::Sealed, &MakeLeaf},
            {ns, StringView("Echo"), objectType,
             TypeFlags::Sealed | TypeFlags::MarkupExtension, nullptr},
            {ns, StringView("NoProvider"), objectType,
             TypeFlags::Sealed | TypeFlags::MarkupExtension, nullptr}
        };
        for (const TypeRegistration& registration : registrations) {
            Result<TypeId> registered = types.TryRegisterType(registration);
            if (!registered) return registered.GetStatus();
        }

        Result<MemberId> member = types.TryRegisterProperty(
            nodeType,
            {StringView("Title"), stringType, PropertyFlags::Structural});
        if (!member) return member.GetStatus();
        title = member.Value();
        Result<void> status = types.TrySetContentMember(nodeType, title);
        if (!status) return status.GetStatus();

        member = types.TryRegisterProperty(
            nodeType, {StringView("Reference"), nodeType, PropertyFlags::None});
        if (!member) return member.GetStatus();
        reference = member.Value();
        member = types.TryRegisterProperty(
            nodeType, {StringView("Optional"), nodeType, PropertyFlags::None});
        if (!member) return member.GetStatus();
        optional = member.Value();
        member = types.TryRegisterProperty(
            nodeType, {StringView("Probe"), stringType, PropertyFlags::None});
        if (!member) return member.GetStatus();
        probe = member.Value();
        member = types.TryRegisterProperty(
            rootType,
            {StringView("Children"), nodeType,
             PropertyFlags::Structural | PropertyFlags::Collection});
        if (!member) return member.GetStatus();
        children = member.Value();
        return types.TrySetContentMember(rootType, children);
    }

    bool Build() {
        gFixture = this;
        const StringView ns("urn:directives");
        objectType = MakeTypeId(ns, StringView("Object"));
        stringType = MakeTypeId(ns, StringView("String"));
        nodeType = MakeTypeId(ns, StringView("Node"));
        rootType = MakeTypeId(ns, StringView("Root"));
        leafType = MakeTypeId(ns, StringView("Leaf"));
        gRootType = rootType;
        gLeafType = leafType;
        echoExtensionType = MakeTypeId(ns, StringView("Echo"));
        noProviderExtensionType = MakeTypeId(ns, StringView("NoProvider"));

        const StringView moduleName("Tests.XamlDirectivesResources");
        CHECK(metadata.TryRegisterModule({
            MakeMetadataModuleId(moduleName), moduleName, 1U,
            &Fixture::RegisterModule, this}));
        CHECK(metadata.Seal());
        runtime = std::make_unique<MetadataRuntime>(metadata);
        schema = std::make_unique<XamlSchemaContext>(metadata, *runtime);

        CHECK(schema->TryRegisterScalarType(
            stringType, XamlScalarKind::String));
        CHECK(schema->TryRegisterMemberAdapter({
            title, XamlMemberWriteMode::SetOnce, &SetTitle, nullptr}));
        CHECK(schema->TryRegisterMemberAdapter({
            reference, XamlMemberWriteMode::SetOnce, &SetReference, nullptr}));
        CHECK(schema->TryRegisterMemberAdapter({
            optional, XamlMemberWriteMode::SetOnce, &SetOptional, nullptr}));
        CHECK(schema->TryRegisterMemberAdapter({
            probe, XamlMemberWriteMode::SetOnce, nullptr, this, &SetProbe}));
        CHECK(schema->TryRegisterMemberAdapter({
            children, XamlMemberWriteMode::Collection, &AddChild, nullptr}));
        CHECK(schema->TryRegisterTypeAdapter({
            rootType,
            &BeginNode, &EndNode, &AbortNode, nullptr,
            true, true, &RegisterName, &AddResource}));
        CHECK(schema->TryRegisterTypeAdapter({
            leafType, &BeginNode, &EndNode, &AbortNode, nullptr}));
        CHECK(schema->TryRegisterMarkupExtension({
            echoExtensionType, &ProvideEcho, this}));
        Result<void> duplicateExtension = schema->TryRegisterMarkupExtension({
            echoExtensionType, &ProvideEcho, this});
        CHECK(!duplicateExtension);
        CHECK(duplicateExtension.GetStatus().code == ErrorCode::AlreadyExists);
        Result<void> invalidExtension = schema->TryRegisterMarkupExtension({
            leafType, &ProvideEcho, this});
        CHECK(!invalidExtension);
        CHECK(invalidExtension.GetStatus().code == ErrorCode::InvalidArgument);
        CHECK(runtime->Freeze());
        CHECK(schema->Freeze());
        return true;
    }
};

Result<void> SetProbe(
    Object& object,
    const XamlValue& value,
    const XamlServiceProvider& services,
    void* context) noexcept {
    Fixture* fixture = static_cast<Fixture*>(context);
    if (fixture == nullptr || value.Kind() != XamlValueKind::String ||
        services.schema != fixture->schema.get() ||
        services.targetObject != &object ||
        services.targetObjectType != fixture->leafType ||
        services.targetMember != fixture->probe ||
        services.targetValueType != fixture->stringType ||
        services.rootObject == nullptr ||
        services.source.begin.line == 0U ||
        services.nameScope == nullptr ||
        services.nameScope->Find(StringView("root")) != services.rootObject) {
        return Status::Failure(
            ErrorCode::ValidationFailed,
            "Service provider target context is invalid");
    }

    Result<StringView> defaultNamespace = services.namespaces.Lookup(
        StringView());
    if (!defaultNamespace ||
        defaultNamespace.Value() != StringView("urn:directives")) {
        return Status::Failure(
            ErrorCode::ValidationFailed,
            "Service provider default namespace is invalid");
    }
    Result<StringView> xamlNamespace = services.namespaces.Lookup(
        StringView("x"));
    if (!xamlNamespace ||
        xamlNamespace.Value() != XamlLanguageNamespaceUri()) {
        return Status::Failure(
            ErrorCode::ValidationFailed,
            "Service provider XAML namespace is invalid");
    }
    Result<XamlResourceValue> resource = services.resources.Lookup(
        StringView("resource"));
    if (!resource || !resource.Value().object ||
        resource.Value().type != fixture->leafType) {
        return Status::Failure(
            ErrorCode::ValidationFailed,
            "Service provider resource lookup is invalid");
    }

    static_cast<DirectiveNode&>(object).serviceChecked_ = true;
    return {};
}

Result<XamlValue> ProvideEcho(
    StringView arguments,
    const XamlServiceProvider& services,
    void* context) noexcept {
    Fixture* fixture = static_cast<Fixture*>(context);
    if (fixture == nullptr || services.schema != fixture->schema.get() ||
        services.targetObject == nullptr ||
        services.targetObjectType != fixture->leafType ||
        services.targetMember != fixture->title ||
        services.targetValueType != fixture->stringType ||
        services.rootObject == nullptr || services.source.begin.line == 0U) {
        return Status::Failure(
            ErrorCode::ValidationFailed,
            "Markup-extension service context is invalid");
    }
    Result<StringView> currentNamespace = services.namespaces.Lookup(
        StringView("local"));
    if (!currentNamespace ||
        currentNamespace.Value() != StringView("urn:directives")) {
        return Status::Failure(
            ErrorCode::ValidationFailed,
            "Markup-extension namespace context is invalid");
    }
    if (arguments == StringView("fail")) {
        return Status::Failure(
            ErrorCode::ValidationFailed,
            "Echo extension failure requested by test");
    }
    return XamlValue::TryFromString(fixture->stringType, arguments);
}

Result<Ref<Object>> LoadDocument(
    Fixture& fixture,
    StringView xaml,
    DiagnosticBag& diagnostics) noexcept {
    Utf8XmlTokenizer tokenizer;
    Result<void> reset = tokenizer.Reset(xaml, &diagnostics);
    if (!reset) {
        return reset.GetStatus();
    }
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    return writer.Load(reader);
}

bool TestNamesResourcesStaticResourceAndServices() {
    DirectiveNode::ResetCounters();
    {
        Fixture fixture;
        CHECK(fixture.Build());
        DiagnosticBag diagnostics;
        Utf8XmlTokenizer tokenizer;
        const StringView xaml(
            "<Root xmlns=\"urn:directives\" "
            "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
            "x:Name=\"root\">\n"
            "  <Leaf x:Key=\"resource\" Title=\"Resource\"/>\n"
            "  <Leaf x:Name=\"consumer\" Title=\"Consumer\" "
            "Reference=\"{StaticResource resource}\" "
            "Optional=\"{x:Null}\" Probe=\"checked\"/>\n"
            "</Root>");
        CHECK(tokenizer.Reset(xaml, &diagnostics));
        XamlNodeReader reader(tokenizer, &diagnostics);
        XamlObjectWriter writer(*fixture.schema, &diagnostics);
        Result<Ref<Object>> loaded = writer.Load(reader);
        CHECK(loaded);
        CHECK(diagnostics.Size() == 0U);

        Ref<Object> rootObject = std::move(loaded).Value();
        DirectiveNode* root = static_cast<DirectiveNode*>(rootObject.Get());
        CHECK(root != nullptr && root->IsRoot());
        CHECK(root->Children().Size() == 1U);
        DirectiveNode* consumer = static_cast<DirectiveNode*>(
            root->Children()[0].Get());
        CHECK(consumer != nullptr);
        CHECK(consumer->Title() == StringView("Consumer"));
        CHECK(consumer->ServiceChecked());
        CHECK(!consumer->Optional());

        Result<XamlResourceValue> resource = root->Resources().Lookup(
            StringView("resource"));
        CHECK(resource);
        CHECK(resource.Value().type == fixture.leafType);
        CHECK(resource.Value().object.Get() == consumer->Reference().Get());
        CHECK(static_cast<DirectiveNode*>(resource.Value().object.Get())->Title() ==
            StringView("Resource"));

        CHECK(root->Names().Find(StringView("root")) == root);
        CHECK(root->Names().Find(StringView("consumer")) == consumer);
        CHECK(writer.DocumentNameScope().Find(StringView("root")) == root);
        CHECK(writer.DocumentNameScope().Find(StringView("consumer")) == consumer);
        Result<XamlResourceValue> committed =
            writer.DocumentResources().Lookup(StringView("resource"));
        CHECK(committed);
        CHECK(committed.Value().object.Get() == resource.Value().object.Get());
        CHECK(DirectiveNode::LiveCount() == 3U);
    }
    CHECK(DirectiveNode::LiveCount() == 0U);
    return true;
}

bool TestNullObjectElement() {
    DirectiveNode::ResetCounters();
    {
        Fixture fixture;
        CHECK(fixture.Build());
        DiagnosticBag diagnostics;
        Result<Ref<Object>> loaded = LoadDocument(
            fixture,
            StringView(
                "<Root xmlns=\"urn:directives\" "
                "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
                "<Leaf><Leaf.Optional><x:Null/></Leaf.Optional></Leaf>"
                "</Root>"),
            diagnostics);
        CHECK(loaded);
        Ref<Object> rootObject = std::move(loaded).Value();
        DirectiveNode* root = static_cast<DirectiveNode*>(rootObject.Get());
        CHECK(root->Children().Size() == 1U);
        DirectiveNode* leaf = static_cast<DirectiveNode*>(
            root->Children()[0].Get());
        CHECK(!leaf->Optional());
        CHECK(diagnostics.Size() == 0U);
    }
    CHECK(DirectiveNode::LiveCount() == 0U);
    return true;
}

bool TestDuplicateNameRollback() {
    DirectiveNode::ResetCounters();
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Result<Ref<Object>> loaded = LoadDocument(
        fixture,
        StringView(
            "<Root xmlns=\"urn:directives\" "
            "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
            "<Leaf x:Name=\"same\"/><Leaf x:Name=\"same\"/>"
            "</Root>"),
        diagnostics);
    CHECK(!loaded);
    CHECK(diagnostics.Size() == 1U);
    CHECK(diagnostics.Items()[0].Code() ==
        XamlObjectWriterDiagnosticCodes::DuplicateName);
    CHECK(DirectiveNode::AbortCount() >= 2U);
    CHECK(DirectiveNode::LiveCount() == 0U);
    return true;
}

bool TestDuplicateKeyRollback() {
    DirectiveNode::ResetCounters();
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Result<Ref<Object>> loaded = LoadDocument(
        fixture,
        StringView(
            "<Root xmlns=\"urn:directives\" "
            "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
            "<Leaf x:Key=\"same\"/><Leaf x:Key=\"same\"/>"
            "</Root>"),
        diagnostics);
    CHECK(!loaded);
    CHECK(diagnostics.Size() == 1U);
    CHECK(diagnostics.Items()[0].Code() ==
        XamlObjectWriterDiagnosticCodes::DuplicateResourceKey);
    CHECK(DirectiveNode::LiveCount() == 0U);
    return true;
}

bool TestStaticResourceForwardReferenceRejected() {
    DirectiveNode::ResetCounters();
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Result<Ref<Object>> loaded = LoadDocument(
        fixture,
        StringView(
            "<Root xmlns=\"urn:directives\" "
            "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
            "<Leaf Reference=\"{StaticResource later}\"/>"
            "<Leaf x:Key=\"later\"/>"
            "</Root>"),
        diagnostics);
    CHECK(!loaded);
    CHECK(diagnostics.Size() == 1U);
    CHECK(diagnostics.Items()[0].Code() ==
        XamlObjectWriterDiagnosticCodes::StaticResourceNotFound);
    CHECK(DirectiveNode::LiveCount() == 0U);
    return true;
}

bool TestInvalidDirectiveAndRootKey() {
    DirectiveNode::ResetCounters();
    {
        Fixture fixture;
        CHECK(fixture.Build());
        DiagnosticBag diagnostics;
        Result<Ref<Object>> loaded = LoadDocument(
            fixture,
            StringView(
                "<Root xmlns=\"urn:directives\" "
                "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
                "x:Unknown=\"bad\"/>"),
            diagnostics);
        CHECK(!loaded);
        CHECK(diagnostics.Size() == 1U);
        CHECK(diagnostics.Items()[0].Code() ==
            XamlObjectWriterDiagnosticCodes::InvalidDirective);
        CHECK(DirectiveNode::LiveCount() == 0U);
    }

    DirectiveNode::ResetCounters();
    {
        Fixture fixture;
        CHECK(fixture.Build());
        DiagnosticBag diagnostics;
        Result<Ref<Object>> loaded = LoadDocument(
            fixture,
            StringView(
                "<Root xmlns=\"urn:directives\" "
                "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
                "x:Key=\"orphan\"/>"),
            diagnostics);
        CHECK(!loaded);
        CHECK(diagnostics.Size() == 1U);
        CHECK(diagnostics.Items()[0].Code() ==
            XamlObjectWriterDiagnosticCodes::MissingResourceScope);
        CHECK(DirectiveNode::LiveCount() == 0U);
    }
    return true;
}

bool TestTypedMarkupExtensionAndLiteralEscape() {
    DirectiveNode::ResetCounters();
    {
        Fixture fixture;
        CHECK(fixture.Build());
        DiagnosticBag diagnostics;
        Result<Ref<Object>> loaded = LoadDocument(
            fixture,
            StringView(
                "<Root xmlns=\"urn:directives\" "
                "xmlns:local=\"urn:directives\">"
                "<Leaf Title=\"{local:Echo hello world}\"/>"
                "<Leaf Title=\"{Echo, default namespace}\"/>"
                "<Leaf Title=\"{}{Echo literal}\"/>"
                "<Leaf>{local:Echo content value}</Leaf>"
                "</Root>"),
            diagnostics);
        CHECK(loaded);
        CHECK(diagnostics.Size() == 0U);

        Ref<Object> rootObject = std::move(loaded).Value();
        DirectiveNode* root = static_cast<DirectiveNode*>(rootObject.Get());
        CHECK(root->Children().Size() == 4U);
        CHECK(static_cast<DirectiveNode*>(root->Children()[0].Get())->Title() ==
            StringView("hello world"));
        CHECK(static_cast<DirectiveNode*>(root->Children()[1].Get())->Title() ==
            StringView("default namespace"));
        CHECK(static_cast<DirectiveNode*>(root->Children()[2].Get())->Title() ==
            StringView("{Echo literal}"));
        CHECK(static_cast<DirectiveNode*>(root->Children()[3].Get())->Title() ==
            StringView("content value"));
        CHECK(DirectiveNode::LiveCount() == 5U);
    }
    CHECK(DirectiveNode::LiveCount() == 0U);
    return true;
}

bool TestMarkupExtensionDiagnosticsAndRollback() {
    DirectiveNode::ResetCounters();
    {
        Fixture fixture;
        CHECK(fixture.Build());
        DiagnosticBag diagnostics;
        Result<Ref<Object>> loaded = LoadDocument(
            fixture,
            StringView(
                "<Root xmlns=\"urn:directives\" "
                "xmlns:local=\"urn:directives\">"
                "<Leaf Title=\"{local:Missing value}\"/>"
                "</Root>"),
            diagnostics);
        CHECK(!loaded);
        CHECK(diagnostics.Size() == 1U);
        CHECK(diagnostics.Items()[0].Code() ==
            XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension);
        CHECK(DirectiveNode::LiveCount() == 0U);
    }

    DirectiveNode::ResetCounters();
    {
        Fixture fixture;
        CHECK(fixture.Build());
        DiagnosticBag diagnostics;
        Result<Ref<Object>> loaded = LoadDocument(
            fixture,
            StringView(
                "<Root xmlns=\"urn:directives\">"
                "<Leaf Title=\"{NoProvider value}\"/>"
                "</Root>"),
            diagnostics);
        CHECK(!loaded);
        CHECK(diagnostics.Size() == 1U);
        CHECK(diagnostics.Items()[0].Code() ==
            XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension);
        CHECK(DirectiveNode::LiveCount() == 0U);
    }

    DirectiveNode::ResetCounters();
    {
        Fixture fixture;
        CHECK(fixture.Build());
        DiagnosticBag diagnostics;
        Result<Ref<Object>> loaded = LoadDocument(
            fixture,
            StringView(
                "<Root xmlns=\"urn:directives\">"
                "<Leaf Title=\"{Echo fail}\"/>"
                "</Root>"),
            diagnostics);
        CHECK(!loaded);
        CHECK(diagnostics.Size() == 1U);
        CHECK(diagnostics.Items()[0].Code() ==
            XamlObjectWriterDiagnosticCodes::MarkupExtensionFailed);
        CHECK(DirectiveNode::AbortCount() >= 2U);
        CHECK(DirectiveNode::LiveCount() == 0U);
    }
    return true;
}

} // namespace

int main() {
    if (!TestNamesResourcesStaticResourceAndServices()) return 1;
    if (!TestNullObjectElement()) return 1;
    if (!TestDuplicateNameRollback()) return 1;
    if (!TestDuplicateKeyRollback()) return 1;
    if (!TestStaticResourceForwardReferenceRejected()) return 1;
    if (!TestInvalidDirectiveAndRootKey()) return 1;
    if (!TestTypedMarkupExtensionAndLiteralEscape()) return 1;
    if (!TestMarkupExtensionDiagnosticsAndRollback()) return 1;
    std::puts("Aero XAML directives and resources tests passed");
    return 0;
}
