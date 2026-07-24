#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Core/MetadataDomain.hpp>
#include <Aero/Core/MetadataRuntime.hpp>
#include <Aero/Core/Presentation.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlDependencyProperty.hpp>
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

class TestElement final : public DependencyObject {
public:
    explicit TestElement(TypeId runtimeType) noexcept
        : DependencyObject(runtimeType) {
        ++liveCount_;
    }

    ~TestElement() override {
        --liveCount_;
    }

    static void ResetCounters() noexcept {
        liveCount_ = 0U;
        beginCount_ = 0U;
        endCount_ = 0U;
        abortCount_ = 0U;
    }

    static std::uint32_t LiveCount() noexcept { return liveCount_; }
    static std::uint32_t BeginCount() noexcept { return beginCount_; }
    static std::uint32_t EndCount() noexcept { return endCount_; }
    static std::uint32_t AbortCount() noexcept { return abortCount_; }

    static Result<void> Begin(Object& object, void*) noexcept {
        TestElement& element = static_cast<TestElement&>(object);
        if (element.begun_) {
            return Status::Failure(
                ErrorCode::InvalidState,
                "BeginInit called twice");
        }
        element.begun_ = true;
        ++beginCount_;
        return {};
    }

    static Result<void> End(Object& object, void*) noexcept {
        TestElement& element = static_cast<TestElement&>(object);
        if (!element.begun_ || element.ended_) {
            return Status::Failure(
                ErrorCode::InvalidState,
                "EndInit state is invalid");
        }
        element.ended_ = true;
        ++endCount_;
        return {};
    }

    static void Abort(Object& object, void*) noexcept {
        TestElement& element = static_cast<TestElement&>(object);
        if (!element.aborted_) {
            element.aborted_ = true;
            ++abortCount_;
        }
    }

private:
    bool begun_ = false;
    bool ended_ = false;
    bool aborted_ = false;

    static std::uint32_t liveCount_;
    static std::uint32_t beginCount_;
    static std::uint32_t endCount_;
    static std::uint32_t abortCount_;
};

std::uint32_t TestElement::liveCount_ = 0U;
std::uint32_t TestElement::beginCount_ = 0U;
std::uint32_t TestElement::endCount_ = 0U;
std::uint32_t TestElement::abortCount_ = 0U;

bool ValidateWidth(const PropertyValue& value) noexcept {
    return value.Kind() == PropertyValueKind::Double &&
        value.AsDouble() >= 0.0;
}

Result<PropertyValue> CoerceWidth(
    DependencyObject&,
    const DependencyProperty&,
    const PropertyValue& value) noexcept {
    const double current = value.AsDouble();
    return PropertyValue::FromDouble(
        value.Type(),
        current > 100.0 ? 100.0 : current);
}

struct Fixture final {
    Dispatcher dispatcher;
    MetadataDomain metadata;
    std::unique_ptr<MetadataRuntime> runtime;
    std::unique_ptr<XamlSchemaContext> schema;
    std::unique_ptr<XamlActivationProviderRegistry> activations;
    std::unique_ptr<XamlDependencyPropertyBridge> bridge;

    TypeId objectType = InvalidTypeId;
    TypeId booleanType = InvalidTypeId;
    TypeId integerType = InvalidTypeId;
    TypeId unsignedType = InvalidTypeId;
    TypeId doubleType = InvalidTypeId;
    TypeId elementType = InvalidTypeId;
    TypeId buttonType = InvalidTypeId;
    TypeId gridType = InvalidTypeId;

    DependencyPropertyHandle width;
    DependencyPropertyHandle enabled;
    DependencyPropertyHandle count;
    DependencyPropertyHandle unsignedValue;
    DependencyPropertyHandle dataContext;
    DependencyPropertyHandle child;
    DependencyPropertyHandle isLocked;
    DependencyPropertyHandle row;

    std::uint32_t activationCount = 0U;
    TypeId lastRequestedType = InvalidTypeId;
    bool failActivation = false;
    std::uint32_t applicationMarker = 0xA311U;
    std::uint32_t hostMarker = 0xB411U;

    static Result<Ref<Object>> Activate(
        TypeId requestedType,
        const XamlActivationContext& activation,
        void* context) noexcept {
        auto* fixture = static_cast<Fixture*>(context);
        if (fixture == nullptr ||
            activation.dispatcher != &fixture->dispatcher ||
            activation.dependencyProperties !=
                &fixture->metadata.DependencyProperties() ||
            activation.applicationServices != &fixture->applicationMarker ||
            activation.hostContext != &fixture->hostMarker) {
            return Status::Failure(
                ErrorCode::InvalidArgument,
                "XAML activation services do not match the fixture");
        }
        if (fixture->failActivation) {
            return Status::Failure(
                ErrorCode::InternalError,
                "Fixture activation failure");
        }

        ++fixture->activationCount;
        fixture->lastRequestedType = requestedType;
        Result<Ref<TestElement>> made = MakeRef<TestElement>(requestedType);
        if (!made) return made.GetStatus();
        Ref<TestElement> typed = std::move(made).Value();
        return Ref<Object>(std::move(typed));
    }

    static DependencyObject* CastDependencyObject(
        Object& object,
        void*) noexcept {
        return &static_cast<TestElement&>(object);
    }

    static Result<void> RegisterModule(
        MetaRegistrationContext& context,
        void* userContext) noexcept {
        return static_cast<Fixture*>(userContext)->RegisterMetadata(context);
    }

    Result<void> RegisterMetadata(MetaRegistrationContext& context) noexcept {
        MetadataRegistrationTypes types = context.Types();
        DependencyPropertyRegistry& properties = context.DependencyProperties();
        const StringView ns("urn:dp");
        const TypeRegistration registrations[] = {
            TypeRegistration::Object(ns, "Object"),
            TypeRegistration::Primitive(ns, "Boolean"),
            TypeRegistration::Primitive(ns, "Int64"),
            TypeRegistration::Primitive(ns, "UInt64"),
            TypeRegistration::Primitive(ns, "Double"),
            TypeRegistration::Object(ns, "Element", objectType),
            TypeRegistration::Object(
                ns, "Button", elementType, TypeFlags::Sealed),
            TypeRegistration::Object(ns, "Grid", objectType)
        };
        for (const TypeRegistration& registration : registrations) {
            Result<TypeId> registered = types.TryRegisterType(registration);
            if (!registered) return registered.GetStatus();
        }

        DependencyPropertyRegistration registration;
        registration.name = StringView("Width");
        registration.ownerType = elementType;
        registration.valueType = doubleType;
        registration.metadata.defaultValue =
            PropertyValue::FromDouble(doubleType, 10.0);
        registration.metadata.flags =
            PropertyMetadataFlags::AffectsMeasure |
            PropertyMetadataFlags::AffectsArrange |
            PropertyMetadataFlags::AffectsRender;
        registration.metadata.validate = &ValidateWidth;
        registration.metadata.coerce = &CoerceWidth;
        Result<DependencyPropertyRegistrationResult> result =
            properties.TryRegister(registration);
        if (!result) return result.GetStatus();
        width = result.Value().property;

        registration = {};
        registration.name = StringView("Enabled");
        registration.ownerType = elementType;
        registration.valueType = booleanType;
        registration.metadata.defaultValue =
            PropertyValue::FromBoolean(booleanType, false);
        result = properties.TryRegister(registration);
        if (!result) return result.GetStatus();
        enabled = result.Value().property;

        registration = {};
        registration.name = StringView("Count");
        registration.ownerType = elementType;
        registration.valueType = integerType;
        registration.metadata.defaultValue =
            PropertyValue::FromSignedInteger(integerType, 0);
        result = properties.TryRegister(registration);
        if (!result) return result.GetStatus();
        count = result.Value().property;

        registration = {};
        registration.name = StringView("UnsignedValue");
        registration.ownerType = elementType;
        registration.valueType = unsignedType;
        registration.metadata.defaultValue =
            PropertyValue::FromUnsignedInteger(unsignedType, 0U);
        result = properties.TryRegister(registration);
        if (!result) return result.GetStatus();
        unsignedValue = result.Value().property;

        registration = {};
        registration.name = StringView("DataContext");
        registration.ownerType = elementType;
        registration.valueType = objectType;
        registration.metadata.defaultValue = PropertyValue::NullObject(objectType);
        registration.metadata.flags = PropertyMetadataFlags::Inherits;
        result = properties.TryRegister(registration);
        if (!result) return result.GetStatus();
        dataContext = result.Value().property;

        registration = {};
        registration.name = StringView("Child");
        registration.ownerType = elementType;
        registration.valueType = objectType;
        registration.metadata.defaultValue = PropertyValue::NullObject(objectType);
        result = properties.TryRegister(registration);
        if (!result) return result.GetStatus();
        child = result.Value().property;

        registration = {};
        registration.name = StringView("IsLocked");
        registration.ownerType = elementType;
        registration.valueType = booleanType;
        registration.flags = DependencyPropertyFlags::ReadOnly;
        registration.metadata.defaultValue =
            PropertyValue::FromBoolean(booleanType, false);
        result = properties.TryRegister(registration);
        if (!result) return result.GetStatus();
        isLocked = result.Value().property;

        registration = {};
        registration.name = StringView("Row");
        registration.ownerType = gridType;
        registration.valueType = integerType;
        registration.flags = DependencyPropertyFlags::Attached;
        registration.metadata.defaultValue =
            PropertyValue::FromSignedInteger(integerType, 0);
        result = properties.TryRegister(registration);
        if (!result) return result.GetStatus();
        row = result.Value().property;
        return properties.TryAddOwner(row, buttonType, registration.metadata);
    }

    XamlActivationContext Activation() noexcept {
        XamlActivationContext activation = XamlActivationContext::Create();
        activation.dispatcher = &dispatcher;
        activation.dependencyProperties = &metadata.DependencyProperties();
        activation.applicationServices = &applicationMarker;
        activation.hostContext = &hostMarker;
        return activation;
    }

    bool RegisterScalarTypes() {
        CHECK(schema->TryRegisterScalarType(
            booleanType, XamlScalarKind::Boolean));
        CHECK(schema->TryRegisterScalarType(
            integerType, XamlScalarKind::SignedInteger));
        CHECK(schema->TryRegisterScalarType(
            unsignedType, XamlScalarKind::UnsignedInteger));
        CHECK(schema->TryRegisterScalarType(
            doubleType, XamlScalarKind::Double));
        return true;
    }

    bool Build() {
        const StringView ns("urn:dp");
        objectType = MakeTypeId(ns, StringView("Object"));
        booleanType = MakeTypeId(ns, StringView("Boolean"));
        integerType = MakeTypeId(ns, StringView("Int64"));
        unsignedType = MakeTypeId(ns, StringView("UInt64"));
        doubleType = MakeTypeId(ns, StringView("Double"));
        elementType = MakeTypeId(ns, StringView("Element"));
        buttonType = MakeTypeId(ns, StringView("Button"));
        gridType = MakeTypeId(ns, StringView("Grid"));

        const StringView moduleName("Tests.XamlDependencyProperty");
        CHECK(metadata.TryRegisterModule({
            MakeMetadataModuleId(moduleName), moduleName, 1U,
            &Fixture::RegisterModule, this}));
        CHECK(metadata.Seal());
        runtime = std::make_unique<MetadataRuntime>(metadata);
        schema = std::make_unique<XamlSchemaContext>(metadata, *runtime);
        activations = std::make_unique<XamlActivationProviderRegistry>(*schema);
        bridge = std::make_unique<XamlDependencyPropertyBridge>(
            *schema, metadata.DependencyProperties());

        CHECK(RegisterScalarTypes());
        CHECK(activations->TryRegister({
            elementType, &Fixture::Activate, this}));
        CHECK(bridge->TryRegisterType({
            elementType, &Fixture::CastDependencyObject, nullptr}));
        Result<std::uint32_t> bridged = bridge->TryRegisterProperties();
        CHECK(bridged);
        CHECK(bridged.Value() >= 8U);
        CHECK(bridge->IsTypeRegistered(buttonType));
        CHECK(runtime->Freeze());
        CHECK(schema->FindMemberAdapter(width.value) == nullptr);
        CHECK(schema->FindMemberAdapter(row.value) == nullptr);
        const PropertyInfo* widthInfo = metadata.Types().FindProperty(width.value);
        CHECK(widthInfo != nullptr);
        XamlResolvedMember widthMember;
        widthMember.id = widthInfo->Id();
        widthMember.ownerType = widthInfo->OwnerType();
        widthMember.valueType = widthInfo->ValueType();
        widthMember.propertyFlags = widthInfo->Flags();
        CHECK(schema->ResolveMemberWritePolicy(widthMember).writable);

        XamlTypeAdapterRegistration adapter;
        adapter.type = elementType;
        adapter.beginInit = &TestElement::Begin;
        adapter.endInit = &TestElement::End;
        adapter.abortInit = &TestElement::Abort;
        CHECK(schema->TryRegisterTypeAdapter(adapter));
        CHECK(schema->Freeze());
        CHECK(activations->Freeze());
        return true;
    }
};

Result<Ref<Object>> LoadDocument(
    Fixture& fixture,
    StringView xaml,
    DiagnosticBag& diagnostics,
    const XamlActivationContext& activation) noexcept {
    Utf8XmlTokenizer tokenizer;
    Result<void> reset = tokenizer.Reset(xaml, &diagnostics);
    if (!reset) {
        return reset.GetStatus();
    }
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    return LoadXamlWithActivation(
        writer,
        reader,
        *fixture.activations,
        activation);
}

bool TestActivationAndAutomaticDependencyProperties() {
    TestElement::ResetCounters();
    {
        Fixture fixture;
        CHECK(fixture.Build());
        DiagnosticBag diagnostics;
        XamlActivationContext activation = fixture.Activation();
        Result<Ref<Object>> loaded = LoadDocument(
            fixture,
            StringView(
                "<Button xmlns=\"urn:dp\" "
                "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
                "Width=\"250\" Enabled=\"true\" Count=\"-7\" "
                "UnsignedValue=\"42\" Grid.Row=\"3\" "
                "Child=\"{x:Null}\">"
                "<Button.DataContext>"
                "<Button Width=\"25\"/>"
                "</Button.DataContext>"
                "</Button>"),
            diagnostics,
            activation);
        CHECK(loaded);
        CHECK(diagnostics.Size() == 0U);
        CHECK(fixture.activationCount == 2U);
        CHECK(fixture.lastRequestedType == fixture.buttonType);

        Ref<Object> rootObject = std::move(loaded).Value();
        TestElement* root = static_cast<TestElement*>(rootObject.Get());
        CHECK(root != nullptr);
        CHECK(root->RuntimeType() == fixture.buttonType);

        Result<PropertyValue> width = root->GetValue(fixture.width);
        Result<PropertyValue> localWidth = root->ReadLocalValue(fixture.width);
        CHECK(width && width.Value().AsDouble() == 100.0);
        CHECK(localWidth && localWidth.Value().AsDouble() == 250.0);

        Result<PropertyValue> enabled = root->GetValue(fixture.enabled);
        Result<PropertyValue> count = root->GetValue(fixture.count);
        Result<PropertyValue> unsignedValue = root->GetValue(
            fixture.unsignedValue);
        Result<PropertyValue> row = root->GetValue(fixture.row);
        Result<PropertyValue> dataContext = root->GetValue(fixture.dataContext);
        Result<PropertyValue> childValue = root->GetValue(fixture.child);
        CHECK(enabled && enabled.Value().AsBoolean());
        CHECK(count && count.Value().AsSignedInteger() == -7);
        CHECK(unsignedValue && unsignedValue.Value().AsUnsignedInteger() == 42U);
        CHECK(row && row.Value().AsSignedInteger() == 3);
        CHECK(dataContext && dataContext.Value().AsObject());
        CHECK(childValue && childValue.Value().IsNullObject());

        TestElement* child = static_cast<TestElement*>(
            dataContext.Value().AsObject().Get());
        CHECK(child != nullptr);
        CHECK(child->RuntimeType() == fixture.buttonType);
        Result<PropertyValue> childWidth = child->GetValue(fixture.width);
        CHECK(childWidth && childWidth.Value().AsDouble() == 25.0);

        Result<PropertyInvalidationFlags> invalidations =
            root->TakeInvalidations();
        CHECK(invalidations);
        CHECK(HasFlag(
            invalidations.Value(), PropertyInvalidationFlags::Measure));
        CHECK(HasFlag(
            invalidations.Value(), PropertyInvalidationFlags::Arrange));
        CHECK(HasFlag(
            invalidations.Value(), PropertyInvalidationFlags::Render));
        CHECK(HasFlag(
            invalidations.Value(), PropertyInvalidationFlags::Inheritance));

        CHECK(TestElement::BeginCount() == 2U);
        CHECK(TestElement::EndCount() == 2U);
        CHECK(TestElement::AbortCount() == 0U);
        CHECK(TestElement::LiveCount() == 2U);
    }
    CHECK(TestElement::LiveCount() == 0U);
    return true;
}

bool TestReadOnlyRollbackAfterCompletedChild() {
    TestElement::ResetCounters();
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    XamlActivationContext activation = fixture.Activation();
    Result<Ref<Object>> loaded = LoadDocument(
        fixture,
        StringView(
            "<Button xmlns=\"urn:dp\">\n"
            "  <Button.Child><Button/></Button.Child>\n"
            "  <Button.IsLocked>true</Button.IsLocked>\n"
            "</Button>"),
        diagnostics,
        activation);
    CHECK(!loaded);
    CHECK(loaded.GetStatus().code == ErrorCode::ReadOnly);
    CHECK(diagnostics.Size() == 1U);
    CHECK(diagnostics.Items()[0].Code() ==
        XamlObjectWriterDiagnosticCodes::InvalidValue);
    CHECK(diagnostics.Items()[0].Source().begin.line == 3U);
    CHECK(TestElement::AbortCount() == 2U);
    CHECK(TestElement::LiveCount() == 0U);
    return true;
}

bool TestValidationDiagnosticAndRollback() {
    TestElement::ResetCounters();
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    XamlActivationContext activation = fixture.Activation();
    Result<Ref<Object>> loaded = LoadDocument(
        fixture,
        StringView("<Button xmlns=\"urn:dp\" Width=\"-1\"/>"),
        diagnostics,
        activation);
    CHECK(!loaded);
    CHECK(loaded.GetStatus().code == ErrorCode::ValidationFailed);
    CHECK(diagnostics.Size() == 1U);
    CHECK(diagnostics.Items()[0].Code() ==
        XamlObjectWriterDiagnosticCodes::InvalidValue);
    CHECK(diagnostics.Items()[0].Source().begin.line == 1U);
    CHECK(TestElement::AbortCount() == 1U);
    CHECK(TestElement::LiveCount() == 0U);
    return true;
}

bool TestActivationValidationAndProviderFailure() {
    TestElement::ResetCounters();
    Fixture fixture;
    CHECK(fixture.Build());

    XamlActivationContext invalid = fixture.Activation();
    invalid.structSize = static_cast<std::uint32_t>(
        sizeof(XamlActivationContext) - 1U);
    DiagnosticBag invalidDiagnostics;
    Result<Ref<Object>> invalidLoad = LoadDocument(
        fixture,
        StringView("<Button xmlns=\"urn:dp\"/>"),
        invalidDiagnostics,
        invalid);
    CHECK(!invalidLoad);
    CHECK(invalidLoad.GetStatus().code == ErrorCode::InvalidArgument);
    CHECK(invalidDiagnostics.Size() == 0U);

    fixture.failActivation = true;
    DiagnosticBag failureDiagnostics;
    XamlActivationContext activation = fixture.Activation();
    Result<Ref<Object>> failed = LoadDocument(
        fixture,
        StringView("<Button xmlns=\"urn:dp\"/>"),
        failureDiagnostics,
        activation);
    CHECK(!failed);
    CHECK(failed.GetStatus().code == ErrorCode::InternalError);
    CHECK(failureDiagnostics.Size() == 1U);
    CHECK(failureDiagnostics.Items()[0].Code() ==
        XamlObjectWriterDiagnosticCodes::FactoryFailed);
    CHECK(TestElement::LiveCount() == 0U);
    return true;
}

Result<void> RegisterGuardMetadata(
    MetaRegistrationContext& context,
    void*) noexcept {
    const StringView ns("urn:guard");
    Result<TypeId> registered = context.Types().TryRegisterType(TypeRegistration::Object(ns, StringView("Object"), InvalidTypeId, TypeFlags::None, nullptr));
    return registered ? Result<void>() : Result<void>(registered.GetStatus());
}

bool TestActivationRegistrationGuards() {
    MetadataDomain metadata;
    const StringView moduleName("Tests.XamlActivationGuard");
    CHECK(metadata.TryRegisterModule({
        MakeMetadataModuleId(moduleName), moduleName, 1U,
        &RegisterGuardMetadata, nullptr}));
    CHECK(metadata.Seal());
    MetadataRuntime runtime(metadata);
    XamlSchemaContext schema(metadata, runtime);
    XamlActivationProviderRegistry activations(schema);
    const StringView ns("urn:guard");
    const TypeId objectType = MakeTypeId(ns, StringView("Object"));

    XamlActivationProviderRegistration invalid;
    invalid.type = objectType;
    Result<void> bad = activations.TryRegister(invalid);
    CHECK(!bad && bad.GetStatus().code == ErrorCode::InvalidArgument);

    CHECK(activations.TryRegister({
        objectType, &Fixture::Activate, nullptr}));
    Result<void> duplicate = activations.TryRegister({
        objectType, &Fixture::Activate, nullptr});
    CHECK(!duplicate && duplicate.GetStatus().code == ErrorCode::AlreadyExists);

    Result<void> premature = activations.Freeze();
    CHECK(!premature && premature.GetStatus().code == ErrorCode::InvalidState);
    CHECK(runtime.Freeze());
    CHECK(schema.Freeze());
    CHECK(activations.Freeze());
    Result<void> late = activations.TryRegister({
        objectType, &Fixture::Activate, nullptr});
    CHECK(!late && late.GetStatus().code == ErrorCode::InvalidState);
    return true;
}

} // namespace

int main() {
    if (!TestActivationAndAutomaticDependencyProperties()) return 1;
    if (!TestReadOnlyRollbackAfterCompletedChild()) return 1;
    if (!TestValidationDiagnosticAndRollback()) return 1;
    if (!TestActivationValidationAndProviderFailure()) return 1;
    if (!TestActivationRegistrationGuards()) return 1;
    std::puts("Aero XAML dependency-property tests passed");
    return 0;
}
