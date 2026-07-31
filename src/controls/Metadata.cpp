#include <Aero/Controls/Metadata.hpp>
#include <Aero/Documents/Documents.hpp>

#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Bars.hpp>
#include <Aero/Controls/ContentControls.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/ListView.hpp>
#include <Aero/Controls/Menus.hpp>
#include <Aero/Controls/Scroll.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Controls/Shapes.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Controls/Trees.hpp>
#include <Aero/Controls/Virtualization.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Core/Metadata/Describe.hpp>
#include <Aero/Core/Metadata/ValueConversion.hpp>
#include <Aero/Style.hpp>

#include "PathServicesAccess.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Aero::Controls {
    using namespace Aero::Core;
namespace {
#include "metadata/Support.inl"
#include "metadata/Values.inl"
#include "metadata/Templates.inl"
#include "metadata/Primitives.inl"
#include "metadata/Items.inl"
#include "metadata/Panels.inl"
#include "metadata/TextMedia.inl"
} // namespace

Base::Result<void> Detail::PopulateControlsMetadata(
    Core::MetadataContext& context) noexcept {
    Base::Result<void> status;
    status = PopulateControlsValues(context);
    if (!status) return status.GetStatus();
    status = PopulateControlsTemplates(context);
    if (!status) return status.GetStatus();
    status = PopulateControlsPrimitives(context);
    if (!status) return status.GetStatus();
    status = PopulateControlsItems(context);
    if (!status) return status.GetStatus();
    status = PopulateControlsPanels(context);
    if (!status) return status.GetStatus();
    status = PopulateControlsTextMedia(context);
    if (!status) return status.GetStatus();
    return {};
}

} // namespace Aero::Controls
