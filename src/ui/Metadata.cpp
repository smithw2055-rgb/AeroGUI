#include "UiMetadata.hpp"
#include "ThemeCompatibilityTypes.hpp"

#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/Metadata/Describe.hpp>
#include <Aero/Core/Metadata/ValueConversion.hpp>
#include <Aero/Input/Commands.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Input/Navigation.hpp>
#include <Aero/Layout.hpp>
#include <Aero/ObjectTree.hpp>
#include <Aero/Rendering.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Style.hpp>
#include <Aero/Media/Transforms.hpp>

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace Aero::Detail {

using namespace Aero::Core;
using namespace Aero::Input;
using namespace Aero::Media;
using namespace Aero::Data;
using namespace Aero::Detail::Animation;
namespace {
#include "metadata/Support.inl"
#include "metadata/Resources.inl"
#include "metadata/Styling.inl"
#include "metadata/Input.inl"
#include "metadata/Media.inl"
#include "metadata/Animation.inl"
#include "metadata/Elements.inl"
} // namespace

Base::Result<void> Detail::PopulateUiMetadata(
    Core::MetadataContext& context) noexcept {
    Base::Result<void> status;
    status = PopulateUiResources(context);
    if (!status) return status.GetStatus();
    status = PopulateUiStyling(context);
    if (!status) return status.GetStatus();
    status = PopulateUiInput(context);
    if (!status) return status.GetStatus();
    status = PopulateUiMedia(context);
    if (!status) return status.GetStatus();
    status = PopulateUiAnimation(context);
    if (!status) return status.GetStatus();
    status = PopulateUiElements(context);
    if (!status) return status.GetStatus();
    return {};
}

} // namespace Aero::Detail
