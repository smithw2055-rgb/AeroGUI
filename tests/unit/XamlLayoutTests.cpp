#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Core/Layout.hpp>
#include <Aero/Core/Rendering.hpp>
#include <Aero/Core/MetadataRuntime.hpp>
#include <Aero/Core/RuntimeMetadata.hpp>
#include <Aero/Core/Presentation.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlDependencyProperty.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <cstdio>
#include <memory>
#include <utility>

namespace {
using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Markup;

#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    return false; } } while (false)

class TestElement final : public FrameworkElement {
public:
    explicit TestElement(TypeId type) noexcept
        : FrameworkElement(type) {}
};

struct Fixture final {
    Dispatcher dispatcher;
    MetadataDomain metadata;
    std::unique_ptr<MetadataRuntime> runtime;
    std::unique_ptr<XamlSchemaContext> schema;
    std::unique_ptr<XamlActivationProviderRegistry> activation;
    std::unique_ptr<XamlDependencyPropertyBridge> dependencyProperties;
    TypeId objectType = InvalidTypeId;
    TypeId doubleType = InvalidTypeId;
    TypeId stringType = InvalidTypeId;
    TypeId layoutType = InvalidTypeId;
    TypeId testType = InvalidTypeId;

    static Result<Ref<Object>> Activate(
        TypeId type,
        const XamlActivationContext& activationContext,
        void*) noexcept {
        if (activationContext.dispatcher == nullptr ||
            activationContext.dependencyProperties == nullptr) {
            return Status::Failure(ErrorCode::InvalidArgument,
                "Activation services are missing");
        }
        Result<Ref<TestElement>> made = MakeRef<TestElement>(type);
        if (!made) return made.GetStatus();
        Ref<TestElement> typed = std::move(made).Value();
        return Ref<Object>(std::move(typed));
    }

    static Result<void> RegisterTestModule(
        MetaRegistrationContext& context,
        void* userContext) noexcept {
        auto* fixture = static_cast<Fixture*>(userContext);
        const StringView ns("urn:xaml-layout");
        Result<TypeId> registered = context.Types().TryRegisterType(TypeRegistration::Object(ns, StringView("TestElement"), BuiltinTypes::FrameworkElement, TypeFlags::Sealed, nullptr));
        if (!registered) return registered.GetStatus();
        return registered.Value() == fixture->testType
            ? Result<void>()
            : Result<void>(Status::Failure(
                ErrorCode::IdCollision, "Unexpected TestElement TypeId"));
    }

    bool Build() {
        const StringView ns("urn:xaml-layout");
        objectType = BuiltinTypes::Object;
        doubleType = BuiltinTypes::Double;
        stringType = BuiltinTypes::String;
        layoutType = BuiltinTypes::FrameworkElement;
        testType = MakeTypeId(ns, StringView("TestElement"));
        CHECK(TryRegisterAeroPresentationMetadata(metadata));
        const StringView moduleName("Tests.XamlLayout");
        CHECK(metadata.TryRegisterModule({
            MakeMetadataModuleId(moduleName), moduleName, 1U,
            &Fixture::RegisterTestModule, this}));
        CHECK(metadata.Seal());
        runtime = std::make_unique<MetadataRuntime>(metadata);
        schema = std::make_unique<XamlSchemaContext>(metadata, *runtime);
        activation = std::make_unique<XamlActivationProviderRegistry>(*schema);
        dependencyProperties = std::make_unique<XamlDependencyPropertyBridge>(
            *schema, metadata.DependencyProperties());
        CHECK(activation->TryRegister({testType, &Activate, this}));
        CHECK(TryRegisterAeroPresentationXaml(*dependencyProperties));
        CHECK(runtime->Freeze());
        CHECK(schema->Freeze());
        CHECK(activation->Freeze());
        return true;
    }

    XamlActivationContext Activation() noexcept {
        XamlActivationContext context = XamlActivationContext::Create();
        context.dispatcher = &dispatcher;
        context.dependencyProperties = &metadata.DependencyProperties();
        return context;
    }
};

Result<Ref<Object>> Load(Fixture& fixture, StringView xaml, DiagnosticBag& diagnostics) noexcept {
    Utf8XmlTokenizer tokenizer;
    Result<void> reset = tokenizer.Reset(xaml, &diagnostics);
    if (!reset) return reset.GetStatus();
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    return LoadXamlWithActivation(writer, reader, *fixture.activation, fixture.Activation());
}

bool TestLayoutAttributes() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Result<Ref<Object>> loaded = Load(fixture, StringView(
        "<TestElement xmlns=\"urn:xaml-layout\" Width=\"40\" Height=\"25\" "
        "MinWidth=\"30\" MaxWidth=\"60\" MinHeight=\"20\" MaxHeight=\"30\" "
        "Margin=\"10, 5, 20, 15\" HorizontalAlignment=\"Center\" "
        "VerticalAlignment=\"Bottom\"/>"), diagnostics);
    CHECK(loaded);
    CHECK(diagnostics.Size() == 0U);
    TestElement* element = static_cast<TestElement*>(loaded.Value().Get());
    CHECK(element != nullptr && element->RuntimeType() == fixture.testType);
    CHECK(element->HasWidth() && element->Width() == 40.0);
    CHECK(element->HasHeight() && element->Height() == 25.0);
    CHECK(element->MinSize().width == 30.0 && element->MinSize().height == 20.0);
    CHECK(element->MaxSize().width == 60.0 && element->MaxSize().height == 30.0);
    CHECK(element->Margin().left == 10.0 && element->Margin().bottom == 15.0);
    CHECK(element->GetHorizontalAlignment() == HorizontalAlignment::Center);
    CHECK(element->GetVerticalAlignment() == VerticalAlignment::Bottom);
    return true;
}

bool TestLayoutInvalidValues() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Result<Ref<Object>> loaded = Load(fixture,
        StringView("<TestElement xmlns=\"urn:xaml-layout\" Margin=\"1,2,3\"/>"), diagnostics);
    CHECK(!loaded);
    CHECK(diagnostics.Size() > 0U);
    return true;
}

} // namespace

int main() {
    if (!TestLayoutAttributes()) return 1;
    if (!TestLayoutInvalidValues()) return 1;
    std::puts("Aero XAML layout tests passed");
    return 0;
}
