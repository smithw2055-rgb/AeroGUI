#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Meta/Registry.hpp>

namespace Aero::Controls {

namespace Detail {

// Module population is an implementation callback; hosts register through the
// Meta::Registry overload below.
AERO_API Base::Result<void> PopulateControlsMetadata(
    ::Aero::Meta::Registration& context) noexcept;

} // namespace Detail

inline constexpr Base::StringView ControlsMetadataModuleName() noexcept {
    return "Aero.Controls";
}

inline Base::Result<void> TryRegisterControlsMetadata(
    ::Aero::Meta::Registry& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 28U;
    const Base::StringView name = ControlsMetadataModuleName();
    return domain.TryRegisterModule({
        Core::MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &Detail::PopulateControlsMetadata,
        nullptr,
        nullptr});
}


} // namespace Aero::Controls
