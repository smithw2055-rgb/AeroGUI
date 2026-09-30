#include "gui/core/EnumRegistration.hpp"

#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/Menus.hpp>
#include <Aero/Controls/Primitives/ButtonBase.hpp>
#include <Aero/Controls/Ranges.hpp>
#include <Aero/Controls/ScrollViewer.hpp>
#include <Aero/Controls/GridSplitter.hpp>
#include <Aero/Controls/Primitives/Selector.hpp>
#include <Aero/Controls/Headers.hpp>
#include <Aero/Controls/Popup.hpp>
#include <Aero/Controls/GridViews.hpp>
#include <Aero/Controls/VirtualizingPanel.hpp>
#include <Aero/Controls/VirtualizationCacheLength.hpp>

namespace Aero {

Base::Result<void> PopulateControlsEnums(
    Meta::Registration& context) noexcept {
    using namespace Controls;
    using namespace Controls::Primitives;

    Base::Result<void> status;

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
