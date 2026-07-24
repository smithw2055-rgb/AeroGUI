#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Core/Metadata/MetadataDomain.hpp>

namespace Aero::Presentation {

// Registers the complete built-in presentation schema through the typed
// Fluent metadata DSL. The function leaves all stores mutable for host modules.
namespace Detail {

// Module population is an implementation callback; hosts register through the
// MetadataDomain overload below.
AERO_API Base::Result<void> PopulatePresentationMetadata(
    Core::MetaRegistrationContext& context) noexcept;

inline Base::Result<void> RegisterPresentationMetadataModule(
    Core::MetaRegistrationContext& context,
    void*) noexcept {
    return PopulatePresentationMetadata(context);
}

} // namespace Detail

inline constexpr Base::StringView PresentationMetadataModuleName() noexcept {
    return "Aero.Presentation";
}

inline Base::Result<void> TryRegisterPresentationMetadata(
    Core::MetadataDomain& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 5U;
    const Base::StringView name = PresentationMetadataModuleName();
    return domain.TryRegisterModule({
        Core::MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &Detail::RegisterPresentationMetadataModule,
        nullptr});
}

} // namespace Aero::Presentation
