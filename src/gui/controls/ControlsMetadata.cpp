#include "Metadata.hpp"
#include "ControlsMetadata.hpp"

// Templates
#include <Aero/FrameworkTemplate.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/DataTemplateSelector.hpp>
#include <Aero/HierarchicalDataTemplate.hpp>
#include <Aero/Controls/ItemsPanelTemplate.hpp>

// Panels
#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/StackPanel.hpp>
#include <Aero/Controls/DockPanel.hpp>
#include <Aero/Controls/WrapPanel.hpp>
#include <Aero/Controls/UniformGrid.hpp>
#include <Aero/Controls/VirtualizingPanel.hpp>
#include <Aero/Controls/VirtualizingStackPanel.hpp>
#include <Aero/Controls/VirtualizingWrapPanel.hpp>
#include <Aero/Controls/Canvas.hpp>
#include <Aero/Controls/Grid.hpp>

// Primitives & Buttons
#include <Aero/Controls/Primitives/ButtonBase.hpp>
#include <Aero/Controls/Button.hpp>
#include <Aero/Controls/Primitives/RepeatButton.hpp>
#include <Aero/Controls/Primitives/ToggleButton.hpp>
#include <Aero/Controls/CheckBox.hpp>
#include <Aero/Controls/RadioButton.hpp>
#include <Aero/Controls/Primitives/Thumb.hpp>
#include <Aero/Controls/Primitives/Track.hpp>
#include <Aero/Controls/Primitives/RangeBase.hpp>
#include <Aero/Controls/Primitives/ScrollBar.hpp>
#include <Aero/Controls/Slider.hpp>
#include <Aero/Controls/Primitives/TickBar.hpp>
#include <Aero/Controls/ProgressBar.hpp>
#include <Aero/Controls/GridSplitter.hpp>
#include <Aero/Controls/ScrollContentPresenter.hpp>
#include <Aero/Controls/ScrollViewer.hpp>

// Content & Decorators
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Controls/HeaderedContentControl.hpp>
#include <Aero/Controls/Decorator.hpp>
#include <Aero/Controls/Border.hpp>
#include <Aero/Controls/BulletDecorator.hpp>
#include <Aero/Controls/Viewbox.hpp>
#include <Aero/Controls/ContentPresenter.hpp>
#include <Aero/Controls/UserControl.hpp>
#include <Aero/Controls/Page.hpp>
#include <Aero/Controls/GroupBox.hpp>
#include <Aero/Controls/Label.hpp>
#include <Aero/Controls/Expander.hpp>
#include <Aero/Controls/Popup.hpp>

// Items
#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Controls/HeaderedItemsControl.hpp>
#include <Aero/Controls/ItemsPresenter.hpp>

// Selection
#include <Aero/Controls/Primitives/Selector.hpp>
#include <Aero/Controls/ListBox.hpp>
#include <Aero/Controls/ListBoxItem.hpp>
#include <Aero/Controls/ComboBox.hpp>
#include <Aero/Controls/ComboBoxItem.hpp>
#include <Aero/Controls/TabItem.hpp>
#include <Aero/Controls/TabControl.hpp>
#include <Aero/Controls/TabPanel.hpp>

// Trees
#include <Aero/Controls/TreeView.hpp>
#include <Aero/Controls/TreeViewItem.hpp>

// Menus
#include <Aero/Controls/Menu.hpp>
#include <Aero/Controls/MenuItem.hpp>
#include <Aero/Controls/ContextMenu.hpp>
#include <Aero/Controls/ContextMenuService.hpp>
#include <Aero/Controls/Separator.hpp>

// Bars & ToolTips
#include <Aero/Controls/ToolBar.hpp>
#include <Aero/Controls/StatusBar.hpp>
#include <Aero/Controls/StatusBarItem.hpp>
#include <Aero/Controls/ToolTip.hpp>

// Text & Media
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/PasswordBox.hpp>
#include <Aero/Controls/Image.hpp>

// Shapes
#include <Aero/Shapes/Shape.hpp>
#include <Aero/Shapes/Rectangle.hpp>
#include <Aero/Shapes/Ellipse.hpp>
#include <Aero/Shapes/Path.hpp>
#include <Aero/Shapes/Line.hpp>
#include <Aero/Shapes/Polygon.hpp>
#include <Aero/Shapes/Polyline.hpp>

// ListView & GridView
#include <Aero/Controls/GridViewColumnHeader.hpp>
#include <Aero/Controls/GridViewColumn.hpp>
#include <Aero/Controls/GridView.hpp>
#include <Aero/Controls/GridViewHeaderRowPresenter.hpp>
#include <Aero/Controls/GridViewRowPresenter.hpp>
#include <Aero/Controls/ListView.hpp>
#include <Aero/Controls/ListViewItem.hpp>

namespace Aero::Controls {

Base::Result<void> PopulateControlsMetadata(
    ::Aero::Meta::Registration& context) noexcept {
    // Templates
    FrameworkTemplate::RegisterMetadata(context);
    ControlTemplate::RegisterMetadata(context);
    DataTemplate::RegisterMetadata(context);
    DataTemplateSelector::RegisterMetadata(context);
    HierarchicalDataTemplate::RegisterMetadata(context);
    ItemsPanelTemplate::RegisterMetadata(context);

    // Panels
    Panel::RegisterMetadata(context);
    StackPanel::RegisterMetadata(context);
    DockPanel::RegisterMetadata(context);
    WrapPanel::RegisterMetadata(context);
    UniformGrid::RegisterMetadata(context);
    VirtualizingPanel::RegisterMetadata(context);
    VirtualizingStackPanel::RegisterMetadata(context);
    VirtualizingWrapPanel::RegisterMetadata(context);
    Canvas::RegisterMetadata(context);
    Grid::RegisterMetadata(context);

    // Content & Decorators (base types first: metadata Override() on a
    // derived type requires its base chain to be registered already)
    Control::RegisterMetadata(context);
    ContentControl::RegisterMetadata(context);
    HeaderedContentControl::RegisterMetadata(context);
    Decorator::RegisterMetadata(context);
    Border::RegisterMetadata(context);
    BulletDecorator::RegisterMetadata(context);
    Viewbox::RegisterMetadata(context);
    ContentPresenter::RegisterMetadata(context);

    // Primitives & Buttons
    Primitives::ButtonBase::RegisterMetadata(context);
    Button::RegisterMetadata(context);
    Primitives::RepeatButton::RegisterMetadata(context);
    Primitives::ToggleButton::RegisterMetadata(context);
    CheckBox::RegisterMetadata(context);
    RadioButton::RegisterMetadata(context);
    Primitives::Thumb::RegisterMetadata(context);
    Primitives::Track::RegisterMetadata(context);
    Primitives::RangeBase::RegisterMetadata(context);
    Primitives::ScrollBar::RegisterMetadata(context);
    Slider::RegisterMetadata(context);
    TickBar::RegisterMetadata(context);
    ProgressBar::RegisterMetadata(context);
    GridSplitter::RegisterMetadata(context);
    ScrollContentPresenter::RegisterMetadata(context);
    ScrollViewer::RegisterMetadata(context);

    UserControl::RegisterMetadata(context);
    Page::RegisterMetadata(context);
    GroupBox::RegisterMetadata(context);
    Label::RegisterMetadata(context);
    Expander::RegisterMetadata(context);
    Primitives::Popup::RegisterMetadata(context);

    // Items
    ItemsControl::RegisterMetadata(context);
    HeaderedItemsControl::RegisterMetadata(context);
    ItemsPresenter::RegisterMetadata(context);

    // Selection
    Primitives::Selector::RegisterMetadata(context);
    ListBox::RegisterMetadata(context);
    ListBoxItem::RegisterMetadata(context);
    ComboBox::RegisterMetadata(context);
    ComboBoxItem::RegisterMetadata(context);
    TabItem::RegisterMetadata(context);
    TabControl::RegisterMetadata(context);
    TabPanel::RegisterMetadata(context);

    // Trees
    TreeView::RegisterMetadata(context);
    TreeViewItem::RegisterMetadata(context);

    // Menus
    Menu::RegisterMetadata(context);
    MenuItem::RegisterMetadata(context);
    ContextMenu::RegisterMetadata(context);
    ContextMenuService::RegisterMetadata(context);
    Separator::RegisterMetadata(context);

    // Bars & ToolTips
    ToolBar::RegisterMetadata(context);
    ToolBarPanel::RegisterMetadata(context);
    ToolBarOverflowPanel::RegisterMetadata(context);
    ToolBarTray::RegisterMetadata(context);
    StatusBar::RegisterMetadata(context);
    StatusBarItem::RegisterMetadata(context);
    ToolTip::RegisterMetadata(context);
    ToolTipService::RegisterMetadata(context);

    // Text & Media
    TextBlock::RegisterMetadata(context);
    Primitives::TextBoxBase::RegisterMetadata(context);
    TextBox::RegisterMetadata(context);
    PasswordBox::RegisterMetadata(context);
    Image::RegisterMetadata(context);

    // Shapes
    Shapes::Shape::RegisterMetadata(context);
    Shapes::Rectangle::RegisterMetadata(context);
    Shapes::Ellipse::RegisterMetadata(context);
    Shapes::Path::RegisterMetadata(context);
    Shapes::Line::RegisterMetadata(context);
    Shapes::Polygon::RegisterMetadata(context);
    Shapes::Polyline::RegisterMetadata(context);

    // ListView & GridView
    GridViewColumnHeader::RegisterMetadata(context);
    GridViewColumn::RegisterMetadata(context);
    GridView::RegisterMetadata(context);
    GridViewHeaderRowPresenter::RegisterMetadata(context);
    GridViewRowPresenter::RegisterMetadata(context);
    ListView::RegisterMetadata(context);
    ListViewItem::RegisterMetadata(context);

    return {};
}

} // namespace Aero::Controls
