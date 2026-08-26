#include "Metadata.hpp"
#include <Aero/Documents.hpp>
#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/controls/State.hpp"
#include "gui/controls/State.hpp"
#include "gui/templates/TemplateState.hpp"
#include "gui/media/MediaState.hpp"

#include <Aero/Controls.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/PasswordBox.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/HierarchicalDataTemplate.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Value.hpp>
#include <Aero/TryCast.hpp>
#include "gui/text/EditableText.hpp"


#include <algorithm>
#include <cmath>
#include <utility>

namespace Aero::Controls {
    using namespace Aero::Meta;
using namespace Aero::Threading;
    using namespace Aero::Controls::Primitives;
    using Aero::Shapes::Shape;
    using Aero::Shapes::Rectangle;
    using Aero::Shapes::Ellipse;
    using Aero::Shapes::Path;
    using Aero::Shapes::Line;
    using Aero::Shapes::Polygon;
    using Aero::Shapes::Polyline;
    using Aero::Shapes::FillRule;
    using Aero::Shapes::PenLineJoin;
    using Aero::Shapes::PenLineCap;
    using Aero::TryCastToInterface;
namespace {
#include "metadata/Support.inl"
#include "metadata/Values.inl"
#include "metadata/Templates.inl"
#include "metadata/Primitives.inl"
#include "metadata/Items.inl"
#include "metadata/Panels.inl"
#include "metadata/TextMedia.inl"
} // namespace

Base::Result<void> PopulateControlsMetadata(
    ::Aero::Meta::Registration& context) noexcept {
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
