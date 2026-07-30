#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Core/Metadata/MetadataDomain.hpp>

namespace Aero::App {

namespace Detail {

AERO_API Base::Result<void> PopulateAppMetadata(
    Core::MetadataContext& context) noexcept;

} // namespace Detail

inline constexpr Base::StringView AppMetadataModuleName() noexcept {
    return "Aero.App";
}

inline Base::Result<void> TryRegisterAppMetadata(
    Core::MetadataDomain& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 1U;
    const Base::StringView name = AppMetadataModuleName();
    return domain.TryRegisterModule({
        Core::MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &Detail::PopulateAppMetadata,
        nullptr,
        nullptr});
}

} // namespace Aero::App
