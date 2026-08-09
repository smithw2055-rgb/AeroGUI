#include "Metadata.hpp"
#include <Aero/Documents.hpp>
#include "gui/MetadataRuntime.hpp"
#include "gui/PropertyRuntime.hpp"
#include "gui/FreezableRuntime.hpp"
#include "gui/ElementRuntime.hpp"
#include "gui/RoutedEventRuntime.hpp"
#include "gui/InputRuntime.hpp"
#include "gui/LayoutRuntime.hpp"
#include "gui/BindingRuntime.hpp"
#include "gui/AnimationRuntime.hpp"
#include "gui/StyleRuntime.hpp"
#include "gui/controls/ControlRuntime.hpp"
#include "gui/controls/ItemsRuntime.hpp"
#include "gui/controls/TemplateRuntime.hpp"
#include "gui/MetadataRuntime.hpp"
#include "gui/PropertyRuntime.hpp"
#include "gui/FreezableRuntime.hpp"
#include "gui/ElementRuntime.hpp"
#include "gui/RoutedEventRuntime.hpp"
#include "gui/InputRuntime.hpp"
#include "gui/LayoutRuntime.hpp"
#include "gui/BindingRuntime.hpp"
#include "gui/AnimationRuntime.hpp"
#include "gui/StyleRuntime.hpp"
#include "gui/media/AnimationRuntime.hpp"
#include "gui/media/BrushRuntime.hpp"
#include "gui/media/EffectRuntime.hpp"
#include "gui/media/TransformRuntime.hpp"

#include <Aero/Controls.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/PasswordBox.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Value.hpp>
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
