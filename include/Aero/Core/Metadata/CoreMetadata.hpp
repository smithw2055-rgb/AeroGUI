#pragma once

#include <Aero/Core/Metadata/MetadataDomain.hpp>

namespace Aero::Core {

namespace Detail {

// Module population is an implementation callback; hosts register through the
// MetadataDomain overload below.
AERO_API Base::Result<void> PopulateCoreMetadata(
    MetaRegistrationContext& context) noexcept;

inline Base::Result<void> RegisterCoreMetadataModule(
    MetaRegistrationContext& context,
    void*) noexcept {
    return PopulateCoreMetadata(context);
}

} // namespace Detail

inline constexpr Base::StringView CoreMetadataModuleName() noexcept {
    return "Aero.Core";
}

inline Base::Result<void> TryRegisterCoreMetadata(
    MetadataDomain& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 1U;
    const Base::StringView name = CoreMetadataModuleName();
    return domain.TryRegisterModule({
        MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &Detail::RegisterCoreMetadataModule,
        nullptr});
}

} // namespace Aero::Core
