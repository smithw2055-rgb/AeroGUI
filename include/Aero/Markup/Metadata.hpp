#pragma once

#include <Aero/Core/Metadata/MetadataDomain.hpp>

namespace Aero::Markup {

namespace Detail {

AERO_API Base::Result<void> PopulateMarkupMetadata(
    Core::MetaRegistrationContext& context) noexcept;

inline Base::Result<void> RegisterMarkupMetadataModule(
    Core::MetaRegistrationContext& context,
    void*) noexcept {
    return PopulateMarkupMetadata(context);
}

} // namespace Detail

inline constexpr Base::StringView MarkupMetadataModuleName() noexcept {
    return "Aero.Markup";
}

inline Base::Result<void> TryRegisterMarkupMetadata(
    Core::MetadataDomain& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 1U;
    const Base::StringView name = MarkupMetadataModuleName();
    return domain.TryRegisterModule({
        Core::MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &Detail::RegisterMarkupMetadataModule,
        nullptr});
}

} // namespace Aero::Markup
