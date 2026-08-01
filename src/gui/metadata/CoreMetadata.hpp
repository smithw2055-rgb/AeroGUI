#pragma once

#include <Aero/Meta/MetadataDomain.hpp>

namespace Aero::Core {

namespace Detail {

// Module population is an implementation callback; hosts register through the
// MetadataDomain overload below.
AERO_API Base::Result<void> PopulateCoreMetadata(
    MetadataContext& context) noexcept;

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
        &Detail::PopulateCoreMetadata,
        nullptr,
        nullptr});
}

} // namespace Aero::Core
