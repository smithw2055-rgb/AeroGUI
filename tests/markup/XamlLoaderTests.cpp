#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/RuntimeMetadata.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Markup/Runtime/XamlActivation.hpp>
#include <Aero/Markup/Compiled/XamlCompiledDocument.hpp>
#include <Aero/Markup/Runtime/XamlLoader.hpp>
#include <Aero/Markup/Parsing/XamlNodeReader.hpp>
#include <Aero/Markup/Schema/XamlSchemaContext.hpp>
#include <Aero/Markup/Parsing/XmlTokenizer.hpp>

#include <cstdio>
#include <memory>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
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
    XamlSourceProviderRegistry providers;
    EmbeddedXamlSourceProvider embedded;
    DiagnosticBag diagnostics;

    bool Build() {
        CHECK(Aero::Controls::TryRegisterBuiltInUiMetadata(metadata));
        CHECK(metadata.Seal());
        runtime = std::make_unique<MetadataRuntime>(metadata);
        schema = std::make_unique<XamlSchemaContext>(
            metadata, *runtime);
        activation =
            std::make_unique<ActivationProviderRegistry>(
                metadata.Descriptors());
        CHECK(runtime->Freeze());
        CHECK(schema->Freeze());
        CHECK(activation->Freeze());
        return true;
    }

    ObjectActivationContext Activation() noexcept {
        ObjectActivationContext context =
            ObjectActivationContext::Create();
        context.dispatcher = &dispatcher;
        context.dependencyProperties =
            &metadata.DependencyProperties();
        return context;
    }
};

bool TestProviderRoutingPriority() {
    EmbeddedXamlSourceProvider fallback;
    EmbeddedXamlSourceProvider assembly;
    EmbeddedXamlSourceProvider scheme;
    EmbeddedXamlSourceProvider exact;
    XamlSourceProviderRegistry providers;
    CHECK(providers.TryRegister(fallback));
    CHECK(providers.TryRegister(
        assembly, {}, StringView("Aero.Controls")));
    CHECK(providers.TryRegister(
        scheme, StringView("pack")));
    CHECK(providers.TryRegister(
        exact,
        StringView("PACK"),
        StringView("Aero.Controls")));

    Result<ResourceUri> pack = ResourceUri::Parse(
        StringView("pack://application:,,,/Aero.Controls;component/Views/Main.xaml"));
    CHECK(pack);
    Result<IXamlSourceProvider*> resolved =
        providers.Resolve(pack.Value());
    CHECK(resolved && resolved.Value() == &exact);

    Result<ResourceUri> schemeOnly =
        ResourceUri::Parse(StringView("pack://application:,,,/Views/Main.xaml"));
    CHECK(schemeOnly);
    resolved = providers.Resolve(schemeOnly.Value());
    CHECK(resolved && resolved.Value() == &scheme);

    Result<ResourceUri> assemblyOnly = ResourceUri::Parse(
        StringView("Aero.Controls;component/Views/Main.xaml"));
    CHECK(assemblyOnly);
    resolved = providers.Resolve(assemblyOnly.Value());
    CHECK(resolved && resolved.Value() == &assembly);

    Result<ResourceUri> relative =
        ResourceUri::Parse(StringView("Views/Main.xaml"));
    CHECK(relative);
    resolved = providers.Resolve(relative.Value());
    CHECK(resolved && resolved.Value() == &fallback);

    Result<void> duplicate =
        providers.TryRegister(fallback);
    CHECK(!duplicate);
    CHECK(duplicate.GetStatus().code == ErrorCode::AlreadyExists);
    return true;
}

bool TestUriLoadParseAndPolicy() {
    Fixture fixture;
    CHECK(fixture.Build());
    Result<ResourceUri> uri = ResourceUri::Parse(
        StringView("pack://application:,,,/Aero.Controls;component/Views/Main.xaml"));
    CHECK(uri);
    CHECK(fixture.embedded.TryAddText(
        uri.Value(),
        StringView(
            "<Border xmlns=\"urn:aero\" Background=\"#FF336699\"/>"),
        7U));
    CHECK(fixture.embedded.Freeze());
    CHECK(fixture.providers.TryRegister(
        fixture.embedded,
        StringView("pack"),
        StringView("Aero.Controls")));

    ObjectActivationContext activation = fixture.Activation();
    ObjectServicesScope objectServices(
        fixture.dispatcher,
        fixture.metadata.DependencyProperties(),
        *fixture.runtime);
    XamlLoadOptions options;
    options.activationFacets =
        fixture.activation.get();
    options.activation = &activation;
    XamlLoader loader(
        *fixture.schema,
        fixture.providers,
        &fixture.diagnostics);

    Result<XamlLoadResult> loaded =
        loader.Load(uri.Value(), options);
    CHECK(loaded);
    CHECK(loaded.Value().root);
    CHECK(loaded.Value().root->RuntimeType() ==
        BuiltinTypes::Border);
    CHECK(loaded.Value().canonicalUri == uri.Value());
    CHECK(loaded.Value().dependencies.Size() == 1U);
    CHECK(loaded.Value().dependencies[0] == uri.Value());
    CHECK(fixture.diagnostics.Size() == 0U);

    Result<ResourceUri> parseBase = ResourceUri::Parse(
        StringView("file:///C:/ui/Inline.xaml"));
    CHECK(parseBase);
    Result<XamlLoadResult> parsed = loader.Parse(
        StringView("<TextBlock xmlns=\"urn:aero\" Text=\"Inline\"/>"),
        parseBase.Value(),
        options);
    CHECK(parsed);
    CHECK(parsed.Value().root->RuntimeType() ==
        BuiltinTypes::TextBlock);
    CHECK(parsed.Value().canonicalUri == parseBase.Value());

    Result<XamlLoadResult> blocked =
        loader.Load(StringView("https://example.com/App.xaml"), options);
    CHECK(!blocked);
    CHECK(blocked.GetStatus().code == ErrorCode::Unsupported);
    CHECK(fixture.diagnostics.Size() == 1U);
    CHECK(fixture.diagnostics.Items()[0].Code() ==
        XamlLoaderDiagnosticCodes::SourceRejected);
    return true;
}

bool TestLoadComponentAndCompiledEquivalence() {
    Fixture fixture;
    CHECK(fixture.Build());
    Result<ResourceUri> uri = ResourceUri::Parse(
        StringView("pack://application:,,,/Aero.Controls;component/Views/Component.xaml"));
    CHECK(uri);
    const StringView xaml(
        "<Border xmlns=\"urn:aero\" Background=\"#FF010203\"/>");
    CHECK(fixture.embedded.TryAddText(uri.Value(), xaml));
    CHECK(fixture.embedded.Freeze());
    CHECK(fixture.providers.TryRegister(
        fixture.embedded,
        StringView("pack"),
        StringView("Aero.Controls")));

    ObjectActivationContext activation = fixture.Activation();
    XamlLoadOptions options;
    options.activationFacets =
        fixture.activation.get();
    options.activation = &activation;
    XamlLoader loader(
        *fixture.schema,
        fixture.providers,
        &fixture.diagnostics);
    ObjectServicesScope objectServices(
        fixture.dispatcher,
        fixture.metadata.DependencyProperties(),
        *fixture.runtime);

    Result<Ref<Object>> existing =
        fixture.schema->CreateObject(
            BuiltinTypes::Border);
    CHECK(existing);
    Object* expectedRoot = existing.Value().Get();
    Result<XamlLoadResult> component = loader.LoadComponent(
        *expectedRoot, uri.Value(), options);
    if (!component) {
        std::fprintf(
            stderr,
            "LoadComponent failed: %s\n",
            component.GetStatus().message);
    }
    CHECK(component);
    CHECK(component.Value().root.Get() == expectedRoot);

    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(xaml));
    XamlNodeReader reader(tokenizer);
    Result<XamlCompiledDocument> document =
        XamlCompiledDocument::Compile(
            reader, *fixture.schema, uri.Value());
    CHECK(document);
    CHECK(document.Value().OriginUri() == uri.Value());
    CHECK(document.Value().Dependencies().Size() == 1U);
    CHECK(document.Value().Dependencies()[0] == uri.Value());
    Result<Vector<std::uint8_t>> bytes =
        document.Value().Serialize();
    CHECK(bytes);
    Result<XamlCompiledDocument> decoded =
        XamlCompiledDocument::Deserialize(
            bytes.Value().AsSpan(),
            fixture.metadata);
    CHECK(decoded);
    CHECK(decoded.Value().OriginUri() == uri.Value());
    CHECK(decoded.Value().Dependencies().Size() == 1U);
    CHECK(decoded.Value().Dependencies()[0] == uri.Value());
    Result<XamlLoadResult> compiled = loader.LoadCompiled(
        bytes.Value().AsSpan(), uri.Value(), options);
    CHECK(compiled);
    CHECK(compiled.Value().root->RuntimeType() ==
        component.Value().root->RuntimeType());
    CHECK(compiled.Value().canonicalUri ==
        component.Value().canonicalUri);
    CHECK(compiled.Value().dependencies.Size() == 1U);
    CHECK(compiled.Value().dependencies[0] == uri.Value());

    Vector<std::uint8_t> staleBytes = bytes.Value();
    CHECK(staleBytes.Size() > 8U);
    staleBytes[8U] ^= 0x7FU;
    Result<XamlLoadResult> sourceFallback =
        loader.LoadCompiled(
            staleBytes.AsSpan(), uri.Value(), options);
    CHECK(sourceFallback);
    CHECK(sourceFallback.Value().root->RuntimeType() ==
        component.Value().root->RuntimeType());
    Result<XamlLoadResult> noSourceFallback =
        loader.LoadCompiled(
            staleBytes.AsSpan(), ResourceUri{}, options);
    CHECK(!noSourceFallback);
    CHECK(noSourceFallback.GetStatus().code ==
        ErrorCode::Unsupported);

    const StringView templateXaml(
        "<ControlTemplate xmlns=\"urn:aero\" TargetType=\"Button\">"
        "<ControlTemplate.VisualTree><Border/>"
        "</ControlTemplate.VisualTree></ControlTemplate>");
    Utf8XmlTokenizer templateTokenizer;
    CHECK(templateTokenizer.Reset(templateXaml));
    XamlNodeReader templateReader(templateTokenizer);
    Result<XamlCompiledDocument> templateDocument =
        XamlCompiledDocument::Compile(
            templateReader, fixture.metadata, uri.Value());
    CHECK(templateDocument);
    CHECK(!templateDocument.Value().Nodes().Empty());

    Result<Ref<Object>> wrong =
        fixture.schema->CreateObject(
            BuiltinTypes::TextBlock);
    CHECK(wrong);
    Result<XamlLoadResult> mismatch = loader.LoadComponent(
        *wrong.Value(), uri.Value(), options);
    CHECK(!mismatch);
    CHECK(mismatch.GetStatus().code == ErrorCode::InvalidArgument);
    return true;
}

} // namespace

int main() {
    if (!TestProviderRoutingPriority()) return 1;
    if (!TestUriLoadParseAndPolicy()) return 1;
    if (!TestLoadComponentAndCompiledEquivalence()) return 1;
    std::puts("Aero XAML loader tests passed");
    return 0;
}
