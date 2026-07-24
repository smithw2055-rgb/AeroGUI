#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Markup/XamlCompiledDocument.hpp>
#include <Aero/Markup/XamlModuleSdk.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <cstdio>
#include <cstdint>
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
    if ((!checkOnly && argc != 3) ||
        (checkOnly && argc != 3)) {
        return Fail(
            "usage: aero-xamlc <input.xaml> <output.axir> or aero-xamlc --check <input.xaml>");
    }
    const char* inputPath =
        checkOnly ? argv[2] : argv[1];

    std::vector<char> source;
    if (!ReadFile(inputPath, source)) {
        return Fail("cannot read input file");
    }

    Aero::Markup::XamlModuleCatalog modules;
    Aero::Core::MetadataDomain metadata;
    Aero::Base::Result<void> registered =
        modules.RegisterMetadata(metadata);
    if (!registered) return Fail(registered.GetStatus());
    registered = metadata.Seal();
    if (!registered) return Fail(registered.GetStatus());
    Aero::Core::MetadataRuntime runtime(metadata);
    registered = runtime.Freeze();
    if (!registered) return Fail(registered.GetStatus());
    Aero::Markup::XamlSchemaContext schema(
        metadata, runtime);
    Aero::Markup::XamlActivationProviderRegistry activation(
        schema);
    registered = modules.ConfigureXaml(
        schema, activation);
    if (!registered) return Fail(registered.GetStatus());
    registered = schema.Freeze();
    if (!registered) return Fail(registered.GetStatus());

    Aero::Markup::Utf8XmlTokenizer tokenizer;
    registered = tokenizer.Reset(Aero::Base::StringView(
        source.data(),
        static_cast<std::uint32_t>(source.size())));
    if (!registered) return Fail(registered.GetStatus());
    Aero::Markup::XamlNodeReader reader(tokenizer);
    Aero::Base::Result<Aero::Markup::XamlCompiledDocument>
        compiled =
            Aero::Markup::XamlCompiledDocument::Compile(
                reader, schema);
    if (!compiled) return Fail(compiled.GetStatus());
    if (checkOnly) return 0;

    Aero::Base::Result<
        Aero::Base::Vector<std::uint8_t>> encoded =
            compiled.Value().Serialize();
    if (!encoded) return Fail(encoded.GetStatus());
    if (!WriteFile(
            argv[2],
            {encoded.Value().Data(),
             encoded.Value().Size()})) {
        return Fail("cannot write output file");
    }
    return 0;
}
