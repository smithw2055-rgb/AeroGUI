#include <Aero/Controls/Metadata.hpp>

#include <Aero/App/Window.hpp>
#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Bars.hpp>
#include <Aero/Controls/ContentControls.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/ListView.hpp>
#include <Aero/Controls/Menus.hpp>
#include <Aero/Controls/Scroll.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Controls/Shapes.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Controls/Trees.hpp>
#include <Aero/Controls/Virtualization.hpp>
#include <Aero/Presentation/Brushes.hpp>
#include <Aero/Core/Metadata/Describe.hpp>
#include <Aero/Core/Metadata/ValueConversion.hpp>
#include <Aero/Presentation/Style.hpp>

#include "PathServicesAccess.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Aero::Controls {
namespace {

// The Gallery uses a bare Control as a style/template host. Keep the public
// Control base class extensible while providing a concrete runtime instance
// for that XAML form.
class BasicControl final : public Control {
public:
    BasicControl() noexcept : Control(Control::StaticTypeId()) {}
};

Base::Result<Base::Ref<Base::Object>>
CreateBasicControl() noexcept {
    Base::Result<Base::Ref<BasicControl>> created =
        Base::MakeRef<BasicControl>();
    return created
        ? Base::Result<Base::Ref<Base::Object>>(
            Base::Ref<Base::Object>(
                std::move(created).Value()))
        : Base::Result<Base::Ref<Base::Object>>(
            created.GetStatus());
}

Base::Result<void> AddTemplateTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Template Trigger cannot be retained");
    }
    return static_cast<FrameworkTemplate&>(owner)
        .TryAddAuthoredTrigger(
            value);
}

Base::Result<void> ClearTemplateTriggers(
    Base::Object& owner,
    void*) noexcept {
    static_cast<FrameworkTemplate&>(owner)
        .ClearAuthoredTriggers();
    return {};
}

template<class T>
Base::Result<void> SetDeferredTemplateVisualTree(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    return static_cast<T&>(object)
        .SetAuthoredVisualTree(value);
}

template<class T>
Base::Result<void> ClearDeferredTemplateVisualTree(
    Base::Object& object,
    void*) noexcept {
    static_cast<T&>(object)
        .ClearAuthoredVisualTree();
    return {};
}

Base::Result<void> AddTemplateVisualStateGroup(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    return static_cast<ControlTemplate&>(object)
        .TryAddAuthoredVisualStateGroup(value);
}

Base::Result<void> ClearTemplateVisualStateGroups(
    Base::Object& object,
    void*) noexcept {
    static_cast<ControlTemplate&>(object)
        .ClearAuthoredVisualStateGroups();
    return {};
}

Core::TypeReference GetControlTemplateTargetType(
    const ControlTemplate& value) noexcept {
    return {value.TargetType()};
}

Base::Result<void> SetControlTemplateTargetType(
    ControlTemplate& target,
    Core::TypeReference value) noexcept {
    return target.TrySetTargetType(value.type);
}

Core::TypeReference GetDataTemplateType(
    const DataTemplate& value) noexcept {
    return {value.DataType()};
}

Base::Result<void> SetDataTemplateType(
    DataTemplate& target,
    Core::TypeReference value) noexcept {
    return target.SetDataType(value.type);
}

bool ValidateThicknessValue(
    const Presentation::Thickness& thickness) noexcept {
    return Presentation::IsFinite(thickness) &&
        thickness.left >= 0.0 && thickness.top >= 0.0 &&
        thickness.right >= 0.0 && thickness.bottom >= 0.0;
}

bool ValidateColorValue(
    const Presentation::Color& color) noexcept {
    return std::isfinite(color.red) && std::isfinite(color.green) &&
        std::isfinite(color.blue) && std::isfinite(color.alpha) &&
        color.red >= 0.0F && color.red <= 1.0F &&
        color.green >= 0.0F && color.green <= 1.0F &&
        color.blue >= 0.0F && color.blue <= 1.0F &&
        color.alpha >= 0.0F && color.alpha <= 1.0F;
}

bool ValidateCornerRadiusValue(
    const Presentation::CornerRadius& radius) noexcept {
    return std::isfinite(radius.topLeft) &&
        std::isfinite(radius.topRight) &&
        std::isfinite(radius.bottomRight) &&
        std::isfinite(radius.bottomLeft) &&
        radius.topLeft >= 0.0 &&
        radius.topRight >= 0.0 &&
        radius.bottomRight >= 0.0 &&
        radius.bottomLeft >= 0.0;
}

bool ValidatePositiveFiniteDouble(const double& value) noexcept {
    return std::isfinite(value) && value > 0.0;
}

bool ValidateSliderTicks(
    const Base::String& value) noexcept {
    const Base::StringView text = value.View();
    std::uint32_t start = 0U;
    while (start < text.SizeBytes()) {
        while (start < text.SizeBytes() &&
            (text[start] == ' ' ||
             text[start] == '\t' ||
             text[start] == ',' ||
             text[start] == ';')) {
            ++start;
        }
        if (start >= text.SizeBytes()) break;
        std::uint32_t end = start;
        while (end < text.SizeBytes() &&
            text[end] != ' ' &&
            text[end] != '\t' &&
            text[end] != ',' &&
            text[end] != ';') {
            ++end;
        }
        Base::Result<double> parsed =
            Core::ValueConversion::ParseDouble(
                text.Substr(start, end - start));
        if (!parsed ||
            !std::isfinite(parsed.Value())) {
            return false;
        }
        start = end;
    }
    return true;
}

Base::Result<GridLength> ConvertGridLength(
    Base::StringView text) noexcept {
    const Base::StringView value =
        Core::ValueConversion::Trim(text);
    if (Core::ValueConversion::EqualsAsciiInsensitive(
            value, "auto")) {
        return GridLength::Auto();
    }
    if (!value.Empty() &&
        value[value.SizeBytes() - 1U] == '*') {
        const Base::StringView weightText =
            value.Substr(0U, value.SizeBytes() - 1U);
        double weight = 1.0;
        if (!weightText.Empty()) {
            Base::Result<double> parsed =
                Core::ValueConversion::ParseDouble(weightText);
            if (!parsed) return parsed.GetStatus();
            weight = parsed.Value();
        }
        if (!std::isfinite(weight) || weight <= 0.0) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "GridLength star weight must be positive and finite");
        }
        return GridLength::Star(weight);
    }
    Base::Result<double> pixels =
        Core::ValueConversion::ParseDouble(value);
    if (!pixels || pixels.Value() < 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "GridLength must be Auto, a nonnegative pixel value, or a star weight");
    }
    return GridLength::Pixel(pixels.Value());
}

bool EqualGridLength(
    const void* left,
    const void* right,
    void*) noexcept {
    const auto& a =
        *static_cast<const GridLength*>(left);
    const auto& b =
        *static_cast<const GridLength*>(right);
    return a.unit == b.unit && a.value == b.value;
}

Base::Result<void> ParseGridDefinitions(
    Base::StringView text,
    Base::Vector<GridLength>& output) noexcept {
    output.Clear();
    const Base::StringView value =
        Core::ValueConversion::Trim(text);
    if (value.Empty()) return {};
    std::uint32_t start = 0U;
    while (start <= value.SizeBytes()) {
        std::uint32_t end = start;
        while (end < value.SizeBytes() &&
            value[end] != ',') {
            ++end;
        }
        const Base::StringView token =
            Core::ValueConversion::Trim(
                value.Substr(start, end - start));
        if (token.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Grid definitions contain an empty track");
        }
        Base::Result<GridLength> parsed =
            ConvertGridLength(token);
        if (!parsed) return parsed.GetStatus();
        Base::Result<void> added =
            output.TryPushBack(parsed.Value());
        if (!added) return added.GetStatus();
        if (end == value.SizeBytes()) break;
        start = end + 1U;
    }
    return {};
}

Base::Result<void> AddDataTemplateTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Presentation::TriggerBase> retained =
        Base::Ref<Presentation::TriggerBase>::
            TryFromBorrowed(
                static_cast<
                    Presentation::TriggerBase&>(
                        *value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "DataTemplate Trigger cannot be retained");
    }
    return static_cast<DataTemplate&>(owner)
        .TryAddAuthoredTrigger(
            std::move(retained));
}

Base::Result<void> ClearDataTemplateTriggers(
    Base::Object& owner,
    void*) noexcept {
    static_cast<DataTemplate&>(owner)
        .ClearAuthoredTriggers();
    return {};
}

Base::Result<void> AddGridColumnDefinition(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<ColumnDefinition> retained =
        Base::Ref<ColumnDefinition>::TryFromBorrowed(
            static_cast<ColumnDefinition&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Grid ColumnDefinition cannot be retained");
    }
    return static_cast<Grid&>(owner)
        .AddColumnDefinition(std::move(retained));
}

Base::Result<void> ClearGridColumnDefinitions(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<Grid&>(owner)
        .ClearColumnDefinitionObjects();
}

Base::Result<void> AddGridRowDefinition(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<RowDefinition> retained =
        Base::Ref<RowDefinition>::TryFromBorrowed(
            static_cast<RowDefinition&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Grid RowDefinition cannot be retained");
    }
    return static_cast<Grid&>(owner)
        .AddRowDefinition(std::move(retained));
}

Base::Result<void> ClearGridRowDefinitions(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<Grid&>(owner)
        .ClearRowDefinitionObjects();
}

Base::Result<void> AddGridInputBinding(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Presentation::KeyBinding> retained =
        Base::Ref<Presentation::KeyBinding>::TryFromBorrowed(
            static_cast<Presentation::KeyBinding&>(*value));
    return retained
        ? static_cast<Grid&>(owner).AddInputBinding(std::move(retained))
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::InvalidArgument,
              "Grid InputBindings expects KeyBinding"));
}

Base::Result<void> ClearGridInputBindings(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Grid&>(owner).ClearInputBindings();
    return {};
}

bool ValidateGridDefinitionsText(
    const Base::String& value) noexcept {
    Base::Vector<GridLength> parsed;
    Base::Result<void> result =
        ParseGridDefinitions(
            value.View(), parsed);
    return static_cast<bool>(result);
}

void OnGridColumnsChanged(
    Core::DependencyObject& object,
    const Base::String&,
    const Base::String& value) noexcept {
    Base::Vector<GridLength> parsed;
    if (ParseGridDefinitions(
            value.View(), parsed)) {
        static_cast<void>(
            static_cast<Grid&>(object).
                SetColumnDefinitions(
                    parsed.AsSpan()));
    }
}

void OnGridRowsChanged(
    Core::DependencyObject& object,
    const Base::String&,
    const Base::String& value) noexcept {
    Base::Vector<GridLength> parsed;
    if (ParseGridDefinitions(
            value.View(), parsed)) {
        static_cast<void>(
            static_cast<Grid&>(object).
                SetRowDefinitions(
                    parsed.AsSpan()));
    }
}

void OnPathDataChanged(
    Core::DependencyObject& object,
    const Core::DependencyPropertyChangedEventArgs&) noexcept {
    Detail::PathServicesAccess::InvalidateGeometry(
        static_cast<Path&>(object));
}

void OnPathColorChanged(
    Core::DependencyObject& object,
    const Base::Ref<Presentation::Brush>& oldBrush,
    const Base::Ref<Presentation::Brush>& newBrush) noexcept {
    auto* owner = static_cast<Path&>(object).AsFrameworkElement();
    if (owner != nullptr) {
        if (oldBrush && oldBrush->Owner() == owner) {
            oldBrush->SetOwner(nullptr);
        }
        if (newBrush) {
            newBrush->SetOwner(owner);
        }
    }
    Detail::PathServicesAccess::InvalidateGeometry(
        static_cast<Path&>(object));
}

void OnPathDoubleChanged(
    Core::DependencyObject& object,
    const double&,
    const double&) noexcept {
    Detail::PathServicesAccess::InvalidateGeometry(
        static_cast<Path&>(object));
}

void OnPathLineJoinChanged(
    Core::DependencyObject& object,
    const PenLineJoin&,
    const PenLineJoin&) noexcept {
    Detail::PathServicesAccess::InvalidateGeometry(
        static_cast<Path&>(object));
}

void OnPathLineCapChanged(
    Core::DependencyObject& object,
    const PenLineCap&,
    const PenLineCap&) noexcept {
    Detail::PathServicesAccess::InvalidateGeometry(
        static_cast<Path&>(object));
}

void OnShapeFillChanged(
    Core::DependencyObject& object,
    const Core::DependencyPropertyChangedEventArgs&
        args) noexcept {
    auto* owner =
        static_cast<Shape&>(object).
            AsFrameworkElement();
    if (owner == nullptr) return;
    Base::Result<Base::Ref<Brush>> oldBrush =
        ValueCodec<Base::Ref<Brush>>::Decode(
            args.oldValue);
    if (oldBrush && oldBrush.Value() &&
        oldBrush.Value()->Owner() == owner) {
        oldBrush.Value()->SetOwner(nullptr);
    }
    Base::Result<Base::Ref<Brush>> newBrush =
        ValueCodec<Base::Ref<Brush>>::Decode(
            args.newValue);
    if (newBrush && newBrush.Value()) {
        newBrush.Value()->SetOwner(owner);
    }
}

void OnScrollViewerVisibilityChanged(
    Core::DependencyObject& object,
    const Core::DependencyPropertyChangedEventArgs&) noexcept {
    if (!object.PropertyRegistry().Types().IsDerivedFrom(
            object.RuntimeType(),
            ScrollViewer::StaticTypeId())) {
        return;
    }
    auto& viewer = static_cast<ScrollViewer&>(object);
    // Re-enter through the instance setters with the already committed
    // values. SetValue is then a no-op, while the setters refresh the
    // read-only computed visibility against the current extent/viewport.
    static_cast<void>(
        viewer.SetHorizontalScrollBarVisibility(
            viewer.HorizontalScrollBarVisibility()));
    static_cast<void>(
        viewer.SetVerticalScrollBarVisibility(
            viewer.VerticalScrollBarVisibility()));
}

bool ValidateNormalizedDouble(
    const double& value) noexcept {
    return std::isfinite(value) &&
        value >= 0.0 && value <= 1.0;
}

Base::Result<Base::Ref<Base::Object>> CoerceSelectedObject(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const Base::Ref<Base::Object>& value) noexcept {
    const auto& selector =
        static_cast<const Selector&>(object);
    if (value &&
        selector.IndexOfItem(value.Get()) == UINT32_MAX) {
        return Base::Ref<Base::Object>{};
    }
    return value;
}

Base::Result<bool> CoerceButtonEnabled(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const bool& value) noexcept {
    return value &&
        static_cast<ButtonBase&>(object).IsCommandEnabled();
}

Base::Result<Base::String> CoerceTextBoxText(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const Base::String& value) noexcept {
    Text::EditableTextModel validation;
    static_cast<void>(object);
    Base::Result<void> limited =
        validation.SetText(value.View());
    if (!limited) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "TextBox text is not valid UTF-8");
    }
    return value;
}

bool ValidatePasswordChar(
    const Base::String& value) noexcept {
    Text::EditableTextModel validation;
    Base::Result<void> text =
        validation.SetText(value.View());
    return text &&
        validation.GraphemeCount() == 1U;
}

Base::Result<double> CoerceRangeMinimum(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const double& value) noexcept {
    const double maximum =
        static_cast<RangeBase&>(object).Maximum();
    return std::min(value, maximum);
}

Base::Result<double> CoerceRangeMaximum(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const double& value) noexcept {
    const double minimum =
        static_cast<RangeBase&>(object).Minimum();
    return std::max(value, minimum);
}

Base::Result<double> CoerceRangeValue(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const double& value) noexcept {
    const auto& range =
        static_cast<const RangeBase&>(object);
    return std::clamp(
        value,
        range.Minimum(),
        range.Maximum());
}

Base::Result<void> SetPanelContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Panel content child is null");
    }
    return static_cast<Panel&>(owner).AddOwnedChild(
        child, *static_cast<Presentation::UIElement*>(child.Get()));
}

Base::Result<void> ClearPanelContent(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<Panel&>(owner).ClearOwnedChildren();
}

Base::Result<void> SetDecoratorContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Decorator content child is null");
    }
    return static_cast<Decorator&>(owner).SetOwnedChild(
        child, *static_cast<Presentation::UIElement*>(child.Get()));
}

Base::Result<void> ClearDecoratorContent(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<Decorator&>(owner).SetChild(nullptr);
}

Base::Result<void> SetContentControlContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ContentControl content child is null");
    }
    return static_cast<ContentControl&>(owner)
        .SetContentValue(child);
}

Base::Result<void> ClearContentControlContent(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<ContentControl&>(owner)
        .SetContentValue(
            Core::Value::NullObject(
                Core::TypeOf<Base::Object>()));
}

Base::Result<void> SetContentPresenterContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ContentPresenter content child is null");
    }
    return static_cast<ContentPresenter&>(owner).SetOwnedContent(
        child, *static_cast<Presentation::UIElement*>(child.Get()));
}

Base::Result<void> ClearContentPresenterContent(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<ContentPresenter&>(owner).SetContent(nullptr);
}

Base::Result<void> AddTextBlockInline(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextBlock inline child is null");
    }
    auto& text = static_cast<TextBlock&>(owner);
    if (!text.PropertyRegistry().Types().IsDerivedFrom(
            child->RuntimeType(),
            Presentation::UIElement::StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextBlock inline content must be a UIElement");
    }
    return text.AddOwnedInline(
        child,
        *static_cast<Presentation::UIElement*>(
            child.Get()));
}

Base::Result<void> ClearTextBlockInlines(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<TextBlock&>(owner)
        .ClearOwnedInlines();
}

Base::Result<void> AddItemsControlItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& item,
    void*) noexcept {
    if (!item) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ItemsControl content item is null");
    }
    return static_cast<ItemsControl&>(owner).Items().Add(item);
}

Base::Result<void> ClearItemsControlItems(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ItemsControl&>(owner).Items().Reset();
    return {};
}

void OnItemsSourceChanged(
    Core::DependencyObject& object,
    const Base::Ref<Base::Object>&,
    const Base::Ref<Base::Object>& value) noexcept {
    Core::IItemsSource* source = nullptr;
    if (value) {
        if (value->RuntimeType() ==
            ObjectItemsSource::StaticTypeId()) {
            source = static_cast<ObjectItemsSource*>(value.Get());
        } else if (value->RuntimeType() ==
                   Presentation::GradientStopCollection::StaticTypeId()) {
            source = static_cast<Presentation::GradientStopCollection*>(
                value.Get());
        }
    }
    static_cast<void>(
        static_cast<ItemsControl&>(object)
            .SetItemsSource(source));
}

void OnItemTemplateChanged(
    Core::DependencyObject& object,
    const Base::Ref<DataTemplate>&,
    const Base::Ref<DataTemplate>& value) noexcept {
    static_cast<ItemsControl&>(object)
        .SetItemTemplate(value.Get());
}

void OnItemsPanelChanged(
    Core::DependencyObject& object,
    const Base::Ref<ItemsPanelTemplate>&,
    const Base::Ref<ItemsPanelTemplate>& value) noexcept {
    static_cast<ItemsControl&>(object)
        .SetItemsPanel(value.Get());
}

void OnItemContainerStyleChanged(
    Core::DependencyObject& object,
    const Base::Ref<Style>&,
    const Base::Ref<Style>& value) noexcept {
    static_cast<ItemsControl&>(object)
        .SetItemContainerStyle(value.Get());
}

Base::Result<void> AddTreeViewItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& item,
    void*) noexcept {
    if (!item) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TreeViewItem content item is null");
    }
    return static_cast<TreeViewItem&>(
        owner).Items().Add(item);
}

Base::Result<void> ClearTreeViewItems(
    Base::Object& owner,
    void*) noexcept {
    static_cast<TreeViewItem&>(
        owner).Items().Reset();
    return {};
}

Base::Result<void> AddGridViewColumn(
    Base::Object& owner,
    const Base::Ref<Base::Object>& item,
    void*) noexcept {
    if (!item ||
        item->RuntimeType() !=
            GridViewColumn::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "GridView content must be a GridViewColumn");
    }
    return static_cast<GridView&>(
        owner).AddColumn(
            Base::Ref<GridViewColumn>::
                FromBorrowed(
                    static_cast<GridViewColumn&>(
                        *item)));
}

Base::Result<void> ClearGridViewColumns(
    Base::Object& owner,
    void*) noexcept {
    static_cast<GridView&>(
        owner).ClearColumns();
    return {};
}

Base::Result<void> AddTabControlItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& item,
    void*) noexcept {
    if (!item) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TabControl item is null");
    }
    return static_cast<TabControl&>(owner).
        AddOwnedTab(
            Base::Ref<TabItem>::FromBorrowed(
                static_cast<TabItem&>(*item)));
}

Base::Result<void> ClearTabControlItems(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<TabControl&>(owner).
        ClearOwnedTabs();
}

} // namespace

Base::Result<void> Detail::PopulateControlsMetadata(
    Core::MetadataContext& context) noexcept {
    using namespace Aero::Core;
    Base::Result<void> status;

    auto orientation = Describe<Orientation>(context);
    orientation
        .Value("Horizontal", Orientation::Horizontal)
        .Value("Vertical", Orientation::Vertical);
    status = orientation.Result();
    if (!status) return status.GetStatus();

    auto menuItemRole = Describe<MenuItemRole>(context);
    menuItemRole
        .Value("TopLevelItem", MenuItemRole::TopLevelItem)
        .Value("TopLevelHeader", MenuItemRole::TopLevelHeader)
        .Value("SubmenuItem", MenuItemRole::SubmenuItem)
        .Value("SubmenuHeader", MenuItemRole::SubmenuHeader);
    status = menuItemRole.Result();
    if (!status) return status.GetStatus();

    auto gridViewColumnHeaderRole =
        Describe<GridViewColumnHeaderRole>(context);
    gridViewColumnHeaderRole
        .Value("Normal", GridViewColumnHeaderRole::Normal)
        .Value("Floating", GridViewColumnHeaderRole::Floating)
        .Value("Padding", GridViewColumnHeaderRole::Padding);
    status = gridViewColumnHeaderRole.Result();
    if (!status) return status.GetStatus();

    auto penLineJoin =
        Describe<PenLineJoin>(context);
    penLineJoin
        .Value("Miter", PenLineJoin::Miter)
        .Value("Bevel", PenLineJoin::Bevel)
        .Value("Round", PenLineJoin::Round);
    status = penLineJoin.Result();
    if (!status) return status.GetStatus();

    auto penLineCap = Describe<PenLineCap>(context);
    penLineCap
        .Value("Flat", PenLineCap::Flat)
        .Value("Square", PenLineCap::Square)
        .Value("Round", PenLineCap::Round)
        .Value("Triangle", PenLineCap::Triangle);
    status = penLineCap.Result();
    if (!status) return status.GetStatus();

    auto tickPlacement =
        Describe<TickPlacement>(context);
    tickPlacement
        .Value("None", TickPlacement::None)
        .Value("TopLeft", TickPlacement::TopLeft)
        .Value(
            "BottomRight",
            TickPlacement::BottomRight)
        .Value("Both", TickPlacement::Both);
    status = tickPlacement.Result();
    if (!status) return status.GetStatus();

    auto tickBarPlacement =
        Describe<TickBarPlacement>(context);
    tickBarPlacement
        .Value("Top", TickBarPlacement::Top)
        .Value("Bottom", TickBarPlacement::Bottom)
        .Value("Left", TickBarPlacement::Left)
        .Value("Right", TickBarPlacement::Right);
    status = tickBarPlacement.Result();
    if (!status) return status.GetStatus();

    auto scrollBarVisibility =
        Describe<ScrollBarVisibility>(context);
    scrollBarVisibility
        .Value(
            "Disabled",
            ScrollBarVisibility::Disabled)
        .Value("Auto", ScrollBarVisibility::Auto)
        .Value("Hidden", ScrollBarVisibility::Hidden)
        .Value("Visible", ScrollBarVisibility::Visible);
    status = scrollBarVisibility.Result();
    if (!status) return status.GetStatus();

    auto panningMode = Describe<PanningMode>(context);
    panningMode
        .Value("None", PanningMode::None)
        .Value("HorizontalOnly", PanningMode::HorizontalOnly)
        .Value("VerticalOnly", PanningMode::VerticalOnly)
        .Value("Both", PanningMode::Both)
        .Value("HorizontalFirst", PanningMode::HorizontalFirst)
        .Value("VerticalFirst", PanningMode::VerticalFirst);
    status = panningMode.Result();
    if (!status) return status.GetStatus();

    auto gridResizeDirection = Describe<GridResizeDirection>(context);
    gridResizeDirection
        .Value("Auto", GridResizeDirection::Auto)
        .Value("Columns", GridResizeDirection::Columns)
        .Value("Rows", GridResizeDirection::Rows);
    status = gridResizeDirection.Result();
    if (!status) return status.GetStatus();

    auto gridResizeBehavior = Describe<GridResizeBehavior>(context);
    gridResizeBehavior
        .Value(
            "BasedOnAlignment",
            GridResizeBehavior::BasedOnAlignment)
        .Value("CurrentAndNext", GridResizeBehavior::CurrentAndNext)
        .Value(
            "PreviousAndCurrent",
            GridResizeBehavior::PreviousAndCurrent)
        .Value("PreviousAndNext", GridResizeBehavior::PreviousAndNext);
    status = gridResizeBehavior.Result();
    if (!status) return status.GetStatus();

    auto dock = Describe<Dock>(context);
    dock
        .Value("Left", Dock::Left)
        .Value("Top", Dock::Top)
        .Value("Right", Dock::Right)
        .Value("Bottom", Dock::Bottom);
    status = dock.Result();
    if (!status) return status.GetStatus();

    auto textWrapping = Describe<Text::TextWrapping>(context);
    textWrapping
        .Value("NoWrap", Text::TextWrapping::NoWrap)
        .Value("Wrap", Text::TextWrapping::Wrap)
        .Value(
            "WrapWithOverflow",
            Text::TextWrapping::WrapWithOverflow);
    status = textWrapping.Result();
    if (!status) return status.GetStatus();

    auto textTrimming = Describe<Text::TextTrimming>(context);
    textTrimming
        .Value("None", Text::TextTrimming::None)
        .Value(
            "CharacterEllipsis",
            Text::TextTrimming::CharacterEllipsis)
        .Value(
            "WordEllipsis",
            Text::TextTrimming::WordEllipsis);
    status = textTrimming.Result();
    if (!status) return status.GetStatus();

    auto textAlignment = Describe<Text::TextAlignment>(context);
    textAlignment
        .Value("Left", Text::TextAlignment::Start)
        .Value("Center", Text::TextAlignment::Center)
        .Value("Right", Text::TextAlignment::End)
        .Value("Justify", Text::TextAlignment::Justify);
    status = textAlignment.Result();
    if (!status) return status.GetStatus();

    auto fontWeight = Describe<FontWeight>(context);
    fontWeight
        .Value("Normal", FontWeight::Normal)
        .Value("SemiBold", FontWeight::SemiBold)
        .Value("Bold", FontWeight::Bold);
    status = fontWeight.Result();
    if (!status) return status.GetStatus();

    auto textElement = Describe<TextElement>(context, TypeFlags::Abstract);
    textElement
        .Property(
            TextElement::FontWeightProperty,
            PropertyOptions(FontWeight::Normal).Inherits())
        .Property(
            TextElement::ForegroundProperty,
            PropertyOptions(Base::Ref<Brush>{}).Inherits().AffectsRender())
        .Property(
            TextElement::FontSizeProperty,
            PropertyOptions(16.0).Inherits().AffectsMeasure()
                .Validate(&ValidatePositiveFiniteDouble));
    status = textElement.Result();
    if (!status) return status.GetStatus();

    auto fontStyle = Describe<Text::FontStyle>(context);
    fontStyle
        .Value("Normal", Text::FontStyle::Normal)
        .Value("Italic", Text::FontStyle::Italic)
        .Value("Oblique", Text::FontStyle::Oblique);
    status = fontStyle.Result();
    if (!status) return status.GetStatus();

    auto textDecorations =
        Describe<TextDecorations>(context);
    textDecorations
        .Value("None", TextDecorations::None)
        .Value(
            "Underline",
            TextDecorations::Underline);
    status = textDecorations.Result();
    if (!status) return status.GetStatus();

    auto gridLength = Describe<GridLength>(context);
    gridLength
        .ValueSemantics({
            sizeof(GridLength),
            alignof(GridLength),
            nullptr,
            nullptr,
            &EqualGridLength,
            nullptr,
            true})
        .TextConverter<&ConvertGridLength>();
    status = gridLength.Result();
    if (!status) return status.GetStatus();

    auto clickMode = Describe<ClickMode>(context);
    clickMode
        .Value("Release", ClickMode::Release)
        .Value("Press", ClickMode::Press)
        .Value("Hover", ClickMode::Hover);
    status = clickMode.Result();
    if (!status) return status.GetStatus();

    auto selectionMode = Describe<SelectionMode>(context);
    selectionMode
        .Value("Single", SelectionMode::Single)
        .Value("Multiple", SelectionMode::Multiple)
        .Value("Extended", SelectionMode::Extended);
    status = selectionMode.Result();
    if (!status) return status.GetStatus();

    auto expandDirection =
        Describe<ExpandDirection>(context);
    expandDirection
        .Value("Down", ExpandDirection::Down)
        .Value("Up", ExpandDirection::Up)
        .Value("Left", ExpandDirection::Left)
        .Value("Right", ExpandDirection::Right);
    status = expandDirection.Result();
    if (!status) return status.GetStatus();

    auto placementMode =
        Describe<PlacementMode>(context);
    placementMode
        .Value("Bottom", PlacementMode::Bottom)
        .Value("Top", PlacementMode::Top)
        .Value("Left", PlacementMode::Left)
        .Value("Right", PlacementMode::Right)
        .Value("Center", PlacementMode::Center)
        .Value("Mouse", PlacementMode::Mouse);
    status = placementMode.Result();
    if (!status) return status.GetStatus();

    auto popupAnimation =
        Describe<PopupAnimation>(context);
    popupAnimation
        .Value("None", PopupAnimation::None)
        .Value("Fade", PopupAnimation::Fade)
        .Value("Slide", PopupAnimation::Slide)
        .Value("Scroll", PopupAnimation::Scroll);
    status = popupAnimation.Result();
    if (!status) return status.GetStatus();

    status = Describe<ScrollChangedEventArgs>(context).Result();
    if (!status) return status.GetStatus();
    status = Describe<RangeValueChangedEventArgs>(
        context).Result();
    if (!status) return status.GetStatus();

    auto frameworkTemplate = Describe<FrameworkTemplate>(
        context, TypeFlags::Abstract);
    frameworkTemplate
        .Property<
            Base::Ref<Presentation::ResourceDictionary>,
            &FrameworkTemplate::SetResources>(
                "Resources",
                PropertyFlags::Structural)
        .Collection<Base::Object>(
            "Triggers",
            &AddTemplateTrigger,
            &ClearTemplateTriggers);
    status = frameworkTemplate.Result();
    if (!status) return status.GetStatus();

    auto controlTemplate = Describe<ControlTemplate>(context);
    controlTemplate
        .Property<
            Core::TypeReference,
            &GetControlTemplateTargetType,
            &SetControlTemplateTargetType>(
            "TargetType",
            PropertyFlags::None)
        .Collection<Base::Object>(
            "VisualStateGroups",
            &AddTemplateVisualStateGroup,
            &ClearTemplateVisualStateGroups)
        .Content<Base::Object>(
            "VisualTree",
            ContentKind::Single,
            &SetDeferredTemplateVisualTree<
                ControlTemplate>,
            &ClearDeferredTemplateVisualTree<
                ControlTemplate>,
            ContentFlags::Visual)
        .Factory();
    status = controlTemplate.Result();
    if (!status) return status.GetStatus();

    auto dataTemplate = Describe<DataTemplate>(context);
    dataTemplate
        .Property<
            Core::TypeReference,
            &GetDataTemplateType,
            &SetDataTemplateType>(
            "DataType",
            PropertyFlags::None)
        .Property<
            Base::Ref<Base::Object>,
            &DataTemplate::HierarchicalItemsSource,
            &DataTemplate::SetHierarchicalItemsSource>(
                "ItemsSource", PropertyFlags::None)
        .Property<
            Base::Ref<Base::Object>,
            &DataTemplate::HierarchicalItemTemplate,
            &DataTemplate::SetHierarchicalItemTemplate>(
                "ItemTemplate", PropertyFlags::None)
        .Property<
            Base::Ref<Presentation::ResourceDictionary>,
            &DataTemplate::SetResources>(
                "Resources",
                PropertyFlags::Structural)
        .Collection<Presentation::TriggerBase>(
            "Triggers",
            &AddDataTemplateTrigger,
            &ClearDataTemplateTriggers)
        .Content<Base::Object>(
            "VisualTree",
            ContentKind::Single,
            &SetDeferredTemplateVisualTree<DataTemplate>,
            &ClearDeferredTemplateVisualTree<DataTemplate>,
            ContentFlags::Visual)
        .Factory();
    status = dataTemplate.Result();
    if (!status) return status.GetStatus();

    auto itemsPanelTemplate =
        Describe<ItemsPanelTemplate>(context);
    itemsPanelTemplate
        .Property<
            Base::Ref<Presentation::ResourceDictionary>,
            &ItemsPanelTemplate::SetResources>(
                "Resources",
                PropertyFlags::Structural)
        .Content<Base::Object>(
            "VisualTree",
            ContentKind::Single,
            &SetDeferredTemplateVisualTree<ItemsPanelTemplate>,
            &ClearDeferredTemplateVisualTree<ItemsPanelTemplate>,
            ContentFlags::Visual)
        .Factory();
    status = itemsPanelTemplate.Result();
    if (!status) return status.GetStatus();

    auto panel = Describe<Panel>(
        context, TypeFlags::Abstract);
    panel
        .Property(
            Panel::BackgroundProperty,
            PropertyOptions(
                Base::Ref<Presentation::Brush>{})
                .AffectsRender())
        .Property(
            Panel::ZIndexProperty,
            PropertyOptions(std::int32_t{0})
                .AffectsParentArrange())
        .Property(
            Panel::IsItemsHostProperty,
            PropertyOptions(false))
        .Content<Presentation::UIElement>(
            "Children", ContentKind::Collection,
            &SetPanelContent, &ClearPanelContent,
            ContentFlags::Visual);
    status = panel.Result();
    if (!status) return status.GetStatus();

    auto decorator = Describe<Decorator>(context);
    decorator.Content<Presentation::UIElement>(
        "Content", ContentKind::Single,
        &SetDecoratorContent, &ClearDecoratorContent,
        ContentFlags::Visual)
        .Factory();
    status = decorator.Result();
    if (!status) return status.GetStatus();

    auto viewbox = Describe<Viewbox>(context);
    viewbox
        .Property(
            Viewbox::StretchProperty,
            PropertyOptions(Stretch::Uniform)
                .AffectsMeasure())
        .Property(
            Viewbox::StretchDirectionProperty,
            PropertyOptions(StretchDirection::Both)
                .AffectsMeasure())
        .Factory();
    status = viewbox.Result();
    if (!status) return status.GetStatus();

    auto control = Describe<Control>(context);
    control
        .Override(
            UIElement::FocusableProperty,
            PropertyOptions(true))
        .Property(
            Control::BackgroundProperty,
            PropertyOptions(
                Base::Ref<Presentation::Brush>{})
                .AffectsRender())
        .Property(
            Control::BorderBrushProperty,
            PropertyOptions(
                Base::Ref<Presentation::Brush>{})
                .AffectsRender())
        .Property(
            Control::BorderThicknessProperty,
            PropertyOptions(Presentation::Thickness{})
                .AffectsMeasure()
                .AffectsRender()
                .Validate(&ValidateThicknessValue))
        .Property(
            Control::PaddingProperty,
            PropertyOptions(Presentation::Thickness{})
                .AffectsMeasure()
                .Validate(&ValidateThicknessValue))
        .Property(
            Control::FontWeightProperty,
            PropertyOptions(FontWeight::Normal)
                .AffectsMeasure())
        .Property(
            Control::HorizontalContentAlignmentProperty,
            PropertyOptions(
                Presentation::HorizontalAlignment::Left)
                .AffectsArrange())
        .Property(
            Control::VerticalContentAlignmentProperty,
            PropertyOptions(
                Presentation::VerticalAlignment::Top)
                .AffectsArrange())
        .Property(
            Control::FontSizeProperty,
            PropertyOptions(16.0)
                .Inherits()
                .AffectsMeasure()
                .Validate(
                    &ValidatePositiveFiniteDouble))
        .Property(
            Control::FocusVisualStyleProperty,
            PropertyOptions(
                Base::Ref<Presentation::Style>{}))
        .Property(
            Control::OverridesDefaultStyleProperty,
            PropertyOptions(false))
        .Property(
            Control::TemplateProperty,
            PropertyOptions(Base::Ref<ControlTemplate>{})
                .AffectsMeasure())
        .Factory(&CreateBasicControl);
    status = control.Result();
    if (!status) return status.GetStatus();

    auto contentControl = Describe<ContentControl>(
        context, TypeFlags::Abstract);
    contentControl
        .Property(
            ContentControl::ContentProperty,
            PropertyOptions(
                Core::Value::NullObject(
                    Core::TypeOf<Base::Object>()))
                .AffectsMeasure()
                .Structural()
                .Changed(
                    &ContentControl::
                        OnContentPropertyChanged))
        .Property(
            ContentControl::ContentTemplateProperty,
            PropertyOptions(Base::Ref<Base::Object>{})
                .AffectsMeasure())
        .Property(
            ContentControl::
                ContentTemplateSelectorProperty,
            PropertyOptions(
                Base::Ref<Base::Object>{})
                .AffectsMeasure())
        .ContentAccessor(
            MakeMemberId(
                ContentControl::StaticTypeId(),
                MemberKind::Property,
                "Content"),
            ContentKind::Single,
            &SetContentControlContent,
            &ClearContentControlContent,
            ContentFlags::Visual);
    status = contentControl.Result();
    if (!status) return status.GetStatus();

    auto window = Describe<App::Window>(context);
    window
        .Property(
            App::Window::TitleProperty,
            PropertyOptions(Base::String{})
                .AffectsMeasure())
        .Factory();
    status = window.Result();
    if (!status) return status.GetStatus();

    auto buttonBase = Describe<ButtonBase>(
        context, TypeFlags::Abstract);
    buttonBase
        .Event(ButtonBase::ClickEvent)
        .Property(
            ButtonBase::ClickModeProperty,
            PropertyOptions(ClickMode::Release))
        .Property(
            ButtonBase::CommandProperty,
            PropertyOptions(Base::Ref<ICommand>{}))
        .Property(
            ButtonBase::CommandParameterProperty,
            PropertyOptions(Base::Ref<Base::Object>{}))
        .Property(
            ButtonBase::CommandTargetProperty,
            PropertyOptions(Base::Ref<UIElement>{}))
        .Override(
            UIElement::IsEnabledProperty,
            PropertyOptions(true)
                .Inherits()
                .AffectsRender()
                .Coerce(&CoerceButtonEnabled))
        .Override(
            UIElement::IsTabStopProperty,
            PropertyOptions(true));
    status = buttonBase.Result();
    if (!status) return status.GetStatus();

    status = Describe<Button>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto repeatButton = Describe<RepeatButton>(context);
    repeatButton
        .Property(
            RepeatButton::DelayProperty,
            PropertyOptions(std::uint32_t{400}))
        .Property(
            RepeatButton::IntervalProperty,
            PropertyOptions(std::uint32_t{100})
                .Validate(&Validate::Positive<std::uint32_t>))
        .Override(
            ButtonBase::ClickModeProperty,
            PropertyOptions(ClickMode::Press))
        .Factory();
    status = repeatButton.Result();
    if (!status) return status.GetStatus();

    auto toggleButton = Describe<ToggleButton>(context);
    toggleButton
        .Event(ToggleButton::CheckedEvent)
        .Event(ToggleButton::UncheckedEvent)
        .Event(ToggleButton::IndeterminateEvent)
        .Property(
            ToggleButton::IsCheckedProperty,
            PropertyOptions(false)
                .BindsTwoWayByDefault()
                .AffectsRender())
        .Property(
            ToggleButton::IsThreeStateProperty,
            PropertyOptions(false)
                .AffectsRender())
        .Property(
            ToggleButton::IsIndeterminateProperty,
            PropertyOptions(false)
                .AffectsRender())
        .Factory();
    status = toggleButton.Result();
    if (!status) return status.GetStatus();

    status = Describe<CheckBox>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto radioButton = Describe<RadioButton>(context);
    radioButton
        .Property(
            RadioButton::GroupNameProperty,
            PropertyOptions(Base::String{}))
        .Factory();
    status = radioButton.Result();
    if (!status) return status.GetStatus();

    auto hyperlink = Describe<Hyperlink>(context);
    hyperlink
        .Property(
            Hyperlink::NavigateUriProperty,
            PropertyOptions(Base::String{}))
        .Property(
            Hyperlink::TextDecorationsProperty,
            PropertyOptions(TextDecorations::Underline)
                .AffectsRender())
        .Override(
            UIElement::IsTabStopProperty,
            PropertyOptions(true))
        .Factory();
    status = hyperlink.Result();
    if (!status) return status.GetStatus();

    status = Describe<ScrollContentPresenter>(context)
        .Property(
            ScrollContentPresenter::
                CanContentScrollProperty,
            PropertyOptions(false)
                .AffectsMeasure())
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto scrollViewer = Describe<ScrollViewer>(context);
    scrollViewer
        .Event(ScrollViewer::ScrollChangedEvent)
        .Property(
            ScrollViewer::HorizontalOffsetProperty,
            PropertyOptions(0.0)
                .Validate(&Validate::NonNegative<double>))
        .Property(
            ScrollViewer::VerticalOffsetProperty,
            PropertyOptions(0.0)
                .Validate(&Validate::NonNegative<double>))
        .Property(
            ScrollViewer::ExtentWidthProperty,
            PropertyOptions(0.0)
                .Validate(&Validate::NonNegative<double>))
        .Property(
            ScrollViewer::ExtentHeightProperty,
            PropertyOptions(0.0)
                .Validate(&Validate::NonNegative<double>))
        .Property(
            ScrollViewer::ViewportWidthProperty,
            PropertyOptions(0.0)
                .Validate(&Validate::NonNegative<double>))
        .Property(
            ScrollViewer::ViewportHeightProperty,
            PropertyOptions(0.0)
                .Validate(&Validate::NonNegative<double>))
        .Property(
            ScrollViewer::ScrollableWidthProperty,
            PropertyOptions(0.0)
                .Validate(&Validate::NonNegative<double>))
        .Property(
            ScrollViewer::ScrollableHeightProperty,
            PropertyOptions(0.0)
                .Validate(&Validate::NonNegative<double>))
        .Property(
            ScrollViewer::
                ComputedHorizontalScrollBarVisibilityProperty,
            PropertyOptions(Visibility::Collapsed)
                .AffectsMeasure()
                .AffectsRender())
        .Property(
            ScrollViewer::
                ComputedVerticalScrollBarVisibilityProperty,
            PropertyOptions(Visibility::Collapsed)
                .AffectsMeasure()
                .AffectsRender())
        .Property(
            ScrollViewer::
                HorizontalScrollBarVisibilityProperty,
            PropertyOptions(
                ScrollBarVisibility::Disabled)
                .AffectsMeasure()
                .AffectsRender()
                .Changed(
                    &OnScrollViewerVisibilityChanged))
        .Property(
            ScrollViewer::
                VerticalScrollBarVisibilityProperty,
            PropertyOptions(
                ScrollBarVisibility::Visible)
                .AffectsMeasure()
                .AffectsRender()
                .Changed(
                    &OnScrollViewerVisibilityChanged))
        .Property(
            ScrollViewer::CanHorizontallyScrollProperty,
            PropertyOptions(true)
                .AffectsMeasure())
        .Property(
            ScrollViewer::CanVerticallyScrollProperty,
            PropertyOptions(true)
                .AffectsMeasure())
        .Property(
            ScrollViewer::CanContentScrollProperty,
            PropertyOptions(false)
                .AffectsMeasure())
        .Property(
            ScrollViewer::PanningModeProperty,
            PropertyOptions(PanningMode::None))
        .Factory();
    status = scrollViewer.Result();
    if (!status) return status.GetStatus();

    status = Describe<Thumb>(context)
        .Property(
            Thumb::IsDraggingProperty,
            PropertyOptions(false)
                .AffectsRender())
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto track = Describe<Track>(context);
    track
        .Property(
            Track::OrientationProperty,
            PropertyOptions(Orientation::Vertical)
                .AffectsMeasure())
        .Property(
            Track::MinimumProperty,
            PropertyOptions(0.0)
                .AffectsArrange()
                .Validate(&Validate::Finite<double>))
        .Property(
            Track::MaximumProperty,
            PropertyOptions(1.0)
                .AffectsArrange()
                .Validate(&Validate::Finite<double>))
        .Property(
            Track::ValueProperty,
            PropertyOptions(0.0)
                .AffectsArrange()
                .BindsTwoWayByDefault()
                .Validate(&Validate::Finite<double>))
        .Property(
            Track::ViewportSizeProperty,
            PropertyOptions(0.0)
                .AffectsArrange()
                .Validate(&Validate::NonNegative<double>))
        .Property(
            Track::IsDirectionReversedProperty,
            PropertyOptions(false)
                .AffectsArrange())
        .Property<
            Base::Ref<RepeatButton>,
            &Track::SetDecreaseRepeatButton>(
                "DecreaseRepeatButton",
                PropertyFlags::Structural)
        .Property<
            Base::Ref<Thumb>,
            &Track::SetThumb>(
                "Thumb",
                PropertyFlags::Structural)
        .Property<
            Base::Ref<RepeatButton>,
            &Track::SetIncreaseRepeatButton>(
                "IncreaseRepeatButton",
                PropertyFlags::Structural)
        .Factory();
    status = track.Result();
    if (!status) return status.GetStatus();

    auto gridSplitter = Describe<GridSplitter>(context);
    gridSplitter
        .Property(
            GridSplitter::DragIncrementProperty,
            PropertyOptions(1.0)
                .Validate(&Validate::Positive<double>))
        .Property(
            GridSplitter::KeyboardIncrementProperty,
            PropertyOptions(10.0)
                .Validate(&Validate::Positive<double>))
        .Property(
            GridSplitter::ResizeDirectionProperty,
            PropertyOptions(GridResizeDirection::Auto))
        .Property(
            GridSplitter::ResizeBehaviorProperty,
            PropertyOptions(GridResizeBehavior::BasedOnAlignment))
        .Property(
            GridSplitter::ShowsPreviewProperty,
            PropertyOptions(false))
        .Property(
            GridSplitter::PreviewStyleProperty,
            PropertyOptions(Base::Ref<Presentation::Style>{}))
        .Factory();
    status = gridSplitter.Result();
    if (!status) return status.GetStatus();

    auto rangeBase = Describe<RangeBase>(
        context, TypeFlags::Abstract);
    rangeBase
        .Event(RangeBase::ValueChangedEvent)
        .Property(
            RangeBase::MinimumProperty,
            PropertyOptions(0.0)
                .AffectsArrange()
                .Validate(&Validate::Finite<double>)
                .Coerce(&CoerceRangeMinimum))
        .Property(
            RangeBase::MaximumProperty,
            PropertyOptions(1.0)
                .AffectsArrange()
                .Validate(&Validate::Finite<double>)
                .Coerce(&CoerceRangeMaximum))
        .Property(
            RangeBase::ValueProperty,
            PropertyOptions(0.0)
                .AffectsArrange()
                .BindsTwoWayByDefault()
                .Validate(&Validate::Finite<double>)
                .Coerce(&CoerceRangeValue));
    status = rangeBase.Result();
    if (!status) return status.GetStatus();

    auto scrollBar = Describe<ScrollBar>(context);
    scrollBar
        .Property(
            ScrollBar::OrientationProperty,
            PropertyOptions(Orientation::Vertical)
                .AffectsMeasure())
        .Property(
            ScrollBar::ViewportSizeProperty,
            PropertyOptions(0.0)
                .AffectsArrange()
                .Validate(&Validate::NonNegative<double>))
        .Property(
            ScrollBar::SmallChangeProperty,
            PropertyOptions(16.0)
                .Validate(&Validate::Positive<double>))
        .Property(
            ScrollBar::LargeChangeProperty,
            PropertyOptions(0.0)
                .Validate(&Validate::NonNegative<double>))
        .Factory();
    status = scrollBar.Result();
    if (!status) return status.GetStatus();

    auto slider = Describe<Slider>(context);
    slider
        .Property(
            Slider::OrientationProperty,
            PropertyOptions(Orientation::Horizontal)
                .AffectsMeasure())
        .Property(
            Slider::SmallChangeProperty,
            PropertyOptions(1.0)
                .Validate(&Validate::Positive<double>))
        .Property(
            Slider::LargeChangeProperty,
            PropertyOptions(10.0)
                .Validate(&Validate::Positive<double>))
        .Property(
            Slider::TickPlacementProperty,
            PropertyOptions(TickPlacement::None)
                .AffectsRender())
        .Property(
            Slider::TickFrequencyProperty,
            PropertyOptions(1.0)
                .AffectsRender()
                .Validate(&Validate::Positive<double>))
        .Property(
            Slider::TicksProperty,
            PropertyOptions(Base::String{})
                .AffectsRender()
                .Validate(&ValidateSliderTicks))
        .Property(
            Slider::IsSnapToTickEnabledProperty,
            PropertyOptions(false))
        .Property(
            Slider::IsDirectionReversedProperty,
            PropertyOptions(false)
                .AffectsArrange()
                .AffectsRender())
        .Property(
            Slider::IsMoveToPointEnabledProperty,
            PropertyOptions(false))
        .Override(
            UIElement::IsTabStopProperty,
            PropertyOptions(true))
        .Factory();
    status = slider.Result();
    if (!status) return status.GetStatus();

    auto tickBar = Describe<TickBar>(context);
    tickBar
        .Property(
            TickBar::FillProperty,
            PropertyOptions(Base::Ref<Presentation::Brush>{})
                .AffectsRender())
        .Property(
            TickBar::PlacementProperty,
            PropertyOptions(TickBarPlacement::Top)
                .AffectsRender())
        .Factory();
    status = tickBar.Result();
    if (!status) return status.GetStatus();

    auto progressBar = Describe<ProgressBar>(context);
    progressBar
        .Property(
            ProgressBar::IsIndeterminateProperty,
            PropertyOptions(false)
                .AffectsRender())
        .Property(
            ProgressBar::OrientationProperty,
            PropertyOptions(Orientation::Horizontal)
                .AffectsMeasure())
        .Factory();
    status = progressBar.Result();
    if (!status) return status.GetStatus();

    status = Describe<ItemContainer>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    status = Describe<ObjectItemsSource>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto alternationConverter =
        Describe<AlternationConverter>(context);
    alternationConverter
        .Content<Base::Object>(
            "Values", ContentKind::Collection,
            [](Base::Object& owner,
               const Base::Ref<Base::Object>& value,
               void*) noexcept {
                return static_cast<AlternationConverter&>(owner)
                    .AddValue(value);
            },
            [](Base::Object& owner, void*) noexcept {
                static_cast<AlternationConverter&>(owner)
                    .ClearValues();
                return Base::Result<void>{};
            })
        .Factory();
    status = alternationConverter.Result();
    if (!status) return status.GetStatus();

    status = Describe<BoxedItemValue>(context)
        .Result();
    if (!status) return status.GetStatus();

    auto itemsControl = Describe<ItemsControl>(context);
    itemsControl
        .Property(
            ItemsControl::ItemCountProperty,
            PropertyOptions(std::uint32_t{0}))
        .Property(
            ItemsControl::HasItemsProperty,
            PropertyOptions(false))
        .Property(
            ItemsControl::ItemsSourceProperty,
            PropertyOptions(
                Base::Ref<Base::Object>{})
                .AffectsMeasure()
                .Changed(&OnItemsSourceChanged))
        .Property(
            ItemsControl::AlternationCountProperty,
            PropertyOptions(std::uint32_t{0})
                .AffectsMeasure())
        .Property(
            ItemsControl::ItemTemplateProperty,
            PropertyOptions(
                Base::Ref<DataTemplate>{})
                .AffectsMeasure()
                .Changed(&OnItemTemplateChanged))
        .Property(
            ItemsControl::ItemsPanelProperty,
            PropertyOptions(
                Base::Ref<ItemsPanelTemplate>{})
                .AffectsMeasure()
                .Changed(&OnItemsPanelChanged))
        .Property(
            ItemsControl::ItemContainerStyleProperty,
            PropertyOptions(Base::Ref<Style>{})
                .AffectsMeasure()
                .Changed(
                    &OnItemContainerStyleChanged))
        .Content<Base::Object>(
            "Items", ContentKind::Collection,
            &AddItemsControlItem, &ClearItemsControlItems)
        .Factory();
    status = itemsControl.Result();
    if (!status) return status.GetStatus();

    auto headeredItemsControl = Describe<HeaderedItemsControl>(
        context, TypeFlags::Abstract);
    headeredItemsControl
        .Property(
            HeaderedItemsControl::HeaderProperty,
            PropertyOptions(Base::String{}).AffectsMeasure())
        .Property(
            HeaderedItemsControl::HeaderTemplateProperty,
            PropertyOptions(Base::Ref<DataTemplate>{}).AffectsMeasure());
    status = headeredItemsControl.Result();
    if (!status) return status.GetStatus();

    status = Describe<ItemsPresenter>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto selector = Describe<Selector>(
        context, TypeFlags::Abstract);
    selector
        .Event(
            Selector::
                SelectionChangedRoutedEvent)
        .Property(
            Selector::SelectionModeProperty,
            PropertyOptions(SelectionMode::Single))
        .Property(
            Selector::SelectedIndexProperty,
            PropertyOptions(UINT32_MAX)
                .BindsTwoWayByDefault())
        .Property(
            Selector::SelectedItemProperty,
            PropertyOptions(Base::Ref<Base::Object>{})
                .BindsTwoWayByDefault()
                .Coerce(&CoerceSelectedObject))
        .Property(
            Selector::SelectedValueProperty,
            PropertyOptions(Base::Ref<Base::Object>{})
                .BindsTwoWayByDefault()
                .Coerce(&CoerceSelectedObject))
        .Property(
            Selector::SelectedValuePathProperty,
            PropertyOptions(Base::String{}))
        .Property(
            Selector::IsSelectedProperty,
            PropertyOptions(false)
                .AffectsRender()
                .BindsTwoWayByDefault());
    status = selector.Result();
    if (!status) return status.GetStatus();

    status = Describe<ListBox>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto listBoxItem = Describe<ListBoxItem>(context);
    listBoxItem
        .Property(
            ListBoxItem::IsSelectedProperty,
            PropertyOptions(false)
                .AffectsRender()
                .BindsTwoWayByDefault())
        .Override(
            Presentation::UIElement::IsTabStopProperty,
            PropertyOptions(true))
        .Factory();
    status = listBoxItem.Result();
    if (!status) return status.GetStatus();

    auto comboBox = Describe<ComboBox>(context);
    comboBox
        .Event(ComboBox::DropDownOpenedEvent)
        .Event(ComboBox::DropDownClosedEvent)
        .Property(
            ComboBox::IsDropDownOpenProperty,
            PropertyOptions(false)
                .AffectsMeasure()
                .AffectsRender()
                .BindsTwoWayByDefault())
        .Property(
            ComboBox::MaxDropDownHeightProperty,
            PropertyOptions(240.0)
                .AffectsMeasure()
                .Validate(
                    &Validate::Positive<double>))
        .Property(
            ComboBox::IsEditableProperty,
            PropertyOptions(false)
                .AffectsMeasure()
                .AffectsRender())
        .Property(
            ComboBox::IsReadOnlyProperty,
            PropertyOptions(false)
                .AffectsRender())
        .Property(
            ComboBox::TextProperty,
            PropertyOptions(Base::String{})
                .AffectsMeasure()
                .BindsTwoWayByDefault())
        .Property(
            ComboBox::PlaceholderProperty,
            PropertyOptions(Base::String{})
                .AffectsMeasure()
                .AffectsRender())
        .Property(
            ComboBox::SelectionBoxTextProperty,
            PropertyOptions(Base::String{}))
        .Property(
            ComboBox::SelectionBoxItemProperty,
            PropertyOptions(
                Core::Value::NullObject(
                    Core::TypeOf<Base::Object>())))
        .Override(
            Presentation::UIElement::
                IsTabStopProperty,
            PropertyOptions(true))
        .Factory();
    status = comboBox.Result();
    if (!status) return status.GetStatus();

    auto comboBoxItem =
        Describe<ComboBoxItem>(context);
    comboBoxItem
        .Property(
            ComboBoxItem::IsSelectedProperty,
            PropertyOptions(false)
                .AffectsRender()
                .BindsTwoWayByDefault())
        .Override(
            Presentation::UIElement::
                IsTabStopProperty,
            PropertyOptions(true))
        .Factory();
    status = comboBoxItem.Result();
    if (!status) return status.GetStatus();

    status = Describe<UserControl>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    status = Describe<Page>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto popup = Describe<Popup>(context);
    popup
        .Event(Popup::OpenedEvent)
        .Event(Popup::ClosedEvent)
        .Property(
            Popup::IsOpenProperty,
            PropertyOptions(false)
                .AffectsMeasure()
                .AffectsRender()
                .BindsTwoWayByDefault())
        .Property(
            Popup::PlacementProperty,
            PropertyOptions(
                PlacementMode::Bottom)
                .AffectsArrange())
        .Property(
            Popup::HorizontalOffsetProperty,
            PropertyOptions(0.0)
                .AffectsArrange()
                .Validate(
                    &Validate::Finite<double>))
        .Property(
            Popup::VerticalOffsetProperty,
            PropertyOptions(0.0)
                .AffectsArrange()
                .Validate(
                    &Validate::Finite<double>))
        .Property(
            Popup::StaysOpenProperty,
            PropertyOptions(true))
        .Property(
            Popup::
                MatchPlacementTargetWidthProperty,
            PropertyOptions(false)
                .AffectsArrange())
        .Property(
            Popup::PlacementTargetProperty,
            PropertyOptions(
                Base::Ref<UIElement>{})
                .AffectsArrange())
        .Property(
            Popup::PopupAnimationProperty,
            PropertyOptions(
                PopupAnimation::None)
                .AffectsRender())
        .Property(
            Popup::AllowsTransparencyProperty,
            PropertyOptions(false)
                .AffectsRender())
        .Factory();
    status = popup.Result();
    if (!status) return status.GetStatus();

    auto treeView = Describe<TreeView>(context);
    treeView
        .Event(
            TreeView::SelectedItemChangedEvent)
        .Property(
            TreeView::SelectedItemProperty,
            PropertyOptions(
                Base::Ref<Base::Object>{}))
        .Factory();
    status = treeView.Result();
    if (!status) return status.GetStatus();

    auto treeViewItem =
        Describe<TreeViewItem>(context);
    treeViewItem
        .Event(TreeViewItem::ExpandedEvent)
        .Event(TreeViewItem::CollapsedEvent)
        .Event(TreeViewItem::SelectedEvent)
        .Event(TreeViewItem::UnselectedEvent)
        .Property(
            TreeViewItem::HeaderProperty,
            PropertyOptions(Base::String{})
                .AffectsMeasure())
        .Property(
            TreeViewItem::IconProperty,
            PropertyOptions(Base::String{})
                .AffectsMeasure())
        .Property(
            TreeViewItem::HeaderTemplateProperty,
            PropertyOptions(
                Base::Ref<DataTemplate>{})
                .AffectsMeasure())
        .Property(
            TreeViewItem::IsExpandedProperty,
            PropertyOptions(false)
                .AffectsMeasure()
                .BindsTwoWayByDefault())
        .Property(
            TreeViewItem::IsSelectedProperty,
            PropertyOptions(false)
                .AffectsRender()
                .BindsTwoWayByDefault())
        .Property(
            TreeViewItem::HasItemsProperty,
            PropertyOptions(false))
        .Property(
            TreeViewItem::ItemsPanelProperty,
            PropertyOptions(
                Base::Ref<ItemsPanelTemplate>{})
                .AffectsMeasure())
        .Override(
            Presentation::UIElement::
                IsTabStopProperty,
            PropertyOptions(true))
        .Content<Base::Object>(
            "Items", ContentKind::Collection,
            &AddTreeViewItem,
            &ClearTreeViewItems)
        .Factory();
    status = treeViewItem.Result();
    if (!status) return status.GetStatus();

    status = Describe<Menu>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto menuItem = Describe<MenuItem>(context);
    menuItem
        .Event(MenuItem::ClickEvent)
        .Property(
            MenuItem::InputGestureTextProperty,
            PropertyOptions(Base::String{})
                .AffectsMeasure())
        .Property(
            MenuItem::IsCheckableProperty,
            PropertyOptions(false)
                .AffectsMeasure())
        .Property(
            MenuItem::IsCheckedProperty,
            PropertyOptions(false)
                .AffectsRender()
                .BindsTwoWayByDefault())
        .Property(
            MenuItem::IsHighlightedProperty,
            PropertyOptions(false).AffectsRender())
        .Property(
            MenuItem::IsSubmenuOpenProperty,
            PropertyOptions(false)
                .AffectsMeasure()
                .AffectsRender()
                .BindsTwoWayByDefault())
        .Property(
            MenuItem::RoleProperty,
            PropertyOptions(MenuItemRole::TopLevelItem)
                .AffectsMeasure()
                .AffectsRender())
        .Property(
            MenuItem::CommandProperty,
            PropertyOptions(
                Base::Ref<ICommand>{}))
        .Property(
            MenuItem::CommandParameterProperty,
            PropertyOptions(
                Base::Ref<Base::Object>{}))
        .Factory();
    status = menuItem.Result();
    if (!status) return status.GetStatus();

    auto contextMenu =
        Describe<ContextMenu>(context);
    contextMenu
        .Event(ContextMenu::OpenedEvent)
        .Event(ContextMenu::ClosedEvent)
        .Property(
            ContextMenu::IsOpenProperty,
            PropertyOptions(false)
                .AffectsMeasure()
                .AffectsRender()
                .BindsTwoWayByDefault())
        .Property(
            ContextMenu::PlacementTargetProperty,
            PropertyOptions(
                Base::Ref<UIElement>{}))
        .Factory();
    status = contextMenu.Result();
    if (!status) return status.GetStatus();

    auto contextMenuService =
        Describe<ContextMenuService>(
            context, TypeFlags::Abstract);
    contextMenuService.Property(
        ContextMenuService::
            ContextMenuProperty,
        PropertyOptions(
            Base::Ref<ContextMenu>{}));
    status = contextMenuService.Result();
    if (!status) return status.GetStatus();

    auto gridViewColumnHeader =
        Describe<GridViewColumnHeader>(context);
    gridViewColumnHeader
        .Property(
            GridViewColumnHeader::RoleProperty,
            PropertyOptions(
                GridViewColumnHeaderRole::Normal))
        .Factory();
    status = gridViewColumnHeader.Result();
    if (!status) return status.GetStatus();

    auto gridViewColumn =
        Describe<GridViewColumn>(context);
    gridViewColumn
        .Property(
            GridViewColumn::HeaderProperty,
            PropertyOptions(Base::String{}))
        .Property(
            GridViewColumn::WidthProperty,
            PropertyOptions(100.0)
                .Validate(
                    &Validate::NonNegative<double>))
        .Property(
            GridViewColumn::CellTemplateProperty,
            PropertyOptions(
                Base::Ref<DataTemplate>{}))
        .Property(
            GridViewColumn::HeaderTemplateProperty,
            PropertyOptions(
                Base::Ref<DataTemplate>{}))
        .Property(
            GridViewColumn::
                DisplayMemberPathProperty,
            PropertyOptions(Base::String{}))
        .Property(
            GridViewColumn::
                DisplayMemberBindingProperty,
            PropertyOptions(
                Base::Ref<Presentation::BindingSpec>{}))
        .Property(
            GridViewColumn::HeaderContainerStyleProperty,
            PropertyOptions(Base::Ref<Style>{}))
        .Factory();
    status = gridViewColumn.Result();
    if (!status) return status.GetStatus();

    auto gridView = Describe<GridView>(context);
    gridView
        .Property<
            Base::Ref<Style>,
            &GridView::ColumnHeaderContainerStyle,
            &GridView::SetColumnHeaderContainerStyle>(
                "ColumnHeaderContainerStyle",
                PropertyFlags::Structural)
        .Content<Base::Object>(
            "Columns",
            ContentKind::Collection,
            &AddGridViewColumn,
            &ClearGridViewColumns)
        .Factory();
    status = gridView.Result();
    if (!status) return status.GetStatus();

    auto gridViewHeaderPresenter =
        Describe<GridViewHeaderRowPresenter>(context);
    gridViewHeaderPresenter
        .Property(
            GridViewHeaderRowPresenter::
                AllowsColumnReorderProperty,
            PropertyOptions(false))
        .Property(
            GridViewHeaderRowPresenter::
                ColumnHeaderContainerStyleProperty,
            PropertyOptions(Base::Ref<Base::Object>{}))
        .Property(
            GridViewHeaderRowPresenter::
                ColumnHeaderContextMenuProperty,
            PropertyOptions(Base::Ref<Base::Object>{}))
        .Property(
            GridViewHeaderRowPresenter::
                ColumnHeaderTemplateProperty,
            PropertyOptions(Base::Ref<Base::Object>{}))
        .Property(
            GridViewHeaderRowPresenter::
                ColumnHeaderTemplateSelectorProperty,
            PropertyOptions(Base::Ref<Base::Object>{}))
        .Property(
            GridViewHeaderRowPresenter::ColumnHeaderToolTipProperty,
            PropertyOptions(Base::Ref<Base::Object>{}))
        .Property(
            GridViewHeaderRowPresenter::ColumnsProperty,
            PropertyOptions(Base::Ref<Base::Object>{}))
        .Factory();
    status = gridViewHeaderPresenter.Result();
    if (!status) return status.GetStatus();

    auto gridViewRowPresenter =
        Describe<GridViewRowPresenter>(context);
    gridViewRowPresenter
        .Property(
            GridViewRowPresenter::ColumnsProperty,
            PropertyOptions(Base::Ref<Base::Object>{}))
        .Property(
            GridViewRowPresenter::ContentProperty,
            PropertyOptions(Base::Ref<Base::Object>{}))
        .Factory();
    status = gridViewRowPresenter.Result();
    if (!status) return status.GetStatus();

    status = Describe<ListView>(context)
        .Property(
            ListView::ViewProperty,
            PropertyOptions(
                Base::Ref<GridView>{})
                .AffectsMeasure())
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto listViewItem =
        Describe<ListViewItem>(context);
    listViewItem
        .Override(
            Presentation::UIElement::
                IsTabStopProperty,
            PropertyOptions(true))
        .Factory();
    status = listViewItem.Result();
    if (!status) return status.GetStatus();

    status = Describe<Separator>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto toolBar = Describe<ToolBar>(context);
    toolBar
        .Property(
            ToolBar::HeaderProperty,
            PropertyOptions(Core::Value::NullObject(
                Core::TypeOf<Base::Object>()))
                .AffectsMeasure())
        .Property(
            ToolBar::HeaderTemplateProperty,
            PropertyOptions(Base::Ref<DataTemplate>{})
                .AffectsMeasure())
        .Property(
            ToolBar::OrientationProperty,
            PropertyOptions(
                Orientation::Horizontal)
                .AffectsMeasure())
        .Property(
            ToolBar::
                OverflowCapacityProperty,
            PropertyOptions(UINT32_MAX)
                .AffectsMeasure())
        .Property(
            ToolBar::IsOverflowOpenProperty,
            PropertyOptions(false)
                .AffectsRender())
        .Property(
            ToolBar::
                HasOverflowItemsProperty,
            PropertyOptions(false))
        .Property(
            ToolBar::
                OverflowItemCountProperty,
            PropertyOptions(
                std::uint32_t{0U}))
        .Factory();
    status = toolBar.Result();
    if (!status) return status.GetStatus();

    status = Describe<ToolBarPanel>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();
    status = Describe<ToolBarOverflowPanel>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto toolBarTray = Describe<ToolBarTray>(context, TypeFlags::Abstract);
    toolBarTray.Property(
        ToolBarTray::IsLockedProperty,
        PropertyOptions(false));
    status = toolBarTray.Result();
    if (!status) return status.GetStatus();

    status = Describe<StatusBar>(context)
        .Property(
            StatusBar::
                IsSizingGripVisibleProperty,
            PropertyOptions(true)
                .AffectsRender())
        .Factory()
        .Result();
    if (!status) return status.GetStatus();
    status = Describe<StatusBarItem>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto toolTip = Describe<ToolTip>(context);
    toolTip
        .Property(
            ToolTip::InitialShowDelayProperty,
            PropertyOptions(
                std::uint32_t{500U}))
        .Property(
            ToolTip::ShowDurationProperty,
            PropertyOptions(
                std::uint32_t{5000U}))
        .Factory();
    status = toolTip.Result();
    if (!status) return status.GetStatus();

    auto toolTipService =
        Describe<ToolTipService>(
            context, TypeFlags::Abstract);
    toolTipService
        .Property(
            ToolTipService::ToolTipProperty,
            PropertyOptions(
                Base::Ref<ToolTip>{}))
        .Property(
            ToolTipService::
                InitialShowDelayProperty,
            PropertyOptions(
                std::uint32_t{500U}))
        .Property(
            ToolTipService::
                ShowDurationProperty,
            PropertyOptions(
                std::uint32_t{5000U}));
    status = toolTipService.Result();
    if (!status) return status.GetStatus();

    auto headered = Describe<HeaderedContentControl>(
        context, TypeFlags::Abstract);
    headered
        .Property(
            HeaderedContentControl::HeaderProperty,
            PropertyOptions(Core::Value::NullObject(
                Core::TypeOf<Base::Object>()))
                .AffectsMeasure())
        .Property(
            HeaderedContentControl::HeaderTemplateProperty,
            PropertyOptions(Base::Ref<DataTemplate>{})
                .AffectsMeasure());
    status = headered.Result();
    if (!status) return status.GetStatus();

    status = Describe<GroupBox>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    status = Describe<Label>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto expander = Describe<Expander>(context);
    expander
        .Event(Expander::ExpandedEvent)
        .Event(Expander::CollapsedEvent)
        .Property(
            Expander::IsExpandedProperty,
            PropertyOptions(false)
                .AffectsMeasure())
        .Property(
            Expander::ExpandDirectionProperty,
            PropertyOptions(ExpandDirection::Down)
                .AffectsMeasure())
        .Factory();
    status = expander.Result();
    if (!status) return status.GetStatus();

    auto tabItem = Describe<TabItem>(context);
    tabItem
        .Property(
            TabItem::IsSelectedProperty,
            PropertyOptions(false)
                .AffectsRender())
        .Factory();
    status = tabItem.Result();
    if (!status) return status.GetStatus();

    auto tabControl = Describe<TabControl>(context);
    tabControl
        .Event(TabControl::SelectionChangedEvent)
        .Property(
            TabControl::SelectedIndexProperty,
            PropertyOptions(UINT32_MAX)
                .AffectsMeasure()
                .BindsTwoWayByDefault())
        .Property(
            TabControl::SelectedContentProperty,
            PropertyOptions(
                Core::Value::NullObject(
                    Core::TypeOf<Base::Object>())))
        .Property(
            TabControl::ItemsSourceProperty,
            PropertyOptions(Base::Ref<Base::Object>{})
                .AffectsMeasure())
        .Property(
            TabControl::ItemTemplateProperty,
            PropertyOptions(Base::Ref<DataTemplate>{})
                .AffectsMeasure())
        .Property(
            TabControl::ContentTemplateProperty,
            PropertyOptions(Base::Ref<DataTemplate>{})
                .AffectsMeasure())
        .Property(
            TabControl::TabStripPlacementProperty,
            PropertyOptions(Dock::Top)
                .AffectsMeasure())
        .Content<TabItem>(
            "Items",
            ContentKind::Collection,
            &AddTabControlItem,
            &ClearTabControlItems,
            ContentFlags::Visual)
        .Factory();
    status = tabControl.Result();
    if (!status) return status.GetStatus();

    status = Describe<TabPanel>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto stackPanel = Describe<StackPanel>(context);
    stackPanel
        .Property(
            StackPanel::OrientationProperty,
            PropertyOptions(Orientation::Vertical)
                .AffectsMeasure())
        .Factory();
    status = stackPanel.Result();
    if (!status) return status.GetStatus();

    auto dockPanel = Describe<DockPanel>(context);
    dockPanel
        .Property(
            DockPanel::LastChildFillProperty,
            PropertyOptions(true)
                .AffectsArrange())
        .Property(
            DockPanel::DockProperty,
            PropertyOptions(Dock::Left)
                .AffectsParentMeasure())
        .Factory();
    status = dockPanel.Result();
    if (!status) return status.GetStatus();

    auto wrapPanel = Describe<WrapPanel>(context);
    wrapPanel
        .Property(
            WrapPanel::OrientationProperty,
            PropertyOptions(Orientation::Horizontal)
                .AffectsMeasure())
        .Property(
            WrapPanel::ItemWidthProperty,
            PropertyOptions(0.0)
                .AffectsMeasure()
                .Validate(&Validate::NonNegative<double>))
        .Property(
            WrapPanel::ItemHeightProperty,
            PropertyOptions(0.0)
                .AffectsMeasure()
                .Validate(&Validate::NonNegative<double>))
        .Factory();
    status = wrapPanel.Result();
    if (!status) return status.GetStatus();

    auto uniformGrid = Describe<UniformGrid>(context);
    uniformGrid
        .Property(
            UniformGrid::RowsProperty,
            PropertyOptions(std::uint32_t{0})
                .AffectsMeasure())
        .Property(
            UniformGrid::ColumnsProperty,
            PropertyOptions(std::uint32_t{0})
                .AffectsMeasure())
        .Property(
            UniformGrid::FirstColumnProperty,
            PropertyOptions(std::uint32_t{0})
                .AffectsMeasure()
                .AffectsArrange())
        .Factory();
    status = uniformGrid.Result();
    if (!status) return status.GetStatus();

    auto scrollUnit = Describe<ScrollUnit>(context);
    scrollUnit
        .Value("Item", ScrollUnit::Item)
        .Value("Pixel", ScrollUnit::Pixel);
    status = scrollUnit.Result();
    if (!status) return status.GetStatus();

    auto virtualizationMode = Describe<VirtualizationMode>(context);
    virtualizationMode
        .Value("Standard", VirtualizationMode::Standard)
        .Value("Recycling", VirtualizationMode::Recycling);
    status = virtualizationMode.Result();
    if (!status) return status.GetStatus();

    auto virtualizingPanel = Describe<VirtualizingPanel>(
        context, TypeFlags::Abstract);
    virtualizingPanel.Property(
        VirtualizingPanel::ScrollUnitProperty,
        PropertyOptions(ScrollUnit::Item)
            .AffectsParentMeasure());
    virtualizingPanel.Property(
        VirtualizingPanel::VirtualizationModeProperty,
        PropertyOptions(VirtualizationMode::Standard)
            .AffectsParentMeasure());
    status = virtualizingPanel.Result();
    if (!status) return status.GetStatus();

    auto virtualizingStackPanel =
        Describe<VirtualizingStackPanel>(context);
    virtualizingStackPanel
        .Property(
            VirtualizingStackPanel::OrientationProperty,
            PropertyOptions(Orientation::Vertical)
                .AffectsMeasure())
        .Property(
            VirtualizingStackPanel::OverscanCountProperty,
            PropertyOptions(std::uint32_t{2})
                .AffectsMeasure())
        .Property(
            VirtualizingStackPanel::
                EstimatedItemExtentProperty,
            PropertyOptions(24.0)
                .AffectsMeasure()
                .Validate(&Validate::Positive<double>))
        .Factory();
    status = virtualizingStackPanel.Result();
    if (!status) return status.GetStatus();

    auto canvas = Describe<Canvas>(context);
    canvas
        .Property(
            Canvas::LeftProperty,
            PropertyOptions(0.0)
                .AffectsParentMeasure()
                .Validate(&Validate::Finite<double>))
        .Property(
            Canvas::TopProperty,
            PropertyOptions(0.0)
                .AffectsParentMeasure()
                .Validate(&Validate::Finite<double>))
        .Property(
            Canvas::RightProperty,
            PropertyOptions(0.0)
                .AffectsParentArrange()
                .Validate(&Validate::Finite<double>))
        .Property(
            Canvas::BottomProperty,
            PropertyOptions(0.0)
                .AffectsParentArrange()
                .Validate(&Validate::Finite<double>))
        .Factory();
    status = canvas.Result();
    if (!status) return status.GetStatus();

    auto columnDefinition =
        Describe<ColumnDefinition>(context);
    columnDefinition
        .Property<
            GridLength,
            &ColumnDefinition::Width,
            &ColumnDefinition::SetWidth>(
                "Width",
                PropertyFlags::Structural)
        .Property<
            double,
            &ColumnDefinition::MaxWidth,
            &ColumnDefinition::SetMaxWidth>(
                "MaxWidth",
                PropertyFlags::Structural)
        .Property<
            Base::String,
            &ColumnDefinition::SharedSizeGroup,
            &ColumnDefinition::SetSharedSizeGroup>(
                "SharedSizeGroup",
                PropertyFlags::Structural)
        .Factory();
    status = columnDefinition.Result();
    if (!status) return status.GetStatus();

    auto rowDefinition =
        Describe<RowDefinition>(context);
    rowDefinition
        .Property<
            GridLength,
            &RowDefinition::Height,
            &RowDefinition::SetHeight>(
                "Height",
                PropertyFlags::Structural)
        .Property<
            double,
            &RowDefinition::MaxHeight,
            &RowDefinition::SetMaxHeight>(
                "MaxHeight",
                PropertyFlags::Structural)
        .Property<
            Base::String,
            &RowDefinition::SharedSizeGroup,
            &RowDefinition::SetSharedSizeGroup>(
                "SharedSizeGroup",
                PropertyFlags::Structural)
        .Factory();
    status = rowDefinition.Result();
    if (!status) return status.GetStatus();

    auto grid = Describe<Grid>(context);
    grid
        .Property(
            Grid::IsSharedSizeScopeProperty,
            PropertyOptions(false))
        .Property(
            Grid::ColumnDefinitionsTextProperty,
            PropertyOptions(Base::String{})
                .AffectsMeasure()
                .Validate(
                    &ValidateGridDefinitionsText)
                .Changed(
                    &OnGridColumnsChanged))
        .Property(
            Grid::RowDefinitionsTextProperty,
            PropertyOptions(Base::String{})
                .AffectsMeasure()
                .Validate(
                    &ValidateGridDefinitionsText)
                .Changed(
                    &OnGridRowsChanged))
        .Collection<ColumnDefinition>(
            "ColumnDefinitions",
            &AddGridColumnDefinition,
            &ClearGridColumnDefinitions)
        .Collection<RowDefinition>(
            "RowDefinitions",
            &AddGridRowDefinition,
            &ClearGridRowDefinitions)
        .Collection<Presentation::KeyBinding>(
            "InputBindings",
            &AddGridInputBinding,
            &ClearGridInputBindings)
        .Property(
            Grid::RowProperty,
            PropertyOptions(std::uint32_t{0})
                .AffectsParentMeasure())
        .Property(
            Grid::ColumnProperty,
            PropertyOptions(std::uint32_t{0})
                .AffectsParentMeasure())
        .Property(
            Grid::RowSpanProperty,
            PropertyOptions(std::uint32_t{1})
                .AffectsParentMeasure()
                .Validate(&Validate::Positive<std::uint32_t>))
        .Property(
            Grid::ColumnSpanProperty,
            PropertyOptions(std::uint32_t{1})
                .AffectsParentMeasure()
                .Validate(&Validate::Positive<std::uint32_t>))
        .Factory();
    status = grid.Result();
    if (!status) return status.GetStatus();

    auto border = Describe<Border>(context);
    border
        .Property(
            Border::BackgroundProperty,
            PropertyOptions(
                Base::Ref<Presentation::Brush>{})
                .AffectsRender())
        .Property(
            Border::BorderBrushProperty,
            PropertyOptions(
                Base::Ref<Presentation::Brush>{})
                .AffectsRender())
        .Property(
            Border::BorderThicknessProperty,
            PropertyOptions(Presentation::Thickness{})
                .AffectsMeasure()
                .AffectsRender()
                .Validate(&ValidateThicknessValue))
        .Property(
            Border::CornerRadiusProperty,
            PropertyOptions(
                Presentation::CornerRadius{})
                .AffectsRender()
                .Validate(
                    &ValidateCornerRadiusValue))
        .Property(
            Border::PaddingProperty,
            PropertyOptions(Presentation::Thickness{})
                .AffectsMeasure()
                .Validate(&ValidateThicknessValue))
        .Factory();
    status = border.Result();
    if (!status) return status.GetStatus();

    const Presentation::Color black{
        0.0F, 0.0F, 0.0F, 1.0F};
    auto textBlock = Describe<TextBlock>(context);
    textBlock
        .Property(
            TextBlock::TextProperty,
            PropertyOptions(Base::String{})
                .AffectsMeasure())
        .Property(
            TextBlock::BackgroundProperty,
            PropertyOptions(
                Base::Ref<Presentation::Brush>{})
                .AffectsRender())
        .Property(
            TextBlock::StrokeProperty,
            PropertyOptions(
                Base::Ref<Presentation::Brush>{})
                .AffectsRender())
        .AddOwner(
            TextBlock::FontSizeProperty,
            PropertyOptions(16.0)
                .Inherits()
                .AffectsMeasure()
                .Validate(&ValidatePositiveFiniteDouble))
        .Property(
            TextBlock::FontWeightProperty,
            PropertyOptions(FontWeight::Normal)
                .AffectsMeasure())
        .Property(
            TextBlock::FontStyleProperty,
            PropertyOptions(Text::FontStyle::Normal)
                .AffectsMeasure())
        .Property(
            TextBlock::TextDecorationsProperty,
            PropertyOptions(TextDecorations::None)
                .AffectsRender())
        .Property(
            TextBlock::StrokeThicknessProperty,
            PropertyOptions(0.0)
                .AffectsMeasure()
                .AffectsRender()
                .Validate(&Validate::NonNegative<double>))
        .Property(
            TextBlock::TextWrappingProperty,
            PropertyOptions(
                Text::TextWrapping::NoWrap)
                .AffectsMeasure())
        .Property(
            TextBlock::TextTrimmingProperty,
            PropertyOptions(
                Text::TextTrimming::None)
                .AffectsMeasure())
        .Property(
            TextBlock::TextAlignmentProperty,
            PropertyOptions(
                Text::TextAlignment::Start)
                .AffectsMeasure())
        .Property(
            TextBlock::PaddingProperty,
            PropertyOptions(Presentation::Thickness{})
                .AffectsMeasure()
                .AffectsArrange()
                .Validate(&ValidateThicknessValue))
        .Property<
            Value,
            &TextBlock::MetadataInlines,
            &TextBlock::SetInlineValue>(
            "Inlines",
            PropertyFlags::AnyValue |
                PropertyFlags::Collection |
                PropertyFlags::Structural)
        .ContentAccessor(
            MakeMemberId(
                TextBlock::StaticTypeId(),
                MemberKind::Property,
                "Inlines"),
            ContentKind::Collection,
            &AddTextBlockInline,
            &ClearTextBlockInlines,
            ContentFlags::Visual)
        .Factory();
    status = textBlock.Result();
    if (!status) return status.GetStatus();

    auto run = Describe<Run>(context);
    run
        .Property<
            Base::String,
            &Run::Content,
            &Run::SetContent>(
            "Content",
            PropertyFlags::Structural)
        .Content(
            MakeMemberId(
                Run::StaticTypeId(),
                MemberKind::Property,
                "Content"))
        .Factory();
    status = run.Result();
    if (!status) return status.GetStatus();

    auto span = Describe<Span>(context);
    span.Factory();
    status = span.Result();
    if (!status) return status.GetStatus();

    auto bold = Describe<Bold>(context);
    bold
        .Override(
            TextBlock::FontWeightProperty,
            PropertyOptions(FontWeight::Bold)
                .AffectsMeasure())
        .Factory();
    status = bold.Result();
    if (!status) return status.GetStatus();

    auto italic = Describe<Italic>(context);
    italic
        .Override(
            TextBlock::FontStyleProperty,
            PropertyOptions(Text::FontStyle::Italic)
                .AffectsMeasure())
        .Factory();
    status = italic.Result();
    if (!status) return status.GetStatus();

    auto underline = Describe<Underline>(context);
    underline
        .Override(
            TextBlock::TextDecorationsProperty,
            PropertyOptions(
                TextDecorations::Underline)
                .AffectsRender())
        .Factory();
    status = underline.Result();
    if (!status) return status.GetStatus();

    auto lineBreak = Describe<LineBreak>(context);
    lineBreak.Factory();
    status = lineBreak.Result();
    if (!status) return status.GetStatus();

    const Presentation::Color transparent{
        0.0F, 0.0F, 0.0F, 0.0F};
    auto image = Describe<Image>(context);
    image
        .Property(
            Image::SourceProperty,
            PropertyOptions(
                Base::Ref<ImageSource>{})
                .AffectsMeasure()
                .AffectsRender())
        .Property(
            Image::StretchProperty,
            PropertyOptions(Stretch::Uniform)
                .AffectsMeasure()
                .AffectsRender())
        .Property(
            Image::StretchDirectionProperty,
            PropertyOptions(
                StretchDirection::Both)
                .AffectsMeasure()
                .AffectsRender())
        .Factory();
    status = image.Result();
    if (!status) return status.GetStatus();

    status = Describe<Shape>(
        context, TypeFlags::Abstract)
        .Property(
            Shape::FillProperty,
            PropertyOptions(Base::Ref<Brush>{})
                .AffectsRender()
                .Changed(&OnShapeFillChanged))
        .Property(
            Shape::StrokeProperty,
            PropertyOptions(Base::Ref<Brush>{})
                .AffectsRender()
                .Changed(&OnShapeFillChanged))
        .Property(
            Shape::StrokeThicknessProperty,
            PropertyOptions(1.0)
                .AffectsMeasure()
                .AffectsRender()
                .Validate(&Validate::NonNegative<double>))
        .Result();
    if (!status) return status.GetStatus();

    auto rectangle = Describe<Rectangle>(context);
    rectangle
        .Property(
            Rectangle::RadiusXProperty,
            PropertyOptions(0.0)
                .AffectsRender()
                .Validate(&Validate::NonNegative<double>))
        .Property(
            Rectangle::RadiusYProperty,
            PropertyOptions(0.0)
                .AffectsRender()
                .Validate(&Validate::NonNegative<double>))
        .Factory();
    status = rectangle.Result();
    if (!status) return status.GetStatus();

    auto ellipse = Describe<Ellipse>(context);
    ellipse.Factory();
    status = ellipse.Result();
    if (!status) return status.GetStatus();

    auto path = Describe<Path>(context);
    path
        .Property(
            Path::DataProperty,
            PropertyOptions(
                Base::Ref<
                    Presentation::Geometry>{})
                .AffectsMeasure()
                .AffectsRender()
                .Changed(&OnPathDataChanged))
        .Property(
            Path::FillProperty,
            PropertyOptions(
                Base::Ref<Presentation::Brush>{})
                .AffectsRender()
                .Changed(&OnPathColorChanged))
        .Property(
            Path::StrokeProperty,
            PropertyOptions(
                Base::Ref<Presentation::Brush>{})
                .AffectsRender()
                .Changed(&OnPathColorChanged))
        .Property(
            Path::StrokeThicknessProperty,
            PropertyOptions(1.0)
                .AffectsMeasure()
                .AffectsRender()
                .Validate(&Validate::NonNegative<double>)
                .Changed(&OnPathDoubleChanged))
        .Property(
            Path::StrokeLineJoinProperty,
            PropertyOptions(PenLineJoin::Miter)
                .AffectsRender()
                .Changed(&OnPathLineJoinChanged))
        .Property(
            Path::StrokeStartLineCapProperty,
            PropertyOptions(PenLineCap::Flat)
                .AffectsRender()
                .Changed(&OnPathLineCapChanged))
        .Property(
            Path::StrokeEndLineCapProperty,
            PropertyOptions(PenLineCap::Flat)
                .AffectsRender()
                .Changed(&OnPathLineCapChanged))
        .Property(
            Path::TrimStartProperty,
            PropertyOptions(0.0)
                .AffectsRender()
                .Validate(&ValidateNormalizedDouble)
                .Changed(&OnPathDoubleChanged))
        .Property(
            Path::TrimEndProperty,
            PropertyOptions(1.0)
                .AffectsRender()
                .Validate(&ValidateNormalizedDouble)
                .Changed(&OnPathDoubleChanged))
        .Property(
            Path::StretchProperty,
            PropertyOptions(Stretch::Uniform)
                .AffectsMeasure()
                .AffectsRender())
        .Factory();
    status = path.Result();
    if (!status) return status.GetStatus();

    const Presentation::Color selection{
        46.0F / 255.0F,
        174.0F / 255.0F,
        235.0F / 255.0F,
        1.0F};
    const Presentation::Color placeholder{
        123.0F / 255.0F,
        128.0F / 255.0F,
        133.0F / 255.0F,
        1.0F};
    status = Describe<TextBoxBase>(
        context, TypeFlags::Abstract)
        .Result();
    if (!status) return status.GetStatus();

    auto textBox = Describe<TextBox>(context);
    textBox
        .Event(TextBox::TextChangedEvent)
        .Property(
            TextBox::TextProperty,
            PropertyOptions(Base::String{})
                .AffectsMeasure()
                .AffectsRender()
                .BindsTwoWayByDefault()
                .UpdateSource(
                    UpdateSourceTrigger::LostFocus)
                .Coerce(&CoerceTextBoxText))
        .Property(
            TextBox::IsReadOnlyProperty,
            PropertyOptions(false)
                .AffectsRender())
        .Property(
            TextBox::MaxLengthProperty,
            PropertyOptions(std::uint32_t{0}))
        .Property(
            TextBox::AcceptsReturnProperty,
            PropertyOptions(false)
                .AffectsMeasure()
                .AffectsRender())
        .Property(
            TextBox::TextWrappingProperty,
            PropertyOptions(
                Text::TextWrapping::NoWrap)
                .AffectsMeasure()
                .AffectsRender())
        .Property(
            TextBox::PlaceholderProperty,
            PropertyOptions(Base::String{})
                .AffectsMeasure())
        .Property(
            TextBox::PlaceholderForegroundProperty,
            PropertyOptions(placeholder)
                .AffectsRender()
                .Validate(&ValidateColorValue))
        .Property(
            TextBox::FontSizeProperty,
            PropertyOptions(15.0)
                .AffectsMeasure()
                .Validate(
                    &ValidatePositiveFiniteDouble))
        .Property(
            TextBox::FontWeightProperty,
            PropertyOptions(FontWeight::Normal)
                .AffectsMeasure())
        .Property(
            TextBox::FontStyleProperty,
            PropertyOptions(
                Text::FontStyle::Normal)
                .AffectsMeasure())
        .Property(
            TextBox::TextAlignmentProperty,
            PropertyOptions(
                Text::TextAlignment::Start)
                .AffectsMeasure())
        .Property(
            TextBox::MaxLinesProperty,
            PropertyOptions(std::uint32_t{0})
                .AffectsMeasure()
                .AffectsRender())
        .Property(
            TextBox::MinLinesProperty,
            PropertyOptions(std::uint32_t{1})
                .AffectsMeasure()
                .AffectsRender()
                .Validate(
                    &Validate::Positive<
                        std::uint32_t>))
        .Property(
            TextBox::ForegroundProperty,
            PropertyOptions(black)
                .AffectsRender()
                .Validate(&ValidateColorValue))
        .Property(
            TextBox::SelectionBrushProperty,
            PropertyOptions(selection)
                .AffectsRender()
                .Validate(&ValidateColorValue))
        .Property(
            TextBox::SelectionOpacityProperty,
            PropertyOptions(0.25)
                .AffectsRender()
                .Validate(
                    &ValidateNormalizedDouble))
        .Property(
            TextBox::CaretBrushProperty,
            PropertyOptions(black)
                .AffectsRender()
                .Validate(&ValidateColorValue))
        .Override(
            UIElement::IsTabStopProperty,
            PropertyOptions(true))
        .Factory();
    status = textBox.Result();
    if (!status) return status.GetStatus();

    auto passwordBox = Describe<PasswordBox>(context);
    Base::String defaultPasswordChar;
    status = defaultPasswordChar.TryAssign(
        Base::StringView(u8"\u2022"));
    if (!status) return status.GetStatus();
    passwordBox
        .Event(PasswordBox::PasswordChangedEvent)
        .Property<
            Base::String,
            &PasswordBox::Password,
            &PasswordBox::SetPassword>(
                "Password", PropertyFlags::None)
        .Property(
            PasswordBox::PasswordCharProperty,
            PropertyOptions(
                std::move(defaultPasswordChar))
                .AffectsMeasure()
                .Validate(&ValidatePasswordChar))
        .Property(
            PasswordBox::MaxLengthProperty,
            PropertyOptions(std::uint32_t{0})
                .AffectsMeasure())
        .Property(
            PasswordBox::PlaceholderProperty,
            PropertyOptions(Base::String{})
                .AffectsMeasure()
                .AffectsRender())
        .Property(
            PasswordBox::ForegroundProperty,
            PropertyOptions(black)
                .AffectsRender()
                .Validate(&ValidateColorValue))
        .Property(
            PasswordBox::SelectionBrushProperty,
            PropertyOptions(
                Color{
                    0.18F, 0.48F,
                    0.95F, 0.45F})
                .AffectsRender()
                .Validate(&ValidateColorValue))
        .Property(
            PasswordBox::SelectionOpacityProperty,
            PropertyOptions(0.25)
                .AffectsRender()
                .Validate(&ValidateNormalizedDouble))
        .Property(
            PasswordBox::CaretBrushProperty,
            PropertyOptions(black)
                .AffectsRender()
                .Validate(&ValidateColorValue))
        .Override(
            UIElement::IsTabStopProperty,
            PropertyOptions(true))
        .Factory();
    status = passwordBox.Result();
    if (!status) return status.GetStatus();

    auto contentPresenter = Describe<ContentPresenter>(context);
    contentPresenter
        .Property(
            ContentPresenter::ContentProperty,
            PropertyOptions(
                Core::Value::NullObject(
                    Core::TypeOf<Base::Object>()))
                .AffectsMeasure()
                .Structural()
                .Changed(
                    &ContentPresenter::
                        OnContentPropertyChanged))
        .Property(
            ContentPresenter::ContentTemplateProperty,
            PropertyOptions(Base::Ref<Base::Object>{})
                .AffectsMeasure())
        .Property(
            ContentPresenter::ContentSourceProperty,
            PropertyOptions(Base::String{}))
        .ContentAccessor(
            MakeMemberId(
                ContentPresenter::StaticTypeId(),
                MemberKind::Property,
                "Content"),
            ContentKind::Single,
            &SetContentPresenterContent,
            &ClearContentPresenterContent,
            ContentFlags::Visual)
        .Factory();
    return contentPresenter.Result();
}

} // namespace Aero::Controls
