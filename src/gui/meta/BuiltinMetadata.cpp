// Consolidated implementation. Keep sections ordered by dependency.

// ===== CoreMetadata =====

#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/state/ElementTree.hpp"
#include "gui/core/state/LayoutEngine.hpp"
#include "gui/core/state/FreezableState.hpp"
#include "gui/core/state/EffectiveValueEngine.hpp"
#include "gui/core/state/RoutedEvents.hpp"
#include "gui/core/state/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include <Aero/TextProperties.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include "gui/media/MediaHelpers.hpp"
#include "gui/meta/ElementsFill.hpp"
#include "gui/meta/RenderStateCallbacks.hpp"

#include <Aero/Meta.hpp>
#include <Aero/Value.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/DispatcherObject.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include "gui/meta/InputDevices.inl"

namespace Aero::Meta {
Base::Result<void> PopulateCoreMetadata(
    Meta::Registration& context) noexcept {

    Register<Base::Object>(context);

    Register<bool>(context)
        .TextConverter<&Base::ValueConversion::ConvertBoolean>();

    Register<::Aero::Nullable<bool>>(context)
        .TextConverter<&Base::ValueConversion::ConvertNullableBoolean>();

    Register<std::int8_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::int8_t>>();
    Register<std::int16_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::int16_t>>();
    Register<std::int32_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::int32_t>>();
    Register<std::int64_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::int64_t>>();
    Register<std::uint8_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::uint8_t>>();
    Register<std::uint16_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::uint16_t>>();
    Register<std::uint32_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::uint32_t>>();
    Register<std::uint64_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::uint64_t>>();

    Register<double>(context)
        .TextConverter<&Base::ValueConversion::ConvertDouble>();

    Register<Base::String>(context)
        .TextConverter<&Base::ValueConversion::ConvertString>();

    Register<Value>(context)
        .ValueSemantics();

    Register<TypeReference>(context);

    Register<Base::ResourceUri>(context)
        .ValueSemantics()
        .TextConverter<&Base::ValueConversion::ConvertResourceUri>();

    Register<Threading::DispatcherObject>(context, TypeFlags::Abstract);

    Register<DependencyObject>(context, TypeFlags::Abstract);

    return Meta::Register<Freezable>(
        context, TypeFlags::Abstract).Result();
}

} // namespace Aero::Meta


// ===== UiMetadata =====


#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/RoutedUICommand.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/MouseBinding.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/FocusManager.hpp>
#include <Aero/KeyBinding.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/ApplicationCommands.hpp>
#include <Aero/KeyGesture.hpp>
#include <Aero/InputGesture.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/EventTrigger.hpp>
#include <Aero/Media/Animation/MediaActions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/DashStyle.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/StreamGeometry.hpp>
#include <Aero/Media/PathSegment.hpp>
#include <Aero/Media/LineSegment.hpp>
#include <Aero/Media/PathFigure.hpp>
#include <Aero/Media/PathGeometry.hpp>
#include <Aero/Media/BezierSegment.hpp>
#include <Aero/Media/QuadraticBezierSegment.hpp>
#include <Aero/Media/ArcSegment.hpp>
#include <Aero/Media/PolyLineSegment.hpp>
#include <Aero/Media/PolyBezierSegment.hpp>
#include <Aero/Media/PolyQuadraticBezierSegment.hpp>
#include <Aero/Media/LineGeometry.hpp>
#include <Aero/Media/RectangleGeometry.hpp>
#include <Aero/Media/EllipseGeometry.hpp>
#include <Aero/Media/GeometryGroup.hpp>
#include <Aero/Media/CombinedGeometry.hpp>
#include <Aero/Collections.hpp>

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Input;
using namespace Aero::Media;
using namespace Aero::Data;
using namespace Aero::Media::Animation::Model;
namespace {
#include "gui/meta/Support.inl"
#include "gui/meta/Resources.inl"
#include "gui/meta/Styling.inl"
#include "gui/meta/Input.inl"
#include "gui/meta/Media.inl"
#include "gui/meta/Animation.inl"
#include "gui/meta/Elements.inl"
} // namespace

Base::Result<void> PopulateUiMetadata(
    ::Aero::Meta::Registration& context) noexcept {
    PopulateEnumMetadata(context);
    PopulateUiInput(context);
    // Media registers foundational value types such as Point. Resources author
    // Geometry dependency-property defaults that consume those values, so keep
    // Media ahead of Resources in the deterministic metadata bootstrap.
    PopulateUiMedia(context);
    PopulateUiResources(context);
    PopulateUiStyling(context);
    PopulateUiAnimation(context);
    PopulateUiElements(context);
    PopulateInputDevices(context);
    return {};
}

} // namespace Aero
