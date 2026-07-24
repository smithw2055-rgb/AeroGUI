#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Presentation/Binding.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Core/Metadata/MetadataDomain.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Presentation/Metadata.hpp>
#include <Aero/Markup/XamlBinding.hpp>
#include <Aero/Markup/XamlCompiledDocument.hpp>
#include <Aero/Markup/XamlDynamicResource.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include "TestMetadataConverters.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Presentation;
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

class BindableNode final : public DependencyObject {
public:
    explicit BindableNode(TypeId type) noexcept
        : DependencyObject(type), children_() {}

    Result<void> AddChild(Ref<Object> child) noexcept {
        return children_.TryPushBack(std::move(child));
    }
    Span<const Ref<Object>> Children() const noexcept {
        return {children_.Data(), children_.Size()};
    }

private:
    Vector<Ref<Object>> children_;
};

Result<Ref<Object>> MakeRoot() noexcept;
Result<Ref<Object>> MakeElement() noexcept;
Result<void> SetNumber(
    Object& object,
    const XamlValue& value,
    const XamlServiceProvider& services,
    void*) noexcept;
Result<void> SetObject(
    Object& object,
    const XamlValue& value,
    void*) noexcept;
Result<void> SetResource(
    Object& object,
    const XamlValue& value,
    const XamlServiceProvider& services,
    void*) noexcept;
Result<void> AddChild(
    Object& object,
    const XamlValue& value,
    void*) noexcept;
DependencyObject* AsDependencyObject(Object& object, void*) noexcept;

struct Fixture final {
    Dispatcher dispatcher;
    MetadataDomain metadata;
    ResourceDictionary resources;
    BindingManager bindings{dispatcher};
    std::unique_ptr<MetadataRuntime> runtime;
    std::unique_ptr<EffectiveValueEngine> effectiveValues;
    std::unique_ptr<XamlSchemaContext> schema;
    std::unique_ptr<ObjectServicesScope> presentation;
    std::unique_ptr<XamlBindingExtension> extension;
    std::unique_ptr<XamlDynamicResourceExtension> dynamicResource;
    TypeId objectType = InvalidTypeId;
    TypeId doubleType = InvalidTypeId;
    TypeId rootType = InvalidTypeId;
    TypeId elementType = InvalidTypeId;
    TypeId brushType = InvalidTypeId;
    TypeId bindingExtensionType = InvalidTypeId;
    TypeId dynamicResourceExtensionType = InvalidTypeId;
    DependencyPropertyHandle source;
    DependencyPropertyHandle target;
    DependencyPropertyHandle dataContext;
    DependencyPropertyHandle resource;

    static Result<void> RegisterModule(
        MetaRegistrationContext& context,
        void* userContext) noexcept {
        return static_cast<Fixture*>(userContext)->RegisterMetadata(context);
    }

    Result<void> RegisterMetadata(MetaRegistrationContext& context) noexcept {
        MetadataRegistrationTypes types = context.Types();
        DependencyPropertyRegistry& properties = context.DependencyProperties();
        const StringView ns("urn:xaml-binding-tests");

        Result<TypeId> registered = types.TryRegisterType(TypeRegistration::Object(ns, StringView("Object"), InvalidTypeId, TypeFlags::None, nullptr));
        if (!registered) return registered.GetStatus();
        registered = types.TryRegisterType(TypeRegistration::Primitive(ns, StringView("Double"), TypeFlags::ValueType | TypeFlags::Sealed));
        if (!registered) return registered.GetStatus();
        Result<void> converter = Aero::Tests::RegisterTestTextConverter(
            context, doubleType, &Aero::Tests::ConvertTestDouble);
        if (!converter) return converter.GetStatus();
        registered = types.TryRegisterType(TypeRegistration::Object(ns, StringView("Root"), objectType, TypeFlags::None, &MakeRoot));
        if (!registered) return registered.GetStatus();
        registered = types.TryRegisterType(TypeRegistration::Object(ns, StringView("Element"), objectType, TypeFlags::None, &MakeElement));
        if (!registered) return registered.GetStatus();
        registered = types.TryRegisterType(TypeRegistration::Object(ns, StringView("Brush"), objectType, TypeFlags::None, nullptr));
        if (!registered) return registered.GetStatus();
        registered = types.TryRegisterType(TypeRegistration::Object(ns, StringView("Binding"), objectType, TypeFlags::MarkupExtension | TypeFlags::Sealed, nullptr));
        if (!registered) return registered.GetStatus();
        registered = types.TryRegisterType(TypeRegistration::Object(ns, StringView("DynamicResource"), objectType, TypeFlags::MarkupExtension | TypeFlags::Sealed, nullptr));
        if (!registered) return registered.GetStatus();

        DependencyPropertyRegistration sourceRegistration;
        sourceRegistration.name = StringView("Source");
        sourceRegistration.ownerType = elementType;
        sourceRegistration.valueType = doubleType;
        sourceRegistration.metadata.defaultValue =
            PropertyValue::FromDouble(doubleType, 0.0);
        Result<DependencyPropertyRegistrationResult> sourceResult =
            properties.TryRegister(sourceRegistration);
        if (!sourceResult) return sourceResult.GetStatus();
        source = sourceResult.Value().property;

        DependencyPropertyRegistration targetRegistration;
        targetRegistration.name = StringView("Target");
        targetRegistration.ownerType = elementType;
        targetRegistration.valueType = doubleType;
        targetRegistration.metadata.defaultValue =
            PropertyValue::FromDouble(doubleType, -1.0);
        Result<DependencyPropertyRegistrationResult> targetResult =
            properties.TryRegister(targetRegistration);
        if (!targetResult) return targetResult.GetStatus();
        target = targetResult.Value().property;

        DependencyPropertyRegistration dataContextRegistration;
        dataContextRegistration.name = StringView("DataContext");
        dataContextRegistration.ownerType = elementType;
        dataContextRegistration.valueType = objectType;
        dataContextRegistration.metadata.defaultValue =
            PropertyValue::NullObject(objectType);
        Result<DependencyPropertyRegistrationResult> dataContextResult =
            properties.TryRegister(dataContextRegistration);
        if (!dataContextResult) return dataContextResult.GetStatus();
        dataContext = dataContextResult.Value().property;

        DependencyPropertyRegistration resourceRegistration;
        resourceRegistration.name = StringView("Resource");
        resourceRegistration.ownerType = elementType;
        resourceRegistration.valueType = objectType;
        resourceRegistration.metadata.defaultValue =
            PropertyValue::NullObject(objectType);
        Result<DependencyPropertyRegistrationResult> resourceResult =
            properties.TryRegister(resourceRegistration);
        if (!resourceResult) return resourceResult.GetStatus();
        resource = resourceResult.Value().property;

        Result<MemberId> children = types.TryRegisterProperty(
            rootType,
            {StringView("Children"), elementType,
             PropertyFlags::Structural | PropertyFlags::Collection});
        if (!children) return children.GetStatus();
        return types.TrySetContentMember(rootType, children.Value());
    }

    bool Build() {
        gFixture = this;
        const StringView ns("urn:xaml-binding-tests");
        objectType = MakeTypeId(ns, StringView("Object"));
        doubleType = MakeTypeId(ns, StringView("Double"));
        rootType = MakeTypeId(ns, StringView("Root"));
        elementType = MakeTypeId(ns, StringView("Element"));
        brushType = MakeTypeId(ns, StringView("Brush"));
        bindingExtensionType = MakeTypeId(ns, StringView("Binding"));
        dynamicResourceExtensionType = MakeTypeId(ns, StringView("DynamicResource"));

        const StringView moduleName("Tests.XamlBinding");
        CHECK(metadata.TryRegisterModule({
            MakeMetadataModuleId(moduleName), moduleName, 1U,
            &Fixture::RegisterModule, this}));
        CHECK(metadata.Seal());

        runtime = std::make_unique<MetadataRuntime>(metadata);
        effectiveValues = std::make_unique<EffectiveValueEngine>(
            dispatcher, metadata.DependencyProperties());
        CHECK(effectiveValues->Initialize());
        CHECK(bindings.Initialize());
        schema = std::make_unique<XamlSchemaContext>(metadata, *runtime);

        CHECK(schema->TryRegisterMemberAdapter({
            source.value, XamlMemberWriteMode::SetOnce, nullptr, nullptr, &SetNumber}));
        CHECK(schema->TryRegisterMemberAdapter({
            target.value, XamlMemberWriteMode::SetOnce, nullptr, nullptr, &SetNumber}));
        CHECK(schema->TryRegisterMemberAdapter({
            dataContext.value, XamlMemberWriteMode::SetOnce, &SetObject, nullptr}));
        CHECK(schema->TryRegisterMemberAdapter({
            resource.value, XamlMemberWriteMode::SetOnce, nullptr, nullptr, &SetResource}));
        const MemberId children = MakeMemberId(
            rootType, MemberKind::Property, StringView("Children"));
        CHECK(schema->TryRegisterMemberAdapter({
            children, XamlMemberWriteMode::Collection, &AddChild, nullptr}));
        CHECK(schema->TryRegisterTypeAdapter({
            rootType, nullptr, nullptr, nullptr, nullptr, true}));

        extension = std::make_unique<XamlBindingExtension>(
            XamlBindingExtensionOptions{
                &bindings, &AsDependencyObject, nullptr, {}});
        extension->SetDataContextProperty(dataContext);
        CHECK(extension->Register(*schema, bindingExtensionType));
        dynamicResource = std::make_unique<XamlDynamicResourceExtension>(
            XamlDynamicResourceExtensionOptions{
                effectiveValues.get(), &resources, &AsDependencyObject, nullptr});
        CHECK(dynamicResource->Register(*schema, dynamicResourceExtensionType));
        CHECK(TryRegisterDependencyPropertyRuntimeProvider(
            *runtime, metadata.DependencyProperties(), objectType));
        CHECK(runtime->Freeze());
        CHECK(schema->Freeze());
        presentation = std::make_unique<ObjectServicesScope>(
            dispatcher, metadata.DependencyProperties(), *runtime);
        return true;
    }
};

Result<Ref<Object>> MakeRoot() noexcept {
    if (gFixture == nullptr) {
        return Status::Failure(ErrorCode::InvalidState, "Binding fixture is absent");
    }
    Result<Ref<BindableNode>> created =
        MakeRef<BindableNode>(gFixture->rootType);
    if (!created) {
        return created.GetStatus();
    }
    Ref<BindableNode> typed = std::move(created).Value();
    return Ref<Object>(std::move(typed));
}

Result<Ref<Object>> MakeElement() noexcept {
    if (gFixture == nullptr) {
        return Status::Failure(ErrorCode::InvalidState, "Binding fixture is absent");
    }
    Result<Ref<BindableNode>> created =
        MakeRef<BindableNode>(gFixture->elementType);
    if (!created) {
        return created.GetStatus();
    }
    Ref<BindableNode> typed = std::move(created).Value();
    return Ref<Object>(std::move(typed));
}

Result<void> SetNumber(
    Object& object,
    const XamlValue& value,
    const XamlServiceProvider& services,
    void*) noexcept {
    if (value.Kind() != XamlValueKind::Double ||
        services.targetMember == InvalidMemberId) {
        return Status::Failure(ErrorCode::InvalidArgument, "Expected a double DP value");
    }
    BindableNode& node = static_cast<BindableNode&>(object);
    return node.SetValue(
        {services.targetMember},
        PropertyValue::FromDouble(value.Type(), value.AsDouble()));
}

Result<void> SetObject(
    Object& object,
    const XamlValue& value,
    void*) noexcept {
    if (value.Kind() != XamlValueKind::Object) {
        return Status::Failure(ErrorCode::InvalidArgument, "Expected object DP value");
    }
    return static_cast<BindableNode&>(object).SetValue(
        {gFixture->dataContext.value},
        value.AsObject()
            ? PropertyValue::FromObject(value.Type(), value.AsObject())
            : PropertyValue::NullObject(value.Type()));
}

Result<void> SetResource(
    Object& object,
    const XamlValue& value,
    const XamlServiceProvider& services,
    void*) noexcept {
    if (value.Kind() != XamlValueKind::Object ||
        services.targetMember == InvalidMemberId) {
        return Status::Failure(ErrorCode::InvalidArgument, "Expected a resource object DP value");
    }
    return static_cast<BindableNode&>(object).SetValue(
        {services.targetMember},
        value.AsObject()
            ? PropertyValue::FromObject(value.Type(), value.AsObject())
            : PropertyValue::NullObject(value.Type()));
}

Result<void> AddChild(
    Object& object,
    const XamlValue& value,
    void*) noexcept {
    if (value.Kind() != XamlValueKind::Object || !value.AsObject()) {
        return Status::Failure(ErrorCode::InvalidArgument, "Expected child object");
    }
    return static_cast<BindableNode&>(object).AddChild(value.AsObject());
}

DependencyObject* AsDependencyObject(Object& object, void*) noexcept {
    return static_cast<BindableNode*>(&object);
}

bool TestElementNameOneWayBinding() {
    Fixture fixture;
    CHECK(fixture.Build());
    const char* xaml =
        "<Root xmlns=\"urn:xaml-binding-tests\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Element x:Name=\"source\" Source=\"12.5\"/>"
        "<Element Target=\"{Binding ElementName=source, Path=Source, Mode=TwoWay}\"/>"
        "</Root>";
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        xaml,
        static_cast<std::uint32_t>(std::strlen(xaml)))));
    XamlNodeReader reader(tokenizer);
    XamlObjectWriter writer(*fixture.schema);
    Result<Ref<Object>> loaded = writer.Load(reader);
    CHECK(loaded);

    BindableNode& root = static_cast<BindableNode&>(*loaded.Value());
    CHECK(root.Children().Size() == 2U);
    BindableNode& source = static_cast<BindableNode&>(*root.Children()[0]);
    BindableNode& target = static_cast<BindableNode&>(*root.Children()[1]);
    CHECK(target.GetValue(fixture.target).Value().AsDouble() == -1.0);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::DataBind));
    CHECK(target.GetValue(fixture.target).Value().AsDouble() == 12.5);
    CHECK(source.SetValue(
        fixture.source,
        PropertyValue::FromDouble(fixture.doubleType, 28.0)));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::DataBind));
    CHECK(target.GetValue(fixture.target).Value().AsDouble() == 28.0);
    CHECK(target.SetValue(
        fixture.target,
        PropertyValue::FromDouble(fixture.doubleType, 33.0)));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::DataBind));
    CHECK(source.GetValue(fixture.source).Value().AsDouble() == 33.0);
    fixture.bindings.Shutdown();
    return true;
}

bool TestBindingArgumentsAreValidated() {
    Fixture fixture;
    CHECK(fixture.Build());
    const char* xaml =
        "<Root xmlns=\"urn:xaml-binding-tests\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Element x:Name=\"source\" Source=\"12.5\"/>"
        "<Element Target=\"{Binding ElementName=source}\"/>"
        "</Root>";
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        xaml,
        static_cast<std::uint32_t>(std::strlen(xaml)))));
    XamlNodeReader reader(tokenizer);
    XamlObjectWriter writer(*fixture.schema);
    Result<Ref<Object>> loaded = writer.Load(reader);
    CHECK(!loaded);
    CHECK(loaded.GetStatus().code == ErrorCode::ValidationFailed);
    return true;
}

bool TestDataContextBinding() {
    Fixture fixture;
    CHECK(fixture.Build());
    const char* xaml =
        "<Root xmlns=\"urn:xaml-binding-tests\">"
        "<Element>"
        "<Element.DataContext><Element Source=\"15.0\"/></Element.DataContext>"
        "<Element.Target>{Binding Path=Source, Mode=OneTime}</Element.Target>"
        "</Element>"
        "</Root>";
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        xaml,
        static_cast<std::uint32_t>(std::strlen(xaml)))));
    XamlNodeReader reader(tokenizer);
    XamlObjectWriter writer(*fixture.schema);
    Result<Ref<Object>> loaded = writer.Load(reader);
    CHECK(loaded);

    BindableNode& root = static_cast<BindableNode&>(*loaded.Value());
    CHECK(root.Children().Size() == 1U);
    BindableNode& target = static_cast<BindableNode&>(*root.Children()[0]);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::DataBind));
    CHECK(target.GetValue(fixture.target).Value().AsDouble() == 15.0);
    Ref<Object> dataContext = target.GetValue(fixture.dataContext).Value().AsObject();
    CHECK(dataContext);
    BindableNode& source = static_cast<BindableNode&>(*dataContext);
    CHECK(source.SetValue(
        fixture.source,
        PropertyValue::FromDouble(fixture.doubleType, 25.0)));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::DataBind));
    CHECK(target.GetValue(fixture.target).Value().AsDouble() == 15.0);
    fixture.bindings.Shutdown();
    return true;
}

bool TestDataContextBindingReResolvesAndWritesBack() {
    Fixture fixture;
    CHECK(fixture.Build());
    const char* xaml =
        "<Root xmlns=\"urn:xaml-binding-tests\">"
        "<Element>"
        "<Element.DataContext><Element Source=\"10.0\"/></Element.DataContext>"
        "<Element.Target>{Binding Path=Source, Mode=TwoWay}</Element.Target>"
        "</Element>"
        "</Root>";
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        xaml,
        static_cast<std::uint32_t>(std::strlen(xaml)))));
    XamlNodeReader reader(tokenizer);
    XamlObjectWriter writer(*fixture.schema);
    Result<Ref<Object>> loaded = writer.Load(reader);
    CHECK(loaded);

    BindableNode& root = static_cast<BindableNode&>(*loaded.Value());
    BindableNode& target = static_cast<BindableNode&>(*root.Children()[0]);
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::DataBind));
    CHECK(target.GetValue(fixture.target).Value().AsDouble() == 10.0);

    Result<Ref<BindableNode>> replacement =
        MakeRef<BindableNode>(fixture.elementType);
    CHECK(replacement);
    Ref<BindableNode> replacementTyped =
        std::move(replacement).Value();
    CHECK(replacementTyped->SetValue(
        fixture.source,
        PropertyValue::FromDouble(fixture.doubleType, 40.0)));
    Ref<Object> replacementObject(replacementTyped);
    CHECK(target.SetValue(
        fixture.dataContext,
        PropertyValue::FromObject(
            fixture.objectType, replacementObject)));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::DataBind));
    CHECK(target.GetValue(fixture.target).Value().AsDouble() == 40.0);

    CHECK(target.SetValue(
        fixture.target,
        PropertyValue::FromDouble(fixture.doubleType, 55.0)));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::DataBind));
    CHECK(replacementTyped->GetValue(
        fixture.source).Value().AsDouble() == 55.0);
    fixture.bindings.Shutdown();
    return true;
}

bool TestCompiledDocumentReplaysWithoutXmlTokenization() {
    Fixture fixture;
    CHECK(fixture.Build());
    const char* xaml =
        "<Root xmlns=\"urn:xaml-binding-tests\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Element x:Name=\"source\" Source=\"18.0\"/>"
        "<Element Target=\"{Binding ElementName=source, Path=Source}\"/>"
        "</Root>";
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        xaml,
        static_cast<std::uint32_t>(std::strlen(xaml)))));
    XamlNodeReader reader(tokenizer);
    Result<XamlCompiledDocument> compiled =
        XamlCompiledDocument::Compile(
            reader, *fixture.schema);
    CHECK(compiled && compiled.Value().IsValid());
    CHECK(compiled.Value().Nodes().Size() > 1U);
    Result<Vector<std::uint8_t>> serialized =
        compiled.Value().Serialize();
    CHECK(serialized && !serialized.Value().Empty());
    Result<XamlCompiledDocument> decoded =
        XamlCompiledDocument::Deserialize(
            {serialized.Value().Data(),
             serialized.Value().Size()},
            fixture.metadata);
    CHECK(decoded && decoded.Value().IsValid());
    Result<XamlCompiledDocument> truncated =
        XamlCompiledDocument::Deserialize(
            {serialized.Value().Data(),
             serialized.Value().Size() - 1U},
            fixture.metadata);
    CHECK(!truncated);

    XamlObjectWriter writer(*fixture.schema);
    Result<Ref<Object>> loaded =
        writer.Load(decoded.Value());
    CHECK(loaded);
    BindableNode& root =
        static_cast<BindableNode&>(*loaded.Value());
    CHECK(root.Children().Size() == 2U);
    BindableNode& target =
        static_cast<BindableNode&>(*root.Children()[1]);
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::DataBind));
    CHECK(target.GetValue(
        fixture.target).Value().AsDouble() == 18.0);
    fixture.bindings.Shutdown();
    return true;
}

bool TestXamlDynamicResourceReevaluatesAfterDictionaryReplacement() {
    Fixture fixture;
    CHECK(fixture.Build());
    Result<Ref<BindableNode>> first =
        MakeRef<BindableNode>(fixture.brushType);
    CHECK(first);
    Result<Ref<BindableNode>> second =
        MakeRef<BindableNode>(fixture.brushType);
    CHECK(second);
    Ref<Object> firstObject(std::move(first).Value());
    Ref<Object> secondObject(std::move(second).Value());
    CHECK(fixture.resources.TryAdd(StringView("Accent"), fixture.brushType, firstObject));

    const char* xaml =
        "<Root xmlns=\"urn:xaml-binding-tests\">"
        "<Element Resource=\"{DynamicResource Accent}\"/>"
        "</Root>";
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        xaml, static_cast<std::uint32_t>(std::strlen(xaml)))));
    XamlNodeReader reader(tokenizer);
    XamlObjectWriter writer(*fixture.schema);
    Result<Ref<Object>> loaded = writer.Load(reader);
    CHECK(loaded);
    BindableNode& root = static_cast<BindableNode&>(*loaded.Value());
    CHECK(root.Children().Size() == 1U);
    BindableNode& target = static_cast<BindableNode&>(*root.Children()[0]);

    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::PropertyChanges));
    CHECK(target.GetValue(fixture.resource).Value().AsObject().Get() == firstObject.Get());
    Result<EffectiveValueDiagnostics> initial =
        fixture.effectiveValues->Diagnostics(target, fixture.resource);
    CHECK(initial);
    CHECK(initial.Value().provider == EffectiveValueProvider::LocalExpression);
    CHECK(initial.Value().expressionKind == PropertyExpressionKind::DynamicResource);

    CHECK(fixture.resources.TrySet(StringView("Accent"), fixture.brushType, secondObject));
    CHECK(target.GetValue(fixture.resource).Value().AsObject().Get() == firstObject.Get());
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::PropertyChanges));
    CHECK(target.GetValue(fixture.resource).Value().AsObject().Get() == secondObject.Get());

    CHECK(fixture.effectiveValues->ClearLocalExpression(target, fixture.resource));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::PropertyChanges));
    CHECK(fixture.resources.TrySet(StringView("Accent"), fixture.brushType, firstObject));
    CHECK(fixture.effectiveValues->PendingPropertyCount() == 0U);
    fixture.bindings.Shutdown();
    return true;
}

} // namespace

int main() {
    if (!TestElementNameOneWayBinding()) return 1;
    if (!TestBindingArgumentsAreValidated()) return 1;
    if (!TestDataContextBinding()) return 1;
    if (!TestDataContextBindingReResolvesAndWritesBack()) return 1;
    if (!TestCompiledDocumentReplaysWithoutXmlTokenization()) return 1;
    if (!TestXamlDynamicResourceReevaluatesAfterDictionaryReplacement()) return 1;
    std::puts("Aero XAML binding tests passed");
    return 0;
}
