#include "gui/core/EnumRegistration.hpp"

#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Geometries.hpp>

namespace Aero {

void PopulateMediaEnums(
    Meta::Registration& context) noexcept {
    using namespace Media;

    AERO_AUTO_REGISTER_ENUM(Stretch);
    AERO_AUTO_REGISTER_ENUM(StretchDirection);
    AERO_AUTO_REGISTER_ENUM(MediaState);
    AERO_AUTO_REGISTER_ENUM(TileMode);
    AERO_AUTO_REGISTER_ENUM(BrushMappingMode);
    AERO_AUTO_REGISTER_ENUM(GradientSpreadMethod);
    AERO_AUTO_REGISTER_ENUM(PenLineJoin);
    AERO_AUTO_REGISTER_ENUM(PenLineCap);
    AERO_AUTO_REGISTER_ENUM(SweepDirection);
    AERO_AUTO_REGISTER_ENUM(GeometryCombineMode);
}

} // namespace Aero
