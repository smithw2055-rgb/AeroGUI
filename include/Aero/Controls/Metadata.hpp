#pragma once

#include <Aero/Core/Metadata/CoreMetadata.hpp>
#include <Aero/Presentation/Metadata.hpp>

namespace Aero::Controls {

namespace Detail {

// Module population is an implementation callback; hosts register through the
// MetadataDomain overload below.
AERO_API Base::Result<void> PopulateControlsMetadata(
    Core::MetaRegistrationContext& context) noexcept;

inline Base::Result<void> RegisterControlsMetadataModule(
    Core::MetaRegistrationContext& context,
    void*) noexcept {
    return PopulateControlsMetadata(context);
}

} // namespace Detail

inline constexpr Base::StringView ControlsMetadataModuleName() noexcept {
    return "Aero.Controls";
}

inline Base::Result<void> TryRegisterControlsMetadata(
    Core::MetadataDomain& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 9U;
    const Base::StringView name = ControlsMetadataModuleName();
    return domain.TryRegisterModule({
        Core::MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &Detail::RegisterControlsMetadataModule,
        nullptr});
}

inline Base::Result<void> TryRegisterBuiltInUiMetadata(
    Core::MetadataDomain& domain) noexcept {
    Base::Result<void> result = Core::TryRegisterCoreMetadata(domain);
    if (!result) return result.GetStatus();
    result = Presentation::TryRegisterPresentationMetadata(domain);
    if (!result) return result.GetStatus();
    return TryRegisterControlsMetadata(domain);
}

} // namespace Aero::Controls
