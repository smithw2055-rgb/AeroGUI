#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/Metadata/Activation.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Controls/ControlPrimitives.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Markup/Compiled/XamlCompiledDocument.hpp>
#include <Aero/Markup/Extensions/XamlDynamicResource.hpp>
#include <Aero/Markup/Resources/XamlResources.hpp>
#include <Aero/Markup/Schema/XamlSchemaContext.hpp>
#include <Aero/Markup/Resources/XamlPresentationObjectModel.hpp>
#include <Aero/Markup/Runtime/XamlContentWriter.hpp>
#include <Aero/Module.hpp>
#include <Aero/Markup/Parsing/XamlNodeReader.hpp>
#include <Aero/Markup/Parsing/XmlTokenizer.hpp>
#include <Aero/Presentation/Resources.hpp>
#include <Aero/Presentation/Style.hpp>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string_view>
#include <vector>

namespace {

int Fail(const char* message) noexcept {
    std::fprintf(stderr, "aero-xamlc: %s\n", message);
    return 1;
}

int Fail(Aero::Base::Status status) noexcept {
    return Fail(
        status.message != nullptr &&
        status.message[0] != '\0'
            ? status.message
            : "operation failed");
}

bool ReadFile(
    const char* path,
    std::vector<char>& bytes) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    const std::streamoff length = input.tellg();
    if (length < 0 ||
        static_cast<std::uint64_t>(length) >
            UINT32_MAX) {
        return false;
    }
    input.seekg(0, std::ios::beg);
    bytes.resize(static_cast<std::size_t>(length));
    return bytes.empty() ||
        static_cast<bool>(
            input.read(
                bytes.data(),
                static_cast<std::streamsize>(length)));
}

bool WriteFile(
    const char* path,
    Aero::Base::Span<const std::uint8_t> bytes) {
    std::ofstream output(
        path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(
        reinterpret_cast<const char*>(bytes.Data()),
        static_cast<std::streamsize>(bytes.Size()));
    return static_cast<bool>(output);
}



} // namespace

int main(int argc, char** argv) {
    const bool checkOnly =
        argc == 3 &&
        std::string_view(argv[1]) == "--check";
    const bool hasOrigin =
        argc == 5 &&
        std::string_view(argv[1]) == "--origin";
    if (!checkOnly && !hasOrigin && argc != 3) {
        return Fail(
            "usage: aero-xamlc <input.xaml> <output.axir>, "
            "aero-xamlc --origin <uri> <input.xaml> <output.axir>, "
            "or aero-xamlc --check <input.xaml>");
    }
    const char* inputPath =
        checkOnly ? argv[2] :
        hasOrigin ? argv[3] : argv[1];
    const char* outputPath =
        hasOrigin ? argv[4] : argv[2];
    const char* originText =
        hasOrigin ? argv[2] : inputPath;

    std::vector<char> source;
    if (!ReadFile(inputPath, source)) {
        return Fail("cannot read input file");
    }

    Aero::ModuleCatalog modules;
    Aero::Core::MetadataDomain metadata;
    Aero::Base::Result<void> registered =
        modules.RegisterMetadata(metadata);
    if (!registered) return Fail(registered.GetStatus());
    registered = metadata.Seal();
    if (!registered) return Fail(registered.GetStatus());
    Aero::Core::MetadataRuntime runtime(metadata);
    Aero::Markup::XamlSchemaContext schema(
        metadata, runtime);
    Aero::Core::Dispatcher dispatcher;
    Aero::Core::EffectiveValueEngine values(
        dispatcher,
        metadata.DependencyProperties());
    Aero::Presentation::ResourceDictionary resources;
    Aero::Core::ActivationProviderRegistry activation(
        runtime.Descriptors());
    Aero::Markup::XamlResourceExtension resourceXaml;
    Aero::Markup::XamlDynamicResourceExtension dynamicXaml({
        &values,
        &resources});
    Aero::Markup::XamlPresentationObjectModel presentationXaml({
        &runtime,
        &metadata.DependencyProperties()});
    Aero::Markup::XamlContentWriter visualContentXaml;

    registered = resourceXaml.Register(schema);
    if (registered) {
        registered = dynamicXaml.Register(
            schema,
            Aero::Core::MakeTypeId(
                Aero::Base::StringView("urn:aero"),
                Aero::Base::StringView(
                    "DynamicResource")));
    }
    if (registered) {
        registered = presentationXaml.Register(
            schema, activation);
    }
    if (registered) {
        registered = visualContentXaml.Register(schema);
    }
    if (registered) {
        registered = runtime.Freeze();
    }
    if (registered) {
        registered = schema.Freeze();
    }
    if (registered) {
        registered = activation.Freeze();
    }
    if (!registered) return Fail(registered.GetStatus());

    Aero::Markup::Utf8XmlTokenizer tokenizer;
    registered = tokenizer.Reset(Aero::Base::StringView(
        source.data(),
        static_cast<std::uint32_t>(source.size())));
    if (!registered) return Fail(registered.GetStatus());
    Aero::Markup::XamlNodeReader reader(tokenizer);
    Aero::Base::Result<Aero::Base::ResourceUri> origin =
        Aero::Base::ResourceUri::Parse(
            Aero::Base::StringView(
                originText,
                static_cast<std::uint32_t>(
                    std::strlen(originText))));
    if (!origin) return Fail(origin.GetStatus());
    Aero::Base::Result<Aero::Markup::XamlCompiledDocument>
        compiled =
            Aero::Markup::XamlCompiledDocument::Compile(
                reader, schema, origin.Value());
    if (!compiled) return Fail(compiled.GetStatus());
    if (checkOnly) return 0;

    Aero::Base::Result<
        Aero::Base::Vector<std::uint8_t>> encoded =
            compiled.Value().Serialize();
    if (!encoded) return Fail(encoded.GetStatus());
    if (!WriteFile(
            outputPath,
            {encoded.Value().Data(),
             encoded.Value().Size()})) {
        return Fail("cannot write output file");
    }
    return 0;
}
