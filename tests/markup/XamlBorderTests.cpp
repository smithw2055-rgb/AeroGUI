#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Markup/Runtime/XamlActivation.hpp>
#include <Aero/Markup/Parsing/XamlNodeReader.hpp>
#include <Aero/Markup/Runtime/XamlObjectWriter.hpp>
#include <Aero/Markup/Schema/XamlSchemaContext.hpp>
#include <Aero/Markup/Parsing/XmlTokenizer.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Controls/RuntimeMetadata.hpp>
#include <Aero/Presentation/Metadata.hpp>
#include <Aero/Controls/Controls.hpp>

#include <cstdio>
#include <memory>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Presentation;
using namespace Aero::Controls;
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
    MetadataDomain metadata;
    std::unique_ptr<MetadataRuntime> runtime;
    std::unique_ptr<XamlSchemaContext> schema;
    std::unique_ptr<ActivationProviderRegistry> activation;
    TypeId objectType = InvalidTypeId;
    TypeId stringType = InvalidTypeId;
    TypeId borderType = InvalidTypeId;

    bool Build() {
        CHECK(Aero::Controls::TryRegisterBuiltInUiMetadata(metadata));
        CHECK(metadata.Seal());
        objectType = BuiltinTypes::Object;
        stringType = BuiltinTypes::String;
        borderType = BuiltinTypes::Border;
        runtime = std::make_unique<MetadataRuntime>(metadata);
        schema = std::make_unique<XamlSchemaContext>(metadata, *runtime);
        activation = std::make_unique<ActivationProviderRegistry>(
            metadata.Descriptors());
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

Result<Ref<Object>> Load(Fixture& fixture, StringView xaml,
    DiagnosticBag& diagnostics) noexcept {
    Utf8XmlTokenizer tokenizer;
    Result<void> reset = tokenizer.Reset(xaml, &diagnostics);
    if (!reset) return reset.GetStatus();
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    Result<XamlLoadResult> loaded = LoadXamlWithActivation(
        writer, reader, *fixture.activation,
        fixture.Activation());
    if (!loaded) return loaded.GetStatus();
    return loaded.Value().root;
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
