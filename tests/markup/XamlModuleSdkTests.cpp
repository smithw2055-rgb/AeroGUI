#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Markup/XamlCompiledCache.hpp>
#include <Aero/Markup/XamlModuleSdk.hpp>

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

struct ModuleProbe final {
    std::uint32_t metadataCalls = 0U;
    std::uint32_t xamlCalls = 0U;
};

Result<void> RegisterModule(
    MetaRegistrationContext& context,
    void* userContext) noexcept {
    auto* probe = static_cast<ModuleProbe*>(userContext);
    ++probe->metadataCalls;
    Result<TypeId> registered =
        context.Types().TryRegisterType(
            TypeRegistration::Object(
                "urn:module-sdk-tests",
                "Widget",
                BuiltinTypes::FrameworkElement,
                TypeFlags::None,
                nullptr));
    return registered
        ? Result<void>()
        : Result<void>(registered.GetStatus());
}

Result<void> ConfigureModule(
    XamlSchemaContext&,
    XamlActivationProviderRegistry&,
    void* userContext) noexcept {
    ++static_cast<ModuleProbe*>(userContext)->xamlCalls;
    return {};
}

bool TestSharedModuleCatalogAndManifestIdentity() {
    ModuleProbe probe;
    XamlModuleCatalog catalog;
    XamlModuleManifest manifest;
    manifest.name = "Tests.ModuleSdk";
    manifest.metadataSchemaVersion = 3U;
    manifest.xamlSchemaVersion = 7U;
    manifest.registerMetadata = &RegisterModule;
    manifest.configureXaml = &ConfigureModule;
    manifest.context = &probe;
    CHECK(catalog.TryAdd(manifest));
    CHECK(!catalog.TryAdd(manifest));
    CHECK(catalog.ModuleCount() == 1U);
    CHECK(catalog.ManifestHash() != 0U);

    MetadataDomain metadata;
    CHECK(catalog.RegisterMetadata(metadata));
    CHECK(probe.metadataCalls == 1U);
    CHECK(metadata.Seal());
    MetadataRuntime runtime(metadata);
    CHECK(runtime.Freeze());
    XamlSchemaContext schema(metadata, runtime);
    XamlActivationProviderRegistry activation(schema);
    CHECK(catalog.ConfigureXaml(schema, activation));
    CHECK(probe.xamlCalls == 1U);
    CHECK(schema.ModuleManifestHash() ==
        catalog.ManifestHash());
    CHECK(schema.Freeze());

    Result<XamlCompiledCacheIdentity> matching =
        BuildXamlCompiledCacheIdentity(
            metadata, catalog.ManifestHash());
    CHECK(matching);
    CHECK(ValidateXamlCompiledCacheIdentity(
        matching.Value(),
        metadata,
        catalog.ManifestHash()));
    Result<void> mismatch =
        ValidateXamlCompiledCacheIdentity(
            matching.Value(),
            metadata,
            catalog.ManifestHash() + 1U);
    CHECK(!mismatch);
    CHECK(mismatch.GetStatus().code ==
        ErrorCode::ValidationFailed);
    return true;
}

} // namespace

int main() {
    if (!TestSharedModuleCatalogAndManifestIdentity()) return 1;
    std::puts("Aero XAML module SDK tests passed");
    return 0;
}
