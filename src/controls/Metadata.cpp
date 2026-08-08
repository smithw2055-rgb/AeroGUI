#include "Metadata.hpp"
#include <Aero/Documents.hpp>
#include "gui/MetadataInternal.hpp"
#include "gui/PropertyInternal.hpp"
#include "gui/FreezableInternal.hpp"
#include "gui/ElementInternal.hpp"
#include "gui/RoutedEventInternal.hpp"
#include "gui/InputInternal.hpp"
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/AnimationInternal.hpp"
#include "gui/StyleInternal.hpp"
#include "controls/ControlInternal.hpp"
#include "controls/ItemsInternal.hpp"
#include "controls/TemplateInternal.hpp"
#include "gui/MetadataInternal.hpp"
#include "gui/PropertyInternal.hpp"
#include "gui/FreezableInternal.hpp"
#include "gui/ElementInternal.hpp"
#include "gui/RoutedEventInternal.hpp"
#include "gui/InputInternal.hpp"
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/AnimationInternal.hpp"
#include "gui/StyleInternal.hpp"
#include "media/AnimationInternal.hpp"
#include "media/BrushInternal.hpp"
#include "media/EffectInternal.hpp"
#include "media/TransformInternal.hpp"

#include <Aero/Controls.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Gui/Text.hpp>
#include <Aero/Gui/ControlTemplate.hpp>
#include <Aero/Gui/Brush.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Value.hpp>
#include "../text/EditableText.hpp"


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
    using Aero::Shapes::PenLineJoin;
    using Aero::Shapes::PenLineCap;
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
