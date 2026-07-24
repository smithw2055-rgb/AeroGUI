#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/EffectiveValueEngine.hpp>
#include <Aero/Core/MetadataDomain.hpp>
#include <Aero/Core/MetadataRuntime.hpp>
#include <Aero/Core/Presentation.hpp>
#include <Aero/Core/Style.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlDependencyProperty.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XamlStyle.hpp>
#include <Aero/Markup/XamlTypeExtension.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <cstdio>
#include <cstring>
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

class RootNode final : public Object {
public:
    explicit RootNode(TypeId type) noexcept : type_(type) {}
    TypeId RuntimeType() const noexcept override { return type_; }

    Result<void> AddChild(Ref<Object> child) noexcept {
        return children_.TryPushBack(std::move(child));
    }
    Span<const Ref<Object>> Children() const noexcept {
        return {children_.Data(), children_.Size()};
    }

private:
    TypeId type_ = InvalidTypeId;
    Vector<Ref<Object>> children_;
};

class ElementNode final : public DependencyObject {
public:
    explicit ElementNode(TypeId type) noexcept
        : DependencyObject(type) {}
};

class BrushNode final : public Object {
public:
    explicit BrushNode(TypeId type) noexcept : type_(type) {}
    TypeId RuntimeType() const noexcept override { return type_; }
private:
    TypeId type_ = InvalidTypeId;
};

Result<Ref<Object>> MakeRoot() noexcept;
Result<Ref<Object>> MakeElement() noexcept;
Result<Ref<Object>> MakeBrush() noexcept;
Result<void> AddChild(Object& object, const XamlValue& value, void*) noexcept;
DependencyObject* AsDependencyObject(Object& object, void*) noexcept;

struct Fixture final {
    Dispatcher dispatcher;
    MetadataDomain metadata;
    std::unique_ptr<MetadataRuntime> runtime;
    std::unique_ptr<EffectiveValueEngine> effectiveValues;
    std::unique_ptr<StyleManager> styles;
    std::unique_ptr<XamlSchemaContext> schema;
    std::unique_ptr<XamlActivationProviderRegistry> activation;
    std::unique_ptr<XamlDependencyPropertyBridge> dpBridge;
    std::unique_ptr<XamlStyleExtension> styleExtension;
    std::unique_ptr<XamlTypeExtension> typeExtension;

    TypeId objectType = InvalidTypeId;
    TypeId stringType = InvalidTypeId;
    TypeId doubleType = InvalidTypeId;
    TypeId typeReferenceType = InvalidTypeId;
    TypeId rootType = InvalidTypeId;
    TypeId elementType = InvalidTypeId;
    TypeId brushType = InvalidTypeId;
    TypeId styleType = InvalidTypeId;
    TypeId setterType = InvalidTypeId;
    TypeId typeExtensionType = InvalidTypeId;
    DependencyPropertyHandle width;
    DependencyPropertyHandle fill;
    DependencyPropertyHandle style;
    MemberId children = InvalidMemberId;
    Ref<Object> defaultBrush;

    static Result<void> RegisterModule(
        MetaRegistrationContext& context,
        void* userContext) noexcept {
        return static_cast<Fixture*>(userContext)->RegisterMetadata(context);
    }

    Result<void> RegisterMetadata(MetaRegistrationContext& context) noexcept {
        MetadataRegistrationTypes types = context.Types();
        DependencyPropertyRegistry& properties = context.DependencyProperties();
        const StringView ns("urn:xaml-style-tests");

        const TypeRegistration registrations[] = {
            TypeRegistration::Object(ns, "Object"),
            TypeRegistration::Primitive(ns, "String"),
            TypeRegistration::Primitive(ns, "Double"),
            TypeRegistration::Primitive(ns, "TypeReference"),
            TypeRegistration::Object(ns, "Root", objectType,
                TypeFlags::None, &MakeRoot),
            TypeRegistration::Object(ns, "Element", objectType,
                TypeFlags::None, &MakeElement),
            TypeRegistration::Object(ns, "Brush", objectType,
                TypeFlags::None, &MakeBrush),
            TypeRegistration::Object(ns, "Style", objectType),
            TypeRegistration::Object(ns, "Setter", objectType),
            TypeRegistration::Object(XamlLanguageNamespaceUri(), "Type",
                objectType,
                TypeFlags::MarkupExtension | TypeFlags::Sealed)
        };
        for (const TypeRegistration& registration : registrations) {
            Result<TypeId> registered = types.TryRegisterType(registration);
            if (!registered) return registered.GetStatus();
        }

        Result<MemberId> member = types.TryRegisterProperty(
            rootType,
            {StringView("Children"), elementType,
             PropertyFlags::Structural | PropertyFlags::Collection});
        if (!member) return member.GetStatus();
        children = member.Value();
        Result<void> status = types.TrySetContentMember(rootType, children);
        if (!status) return status.GetStatus();

        member = types.TryRegisterProperty(
            styleType, {StringView("TargetType"), stringType, PropertyFlags::None});
        if (!member) return member.GetStatus();
        member = types.TryRegisterProperty(
            styleType, {StringView("BasedOn"), styleType, PropertyFlags::None});
        if (!member) return member.GetStatus();
        member = types.TryRegisterProperty(
            styleType,
            {StringView("Setters"), setterType,
             PropertyFlags::Structural | PropertyFlags::Collection});
        if (!member) return member.GetStatus();
        status = types.TrySetContentMember(styleType, member.Value());
        if (!status) return status.GetStatus();
        member = types.TryRegisterProperty(
            setterType, {StringView("Property"), stringType, PropertyFlags::None});
        if (!member) return member.GetStatus();
        member = types.TryRegisterProperty(
            setterType, {StringView("Value"), stringType, PropertyFlags::None});
        if (!member) return member.GetStatus();

        DependencyPropertyRegistration widthRegistration;
        widthRegistration.name = StringView("Width");
        widthRegistration.ownerType = elementType;
        widthRegistration.valueType = doubleType;
        widthRegistration.metadata.defaultValue =
            PropertyValue::FromDouble(doubleType, 1.0);
        Result<DependencyPropertyRegistrationResult> widthResult =
            properties.TryRegister(widthRegistration);
        if (!widthResult) return widthResult.GetStatus();
        width = widthResult.Value().property;

        DependencyPropertyRegistration fillRegistration;
        fillRegistration.name = StringView("Fill");
        fillRegistration.ownerType = elementType;
        fillRegistration.valueType = brushType;
        Result<Ref<Object>> createdDefaultBrush = MakeBrush();
        if (!createdDefaultBrush) return createdDefaultBrush.GetStatus();
        defaultBrush = std::move(createdDefaultBrush).Value();
        fillRegistration.metadata.defaultValue = PropertyValue::FromObject(
            brushType, defaultBrush);
        Result<DependencyPropertyRegistrationResult> fillResult =
            properties.TryRegister(fillRegistration);
        if (!fillResult) return fillResult.GetStatus();
        fill = fillResult.Value().property;

        DependencyPropertyRegistration styleRegistration;
        styleRegistration.name = StringView("Style");
        styleRegistration.ownerType = elementType;
        styleRegistration.valueType = styleType;
        styleRegistration.metadata.defaultValue =
            PropertyValue::NullObject(styleType);
        Result<DependencyPropertyRegistrationResult> styleResult =
            properties.TryRegister(styleRegistration);
        if (!styleResult) return styleResult.GetStatus();
        style = styleResult.Value().property;
        return {};
    }

    bool Build() {
        gFixture = this;
        const StringView ns("urn:xaml-style-tests");
        objectType = MakeTypeId(ns, StringView("Object"));
        stringType = MakeTypeId(ns, StringView("String"));
        doubleType = MakeTypeId(ns, StringView("Double"));
        typeReferenceType = MakeTypeId(ns, StringView("TypeReference"));
        rootType = MakeTypeId(ns, StringView("Root"));
        elementType = MakeTypeId(ns, StringView("Element"));
        brushType = MakeTypeId(ns, StringView("Brush"));
        styleType = MakeTypeId(ns, StringView("Style"));
        setterType = MakeTypeId(ns, StringView("Setter"));
        typeExtensionType = MakeTypeId(
            XamlLanguageNamespaceUri(), StringView("Type"));

        const StringView moduleName("Tests.XamlStyle");
        CHECK(metadata.TryRegisterModule({
            MakeMetadataModuleId(moduleName), moduleName, 1U,
            &Fixture::RegisterModule, this}));
        CHECK(metadata.Seal());

        runtime = std::make_unique<MetadataRuntime>(metadata);
        effectiveValues = std::make_unique<EffectiveValueEngine>(
            dispatcher, metadata.DependencyProperties());
        styles = std::make_unique<StyleManager>(
            *effectiveValues, metadata.DependencyProperties());
        CHECK(effectiveValues->Initialize());
        schema = std::make_unique<XamlSchemaContext>(metadata, *runtime);
        activation = std::make_unique<XamlActivationProviderRegistry>(*schema);
        dpBridge = std::make_unique<XamlDependencyPropertyBridge>(
            *schema, metadata.DependencyProperties());
        styleExtension = std::make_unique<XamlStyleExtension>(
            XamlStyleExtensionOptions{
                styles.get(), &metadata.DependencyProperties(), InvalidTypeId,
                &AsDependencyObject, nullptr});
        typeExtension = std::make_unique<XamlTypeExtension>(InvalidTypeId);

        CHECK(schema->TryRegisterScalarType(stringType, XamlScalarKind::String));
        CHECK(schema->TryRegisterScalarType(doubleType, XamlScalarKind::Double));
        CHECK(schema->TryRegisterTypeAdapter({
            rootType, nullptr, nullptr, nullptr, nullptr, false, true}));
        CHECK(schema->TryRegisterMemberAdapter({
            children, XamlMemberWriteMode::Collection,
            &AddChild, nullptr, nullptr}));
        typeExtension->SetTypeReferenceType(typeReferenceType);
        styleExtension->SetTypeReferenceType(typeReferenceType);
        CHECK(typeExtension->Register(*schema, typeExtensionType));
        CHECK(styleExtension->Register(
            *schema, *activation, styleType, setterType, style));
        CHECK(dpBridge->TryRegisterType({
            elementType, &AsDependencyObject, nullptr}));
        CHECK(dpBridge->TryRegisterProperties());
        CHECK(runtime->Freeze());
        CHECK(schema->Freeze());
        CHECK(activation->Freeze());
        return true;
    }
};

Result<Ref<Object>> MakeRoot() noexcept {
    Result<Ref<RootNode>> created = MakeRef<RootNode>(gFixture->rootType);
    if (!created) {
        return created.GetStatus();
    }
    return Ref<Object>(std::move(created).Value());
}

Result<Ref<Object>> MakeElement() noexcept {
    if (gFixture == nullptr) {
        return Status::Failure(ErrorCode::InvalidState, "XAML Style fixture is absent");
    }
    Result<Ref<ElementNode>> created =
        MakeRef<ElementNode>(gFixture->elementType);
    if (!created) {
        return created.GetStatus();
    }
    return Ref<Object>(std::move(created).Value());
}

Result<Ref<Object>> MakeBrush() noexcept {
    Result<Ref<BrushNode>> created = MakeRef<BrushNode>(gFixture->brushType);
    if (!created) {
        return created.GetStatus();
    }
    return Ref<Object>(std::move(created).Value());
}

Result<void> AddChild(Object& object, const XamlValue& value, void*) noexcept {
    if (value.Kind() != XamlValueKind::Object || !value.AsObject()) {
        return Status::Failure(ErrorCode::InvalidArgument, "Root child is invalid");
    }
    return static_cast<RootNode&>(object).AddChild(value.AsObject());
}

DependencyObject* AsDependencyObject(Object& object, void*) noexcept {
    return static_cast<ElementNode*>(&object);
}

Result<Ref<Object>> Load(Fixture& fixture, const char* xaml) noexcept {
    Utf8XmlTokenizer tokenizer;
    Result<void> reset = tokenizer.Reset(StringView(
        xaml, static_cast<std::uint32_t>(std::strlen(xaml))));
    if (!reset) {
        return reset.GetStatus();
    }
    XamlNodeReader reader(tokenizer);
    XamlObjectWriter writer(*fixture.schema);
    XamlActivationContext context = XamlActivationContext::Create();
    context.dispatcher = &fixture.dispatcher;
    context.dependencyProperties = &fixture.metadata.DependencyProperties();
    return LoadXamlWithActivation(writer, reader, *fixture.activation, context);
}

bool TestXamlStyleResourceBasedOnAndDetach() {
    Fixture fixture;
    CHECK(fixture.Build());
    const char* xaml =
        "<Root xmlns=\"urn:xaml-style-tests\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Brush x:Key=\"accent\"/>"
        "<Style x:Key=\"base\" TargetType=\"Element\">"
        "<Setter Property=\"Width\" Value=\"12.5\"/>"
        "</Style>"
        "<Style x:Key=\"derived\" TargetType=\"{x:Type Element}\" "
        "BasedOn=\"{StaticResource base}\">"
        "<Setter Property=\"Width\" Value=\"42.5\"/>"
        "<Setter Property=\"Fill\" Value=\"{StaticResource accent}\"/>"
        "</Style>"
        "<Element Style=\"{StaticResource derived}\"/>"
        "</Root>";
    Result<Ref<Object>> loaded = Load(fixture, xaml);
    CHECK(loaded);
    RootNode& root = static_cast<RootNode&>(*loaded.Value());
    CHECK(root.Children().Size() == 1U);
    ElementNode& element = static_cast<ElementNode&>(*root.Children()[0]);
    Result<PropertyValue> assignedStyle = element.GetValue(fixture.style);
    CHECK(assignedStyle && !assignedStyle.Value().IsNullObject());
    CHECK(element.GetValue(fixture.width).Value().AsDouble() == 1.0);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::PropertyChanges));
    CHECK(element.GetValue(fixture.width).Value().AsDouble() == 42.5);
    Result<PropertyValue> fill = element.GetValue(fixture.fill);
    CHECK(fill && !fill.Value().IsNullObject());
    CHECK(fill.Value().Type() == fixture.brushType);
    CHECK(fill.Value().AsObject().Get() != fixture.defaultBrush.Get());
    CHECK(fixture.styleExtension->DetachObject(element).Value());
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::PropertyChanges));
    Result<PropertyValue> clearedStyle = element.GetValue(fixture.style);
    CHECK(clearedStyle && clearedStyle.Value().IsNullObject());
    CHECK(element.GetValue(fixture.width).Value().AsDouble() == 1.0);
    CHECK(element.GetValue(fixture.fill).Value().AsObject().Get() ==
        fixture.defaultBrush.Get());
    return true;
}

bool TestXamlStyleRejectsUnknownSetterProperty() {
    Fixture fixture;
    CHECK(fixture.Build());
    Result<Ref<Object>> loaded = Load(fixture,
        "<Root xmlns=\"urn:xaml-style-tests\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Style x:Key=\"bad\" TargetType=\"Element\">"
        "<Setter Property=\"Missing\" Value=\"1\"/>"
        "</Style></Root>");
    CHECK(!loaded);
    return true;
}

bool TestXamlStyleRejectsMissingTargetType() {
    Fixture fixture;
    CHECK(fixture.Build());
    Result<Ref<Object>> loaded = Load(fixture,
        "<Root xmlns=\"urn:xaml-style-tests\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Style x:Key=\"bad\"><Setter Property=\"Width\" Value=\"1\"/>"
        "</Style></Root>");
    CHECK(!loaded);
    return true;
}

bool TestXamlStyleAcceptsNullSetterValue() {
    Fixture fixture;
    CHECK(fixture.Build());
    Result<Ref<Object>> loaded = Load(fixture,
        "<Root xmlns=\"urn:xaml-style-tests\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Style x:Key=\"empty\" TargetType=\"Element\">"
        "<Setter Property=\"Fill\" Value=\"{x:Null}\"/>"
        "</Style><Element Style=\"{StaticResource empty}\"/>"
        "</Root>");
    CHECK(loaded);
    RootNode& root = static_cast<RootNode&>(*loaded.Value());
    CHECK(root.Children().Size() == 1U);
    ElementNode& element = static_cast<ElementNode&>(*root.Children()[0]);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::PropertyChanges));
    CHECK(element.GetValue(fixture.fill).Value().IsNullObject());
    return true;
}

bool TestXamlTypeRejectsUnknownTarget() {
    Fixture fixture;
    CHECK(fixture.Build());
    Result<Ref<Object>> loaded = Load(fixture,
        "<Root xmlns=\"urn:xaml-style-tests\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Style x:Key=\"bad\" TargetType=\"{x:Type Missing}\">"
        "<Setter Property=\"Width\" Value=\"1\"/>"
        "</Style></Root>");
    CHECK(!loaded);
    return true;
}

} // namespace

int main() {
    if (!TestXamlStyleResourceBasedOnAndDetach()) return 1;
    if (!TestXamlStyleRejectsUnknownSetterProperty()) return 1;
    if (!TestXamlStyleRejectsMissingTargetType()) return 1;
    if (!TestXamlStyleAcceptsNullSetterValue()) return 1;
    if (!TestXamlTypeRejectsUnknownTarget()) return 1;
    std::puts("Aero XAML style tests passed");
    return 0;
}
