#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Meta/MetadataDomain.hpp>

namespace Aero::Detail {

// Registers the complete built-in UI schema through the typed
// Fluent metadata DSL. The function leaves all stores mutable for host modules.
namespace Detail {

// Module population is an implementation callback; hosts register through the
// MetadataDomain overload below.
AERO_API Base::Result<void> PopulateUiMetadata(
    Core::MetadataContext& context) noexcept;

} // namespace Detail

inline constexpr Base::StringView UiMetadataModuleName() noexcept {
    return "Aero.UI";
}

inline Base::Result<void> TryRegisterUiMetadata(
    Core::MetadataDomain& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 11U;
    const Base::StringView name = UiMetadataModuleName();
    return domain.TryRegisterModule({
        Core::MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &Detail::PopulateUiMetadata,
        nullptr,
        nullptr});
}

} // namespace Aero::Detail
