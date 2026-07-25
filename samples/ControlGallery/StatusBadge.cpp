#include "StatusBadge.hpp"

#include <Aero/Core/Metadata/MetadataDsl.hpp>
#include <Aero/Core/Metadata/MetadataRegistrationValues.hpp>
#include <Aero/Core/ObjectServices.hpp>

#include <utility>

namespace Aero::Samples::ControlGallery {
namespace {

using namespace Base;
using namespace Core;
using namespace Markup;
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
    badge.DependencyProperty(
        StatusBadge::AccentProperty,
        "Accent",
        TypeOf<Color>(),
        std::move(accent).Value(),
        PropertyMetadataFlags::AffectsRender);
    return badge.Finish();
}

Result<Ref<Object>> Activate(
    TypeId requestedType,
    const XamlActivationContext& activation,
    void*) noexcept {
    if (!activation.IsCompatible() ||
        activation.dispatcher == nullptr ||
        activation.dependencyProperties == nullptr ||
        requestedType != StatusBadge::StaticTypeId()) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "StatusBadge activation services are invalid");
    }
    Result<Ref<StatusBadge>> made =
        MakeRef<StatusBadge>();
    if (!made) {
        return made.GetStatus();
    }
    return Ref<Object>(
        std::move(made).Value());
}

Result<void> ConfigureXaml(
    XamlSchemaContext&,
    XamlActivationProviderRegistry& activation,
    void*) noexcept {
    return activation.TryRegister({
        StatusBadge::StaticTypeId(),
        &Activate,
        nullptr});
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

Markup::XamlModuleManifest
MakeStatusBadgeModuleManifest() noexcept {
    Markup::XamlModuleManifest manifest;
    manifest.name =
        "Aero.Samples.ControlGallery.StatusBadge";
    manifest.metadataSchemaVersion = 1U;
    manifest.xamlSchemaVersion = 1U;
    manifest.registerMetadata = &RegisterMetadata;
    manifest.configureXaml = &ConfigureXaml;
    return manifest;
}

} // namespace Aero::Samples::ControlGallery
