#include <Aero/Markup/XamlCompiledCache.hpp>

namespace Aero::Markup {

Base::Result<XamlCompiledCacheIdentity> BuildXamlCompiledCacheIdentity(
    const Core::MetadataDomain& domain,
    Base::HashCode moduleManifestHash) noexcept {
    Base::Result<Base::HashCode> hash = domain.ComputeSchemaHash();
    if (!hash) return hash.GetStatus();

    XamlCompiledCacheIdentity identity;
    identity.metadataSchemaHash = hash.Value();
    identity.moduleManifestHash = moduleManifestHash;
    return identity;
}

XamlCompiledCacheCompatibility CompareXamlCompiledCacheIdentity(
    const XamlCompiledCacheIdentity& cached,
    const XamlCompiledCacheIdentity& current) noexcept {
    if (cached.cacheFormatVersion != current.cacheFormatVersion) {
        return XamlCompiledCacheCompatibility::CacheFormatMismatch;
    }
    if (cached.typeIdAlgorithmVersion != current.typeIdAlgorithmVersion) {
        return XamlCompiledCacheCompatibility::TypeIdAlgorithmMismatch;
    }
    if (cached.descriptorFormatVersion != current.descriptorFormatVersion) {
        return XamlCompiledCacheCompatibility::DescriptorFormatMismatch;
    }
    if (cached.facetFormatVersion != current.facetFormatVersion) {
        return XamlCompiledCacheCompatibility::FacetFormatMismatch;
    }
    if (cached.metadataSchemaHash != current.metadataSchemaHash) {
        return XamlCompiledCacheCompatibility::MetadataSchemaMismatch;
    }
    if (cached.moduleManifestHash != current.moduleManifestHash) {
        return XamlCompiledCacheCompatibility::ModuleManifestMismatch;
    }
    return XamlCompiledCacheCompatibility::Compatible;
}

Base::Result<void> ValidateXamlCompiledCacheIdentity(
    const XamlCompiledCacheIdentity& cached,
    const Core::MetadataDomain& currentDomain,
    Base::HashCode moduleManifestHash) noexcept {
    Base::Result<XamlCompiledCacheIdentity> current =
        BuildXamlCompiledCacheIdentity(
            currentDomain, moduleManifestHash);
    if (!current) return current.GetStatus();

    switch (CompareXamlCompiledCacheIdentity(cached, current.Value())) {
    case XamlCompiledCacheCompatibility::Compatible:
        return {};
    case XamlCompiledCacheCompatibility::CacheFormatMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML cache format version is incompatible");
    case XamlCompiledCacheCompatibility::TypeIdAlgorithmMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML TypeId algorithm version is incompatible");
    case XamlCompiledCacheCompatibility::DescriptorFormatMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML metadata descriptor format is incompatible");
    case XamlCompiledCacheCompatibility::FacetFormatMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML metadata facet format is incompatible");
    case XamlCompiledCacheCompatibility::MetadataSchemaMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Compiled XAML metadata schema hash does not match the runtime");
    case XamlCompiledCacheCompatibility::ModuleManifestMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Compiled XAML module manifest does not match the runtime");
    }
    return Base::Status::Failure(
        Base::ErrorCode::InternalError,
        "Compiled XAML compatibility result is invalid");
}

} // namespace Aero::Markup
