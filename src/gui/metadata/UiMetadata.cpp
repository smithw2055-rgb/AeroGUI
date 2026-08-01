#include "gui/metadata/UiMetadata.hpp"
#include "gui/styling/ThemeCompatibilityTypes.hpp"

#include <Aero/Meta/MetadataRuntime.hpp>
#include <Aero/Meta/Describe.hpp>
#include <Aero/Meta/ValueConversion.hpp>
#include <Aero/Input.hpp>
#include <Aero/Animation.hpp>
#include <Aero/Data.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Layout.hpp>
#include "gui/tree/ObjectTree.hpp"
#include <Aero/FrameworkElement.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Styling.hpp>
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
#include "gui/registration/Support.inl"
#include "gui/registration/Resources.inl"
#include "gui/registration/Styling.inl"
#include "gui/registration/Input.inl"
#include "gui/registration/Media.inl"
#include "gui/registration/Animation.inl"
#include "gui/registration/Elements.inl"
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
