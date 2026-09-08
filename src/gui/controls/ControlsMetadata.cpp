#include "Metadata.hpp"
#include <Aero/Documents.hpp>
#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/state/ElementTree.hpp"
#include "gui/core/state/LayoutEngine.hpp"
#include "gui/core/state/FreezableState.hpp"
#include "gui/core/state/EffectiveValueEngine.hpp"
#include "gui/core/state/RoutedEvents.hpp"
#include "gui/core/state/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/controls/ItemsContainers.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include "gui/media/MediaHelpers.hpp"

#include <Aero/Controls.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/PasswordBox.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/HierarchicalDataTemplate.hpp>
#include <Aero/DataTemplateSelector.hpp>
#include <Aero/Data/CollectionView.hpp>
#include <Aero/Data/CollectionViewSource.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Value.hpp>
#include <Aero/TryCast.hpp>
#include "gui/text/EditableText.hpp"


#include <algorithm>
#include <cmath>
#include <limits>
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
// Grouped metadata units; include order preserves the original stable
// registration sequence (Foundation -> Widgets -> Layout).
#include "metadata/Metadata.Foundation.inl"
#include "metadata/Metadata.Widgets.inl"
#include "metadata/Metadata.Layout.inl"
} // namespace

Base::Result<void> PopulateControlsMetadata(
    ::Aero::Meta::Registration& context) noexcept {
    PopulateControlsValues(context);
    PopulateControlsTemplates(context);
    PopulateControlsPrimitives(context);
    PopulateControlsItems(context);
    PopulateControlsPanels(context);
    PopulateControlsTextMedia(context);
    return {};
}

} // namespace Aero::Controls
