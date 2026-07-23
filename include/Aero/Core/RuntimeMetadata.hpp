#pragma once

#include <Aero/Core/ControlPrimitives.hpp>
#include <Aero/Core/MetadataDomain.hpp>

namespace Aero::Core {
namespace Detail {

inline Base::Result<void> RegisterAeroPresentationMetadataModule(
    MetaRegistrationContext& context,
    void*) noexcept {
return TryRegisterPresentationMetadata(context);
}

} // namespace Detail

inline constexpr Base::StringView AeroPresentationMetadataModuleName() noexcept {
    return Base::StringView("Aero.Presentation");
}

inline Base::Result<void> TryRegisterAeroPresentationMetadata(
    MetadataDomain& domain) noexcept {
    // Version 7 keeps structural metadata callback-free and transfers all
    // executable registrations through dedicated behavior/value stores.
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
