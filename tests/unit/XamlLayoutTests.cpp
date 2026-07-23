#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Core/Layout.hpp>
#include <Aero/Core/Presentation.hpp>
#include <Aero/Core/TypeRegistry.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlDependencyProperty.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <cstdio>
#include <utility>

namespace {
using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Markup;

#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    return false; } } while (false)

class TestElement final : public LayoutElement {
public:
    explicit TestElement(TypeId type) noexcept
        : LayoutElement(type) {}
};

struct Fixture final {
    Dispatcher dispatcher;
    TypeRegistry types;
    DependencyPropertyRegistry properties{types};
    PresentationContextScope presentation{dispatcher, properties};
    XamlSchemaContext schema{types};
    XamlActivationProviderRegistry activation{schema};
    XamlDependencyPropertyBridge dependencyProperties{schema, properties};
    TypeId objectType = InvalidTypeId;
    TypeId doubleType = InvalidTypeId;
    TypeId stringType = InvalidTypeId;
    TypeId layoutType = InvalidTypeId;
    TypeId testType = InvalidTypeId;

    static Result<Ref<Object>> Activate(TypeId type,
        const XamlActivationContext& activation,
        void*) noexcept {
        if (activation.dispatcher == nullptr || activation.dependencyProperties == nullptr) {
            return Status::Failure(ErrorCode::InvalidArgument, "Activation services are missing");
        }
        Result<Ref<TestElement>> made = MakeRef<TestElement>(type);
        if (!made) return made.GetStatus();
        Ref<TestElement> typed = std::move(made).Value();
        return Ref<Object>(std::move(typed));
    }

    static LayoutElement* Cast(Object& object, void*) noexcept {
        return &static_cast<TestElement&>(object);
    }

    bool Build() {
        const StringView ns("urn:xaml-layout");
        Result<CorePresentationMetadata> metadata =
            TryRegisterCorePresentationMetadata(types, properties);
        CHECK(metadata);
        objectType = metadata.Value().objectType;
        doubleType = metadata.Value().doubleType;
        stringType = metadata.Value().stringType;
        layoutType = metadata.Value().layoutElementType;
        testType = MakeTypeId(ns, StringView("TestElement"));
        CHECK(types.TryRegisterType({ns, StringView("TestElement"), layoutType,
            TypeFlags::Sealed, nullptr}));
        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        CHECK(activation.TryRegister({testType, &Activate, this}));
        CHECK(TryRegisterCorePresentationXaml(dependencyProperties));
        CHECK(schema.Freeze());
        CHECK(activation.Freeze());
        return true;
    }

    XamlActivationContext Activation() noexcept {
        XamlActivationContext context = XamlActivationContext::Create();
        context.dispatcher = &dispatcher;
        context.dependencyProperties = &properties;
        return context;
    }
};

Result<Ref<Object>> Load(Fixture& fixture, StringView xaml, DiagnosticBag& diagnostics) noexcept {
    Utf8XmlTokenizer tokenizer;
    Result<void> reset = tokenizer.Reset(xaml, &diagnostics);
    if (!reset) return reset.GetStatus();
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(fixture.schema, &diagnostics);
    return LoadXamlWithActivation(writer, reader, fixture.activation, fixture.Activation());
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
