// Ordered metadata installer. Describe bodies live next to each type.

#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/MediaActions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/ApplicationCommands.hpp>
#include <Aero/InputGesture.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/Animatable.hpp>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace Aero::MetadataSupport {
using namespace ::Aero::Meta;
using Media::FontFamily;
namespace {

Base::Result<Value> ConvertFontFamilyText(
    TypeId targetType, Base::StringView text, void*) noexcept {
    if (targetType != FontFamily::StaticTypeId()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "FontFamily text conversion received an invalid target");
    }
    Base::Result<Base::Ref<FontFamily>> family = Base::MakeRef<FontFamily>();
    if (!family) return family.GetStatus();
    family.Value()->SetSource(text);
    return Value::FromObject(FontFamily::StaticTypeId(),
        Base::Ref<Base::Object>(std::move(family).Value()));
}
} // namespace
} // namespace Aero::MetadataSupport

namespace Aero::Media {

AERO_DESCRIBE(FontFamily) {
    using namespace Aero::Meta;
    Register<FontFamily>(context)
            .Property("Source", &FontFamily::GetSource, &FontFamily::SetSource, PropertyFlags::Structural)
            .Content(MakeMemberId(FontFamily::StaticTypeId(), MemberKind::Property, "Source"))
            .TextConverter(&::Aero::MetadataSupport::ConvertFontFamilyText)
            .Factory();
}

} // namespace Aero::Media


namespace Aero {

Base::Result<void> PopulateUiResources(
    ::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    DescribeHook<::Aero::ResourceDictionary>::Run(context);
    DescribeHook<::Aero::Media::Geometry>::Run(context);
    DescribeHook<::Aero::Media::DashStyle>::Run(context);
    DescribeHook<::Aero::Media::Pen>::Run(context);
    DescribeHook<::Aero::Media::StreamGeometry>::Run(context);
    DescribeHook<::Aero::Media::PathSegment>::Run(context);
    DescribeHook<::Aero::Media::LineSegment>::Run(context);
    DescribeHook<::Aero::Media::PathFigure>::Run(context);
    DescribeHook<::Aero::Media::PathGeometry>::Run(context);
    DescribeHook<::Aero::Media::BezierSegment>::Run(context);
    DescribeHook<::Aero::Media::QuadraticBezierSegment>::Run(context);
    DescribeHook<::Aero::Media::ArcSegment>::Run(context);
    DescribeHook<::Aero::Media::PolyLineSegment>::Run(context);
    DescribeHook<::Aero::Media::PolyBezierSegment>::Run(context);
    DescribeHook<::Aero::Media::PolyQuadraticBezierSegment>::Run(context);
    DescribeHook<::Aero::Media::LineGeometry>::Run(context);
    DescribeHook<::Aero::Media::RectangleGeometry>::Run(context);
    DescribeHook<::Aero::Media::EllipseGeometry>::Run(context);
    DescribeHook<::Aero::Media::GeometryGroup>::Run(context);
    DescribeHook<::Aero::Media::CombinedGeometry>::Run(context);
    DescribeHook<::Aero::Media::FontFamily>::Run(context);
    return {};
}

} // namespace Aero
