#include "StatusBadge.hpp"

#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Markup/XamlCompiledDocument.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <vector>

namespace {

using namespace Aero;
using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Markup;
using namespace Aero::Samples::ControlGallery;

int Fail(Status status) noexcept {
    std::fprintf(
        stderr,
        "controlgallery-xamlc: %s\n",
        status.message != nullptr
            ? status.message
            : "operation failed");
    return 1;
}

bool ReadFile(
    const char* path,
    std::vector<char>& output) {
    std::ifstream stream(
        path, std::ios::binary);
    if (!stream) {
        return false;
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff length =
        stream.tellg();
    if (length < 0 ||
        static_cast<std::uint64_t>(length) >
            UINT32_MAX) {
        return false;
    }
    stream.seekg(0, std::ios::beg);
    output.resize(
        static_cast<std::size_t>(length));
    return output.empty() ||
        static_cast<bool>(
            stream.read(
                output.data(),
                static_cast<std::streamsize>(
                    length)));
}

bool WriteFile(
    const char* path,
    Span<const std::uint8_t> bytes) {
    std::ofstream stream(
        path,
        std::ios::binary |
            std::ios::trunc);
    if (!stream) {
        return false;
    }
    stream.write(
        reinterpret_cast<const char*>(
            bytes.Data()),
        static_cast<std::streamsize>(
            bytes.Size()));
    return static_cast<bool>(stream);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(
            stderr,
            "usage: controlgallery-xamlc "
            "<input.xaml> <output.axir>\n");
        return 1;
    }

    std::vector<char> source;
    if (!ReadFile(argv[1], source)) {
        std::fprintf(
            stderr,
            "controlgallery-xamlc: "
            "cannot read input\n");
        return 1;
    }

    ModuleCatalog modules;
    Result<void> status = modules.TryAdd(
        MakeStatusBadgeModuleManifest());
    if (!status) {
        return Fail(status.GetStatus());
    }
    MetadataDomain metadata;
    status = modules.RegisterMetadata(metadata);
    if (status) {
        status = metadata.Seal();
    }
    if (!status) {
        return Fail(status.GetStatus());
    }
    MetadataRuntime runtime(metadata);
    status = runtime.Freeze();
    if (!status) {
        return Fail(status.GetStatus());
    }
    XamlSchemaContext schema(
        metadata, runtime);
    status = schema.Freeze();
    if (!status) {
        return Fail(status.GetStatus());
    }

    Utf8XmlTokenizer tokenizer;
    status = tokenizer.Reset({
        source.data(),
        static_cast<std::uint32_t>(
            source.size())});
    if (!status) {
        return Fail(status.GetStatus());
    }
    XamlNodeReader reader(tokenizer);
    Result<XamlCompiledDocument> compiled =
        XamlCompiledDocument::Compile(
            reader, schema);
    if (!compiled) {
        return Fail(compiled.GetStatus());
    }
    Result<Vector<std::uint8_t>> bytes =
        compiled.Value().Serialize();
    if (!bytes) {
        return Fail(bytes.GetStatus());
    }
    if (!WriteFile(
            argv[2],
            {bytes.Value().Data(),
             bytes.Value().Size()})) {
        std::fprintf(
            stderr,
            "controlgallery-xamlc: "
            "cannot write output\n");
        return 1;
    }
    return 0;
}
