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
    // Version 6 materializes type factories, property accessors and method
    // invokers into dedicated registration records before runtime facets build.
    constexpr std::uint32_t SchemaVersion = 6U;
    const Base::StringView name = AeroPresentationMetadataModuleName();
    return domain.TryRegisterModule({
        MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &Detail::RegisterAeroPresentationMetadataModule,
        nullptr});
}

} // namespace Aero::Core
