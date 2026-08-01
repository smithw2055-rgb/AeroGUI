#pragma once

#include <Aero/Base/Result.hpp>
#include "gui/MetadataInternal.hpp"

namespace Aero::App {

namespace Detail {

AERO_API Base::Result<void> PopulateAppMetadata(
    ::Aero::Meta::Registration& context) noexcept;

} // namespace Detail

inline constexpr Base::StringView AppMetadataModuleName() noexcept {
    return "Aero.App";
}

inline Base::Result<void> TryRegisterAppMetadata(
    ::Aero::Meta::Registry& domain) noexcept {
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
