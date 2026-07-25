#include <Aero/Markup/XamlModuleSdk.hpp>

#include <utility>

namespace Aero::Markup {

Base::Result<void> XamlModuleCatalog::TryAdd(
    const XamlModuleManifest& manifest) noexcept {
    if (manifest.name.Empty() ||
        manifest.metadataSchemaVersion == 0U ||
        manifest.xamlSchemaVersion == 0U ||
        manifest.registerMetadata == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML module manifest is incomplete");
    }
    for (const Module& module : modules_) {
        if (module.name.View() == manifest.name) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "XAML module is already present in the catalog");
        }
    }
    Module module;
    Base::Result<void> named =
        module.name.TryAssign(manifest.name);
    if (!named) return named.GetStatus();
    module.metadataSchemaVersion =
        manifest.metadataSchemaVersion;
    module.xamlSchemaVersion =
        manifest.xamlSchemaVersion;
    module.registerMetadata = manifest.registerMetadata;
    module.configureXaml = manifest.configureXaml;
    module.context = manifest.context;
    return modules_.TryPushBack(std::move(module));
}

Base::Result<void> XamlModuleCatalog::RegisterMetadata(
    Core::MetadataDomain& domain,
    bool includeBuiltInUi) const noexcept {
    if (includeBuiltInUi) {
        Base::Result<void> builtIns =
            Controls::TryRegisterBuiltInUiMetadata(domain);
        if (!builtIns) return builtIns.GetStatus();
    }
    for (const Module& module : modules_) {
        const Base::StringView name = module.name.View();
        Base::Result<void> registered =
            domain.TryRegisterModule({
                Core::MakeMetadataModuleId(name),
                name,
                module.metadataSchemaVersion,
                module.registerMetadata,
                module.context});
        if (!registered) return registered.GetStatus();
    }
    return {};
}

Base::Result<void> XamlModuleCatalog::ConfigureXaml(
    XamlSchemaContext& schema,
    XamlActivationProviderRegistry& activation) const noexcept {
    if (schema.IsFrozen() || activation.IsFrozen() ||
        &activation.Schema() != &schema) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML module configuration requires matching mutable registries");
    }
    Base::Result<void> manifest =
        schema.SetModuleManifestHash(ManifestHash());
    if (!manifest) return manifest.GetStatus();
    for (const Module& module : modules_) {
        if (module.configureXaml == nullptr) continue;
        Base::Result<void> configured =
            module.configureXaml(
                schema, activation, module.context);
        if (!configured) return configured.GetStatus();
    }
    return {};
}

Base::HashCode XamlModuleCatalog::ManifestHash() const noexcept {
    Base::Detail::StableMetadataIdBuilder builder;
    constexpr char domain[] = "AERO.XAML.MODULES.V1";
    builder.AddText(
        domain,
        static_cast<std::uint32_t>(
            sizeof(domain) - 1U));
    builder.AddU32(modules_.Size());
    for (const Module& module : modules_) {
        builder.AddString(module.name.View());
        builder.AddU32(module.metadataSchemaVersion);
        builder.AddU32(module.xamlSchemaVersion);
        builder.AddByte(
            module.configureXaml != nullptr ? 1U : 0U);
    }
    return builder.Finish();
}

} // namespace Aero::Markup

#include "RuntimeHost.inc"
#include "RuntimeWindow.inc"
#include "RuntimeSafety.inc"
#include "RuntimeServices.inc"
