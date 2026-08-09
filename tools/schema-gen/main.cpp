#include <Aero/Gui.hpp>
#include "gui/markup/XamlRuntime.hpp"
#include <AeroApp/App.hpp>
#include <Aero/Module.hpp>
#include "gui/modules/ModuleSet.hpp"


#include <cstdio>
#include <cstdint>
#include <fstream>
#include <string_view>
#include <vector>

namespace {

int Fail(const char* message) noexcept {
    std::fprintf(stderr, "aero-schema-gen: %s\n", message);
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

Aero::Base::Result<Aero::Markup::SchemaManifest>
BuildBuiltInManifest() noexcept {
    Aero::ModuleSet modules;
    Aero::Base::Result<void> status =
        modules.Add(Aero::App::AppMetadataModule());
    if (!status) return status.GetStatus();
    Aero::GuiSchema bundle;
    status = bundle.Prepare(modules);
    if (!status) return status.GetStatus();
    status = bundle.Finalize({});
    if (!status) return status.GetStatus();
    return Aero::Markup::SchemaManifest::Capture(
        bundle.Schema());
}

} // namespace

int main(int argc, char** argv) {
    const bool checkOnly = argc == 3 &&
        std::string_view(argv[1]) == "--check";
    const bool describe = argc == 3 &&
        std::string_view(argv[1]) == "--describe";
    if (checkOnly || describe) {
        std::vector<std::uint8_t> bytes;
        if (!ReadFile(argv[2], bytes)) {
            return Fail("cannot read schema manifest");
        }
        Aero::Base::Result<Aero::Markup::SchemaManifest> manifest =
            Aero::Markup::SchemaManifest::Deserialize({
                bytes.data(), static_cast<std::uint32_t>(bytes.size())});
        if (!manifest) return Fail(manifest.GetStatus());
        if (describe) {
            std::printf(
                "format=%u types=%u members=%u schema_hash=%016llx\n",
                Aero::XamlSchemaManifestFormatVersion,
                manifest.Value().TypeCount(),
                manifest.Value().MemberCount(),
                static_cast<unsigned long long>(
                    manifest.Value().Identity().metadataSchemaHash));
        }
        return 0;
    }
    if (argc != 2) {
        return Fail(
            "usage: aero-schema-gen <output.aeroschema>, "
            "aero-schema-gen --check <input.aeroschema>, or "
            "aero-schema-gen --describe <input.aeroschema>");
    }

    Aero::Base::Result<Aero::Markup::SchemaManifest> manifest =
        BuildBuiltInManifest();
    if (!manifest) return Fail(manifest.GetStatus());
    Aero::Base::Result<Aero::Base::Vector<std::uint8_t>> encoded =
        manifest.Value().Serialize();
    if (!encoded) return Fail(encoded.GetStatus());
    if (!WriteFile(argv[1], {
            encoded.Value().Data(), encoded.Value().Size()})) {
        return Fail("cannot write schema manifest");
    }
    return 0;
}
