#include "Metadata.hpp"
#include <Aero/Documents.hpp>

#include <Aero/Controls/Primitives.hpp>
#include <Aero/Controls/Standard.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Controls/Text.hpp>
#include <Aero/Styling.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Core/Metadata/Describe.hpp>
#include <Aero/Core/Metadata/ValueConversion.hpp>

#include "PathServicesAccess.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Aero::Controls {
    using namespace Aero::Core;
    using Aero::Shapes::Shape;
    using Aero::Shapes::Rectangle;
    using Aero::Shapes::Ellipse;
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
