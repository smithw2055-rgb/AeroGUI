#pragma once

#include <Aero/Core/ControlPrimitives.hpp>
#include <Aero/Core/MetadataDomain.hpp>

namespace Aero::Core {
namespace Detail {

inline Base::Result<void> RegisterAeroPresentationMetadataModule(
    MetaRegistrationContext& context,
    void*) noexcept {
    return TryRegisterPresentationMetadata(
        context.types,
        context.dependencyProperties,
        context.routedEvents);
}

} // namespace Detail

inline constexpr Base::StringView AeroPresentationMetadataModuleName() noexcept {
    return Base::StringView("Aero.Presentation");
}

inline Base::Result<void> TryRegisterAeroPresentationMetadata(
    MetadataDomain& domain) noexcept {
    // Version 4 adds sealed value-semantics and text-converter runtime facets.
    constexpr std::uint32_t SchemaVersion = 4U;
    const Base::StringView name = AeroPresentationMetadataModuleName();
    return domain.TryRegisterModule({
        MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &Detail::RegisterAeroPresentationMetadataModule,
        nullptr});
}

} // namespace Aero::Core
