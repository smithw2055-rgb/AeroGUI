#pragma once

#include <Aero/Base/Result.hpp>
#include "gui/GuiPrivate.hpp"

namespace Aero::App::Detail {

AERO_API Base::Result<void> PopulateAppMetadata(
    ::Aero::Meta::Registration& context) noexcept;

} // namespace Aero::App::Detail

namespace Aero::App {

inline constexpr Base::StringView AppMetadataModuleName() noexcept {
    return "Aero.App";
}

inline Base::Result<void> RegisterAppMetadata(
    ::Aero::Meta::Registry& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 2U;
    const Base::StringView name = AppMetadataModuleName();
    return domain.RegisterModule({
        Meta::MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &::Aero::App::Detail::PopulateAppMetadata,
        nullptr,
        nullptr});
}

} // namespace Aero::App
