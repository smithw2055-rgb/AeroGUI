#include "gui/meta/MetadataState.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/media/AnimationEngine.hpp"

#include <Aero/Media/Animation.hpp>
#include <AeroApp/Application.hpp>
#include <Aero/Controls.hpp> 
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Data/SortDescription.hpp>
#include <Aero/Media/ArcSegment.hpp>
#include <Aero/Media/CombinedGeometry.hpp>
#include <AeroApp/Window.hpp>

namespace Aero {
namespace {

template<class T, class TPopulate>
Base::Result<void> RegisterEnum(
    Meta::Registration& context,
    Base::StringView name,
    TPopulate&& populate) noexcept {
    auto description = Meta::Register<T>(context, name);
    populate(description);
    return description.Result();
}

} // namespace

Base::Result<void> PopulateEnumMetadata(
    Meta::Registration& context) noexcept {
    using namespace Input;
    using namespace Media::Animation;
    using namespace Interactivity;
    using namespace Media;
    using namespace Shapes;
    using namespace Data;
    using namespace Controls;
    using namespace Controls::Primitives;
    // Prefer public Animation enums over Model aliases (AnimationEngine.hpp).
    using Media::Animation::FillBehavior;
    using Media::Animation::EasingMode;

    Base::Result<void> status;

#define AERO_REGISTER_ENUM(Type, Name, Values) \
    status = RegisterEnum<Type>(context, Name, \
        [](auto& description) { Values }); \
    if (!status) return status.GetStatus()

    AERO_REGISTER_ENUM(
        ShutdownMode,
        "ShutdownMode",
        description
            .Value("OnLastWindowClose", ShutdownMode::OnLastWindowClose)
            .Value("OnMainWindowClose", ShutdownMode::OnMainWindowClose)
            .Value("OnExplicitShutdown", ShutdownMode::OnExplicitShutdown););
    AERO_REGISTER_ENUM(
        WindowState,
        "WindowState",
        description
            .Value("Normal", WindowState::Normal)
            .Value("Minimized", WindowState::Minimized)
            .Value("Maximized", WindowState::Maximized););
    AERO_REGISTER_ENUM(
        WindowStyle,
        "WindowStyle",
        description
            .Value("None", WindowStyle::None)
            .Value("SingleBorderWindow", WindowStyle::SingleBorderWindow)
            .Value("ThreeDBorderWindow", WindowStyle::ThreeDBorderWindow)
            .Value("ToolWindow", WindowStyle::ToolWindow););
    AERO_REGISTER_ENUM(
        ResizeMode,
        "ResizeMode",
        description
            .Value("NoResize", ResizeMode::NoResize)
            .Value("CanMinimize", ResizeMode::CanMinimize)
            .Value("CanResize", ResizeMode::CanResize)
            .Value("CanResizeWithGrip", ResizeMode::CanResizeWithGrip););
    AERO_REGISTER_ENUM(
        SizeToContent,
        "SizeToContent",
        description
            .Value("Manual", SizeToContent::Manual)
            .Value("Width", SizeToContent::Width)
            .Value("Height", SizeToContent::Height)
            .Value("WidthAndHeight", SizeToContent::WidthAndHeight););

    AERO_REGISTER_ENUM(
        InputScope,
        "InputScope",
        description
            .Value("Default", InputScope::Default)
            .Value("Url", InputScope::Url)
            .Value("EmailSmtpAddress", InputScope::EmailSmtpAddress)
            .Value("Digits", InputScope::Digits)
            .Value("Number", InputScope::Number)
            .Value("Password", InputScope::Password)
            .Value("TelephoneNumber", InputScope::TelephoneNumber););
    AERO_REGISTER_ENUM(
        DragDropEffects,
        "DragDropEffects",
        description
            .Value("None", DragDropEffects::None)
            .Value("Copy", DragDropEffects::Copy)
            .Value("Move", DragDropEffects::Move)
            .Value("Link", DragDropEffects::Link)
            .Value("All", DragDropEffects::All););
    AERO_REGISTER_ENUM(
        CursorType,
        "CursorType",
        description
            .Value("None", CursorType::None)
            .Value("No", CursorType::No)
            .Value("Arrow", CursorType::Arrow)
            .Value("AppStarting", CursorType::AppStarting)
            .Value("Cross", CursorType::Cross)
            .Value("Help", CursorType::Help)
            .Value("IBeam", CursorType::IBeam)
            .Value("SizeAll", CursorType::SizeAll)
            .Value("SizeNESW", CursorType::SizeNESW)
            .Value("SizeNS", CursorType::SizeNS)
            .Value("SizeNWSE", CursorType::SizeNWSE)
            .Value("SizeWE", CursorType::SizeWE)
            .Value("UpArrow", CursorType::UpArrow)
            .Value("Wait", CursorType::Wait)
            .Value("Hand", CursorType::Hand)
            .Value("Pen", CursorType::Pen)
            .Value("ScrollNS", CursorType::ScrollNS)
            .Value("ScrollWE", CursorType::ScrollWE)
            .Value("ScrollAll", CursorType::ScrollAll)
            .Value("ScrollN", CursorType::ScrollN)
            .Value("ScrollS", CursorType::ScrollS)
            .Value("ScrollW", CursorType::ScrollW)
            .Value("ScrollE", CursorType::ScrollE)
            .Value("ScrollNW", CursorType::ScrollNW)
            .Value("ScrollNE", CursorType::ScrollNE)
            .Value("ScrollSW", CursorType::ScrollSW)
            .Value("ScrollSE", CursorType::ScrollSE)
            .Value("ArrowCD", CursorType::ArrowCD)
            .Value("Custom", CursorType::Custom););
    AERO_REGISTER_ENUM(
        KeyboardNavigationMode,
        "KeyboardNavigationMode",
        description
            .Value("Continue", KeyboardNavigationMode::Continue)
            .Value("Once", KeyboardNavigationMode::Once)
            .Value("Cycle", KeyboardNavigationMode::Cycle)
            .Value("None", KeyboardNavigationMode::None)
            .Value("Contained", KeyboardNavigationMode::Contained)
            .Value("Local", KeyboardNavigationMode::Local););

    AERO_REGISTER_ENUM(
        FillBehavior,
        "FillBehavior",
        description
            .Value("HoldEnd", FillBehavior::HoldEnd)
            .Value("Stop", FillBehavior::Stop););
    AERO_REGISTER_ENUM(
        EasingMode,
        "EasingMode",
        description
            .Value("EaseOut", EasingMode::EaseOut)
            .Value("EaseIn", EasingMode::EaseIn)
            .Value("EaseInOut", EasingMode::EaseInOut););
    AERO_REGISTER_ENUM(
        ControlStoryboardAction::Option,
        "ControlStoryboardOption",
        description
            .Value("Play", ControlStoryboardAction::Option::Play)
            .Value("Stop", ControlStoryboardAction::Option::Stop)
            .Value("TogglePlayPause", ControlStoryboardAction::Option::TogglePlayPause)
            .Value("Pause", ControlStoryboardAction::Option::Pause)
            .Value("Resume", ControlStoryboardAction::Option::Resume)
            .Value("SkipToFill", ControlStoryboardAction::Option::SkipToFill););
    AERO_REGISTER_ENUM(
        ComparisonCondition::Operator,
        "ComparisonConditionOperator",
        description
            .Value("Equal", ComparisonCondition::Operator::Equal)
            .Value("NotEqual", ComparisonCondition::Operator::NotEqual)
            .Value("LessThan", ComparisonCondition::Operator::LessThan)
            .Value("LessThanOrEqual", ComparisonCondition::Operator::LessThanOrEqual)
            .Value("GreaterThan", ComparisonCondition::Operator::GreaterThan)
            .Value("GreaterThanOrEqual", ComparisonCondition::Operator::GreaterThanOrEqual););
    AERO_REGISTER_ENUM(
        ConditionalExpression::ForwardChaining,
        "ForwardChaining",
        description
            .Value("And", ConditionalExpression::ForwardChaining::And)
            .Value("Or", ConditionalExpression::ForwardChaining::Or););

    AERO_REGISTER_ENUM(
        HorizontalAlignment,
        "HorizontalAlignment",
        description
            .Value("Stretch", HorizontalAlignment::Stretch)
            .Value("Left", HorizontalAlignment::Left)
            .Value("Center", HorizontalAlignment::Center)
            .Value("Right", HorizontalAlignment::Right););
    AERO_REGISTER_ENUM(
        VerticalAlignment,
        "VerticalAlignment",
        description
            .Value("Stretch", VerticalAlignment::Stretch)
            .Value("Top", VerticalAlignment::Top)
            .Value("Center", VerticalAlignment::Center)
            .Value("Bottom", VerticalAlignment::Bottom););
    AERO_REGISTER_ENUM(
        Visibility,
        "Visibility",
        description
            .Value("Visible", Visibility::Visible)
            .Value("Hidden", Visibility::Hidden)
            .Value("Collapsed", Visibility::Collapsed););
    AERO_REGISTER_ENUM(
        BlendMode,
        "BlendMode",
        description
            .Value("Normal", BlendMode::Normal)
            .Value("Multiply", BlendMode::Multiply)
            .Value("Screen", BlendMode::Screen)
            .Value("Additive", BlendMode::Additive););
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
        FillRule,
        "FillRule",
        description
            .Value("EvenOdd", FillRule::EvenOdd)
            .Value("Nonzero", FillRule::Nonzero););
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
    AERO_REGISTER_ENUM(
        ListSortDirection,
        "ListSortDirection",
        description
            .Value("Ascending", ListSortDirection::Ascending)
            .Value("Descending", ListSortDirection::Descending););

    AERO_REGISTER_ENUM(
        TextWrapping,
        "TextWrapping",
        description
            .Value("NoWrap", TextWrapping::NoWrap)
            .Value("Wrap", TextWrapping::Wrap)
            .Value("WrapWithOverflow", TextWrapping::WrapWithOverflow););
    AERO_REGISTER_ENUM(
        TextTrimming,
        "TextTrimming",
        description
            .Value("None", TextTrimming::None)
            .Value("CharacterEllipsis", TextTrimming::CharacterEllipsis)
            .Value("WordEllipsis", TextTrimming::WordEllipsis););
    AERO_REGISTER_ENUM(
        TextAlignment,
        "TextAlignment",
        description
            .Value("Left", TextAlignment::Left)
            .Value("Center", TextAlignment::Center)
            .Value("Right", TextAlignment::Right)
            .Value("Justify", TextAlignment::Justify););
    AERO_REGISTER_ENUM(
        FlowDirection,
        "FlowDirection",
        description
            .Value("LeftToRight", FlowDirection::LeftToRight)
            .Value("RightToLeft", FlowDirection::RightToLeft););
    AERO_REGISTER_ENUM(
        FontStyle,
        "FontStyle",
        description
            .Value("Normal", FontStyle::Normal)
            .Value("Italic", FontStyle::Italic)
            .Value("Oblique", FontStyle::Oblique););
    AERO_REGISTER_ENUM(
        FontWeight,
        "FontWeight",
        description
            .Value("Normal", FontWeight::Normal)
            .Value("SemiBold", FontWeight::SemiBold)
            .Value("Bold", FontWeight::Bold)
            .Value("Regular", FontWeight::Regular););
    AERO_REGISTER_ENUM(
        TextDecorations,
        "TextDecorations",
        description
            .Value("None", TextDecorations::None)
            .Value("Underline", TextDecorations::Underline););
    AERO_REGISTER_ENUM(
        Orientation,
        "Orientation",
        description
            .Value("Horizontal", Orientation::Horizontal)
            .Value("Vertical", Orientation::Vertical););
    AERO_REGISTER_ENUM(
        Dock,
        "Dock",
        description
            .Value("Left", Dock::Left)
            .Value("Top", Dock::Top)
            .Value("Right", Dock::Right)
            .Value("Bottom", Dock::Bottom););
    AERO_REGISTER_ENUM(
        MenuItemRole,
        "MenuItemRole",
        description
            .Value("TopLevelItem", MenuItemRole::TopLevelItem)
            .Value("TopLevelHeader", MenuItemRole::TopLevelHeader)
            .Value("SubmenuItem", MenuItemRole::SubmenuItem)
            .Value("SubmenuHeader", MenuItemRole::SubmenuHeader););
    AERO_REGISTER_ENUM(
        ClickMode,
        "ClickMode",
        description
            .Value("Release", ClickMode::Release)
            .Value("Press", ClickMode::Press)
            .Value("Hover", ClickMode::Hover););
    AERO_REGISTER_ENUM(
        TickPlacement,
        "TickPlacement",
        description
            .Value("None", TickPlacement::None)
            .Value("TopLeft", TickPlacement::TopLeft)
            .Value("BottomRight", TickPlacement::BottomRight)
            .Value("Both", TickPlacement::Both););
    AERO_REGISTER_ENUM(
        TickBarPlacement,
        "TickBarPlacement",
        description
            .Value("Top", TickBarPlacement::Top)
            .Value("Bottom", TickBarPlacement::Bottom)
            .Value("Left", TickBarPlacement::Left)
            .Value("Right", TickBarPlacement::Right););
    AERO_REGISTER_ENUM(
        ScrollBarVisibility,
        "ScrollBarVisibility",
        description
            .Value("Disabled", ScrollBarVisibility::Disabled)
            .Value("Auto", ScrollBarVisibility::Auto)
            .Value("Hidden", ScrollBarVisibility::Hidden)
            .Value("Visible", ScrollBarVisibility::Visible););
    AERO_REGISTER_ENUM(
        PanningMode,
        "PanningMode",
        description
            .Value("None", PanningMode::None)
            .Value("HorizontalOnly", PanningMode::HorizontalOnly)
            .Value("VerticalOnly", PanningMode::VerticalOnly)
            .Value("Both", PanningMode::Both)
            .Value("HorizontalFirst", PanningMode::HorizontalFirst)
            .Value("VerticalFirst", PanningMode::VerticalFirst););
    AERO_REGISTER_ENUM(
        GridResizeDirection,
        "GridResizeDirection",
        description
            .Value("Auto", GridResizeDirection::Auto)
            .Value("Columns", GridResizeDirection::Columns)
            .Value("Rows", GridResizeDirection::Rows););
    AERO_REGISTER_ENUM(
        GridResizeBehavior,
        "GridResizeBehavior",
        description
            .Value("BasedOnAlignment", GridResizeBehavior::BasedOnAlignment)
            .Value("CurrentAndNext", GridResizeBehavior::CurrentAndNext)
            .Value("PreviousAndCurrent", GridResizeBehavior::PreviousAndCurrent)
            .Value("PreviousAndNext", GridResizeBehavior::PreviousAndNext););
    AERO_REGISTER_ENUM(
        SelectionMode,
        "SelectionMode",
        description
            .Value("Single", SelectionMode::Single)
            .Value("Multiple", SelectionMode::Multiple)
            .Value("Extended", SelectionMode::Extended););
    AERO_REGISTER_ENUM(
        ExpandDirection,
        "ExpandDirection",
        description
            .Value("Down", ExpandDirection::Down)
            .Value("Up", ExpandDirection::Up)
            .Value("Left", ExpandDirection::Left)
            .Value("Right", ExpandDirection::Right););
    AERO_REGISTER_ENUM(
        PlacementMode,
        "PlacementMode",
        description
            .Value("Bottom", PlacementMode::Bottom)
            .Value("Top", PlacementMode::Top)
            .Value("Left", PlacementMode::Left)
            .Value("Right", PlacementMode::Right)
            .Value("Center", PlacementMode::Center)
            .Value("Mouse", PlacementMode::Mouse););
    AERO_REGISTER_ENUM(
        PopupAnimation,
        "PopupAnimation",
        description
            .Value("None", PopupAnimation::None)
            .Value("Fade", PopupAnimation::Fade)
            .Value("Slide", PopupAnimation::Slide)
            .Value("Scroll", PopupAnimation::Scroll););
    AERO_REGISTER_ENUM(
        GridViewColumnHeaderRole,
        "GridViewColumnHeaderRole",
        description
            .Value("Normal", GridViewColumnHeaderRole::Normal)
            .Value("Floating", GridViewColumnHeaderRole::Floating)
            .Value("Padding", GridViewColumnHeaderRole::Padding););
    AERO_REGISTER_ENUM(
        ScrollUnit,
        "ScrollUnit",
        description
            .Value("Item", ScrollUnit::Item)
            .Value("Pixel", ScrollUnit::Pixel););
    AERO_REGISTER_ENUM(
        VirtualizationMode,
        "VirtualizationMode",
        description
            .Value("Standard", VirtualizationMode::Standard)
            .Value("Recycling", VirtualizationMode::Recycling););
    AERO_REGISTER_ENUM(
        VirtualizationCacheLengthUnit,
        "VirtualizationCacheLengthUnit",
        description
            .Value("Pixel", VirtualizationCacheLengthUnit::Pixel)
            .Value("Item", VirtualizationCacheLengthUnit::Item)
            .Value("Page", VirtualizationCacheLengthUnit::Page););

#undef AERO_REGISTER_ENUM
    return {};
}

} // namespace Aero
