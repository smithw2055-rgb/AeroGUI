#include "StatusBadge.hpp"

#include <Aero/Core/Metadata/MetadataDsl.hpp>
#include <Aero/Core/Metadata/MetadataRegistrationValues.hpp>
#include <Aero/Core/ObjectServices.hpp>

#include <utility>

namespace Aero::Samples::ControlGallery {
namespace {

using namespace Base;
using namespace Core;
using namespace Presentation;

Result<void> RegisterMetadata(
    MetaRegistrationContext& context,
    void*) noexcept {
    const Color defaultAccent{
        0.12F, 0.55F, 0.34F, 1.0F};
    Result<Value> accent =
        context.Values().TryCreateValue(
            TypeOf<Color>(), &defaultAccent);
    if (!accent) {
        return accent.GetStatus();
    }
    MetaTypeBuilder<StatusBadge> badge =
        MetaTypeBuilder<StatusBadge>::Object(
            context);
    badge
        .DefaultFactory()
        .DependencyProperty(
            StatusBadge::AccentProperty,
            "Accent",
            TypeOf<Color>(),
            std::move(accent).Value(),
            PropertyMetadataFlags::AffectsRender);
    return badge.Finish();
}

} // namespace

Presentation::Color
StatusBadge::Accent() const noexcept {
    Base::Result<Core::Value> value =
        GetValue(AccentProperty);
    return value
        ? *static_cast<const Presentation::Color*>(
            value.Value().AsCustom())
        : Presentation::Color{
            0.12F, 0.55F, 0.34F, 1.0F};
}

Base::Result<void> StatusBadge::SetAccent(
    Presentation::Color value) noexcept {
    if (!Base::IsFiniteColor(value) ||
        value.red < 0.0F ||
        value.red > 1.0F ||
        value.green < 0.0F ||
        value.green > 1.0F ||
        value.blue < 0.0F ||
        value.blue > 1.0F ||
        value.alpha < 0.0F ||
        value.alpha > 1.0F) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "StatusBadge accent is invalid");
    }
    Base::Result<Core::Value> stored =
        Core::TryCreateRuntimeValue(
            Core::TypeOf<Presentation::Color>(),
            &value);
    if (!stored) {
        return stored.GetStatus();
    }
    return SetValue(
        AccentProperty,
        std::move(stored).Value());
}

Base::Result<void> StatusBadge::BuildDisplayList(
    Presentation::DisplayListBuilder& builder) noexcept {
    return builder.FillRoundedRect(
        {0.0, 0.0,
         RenderSize().width,
         RenderSize().height},
        Accent(),
        6.0);
}

Aero::ModuleRegistration
MakeStatusBadgeModuleManifest() noexcept {
    Aero::ModuleRegistration manifest;
    manifest.name =
        "Aero.Samples.ControlGallery.StatusBadge";
    manifest.schemaVersion = 1U;
    manifest.registerModule = &RegisterMetadata;
    return manifest;
}

Base::Result<void> RegisterControlGalleryModules(
    Aero::ModuleCatalog& modules) noexcept {
    return modules.TryAdd(MakeStatusBadgeModuleManifest());
}

} // namespace Aero::Samples::ControlGallery
