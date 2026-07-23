#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlDependencyProperty.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>
#include <Aero/Core/Presentation.hpp>
#include <Aero/Core/Controls.hpp>

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
    XamlDependencyPropertyBridge dependencyProperties{schema, properties};
    TypeId objectType = InvalidTypeId;
    TypeId stringType = InvalidTypeId;
    TypeId borderType = InvalidTypeId;

    static Result<Ref<Object>> Activate(TypeId type,
        const XamlActivationContext& context, void*) noexcept {
        if (context.dispatcher == nullptr || context.dependencyProperties == nullptr) {
            return Status::Failure(ErrorCode::InvalidArgument, "Activation services are missing");
        }
        if (type != Border::StaticTypeId()) {
            return Status::Failure(ErrorCode::InvalidArgument,
                "Activation type is not Border");
        }
        Result<Ref<Border>> made = MakeRef<Border>();
        if (!made) return made.GetStatus();
        return Ref<Object>(std::move(made).Value());
    }

    bool Build() {
        Result<CorePresentationMetadata> metadata =
            TryRegisterCorePresentationMetadata(types, properties);
        CHECK(metadata);
        objectType = metadata.Value().objectType;
        stringType = metadata.Value().stringType;
        borderType = metadata.Value().borderType;
        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        CHECK(activation.TryRegister({borderType, &Activate, nullptr}));
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
        "<Border xmlns=\"urn:aero\" Background=\"#80FF4000\"/>"), diagnostics);
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
        "<Border xmlns=\"urn:aero\" Background=\"blue\"/>"), diagnostics);
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
