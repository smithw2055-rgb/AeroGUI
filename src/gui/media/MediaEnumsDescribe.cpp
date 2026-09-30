#include "gui/core/EnumRegistration.hpp"

#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Geometries.hpp>

namespace Aero {

Base::Result<void> PopulateMediaEnums(
    Meta::Registration& context) noexcept {
    using namespace Media;

    Base::Result<void> status;

    AERO_REGISTER_ENUM(
        Stretch,
        "Stretch",
        description
            .Value("None", Stretch::None)
            .Value("Fill", Stretch::Fill)
            .Value("Uniform", Stretch::Uniform)
            .Value("UniformToFill", Stretch::UniformToFill););
    AERO_REGISTER_ENUM(
        StretchDirection,
        "StretchDirection",
        description
            .Value("UpOnly", StretchDirection::UpOnly)
            .Value("DownOnly", StretchDirection::DownOnly)
            .Value("Both", StretchDirection::Both););
    AERO_REGISTER_ENUM(
        MediaState,
        "MediaState",
        description
            .Value("Manual", MediaState::Manual)
            .Value("Play", MediaState::Play)
            .Value("Close", MediaState::Close)
            .Value("Pause", MediaState::Pause)
            .Value("Stop", MediaState::Stop););
    AERO_REGISTER_ENUM(
        TileMode,
        "TileMode",
        description
            .Value("None", TileMode::None)
            .Value("Tile", TileMode::Tile)
            .Value("FlipX", TileMode::FlipX)
            .Value("FlipY", TileMode::FlipY)
            .Value("FlipXY", TileMode::FlipXY););
    AERO_REGISTER_ENUM(
        BrushMappingMode,
        "BrushMappingMode",
        description
            .Value("RelativeToBoundingBox", BrushMappingMode::RelativeToBoundingBox)
            .Value("Absolute", BrushMappingMode::Absolute););
    AERO_REGISTER_ENUM(
        GradientSpreadMethod,
        "GradientSpreadMethod",
        description
            .Value("Pad", GradientSpreadMethod::Pad)
            .Value("Reflect", GradientSpreadMethod::Reflect)
            .Value("Repeat", GradientSpreadMethod::Repeat););
    AERO_REGISTER_ENUM(
        PenLineJoin,
        "PenLineJoin",
        description
            .Value("Miter", PenLineJoin::Miter)
            .Value("Bevel", PenLineJoin::Bevel)
            .Value("Round", PenLineJoin::Round););
    AERO_REGISTER_ENUM(
        PenLineCap,
        "PenLineCap",
        description
            .Value("Flat", PenLineCap::Flat)
            .Value("Square", PenLineCap::Square)
            .Value("Round", PenLineCap::Round)
            .Value("Triangle", PenLineCap::Triangle););
    AERO_REGISTER_ENUM(
        SweepDirection,
        "SweepDirection",
        description
            .Value("Counterclockwise", SweepDirection::Counterclockwise)
            .Value("Clockwise", SweepDirection::Clockwise););
    AERO_REGISTER_ENUM(
        GeometryCombineMode,
        "GeometryCombineMode",
        description
            .Value("Union", GeometryCombineMode::Union)
            .Value("Intersect", GeometryCombineMode::Intersect)
            .Value("Xor", GeometryCombineMode::Xor)
            .Value("Exclude", GeometryCombineMode::Exclude););

#undef AERO_REGISTER_ENUM
    return {};
}

} // namespace Aero
