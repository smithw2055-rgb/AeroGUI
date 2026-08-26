#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp"
#include "gui/media/AnimationEngine.hpp"

#include <Aero/Media/Animation.hpp>
#include <AeroApp/Application.hpp>
#include <Aero/Controls.hpp> 
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
    Base::Result<void> status;

#define AERO_REGISTER_ENUM(Type, Name, Values) \
    status = RegisterEnum<Type>(context, Name, \
        [](auto& description) { Values }); \
    if (!status) return status.GetStatus()

    AERO_REGISTER_ENUM(
        ::Aero::ShutdownMode,
        "ShutdownMode",
        description
            .Value("OnLastWindowClose", ::Aero::ShutdownMode::OnLastWindowClose)
            .Value("OnMainWindowClose", ::Aero::ShutdownMode::OnMainWindowClose)
            .Value("OnExplicitShutdown", ::Aero::ShutdownMode::OnExplicitShutdown););
    AERO_REGISTER_ENUM(
        ::Aero::WindowState,
        "WindowState",
        description
            .Value("Normal", ::Aero::WindowState::Normal)
            .Value("Minimized", ::Aero::WindowState::Minimized)
            .Value("Maximized", ::Aero::WindowState::Maximized););
    AERO_REGISTER_ENUM(
        ::Aero::WindowStyle,
        "WindowStyle",
        description
            .Value("None", ::Aero::WindowStyle::None)
            .Value("SingleBorderWindow", ::Aero::WindowStyle::SingleBorderWindow)
            .Value("ThreeDBorderWindow", ::Aero::WindowStyle::ThreeDBorderWindow)
            .Value("ToolWindow", ::Aero::WindowStyle::ToolWindow););
    AERO_REGISTER_ENUM(
        ::Aero::ResizeMode,
        "ResizeMode",
        description
            .Value("NoResize", ::Aero::ResizeMode::NoResize)
            .Value("CanMinimize", ::Aero::ResizeMode::CanMinimize)
            .Value("CanResize", ::Aero::ResizeMode::CanResize)
            .Value("CanResizeWithGrip", ::Aero::ResizeMode::CanResizeWithGrip););
    AERO_REGISTER_ENUM(
        ::Aero::SizeToContent,
        "SizeToContent",
        description
            .Value("Manual", ::Aero::SizeToContent::Manual)
            .Value("Width", ::Aero::SizeToContent::Width)
            .Value("Height", ::Aero::SizeToContent::Height)
            .Value("WidthAndHeight", ::Aero::SizeToContent::WidthAndHeight););

    AERO_REGISTER_ENUM(
        ::Aero::Input::InputScope,
        "InputScope",
        description
            .Value("Default", ::Aero::Input::InputScope::Default)
            .Value("Url", ::Aero::Input::InputScope::Url)
            .Value("EmailSmtpAddress", ::Aero::Input::InputScope::EmailSmtpAddress)
            .Value("Digits", ::Aero::Input::InputScope::Digits)
            .Value("Number", ::Aero::Input::InputScope::Number)
            .Value("Password", ::Aero::Input::InputScope::Password)
            .Value("TelephoneNumber", ::Aero::Input::InputScope::TelephoneNumber););
    AERO_REGISTER_ENUM(
        ::Aero::Input::DragDropEffects,
        "DragDropEffects",
        description
            .Value("None", ::Aero::Input::DragDropEffects::None)
            .Value("Copy", ::Aero::Input::DragDropEffects::Copy)
            .Value("Move", ::Aero::Input::DragDropEffects::Move)
            .Value("Link", ::Aero::Input::DragDropEffects::Link)
            .Value("All", ::Aero::Input::DragDropEffects::All););
    AERO_REGISTER_ENUM(
        ::Aero::Input::KeyboardNavigationMode,
        "KeyboardNavigationMode",
        description
            .Value("Continue", ::Aero::Input::KeyboardNavigationMode::Continue)
            .Value("Once", ::Aero::Input::KeyboardNavigationMode::Once)
            .Value("Cycle", ::Aero::Input::KeyboardNavigationMode::Cycle)
            .Value("None", ::Aero::Input::KeyboardNavigationMode::None)
            .Value("Contained", ::Aero::Input::KeyboardNavigationMode::Contained)
            .Value("Local", ::Aero::Input::KeyboardNavigationMode::Local););

    AERO_REGISTER_ENUM(
        ::Aero::Media::Animation::FillBehavior,
        "FillBehavior",
        description
            .Value("HoldEnd", ::Aero::Media::Animation::FillBehavior::HoldEnd)
            .Value("Stop", ::Aero::Media::Animation::FillBehavior::Stop););
    AERO_REGISTER_ENUM(
        ::Aero::Media::Animation::EasingMode,
        "EasingMode",
        description
            .Value("EaseOut", ::Aero::Media::Animation::EasingMode::EaseOut)
            .Value("EaseIn", ::Aero::Media::Animation::EasingMode::EaseIn)
            .Value("EaseInOut", ::Aero::Media::Animation::EasingMode::EaseInOut););
    AERO_REGISTER_ENUM(
        ::Aero::Media::Animation::ControlStoryboardAction::Option,
        "ControlStoryboardOption",
        description
            .Value("Play", ::Aero::Media::Animation::ControlStoryboardAction::Option::Play)
            .Value("Stop", ::Aero::Media::Animation::ControlStoryboardAction::Option::Stop)
            .Value("TogglePlayPause", ::Aero::Media::Animation::ControlStoryboardAction::Option::TogglePlayPause)
            .Value("Pause", ::Aero::Media::Animation::ControlStoryboardAction::Option::Pause)
            .Value("Resume", ::Aero::Media::Animation::ControlStoryboardAction::Option::Resume)
            .Value("SkipToFill", ::Aero::Media::Animation::ControlStoryboardAction::Option::SkipToFill););
    AERO_REGISTER_ENUM(
        ::Aero::Interactivity::ComparisonCondition::Operator,
        "ComparisonConditionOperator",
        description
            .Value("Equal", ::Aero::Interactivity::ComparisonCondition::Operator::Equal)
            .Value("NotEqual", ::Aero::Interactivity::ComparisonCondition::Operator::NotEqual)
            .Value("LessThan", ::Aero::Interactivity::ComparisonCondition::Operator::LessThan)
            .Value("LessThanOrEqual", ::Aero::Interactivity::ComparisonCondition::Operator::LessThanOrEqual)
            .Value("GreaterThan", ::Aero::Interactivity::ComparisonCondition::Operator::GreaterThan)
            .Value("GreaterThanOrEqual", ::Aero::Interactivity::ComparisonCondition::Operator::GreaterThanOrEqual););
    AERO_REGISTER_ENUM(
        ::Aero::Interactivity::ConditionalExpression::ForwardChaining,
        "ForwardChaining",
        description
            .Value("And", ::Aero::Interactivity::ConditionalExpression::ForwardChaining::And)
            .Value("Or", ::Aero::Interactivity::ConditionalExpression::ForwardChaining::Or););

    AERO_REGISTER_ENUM(
        ::Aero::HorizontalAlignment,
        "HorizontalAlignment",
        description
            .Value("Stretch", ::Aero::HorizontalAlignment::Stretch)
            .Value("Left", ::Aero::HorizontalAlignment::Left)
            .Value("Center", ::Aero::HorizontalAlignment::Center)
            .Value("Right", ::Aero::HorizontalAlignment::Right););
    AERO_REGISTER_ENUM(
        ::Aero::VerticalAlignment,
        "VerticalAlignment",
        description
            .Value("Stretch", ::Aero::VerticalAlignment::Stretch)
            .Value("Top", ::Aero::VerticalAlignment::Top)
            .Value("Center", ::Aero::VerticalAlignment::Center)
            .Value("Bottom", ::Aero::VerticalAlignment::Bottom););
    AERO_REGISTER_ENUM(
        ::Aero::Visibility,
        "Visibility",
        description
            .Value("Visible", ::Aero::Visibility::Visible)
            .Value("Hidden", ::Aero::Visibility::Hidden)
            .Value("Collapsed", ::Aero::Visibility::Collapsed););
    AERO_REGISTER_ENUM(
        ::Aero::BlendMode,
        "BlendMode",
        description
            .Value("Normal", ::Aero::BlendMode::Normal)
            .Value("Multiply", ::Aero::BlendMode::Multiply)
            .Value("Screen", ::Aero::BlendMode::Screen)
            .Value("Additive", ::Aero::BlendMode::Additive););
    AERO_REGISTER_ENUM(
        ::Aero::Media::Stretch,
        "Stretch",
        description
            .Value("None", ::Aero::Media::Stretch::None)
            .Value("Fill", ::Aero::Media::Stretch::Fill)
            .Value("Uniform", ::Aero::Media::Stretch::Uniform)
            .Value("UniformToFill", ::Aero::Media::Stretch::UniformToFill););
    AERO_REGISTER_ENUM(
        ::Aero::Media::StretchDirection,
        "StretchDirection",
        description
            .Value("UpOnly", ::Aero::Media::StretchDirection::UpOnly)
            .Value("DownOnly", ::Aero::Media::StretchDirection::DownOnly)
            .Value("Both", ::Aero::Media::StretchDirection::Both););
    AERO_REGISTER_ENUM(
        ::Aero::Media::MediaState,
        "MediaState",
        description
            .Value("Manual", ::Aero::Media::MediaState::Manual)
            .Value("Play", ::Aero::Media::MediaState::Play)
            .Value("Close", ::Aero::Media::MediaState::Close)
            .Value("Pause", ::Aero::Media::MediaState::Pause)
            .Value("Stop", ::Aero::Media::MediaState::Stop););
    AERO_REGISTER_ENUM(
        ::Aero::Media::TileMode,
        "TileMode",
        description
            .Value("None", ::Aero::Media::TileMode::None)
            .Value("Tile", ::Aero::Media::TileMode::Tile)
            .Value("FlipX", ::Aero::Media::TileMode::FlipX)
            .Value("FlipY", ::Aero::Media::TileMode::FlipY)
            .Value("FlipXY", ::Aero::Media::TileMode::FlipXY););
    AERO_REGISTER_ENUM(
        ::Aero::Media::BrushMappingMode,
        "BrushMappingMode",
        description
            .Value("RelativeToBoundingBox", ::Aero::Media::BrushMappingMode::RelativeToBoundingBox)
            .Value("Absolute", ::Aero::Media::BrushMappingMode::Absolute););
    AERO_REGISTER_ENUM(
        ::Aero::Media::GradientSpreadMethod,
        "GradientSpreadMethod",
        description
            .Value("Pad", ::Aero::Media::GradientSpreadMethod::Pad)
            .Value("Reflect", ::Aero::Media::GradientSpreadMethod::Reflect)
            .Value("Repeat", ::Aero::Media::GradientSpreadMethod::Repeat););
    AERO_REGISTER_ENUM(
        ::Aero::Shapes::FillRule,
        "FillRule",
        description
            .Value("EvenOdd", ::Aero::Shapes::FillRule::EvenOdd)
            .Value("Nonzero", ::Aero::Shapes::FillRule::Nonzero););
    AERO_REGISTER_ENUM(
        ::Aero::Shapes::PenLineJoin,
        "PenLineJoin",
        description
            .Value("Miter", ::Aero::Shapes::PenLineJoin::Miter)
            .Value("Bevel", ::Aero::Shapes::PenLineJoin::Bevel)
            .Value("Round", ::Aero::Shapes::PenLineJoin::Round););
    AERO_REGISTER_ENUM(
        ::Aero::Shapes::PenLineCap,
        "PenLineCap",
        description
            .Value("Flat", ::Aero::Shapes::PenLineCap::Flat)
            .Value("Square", ::Aero::Shapes::PenLineCap::Square)
            .Value("Round", ::Aero::Shapes::PenLineCap::Round)
            .Value("Triangle", ::Aero::Shapes::PenLineCap::Triangle););
    AERO_REGISTER_ENUM(
        ::Aero::Data::ListSortDirection,
        "ListSortDirection",
        description
            .Value("Ascending", ::Aero::Data::ListSortDirection::Ascending)
            .Value("Descending", ::Aero::Data::ListSortDirection::Descending););

    AERO_REGISTER_ENUM(
        ::Aero::TextWrapping,
        "TextWrapping",
        description
            .Value("NoWrap", ::Aero::TextWrapping::NoWrap)
            .Value("Wrap", ::Aero::TextWrapping::Wrap)
            .Value("WrapWithOverflow", ::Aero::TextWrapping::WrapWithOverflow););
    AERO_REGISTER_ENUM(
        ::Aero::TextTrimming,
        "TextTrimming",
        description
            .Value("None", ::Aero::TextTrimming::None)
            .Value("CharacterEllipsis", ::Aero::TextTrimming::CharacterEllipsis)
            .Value("WordEllipsis", ::Aero::TextTrimming::WordEllipsis););
    AERO_REGISTER_ENUM(
        ::Aero::TextAlignment,
        "TextAlignment",
        description
            .Value("Left", ::Aero::TextAlignment::Left)
            .Value("Center", ::Aero::TextAlignment::Center)
            .Value("Right", ::Aero::TextAlignment::Right)
            .Value("Justify", ::Aero::TextAlignment::Justify););
    AERO_REGISTER_ENUM(
        ::Aero::FlowDirection,
        "FlowDirection",
        description
            .Value("LeftToRight", ::Aero::FlowDirection::LeftToRight)
            .Value("RightToLeft", ::Aero::FlowDirection::RightToLeft););
    AERO_REGISTER_ENUM(
        ::Aero::FontStyle,
        "FontStyle",
        description
            .Value("Normal", ::Aero::FontStyle::Normal)
            .Value("Italic", ::Aero::FontStyle::Italic)
            .Value("Oblique", ::Aero::FontStyle::Oblique););
    AERO_REGISTER_ENUM(
        ::Aero::FontWeight,
        "FontWeight",
        description
            .Value("Normal", ::Aero::FontWeight::Normal)
            .Value("SemiBold", ::Aero::FontWeight::SemiBold)
            .Value("Bold", ::Aero::FontWeight::Bold)
            .Value("Regular", ::Aero::FontWeight::Regular););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::TextDecorations,
        "TextDecorations",
        description
            .Value("None", ::Aero::Controls::TextDecorations::None)
            .Value("Underline", ::Aero::Controls::TextDecorations::Underline););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::Orientation,
        "Orientation",
        description
            .Value("Horizontal", ::Aero::Controls::Orientation::Horizontal)
            .Value("Vertical", ::Aero::Controls::Orientation::Vertical););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::Dock,
        "Dock",
        description
            .Value("Left", ::Aero::Controls::Dock::Left)
            .Value("Top", ::Aero::Controls::Dock::Top)
            .Value("Right", ::Aero::Controls::Dock::Right)
            .Value("Bottom", ::Aero::Controls::Dock::Bottom););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::MenuItemRole,
        "MenuItemRole",
        description
            .Value("TopLevelItem", ::Aero::Controls::MenuItemRole::TopLevelItem)
            .Value("TopLevelHeader", ::Aero::Controls::MenuItemRole::TopLevelHeader)
            .Value("SubmenuItem", ::Aero::Controls::MenuItemRole::SubmenuItem)
            .Value("SubmenuHeader", ::Aero::Controls::MenuItemRole::SubmenuHeader););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::ClickMode,
        "ClickMode",
        description
            .Value("Release", ::Aero::Controls::ClickMode::Release)
            .Value("Press", ::Aero::Controls::ClickMode::Press)
            .Value("Hover", ::Aero::Controls::ClickMode::Hover););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::TickPlacement,
        "TickPlacement",
        description
            .Value("None", ::Aero::Controls::TickPlacement::None)
            .Value("TopLeft", ::Aero::Controls::TickPlacement::TopLeft)
            .Value("BottomRight", ::Aero::Controls::TickPlacement::BottomRight)
            .Value("Both", ::Aero::Controls::TickPlacement::Both););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::TickBarPlacement,
        "TickBarPlacement",
        description
            .Value("Top", ::Aero::Controls::TickBarPlacement::Top)
            .Value("Bottom", ::Aero::Controls::TickBarPlacement::Bottom)
            .Value("Left", ::Aero::Controls::TickBarPlacement::Left)
            .Value("Right", ::Aero::Controls::TickBarPlacement::Right););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::ScrollBarVisibility,
        "ScrollBarVisibility",
        description
            .Value("Disabled", ::Aero::Controls::ScrollBarVisibility::Disabled)
            .Value("Auto", ::Aero::Controls::ScrollBarVisibility::Auto)
            .Value("Hidden", ::Aero::Controls::ScrollBarVisibility::Hidden)
            .Value("Visible", ::Aero::Controls::ScrollBarVisibility::Visible););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::PanningMode,
        "PanningMode",
        description
            .Value("None", ::Aero::Controls::PanningMode::None)
            .Value("HorizontalOnly", ::Aero::Controls::PanningMode::HorizontalOnly)
            .Value("VerticalOnly", ::Aero::Controls::PanningMode::VerticalOnly)
            .Value("Both", ::Aero::Controls::PanningMode::Both)
            .Value("HorizontalFirst", ::Aero::Controls::PanningMode::HorizontalFirst)
            .Value("VerticalFirst", ::Aero::Controls::PanningMode::VerticalFirst););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::GridResizeDirection,
        "GridResizeDirection",
        description
            .Value("Auto", ::Aero::Controls::GridResizeDirection::Auto)
            .Value("Columns", ::Aero::Controls::GridResizeDirection::Columns)
            .Value("Rows", ::Aero::Controls::GridResizeDirection::Rows););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::GridResizeBehavior,
        "GridResizeBehavior",
        description
            .Value("BasedOnAlignment", ::Aero::Controls::GridResizeBehavior::BasedOnAlignment)
            .Value("CurrentAndNext", ::Aero::Controls::GridResizeBehavior::CurrentAndNext)
            .Value("PreviousAndCurrent", ::Aero::Controls::GridResizeBehavior::PreviousAndCurrent)
            .Value("PreviousAndNext", ::Aero::Controls::GridResizeBehavior::PreviousAndNext););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::SelectionMode,
        "SelectionMode",
        description
            .Value("Single", ::Aero::Controls::SelectionMode::Single)
            .Value("Multiple", ::Aero::Controls::SelectionMode::Multiple)
            .Value("Extended", ::Aero::Controls::SelectionMode::Extended););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::ExpandDirection,
        "ExpandDirection",
        description
            .Value("Down", ::Aero::Controls::ExpandDirection::Down)
            .Value("Up", ::Aero::Controls::ExpandDirection::Up)
            .Value("Left", ::Aero::Controls::ExpandDirection::Left)
            .Value("Right", ::Aero::Controls::ExpandDirection::Right););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::Primitives::PlacementMode,
        "PlacementMode",
        description
            .Value("Bottom", ::Aero::Controls::Primitives::PlacementMode::Bottom)
            .Value("Top", ::Aero::Controls::Primitives::PlacementMode::Top)
            .Value("Left", ::Aero::Controls::Primitives::PlacementMode::Left)
            .Value("Right", ::Aero::Controls::Primitives::PlacementMode::Right)
            .Value("Center", ::Aero::Controls::Primitives::PlacementMode::Center)
            .Value("Mouse", ::Aero::Controls::Primitives::PlacementMode::Mouse););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::Primitives::PopupAnimation,
        "PopupAnimation",
        description
            .Value("None", ::Aero::Controls::Primitives::PopupAnimation::None)
            .Value("Fade", ::Aero::Controls::Primitives::PopupAnimation::Fade)
            .Value("Slide", ::Aero::Controls::Primitives::PopupAnimation::Slide)
            .Value("Scroll", ::Aero::Controls::Primitives::PopupAnimation::Scroll););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::GridViewColumnHeaderRole,
        "GridViewColumnHeaderRole",
        description
            .Value("Normal", ::Aero::Controls::GridViewColumnHeaderRole::Normal)
            .Value("Floating", ::Aero::Controls::GridViewColumnHeaderRole::Floating)
            .Value("Padding", ::Aero::Controls::GridViewColumnHeaderRole::Padding););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::ScrollUnit,
        "ScrollUnit",
        description
            .Value("Item", ::Aero::Controls::ScrollUnit::Item)
            .Value("Pixel", ::Aero::Controls::ScrollUnit::Pixel););
    AERO_REGISTER_ENUM(
        ::Aero::Controls::VirtualizationMode,
        "VirtualizationMode",
        description
            .Value("Standard", ::Aero::Controls::VirtualizationMode::Standard)
            .Value("Recycling", ::Aero::Controls::VirtualizationMode::Recycling););

#undef AERO_REGISTER_ENUM
    return {};
}

} // namespace Aero
