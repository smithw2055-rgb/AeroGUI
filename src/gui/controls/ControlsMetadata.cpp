#include "Metadata.hpp"
#include "ControlsMetadata.hpp"

// Templates
#include <Aero/FrameworkTemplate.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Controls/ItemsPanelTemplate.hpp>

// Panels
#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Controls/VirtualizingPanel.hpp>
#include <Aero/Controls/VirtualizingStackPanel.hpp>
#include <Aero/Controls/VirtualizingWrapPanel.hpp>
#include <Aero/Controls/Grid.hpp>

// Primitives & Buttons
#include <Aero/Controls/Primitives/ButtonBase.hpp>
#include <Aero/Controls/Button.hpp>
#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Primitives/Thumb.hpp>
#include <Aero/Controls/Ranges.hpp>
#include <Aero/Controls/GridSplitter.hpp>
#include <Aero/Controls/ScrollViewer.hpp>

// Content & Decorators
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Controls/HeaderedContentControl.hpp>
#include <Aero/Controls/Decorator.hpp>
#include <Aero/Controls/Border.hpp>
#include <Aero/Controls/Viewbox.hpp>
#include <Aero/Controls/ContentPresenter.hpp>
#include <Aero/Controls/UserControl.hpp>
#include <Aero/Controls/Page.hpp>
#include <Aero/Controls/Headers.hpp>
#include <Aero/Controls/Label.hpp>
#include <Aero/Controls/Popup.hpp>

// Items
#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Controls/HeaderedItemsControl.hpp>
#include <Aero/Controls/ItemsPresenter.hpp>

// Selection
#include <Aero/Controls/Primitives/Selector.hpp>
#include <Aero/Controls/Selectors.hpp>

// Trees
#include <Aero/Controls/TreeView.hpp>

// Menus
#include <Aero/Controls/Menus.hpp>
#include <Aero/Controls/ContextMenuService.hpp>
#include <Aero/Controls/Separator.hpp>

// Bars & ToolTips
#include <Aero/Controls/ToolBar.hpp>
#include <Aero/Controls/StatusBar.hpp>
#include <Aero/Controls/ToolTip.hpp>

// Text & Media
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/Image.hpp>

// Shapes
#include <Aero/Shapes.hpp>

// ListView & GridView
#include <Aero/Controls/GridViews.hpp>
#include <Aero/Controls/ListView.hpp>
#include "gui/meta/Describe.hpp"

namespace Aero::Controls {

Base::Result<void> PopulateControlsMetadata(
    ::Aero::Meta::Registration& context) noexcept {
    // Describes live next to each control. Installing them records the
    // function; EnsureRegisteredPack then registers bases before derived types.
    static bool describesInstalled = false;
    if (!describesInstalled) {
        describesInstalled = true;
#include "gui/controls/ControlDescribes.inl"
    }
    ::Aero::Meta::EnsureRegisteredPack<
        FrameworkTemplate,
        ControlTemplate,
        DataTemplate,
        DataTemplateSelector,
        HierarchicalDataTemplate,
        ItemsPanelTemplate,
        Panel,
        StackPanel,
        DockPanel,
        WrapPanel,
        UniformGrid,
        VirtualizingPanel,
        VirtualizingStackPanel,
        VirtualizingWrapPanel,
        Canvas,
        Grid,
        Control,
        ContentControl,
        HeaderedContentControl,
        Decorator,
        Border,
        BulletDecorator,
        Viewbox,
        ContentPresenter,
        Primitives::ButtonBase,
        Button,
        Primitives::RepeatButton,
        Primitives::ToggleButton,
        CheckBox,
        RadioButton,
        Primitives::Thumb,
        Primitives::Track,
        Primitives::RangeBase,
        Primitives::ScrollBar,
        Slider,
        TickBar,
        ProgressBar,
        GridSplitter,
        ScrollContentPresenter,
        ScrollViewer,
        UserControl,
        Page,
        GroupBox,
        Label,
        Expander,
        Primitives::Popup,
        ItemsControl,
        HeaderedItemsControl,
        ItemsPresenter,
        Primitives::Selector,
        ListBox,
        ListBoxItem,
        ComboBox,
        ComboBoxItem,
        TabItem,
        TabControl,
        TabPanel,
        TreeView,
        TreeViewItem,
        Menu,
        MenuItem,
        ContextMenu,
        ContextMenuService,
        Separator,
        ToolBar,
        ToolBarPanel,
        ToolBarOverflowPanel,
        ToolBarTray,
        StatusBar,
        StatusBarItem,
        ToolTip,
        ToolTipService,
        TextBlock,
        Primitives::TextBoxBase,
        TextBox,
        PasswordBox,
        Image,
        Shapes::Shape,
        Shapes::Rectangle,
        Shapes::Ellipse,
        Shapes::Path,
        Shapes::Line,
        Shapes::Polygon,
        Shapes::Polyline,
        GridViewColumnHeader,
        GridViewColumn,
        GridView,
        GridViewHeaderRowPresenter,
        GridViewRowPresenter,
        ListView,
        ListViewItem>(context);
    return {};
}

} // namespace Aero::Controls
