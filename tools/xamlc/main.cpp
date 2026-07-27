#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Markup/Compiled/XamlCompiledDocument.hpp>
#include <Aero/Markup/Parsing/XamlNodeReader.hpp>
#include <Aero/Markup/Parsing/XmlTokenizer.hpp>
#include <Aero/Markup/Schema/XamlSchemaManifest.hpp>
#include <Aero/Module.hpp>
#include <Aero/SchemaBundle.hpp>

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
    return Fail(status.message != nullptr && status.message[0] != '\0'
        ? status.message : "operation failed");
}

bool ReadFile(const char* path, std::vector<std::uint8_t>& bytes) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    const std::streamoff length = input.tellg();
    if (length < 0 || static_cast<std::uint64_t>(length) > UINT32_MAX) {
        return false;
    }
    input.seekg(0, std::ios::beg);
    bytes.resize(static_cast<std::size_t>(length));
    return bytes.empty() || static_cast<bool>(input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(length)));
}

bool WriteFile(
    const char* path,
    Aero::Base::Span<const std::uint8_t> bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(
        reinterpret_cast<const char*>(bytes.Data()),
        static_cast<std::streamsize>(bytes.Size()));
    return static_cast<bool>(output);
}

struct CommandLine final {
    const char* schemaPath = nullptr;
    const char* origin = nullptr;
    const char* input = nullptr;
    const char* output = nullptr;
    bool checkOnly = false;
};

bool ParseCommandLine(int argc, char** argv, CommandLine& options) {
    std::vector<const char*> positional;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--check") {
            if (options.checkOnly) return false;
            options.checkOnly = true;
        } else if (argument == "--schema") {
            if (options.schemaPath != nullptr || index + 1 >= argc) {
                return false;
            }
            options.schemaPath = argv[++index];
        } else if (argument == "--origin") {
            if (options.origin != nullptr || index + 1 >= argc) {
                return false;
            }
            options.origin = argv[++index];
        } else if (!argument.empty() && argument.front() == '-') {
            return false;
        } else {
            positional.push_back(argv[index]);
        }
    }
    if (options.checkOnly) {
        if (positional.size() != 1U) return false;
        options.input = positional[0];
    } else {
        if (positional.size() != 2U) return false;
        options.input = positional[0];
        options.output = positional[1];
    }
    if (options.origin == nullptr) options.origin = options.input;
    return true;
}

Aero::Base::Result<Aero::Markup::XamlCompiledDocument>
CompileWithBuiltInSchema(
    Aero::Markup::XamlNodeReader& reader,
    const Aero::Base::ResourceUri& origin) noexcept {
    Aero::ModuleCatalog modules;
    Aero::SchemaBundle bundle;
    Aero::Base::Result<void> status = bundle.Prepare(modules);
    if (!status) return status.GetStatus();
    status = bundle.Finalize(modules, {});
    if (!status) return status.GetStatus();
    return Aero::Markup::XamlCompiledDocument::Compile(
        reader, bundle.XamlSchema(), origin);
}

Aero::Base::Result<Aero::Markup::XamlCompiledDocument>
CompileWithManifest(
    Aero::Markup::XamlNodeReader& reader,
    const Aero::Base::ResourceUri& origin,
    const char* schemaPath) {
    std::vector<std::uint8_t> schemaBytes;
    if (!ReadFile(schemaPath, schemaBytes)) {
        return Aero::Base::Status::Failure(
            Aero::Base::ErrorCode::NotFound,
            "cannot read schema manifest");
    }
    Aero::Base::Result<Aero::Markup::XamlSchemaManifest> manifest =
        Aero::Markup::XamlSchemaManifest::Deserialize({
            schemaBytes.data(),
            static_cast<std::uint32_t>(schemaBytes.size())});
    if (!manifest) return manifest.GetStatus();
    return Aero::Markup::XamlCompiledDocument::Compile(
        reader, manifest.Value(), origin);
}

} // namespace

int main(int argc, char** argv) {
    CommandLine options;
    if (!ParseCommandLine(argc, argv, options)) {
        return Fail(
            "usage: aero-xamlc [--schema file.aeroschema] "
            "[--origin uri] <input.xaml> <output.axir>, or "
            "aero-xamlc [--schema file.aeroschema] "
            "[--origin uri] --check <input.xaml>");
    }

    std::vector<std::uint8_t> source;
    if (!ReadFile(options.input, source)) {
        return Fail("cannot read input file");
    }
    Aero::Base::Result<Aero::Base::ResourceUri> origin =
        Aero::Base::ResourceUri::Parse(Aero::Base::StringView(
            options.origin,
            static_cast<std::uint32_t>(std::strlen(options.origin))));
    if (!origin) return Fail(origin.GetStatus());

    Aero::Markup::Utf8XmlTokenizer tokenizer;
    Aero::Base::Result<void> reset = tokenizer.Reset(Aero::Base::StringView(
        reinterpret_cast<const char*>(source.data()),
        static_cast<std::uint32_t>(source.size())));
    if (!reset) return Fail(reset.GetStatus());
    Aero::Markup::XamlNodeReader reader(tokenizer);

    Aero::Base::Result<Aero::Markup::XamlCompiledDocument> compiled =
        options.schemaPath != nullptr
        ? CompileWithManifest(reader, origin.Value(), options.schemaPath)
        : CompileWithBuiltInSchema(reader, origin.Value());
    if (!compiled) return Fail(compiled.GetStatus());
    if (options.checkOnly) return 0;

    Aero::Base::Result<Aero::Base::Vector<std::uint8_t>> encoded =
        compiled.Value().Serialize();
    if (!encoded) return Fail(encoded.GetStatus());
    if (!WriteFile(options.output, {
            encoded.Value().Data(), encoded.Value().Size()})) {
        return Fail("cannot write output file");
    }
    return 0;
}
