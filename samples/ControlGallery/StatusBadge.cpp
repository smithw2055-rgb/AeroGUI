#include "StatusBadge.hpp"

namespace Aero::Samples::ControlGallery {
namespace {

using namespace Base;
using namespace Core;
using namespace Presentation;

const Color DefaultAccent{
    0.12F, 0.55F, 0.34F, 1.0F};

bool IsValidAccent(Color value) noexcept {
    return Base::IsFiniteColor(value) &&
        value.red >= 0.0F && value.red <= 1.0F &&
        value.green >= 0.0F && value.green <= 1.0F &&
        value.blue >= 0.0F && value.blue <= 1.0F &&
        value.alpha >= 0.0F && value.alpha <= 1.0F;
}

bool ValidateAccent(const Value& value) noexcept {
    return value.Type() == TypeOf<Color>() &&
        value.Kind() == ValueKind::Custom &&
        value.AsCustom() != nullptr &&
        IsValidAccent(
            *static_cast<const Color*>(value.AsCustom()));
}

Result<void> RegisterMetadata(
    RegistrationContext& context,
    void*) noexcept {
    return Describe<StatusBadge>(context)
        .Factory()
        .Property(
            StatusBadge::AccentProperty,
            DefaultAccent,
            PropertyMetadataFlags::AffectsRender,
            &ValidateAccent)
        .Finish();
}

} // namespace

Presentation::Color
StatusBadge::Accent() const noexcept {
    Base::Result<Presentation::Color> value =
        AccentProperty.Get(*this);
    return value ? value.Value() : DefaultAccent;
}

Base::Result<void> StatusBadge::SetAccent(
    Presentation::Color value) noexcept {
    if (!IsValidAccent(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "StatusBadge accent is invalid");
    }
    return AccentProperty.Set(*this, value);
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
    return Aero::DefineModule(
        "Aero.Samples.ControlGallery.StatusBadge",
        &RegisterMetadata);
}

Base::Result<void> RegisterControlGalleryModules(
    Aero::ModuleCatalog& modules) noexcept {
    return modules.TryAdd(MakeStatusBadgeModuleManifest());
}

} // namespace Aero::Samples::ControlGallery
