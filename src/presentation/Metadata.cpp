#include <Aero/Presentation/Metadata.hpp>

#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/Metadata/Describe.hpp>
#include <Aero/Core/Metadata/ValueConversion.hpp>
#include <Aero/Presentation/Commands.hpp>
#include <Aero/Presentation/AnimationXaml.hpp>
#include <Aero/Presentation/Binding.hpp>
#include <Aero/Presentation/Brushes.hpp>
#include <Aero/Presentation/Effects.hpp>
#include <Aero/Presentation/Images.hpp>
#include <Aero/Presentation/Input.hpp>
#include <Aero/Presentation/Layout.hpp>
#include <Aero/Presentation/ObjectTree.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Presentation/Resources.hpp>
#include <Aero/Presentation/Style.hpp>
#include <Aero/Presentation/Transforms.hpp>

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace Aero::Presentation {

using namespace Aero::Core;
namespace {
#include "metadata/Support.inl"
#include "metadata/Resources.inl"
#include "metadata/Styling.inl"
#include "metadata/Input.inl"
#include "metadata/Media.inl"
#include "metadata/Animation.inl"
#include "metadata/Elements.inl"
} // namespace

Base::Result<void> Detail::PopulatePresentationMetadata(
    Core::MetadataContext& context) noexcept {
    Base::Result<void> status;
    status = PopulatePresentationResources(context);
    if (!status) return status.GetStatus();
    status = PopulatePresentationStyling(context);
    if (!status) return status.GetStatus();
    status = PopulatePresentationInput(context);
    if (!status) return status.GetStatus();
    status = PopulatePresentationMedia(context);
    if (!status) return status.GetStatus();
    status = PopulatePresentationAnimation(context);
    if (!status) return status.GetStatus();
    status = PopulatePresentationElements(context);
    if (!status) return status.GetStatus();
    return {};
}

} // namespace Aero::Presentation
