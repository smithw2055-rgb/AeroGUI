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
    // Version 7 writes factories, property accessors and method invokers
    // directly into stable-ID behavior records. Structural Info records are
    // callback-free throughout registration and sealed runtime use.
    constexpr std::uint32_t SchemaVersion = 7U;
    const Base::StringView name = AeroPresentationMetadataModuleName();
    return domain.TryRegisterModule({
        MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &Detail::RegisterAeroPresentationMetadataModule,
        nullptr});
}

} // namespace Aero::Core
