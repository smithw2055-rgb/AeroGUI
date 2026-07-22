#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Core/Layout.hpp>
#include <Aero/Core/TypeRegistry.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlLayout.hpp>
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
    TestElement(Dispatcher& dispatcher, DependencyPropertyRegistry& properties,
        TypeId type, IAllocator* allocator) noexcept
        : LayoutElement(dispatcher, properties, type, allocator) {}
};

struct Fixture final {
    Dispatcher dispatcher;
    TypeRegistry types;
    DependencyPropertyRegistry properties{types};
    XamlSchemaContext schema{types};
    XamlActivationProviderRegistry activation{schema};
    TypeId objectType = InvalidTypeId;
    TypeId doubleType = InvalidTypeId;
    TypeId stringType = InvalidTypeId;
    TypeId layoutType = InvalidTypeId;
    TypeId testType = InvalidTypeId;
    XamlLayoutExtension layout{InvalidTypeId};

    static Result<Ref<Object>> Activate(TypeId type,
        const XamlActivationContext& activation,
        IAllocator& allocator, void*) noexcept {
        if (activation.dispatcher == nullptr || activation.dependencyProperties == nullptr) {
            return Status::Failure(ErrorCode::InvalidArgument, "Activation services are missing");
        }
        Result<Ref<TestElement>> made = MakeRefWithAllocator<TestElement>(allocator,
            *activation.dispatcher, *activation.dependencyProperties, type, &allocator);
        if (!made) return made.GetStatus();
        Ref<TestElement> typed = std::move(made).Value();
        return Ref<Object>(std::move(typed));
    }

    static LayoutElement* Cast(Object& object, void*) noexcept {
        return &static_cast<TestElement&>(object);
    }

    bool Build() {
        const StringView ns("urn:xaml-layout");
        objectType = MakeTypeId(ns, StringView("Object"));
        doubleType = MakeTypeId(ns, StringView("Double"));
        stringType = MakeTypeId(ns, StringView("String"));
        layoutType = MakeTypeId(ns, StringView("LayoutElement"));
        testType = MakeTypeId(ns, StringView("TestElement"));
        CHECK(types.TryRegisterType({ns, StringView("Object"), InvalidTypeId, TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("Double"), InvalidTypeId,
            TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("String"), InvalidTypeId,
            TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("LayoutElement"), objectType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("TestElement"), layoutType,
            TypeFlags::Sealed, nullptr}));
        const StringView dimensions[] = {StringView("Width"), StringView("Height"),
            StringView("MinWidth"), StringView("MaxWidth"), StringView("MinHeight"),
            StringView("MaxHeight")};
        for (StringView name : dimensions) {
            CHECK(types.TryRegisterProperty(layoutType, {name, doubleType, PropertyFlags::None}));
        }
        CHECK(types.TryRegisterProperty(layoutType, {StringView("Margin"), stringType, PropertyFlags::None}));
        CHECK(types.TryRegisterProperty(layoutType, {StringView("HorizontalAlignment"), stringType, PropertyFlags::None}));
        CHECK(types.TryRegisterProperty(layoutType, {StringView("VerticalAlignment"), stringType, PropertyFlags::None}));
        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        CHECK(schema.TryRegisterScalarType(doubleType, XamlScalarKind::Double));
        CHECK(schema.TryRegisterScalarType(stringType, XamlScalarKind::String));
        CHECK(activation.TryRegister({testType, &Activate, this}));
        layout.SetLayoutElementType(layoutType);
        CHECK(layout.TryRegisterType({testType, &Cast, nullptr}));
        Result<std::uint32_t> registered = layout.Register(schema);
        CHECK(registered && registered.Value() == 9U);
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
