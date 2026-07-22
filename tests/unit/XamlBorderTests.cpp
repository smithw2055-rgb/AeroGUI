#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlBorder.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <cstdio>

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

struct Fixture final {
    Dispatcher dispatcher;
    TypeRegistry types;
    DependencyPropertyRegistry properties{types};
    XamlSchemaContext schema{types};
    XamlActivationProviderRegistry activation{schema};
    XamlBorderExtension border;
    TypeId objectType = InvalidTypeId;
    TypeId stringType = InvalidTypeId;
    TypeId borderType = InvalidTypeId;

    static Result<Ref<Object>> Activate(TypeId type,
        const XamlActivationContext& context, IAllocator& allocator, void*) noexcept {
        if (context.dispatcher == nullptr || context.dependencyProperties == nullptr) {
            return Status::Failure(ErrorCode::InvalidArgument, "Activation services are missing");
        }
        Result<Ref<Border>> made = MakeRefWithAllocator<Border>(allocator,
            *context.dispatcher, *context.dependencyProperties, type, &allocator);
        if (!made) return made.GetStatus();
        return Ref<Object>(std::move(made).Value());
    }

    static Border* Cast(Object& object, void*) noexcept {
        return &static_cast<Border&>(object);
    }

    bool Build() {
        const StringView ns("urn:xaml-border");
        objectType = MakeTypeId(ns, StringView("Object"));
        stringType = MakeTypeId(ns, StringView("String"));
        borderType = MakeTypeId(ns, StringView("Border"));
        CHECK(types.TryRegisterType({ns, StringView("Object"), InvalidTypeId,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("String"), InvalidTypeId,
            TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("Border"), objectType,
            TypeFlags::Sealed, nullptr}));
        CHECK(types.TryRegisterProperty(borderType, {
            StringView("Background"), stringType, PropertyFlags::None}));
        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        CHECK(schema.TryRegisterScalarType(stringType, XamlScalarKind::String));
        CHECK(border.TryRegisterType({borderType, &Cast, nullptr}));
        CHECK(border.Register(schema));
        CHECK(activation.TryRegister({borderType, &Activate, nullptr}));
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

Result<Ref<Object>> Load(Fixture& fixture, StringView xaml,
    DiagnosticBag& diagnostics) noexcept {
    Utf8XmlTokenizer tokenizer;
    Result<void> reset = tokenizer.Reset(xaml, &diagnostics);
    if (!reset) return reset.GetStatus();
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(fixture.schema, &diagnostics);
    return LoadXamlWithActivation(writer, reader, fixture.activation, fixture.Activation());
}

bool TestBackgroundAttributes() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Result<Ref<Object>> loaded = Load(fixture, StringView(
        "<Border xmlns=\"urn:xaml-border\" Background=\"#80FF4000\"/>"), diagnostics);
    CHECK(loaded && diagnostics.Size() == 0U);
    Border* border = static_cast<Border*>(loaded.Value().Get());
    CHECK(border != nullptr);
    const Color color = border->Background();
    CHECK(color.red == 1.0F && color.green > 0.24F && color.green < 0.26F &&
        color.blue == 0.0F && color.alpha > 0.50F && color.alpha < 0.51F);
    return true;
}

bool TestInvalidBackground() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Result<Ref<Object>> loaded = Load(fixture, StringView(
        "<Border xmlns=\"urn:xaml-border\" Background=\"blue\"/>"), diagnostics);
    CHECK(!loaded && diagnostics.Size() > 0U);
    return true;
}

} // namespace

int main() {
    if (!TestBackgroundAttributes()) return 1;
    if (!TestInvalidBackground()) return 1;
    std::puts("Aero XAML Border tests passed");
    return 0;
}
