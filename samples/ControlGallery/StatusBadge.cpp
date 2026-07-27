#include "StatusBadge.hpp"

namespace Aero::Samples::ControlGallery {
namespace {

using namespace Base;
using namespace Presentation;

const Color DefaultAccent{
    0.12F, 0.55F, 0.34F, 1.0F};

bool IsValidAccent(const Color& value) noexcept {
    return Base::IsFiniteColor(value) &&
        value.red >= 0.0F && value.red <= 1.0F &&
        value.green >= 0.0F && value.green <= 1.0F &&
        value.blue >= 0.0F && value.blue <= 1.0F &&
        value.alpha >= 0.0F && value.alpha <= 1.0F;
}

Result<void> RegisterMetadata(
    Core::MetadataContext& context,
    void*) noexcept {
    auto badge = Aero::Describe<StatusBadge>(context);
    badge
        .Factory()
        .Property(
            StatusBadge::AccentProperty,
            Aero::PropertyOptions(DefaultAccent)
                .AffectsRender()
                .Validate(&IsValidAccent));
    return badge.Result();
}

} // namespace

Presentation::Color
StatusBadge::Accent() const noexcept {
    return GetValueOr(AccentProperty, DefaultAccent);
}

Base::Result<void> StatusBadge::SetAccent(
    Presentation::Color value) noexcept {
    if (!IsValidAccent(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "StatusBadge accent is invalid");
    }
    return SetValue(AccentProperty, value);
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
    return modules.Add(MakeStatusBadgeModuleManifest());
}

} // namespace Aero::Samples::ControlGallery
