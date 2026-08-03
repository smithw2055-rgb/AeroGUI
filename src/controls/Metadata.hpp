#pragma once

#include <Aero/Base/Result.hpp>
#include "gui/MetadataInternal.hpp"

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

inline Base::Result<void> RegisterControlsMetadata(
    ::Aero::Meta::Registry& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 29U;
    const Base::StringView name = ControlsMetadataModuleName();
    return domain.RegisterModule({
        Meta::MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &Detail::PopulateControlsMetadata,
        nullptr,
        nullptr});
}


} // namespace Aero::Controls
