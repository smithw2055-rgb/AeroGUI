#include <Aero/Gui.hpp>
#include "gui/markup/XamlRuntime.hpp"
#include "app/Metadata.hpp"
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Module.hpp>
#include "gui/modules/ModuleSet.hpp"

#include <Aero/Version.hpp>

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

bool WriteDepfile(
    const char* path,
    const char* output,
    const char* input,
    const char* schema) {
    if (path == nullptr) return true;
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    const auto escaped = [](std::ofstream& target, const char* value) {
        for (const char* cursor = value; *cursor != '\0'; ++cursor) {
            if (*cursor == ' ' || *cursor == '#' || *cursor == '$' ||
                *cursor == '\\') {
                target.put('\\');
            }
            target.put(*cursor);
        }
    };
    escaped(stream, output);
    stream << ": ";
    escaped(stream, input);
    if (schema != nullptr) {
        stream.put(' ');
        escaped(stream, schema);
    }
    stream.put('\n');
    return static_cast<bool>(stream);
}

struct CommandLine {
    const char* schemaPath = nullptr;
    const char* origin = nullptr;
    const char* depfile = nullptr;
    const char* input = nullptr;
    const char* output = nullptr;
    bool checkOnly = false;
    bool stripSourceMap = false;
    bool showVersion = false;
};

bool ParseCommandLine(int argc, char** argv, CommandLine& options) {
    std::vector<const char*> positional;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--version") {
            if (options.showVersion) return false;
            options.showVersion = true;
        } else if (argument == "--check") {
            if (options.checkOnly) return false;
            options.checkOnly = true;
        } else if (argument == "--strip-source-map") {
            if (options.stripSourceMap) return false;
            options.stripSourceMap = true;
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
        } else if (argument == "--depfile") {
            if (options.depfile != nullptr || index + 1 >= argc) {
                return false;
            }
            options.depfile = argv[++index];
        } else if (!argument.empty() && argument.front() == '-') {
            return false;
        } else {
            positional.push_back(argv[index]);
        }
    }
    if (options.showVersion) {
        return argc == 2;
    }
    if (options.checkOnly) {
        if (options.depfile != nullptr || positional.size() != 1U) return false;
        options.input = positional[0];
    } else {
        if (positional.size() != 2U) return false;
        options.input = positional[0];
        options.output = positional[1];
    }
    if (options.origin == nullptr) options.origin = options.input;
    return true;
}

Aero::Base::Result<Aero::Markup::CompiledDocument>
CompileWithBuiltInSchema(
    Aero::Markup::NodeReader& reader,
    const Aero::Base::ResourceUri& origin) noexcept {
    Aero::ModuleSet modules;
    Aero::Base::Result<void> status =
        modules.Add(Aero::App::AppMetadataModule());
    if (!status) return status.GetStatus();
    Aero::GuiSchema bundle;
    status = bundle.Prepare(modules);
    if (!status) return status.GetStatus();
    status = bundle.Finalize({});
    if (!status) return status.GetStatus();
    return Aero::Markup::CompiledDocument::Compile(
        reader, bundle.Schema(), origin);
}

Aero::Base::Result<Aero::Markup::CompiledDocument>
CompileWithManifest(
    Aero::Markup::NodeReader& reader,
    const Aero::Base::ResourceUri& origin,
    const char* schemaPath) {
    std::vector<std::uint8_t> schemaBytes;
    if (!ReadFile(schemaPath, schemaBytes)) {
        return Aero::Base::Status::Failure(
            Aero::Base::ErrorCode::NotFound,
            "cannot read schema manifest");
    }
    Aero::Base::Result<Aero::Markup::SchemaManifest> manifest =
        Aero::Markup::SchemaManifest::Deserialize({
            schemaBytes.data(),
            static_cast<std::uint32_t>(schemaBytes.size())});
    if (!manifest) return manifest.GetStatus();
    return Aero::Markup::CompiledDocument::Compile(
        reader, manifest.Value(), origin);
}

} // namespace

int main(int argc, char** argv) {
    CommandLine options;
    if (!ParseCommandLine(argc, argv, options)) {
        return Fail(
            "usage: aero-xamlc [--schema file.aeroschema] "
            "[--origin uri] [--strip-source-map] "
            "<input.xaml> <output.axb>, or "
            "aero-xamlc [--schema file.aeroschema] "
            "[--origin uri] --check <input.xaml>, or "
            "aero-xamlc --version");
    }
    if (options.showVersion) {
        std::printf(
            "aero-xamlc %u.%u.%u (schema ABI %u, AXB2 cache %u, encoding %u)\n",
            Aero::VersionMajor,
            Aero::VersionMinor,
            Aero::VersionPatch,
            Aero::XamlSchemaAbiVersion,
            Aero::XamlCompiledCacheFormatVersion,
            Aero::XamlCompiledDocumentEncodingVersion);
        return 0;
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
    Aero::Markup::NodeReader reader(tokenizer);

    Aero::Base::Result<Aero::Markup::CompiledDocument> compiled =
        options.schemaPath != nullptr
        ? CompileWithManifest(reader, origin.Value(), options.schemaPath)
        : CompileWithBuiltInSchema(reader, origin.Value());
    if (!compiled) return Fail(compiled.GetStatus());
    if (options.checkOnly) return 0;

    Aero::Markup::CompiledDocumentSerializeOptions
        serializeOptions;
    serializeOptions.includeSourceMap =
        !options.stripSourceMap;
    Aero::Base::Result<Aero::Base::Vector<std::uint8_t>> encoded =
        compiled.Value().Serialize(serializeOptions);
    if (!encoded) return Fail(encoded.GetStatus());
    if (!WriteFile(options.output, {
            encoded.Value().Data(), encoded.Value().Size()})) {
        return Fail("cannot write output file");
    }
    if (!WriteDepfile(
            options.depfile,
            options.output,
            options.input,
            options.schemaPath)) {
        return Fail("cannot write dependency file");
    }
    return 0;
}
