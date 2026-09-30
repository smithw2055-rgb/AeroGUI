// Ordered metadata installer. Describe bodies live next to each type.

#include "gui/core/Describe.hpp"
#include <Aero/Resources.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Fonts.hpp>
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
