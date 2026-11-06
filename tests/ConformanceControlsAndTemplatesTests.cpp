#include "ConformanceTestHarness.hpp"

bool TestExpanderTemplatedParentIsCheckedWritesBack() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({240.0, 160.0});

    constexpr char kTree[] =
        "<Expander xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"Host\" Header=\"Position\" IsExpanded=\"True\">"
        "<Expander.Template>"
        "<ControlTemplate TargetType=\"{x:Type Expander}\">"
        "<StackPanel>"
        "<ToggleButton x:Name=\"HeaderButton\" "
        "IsChecked=\"{Binding IsExpanded, RelativeSource={RelativeSource TemplatedParent}}\"/>"
        "<ContentPresenter/>"
        "</StackPanel>"
        "</ControlTemplate>"
        "</Expander.Template>"
        "<TextBlock Text=\"Body\"/>"
        "</Expander>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "expander binding XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);

    auto* host = TryCast<Expander>(view.GetContent());
    CHECK(host != nullptr);
    CHECK(host->GetIsExpanded());
    ToggleButton* header = nullptr;
    const std::uint32_t visualCount =
        Aero::Media::VisualTreeHelper::GetChildrenCount(*host);
    for (std::uint32_t index = 0U; index < visualCount && header == nullptr;
         ++index) {
        Aero::Media::Visual* child =
            Aero::Media::VisualTreeHelper::GetChild(*host, index);
        if (child == nullptr) continue;
        header = TryCast<ToggleButton>(child);
        if (header != nullptr) break;
        const std::uint32_t nestedCount =
            Aero::Media::VisualTreeHelper::GetChildrenCount(*child);
        for (std::uint32_t nested = 0U;
             nested < nestedCount && header == nullptr;
             ++nested) {
            Aero::Media::Visual* nestedChild =
                Aero::Media::VisualTreeHelper::GetChild(*child, nested);
            header = nestedChild != nullptr
                ? TryCast<ToggleButton>(nestedChild)
                : nullptr;
        }
    }
    CHECK(header != nullptr);
    const Aero::Nullable<bool> checkedState = header->GetIsChecked();
    CHECK(checkedState.GetHasValue() && checkedState.GetValue());

    header->SetIsChecked(Aero::Nullable<bool>{false});
    Pump(view, 0.048);
    if (host->GetIsExpanded()) {
        std::fprintf(stderr,
            "Expander IsExpanded stayed true after ToggleButton uncheck "
            "(Binding Mode did not write back)\n");
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(!host->GetIsExpanded());
    return true;
}

bool TestExpanderUnnamedHeaderClickWritesBack() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({240.0, 200.0});

    constexpr char kTree[] =
        "<Expander xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"Host\" Header=\"Position\" IsExpanded=\"True\">"
        "<Expander.Template>"
        "<ControlTemplate TargetType=\"{x:Type Expander}\">"
        "<Grid>"
        "<Grid.RowDefinitions>"
        "<RowDefinition Height=\"Auto\"/>"
        "<RowDefinition Height=\"Auto\"/>"
        "</Grid.RowDefinitions>"
        "<Border x:Name=\"ContentBorder\" Grid.Row=\"1\" Height=\"40\" "
        "Background=\"#FFB8B8B8\" Visibility=\"Collapsed\">"
        "<ContentPresenter/>"
        "</Border>"
        "<Grid>"
        "<ToggleButton Height=\"24\" "
        "IsChecked=\"{Binding IsExpanded, RelativeSource={RelativeSource TemplatedParent}}\">"
        "<ToggleButton.Template>"
        "<ControlTemplate TargetType=\"{x:Type ToggleButton}\">"
        "<Grid Background=\"#FFA0A0A0\">"
        "<Path Data=\"M0,0L5,4 0,8\" Fill=\"White\" Margin=\"8,8,0,0\" "
        "HorizontalAlignment=\"Left\" VerticalAlignment=\"Center\"/>"
        "</Grid>"
        "</ControlTemplate>"
        "</ToggleButton.Template>"
        "</ToggleButton>"
        "<ContentPresenter ContentSource=\"Header\" Margin=\"18,0,8,0\" "
        "IsHitTestVisible=\"False\" VerticalAlignment=\"Center\"/>"
        "</Grid>"
        "</Grid>"
        "<ControlTemplate.Triggers>"
        "<Trigger Property=\"IsExpanded\" Value=\"True\">"
        "<Setter TargetName=\"ContentBorder\" Property=\"Visibility\" Value=\"Visible\"/>"
        "</Trigger>"
        "</ControlTemplate.Triggers>"
        "</ControlTemplate>"
        "</Expander.Template>"
        "<TextBlock Text=\"Body\"/>"
        "</Expander>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "unnamed expander XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {240.0, 200.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);

    auto* host = TryCast<Expander>(view.GetContent());
    CHECK(host != nullptr);
    CHECK(host->GetIsExpanded());

    ToggleButton* header = nullptr;
    const auto findToggle = [&](auto& self, Aero::Media::Visual& visual)
        -> ToggleButton* {
        if (auto* toggle = TryCast<ToggleButton>(&visual)) return toggle;
        const std::uint32_t count =
            Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
        for (std::uint32_t index = 0U; index < count; ++index) {
            Aero::Media::Visual* child =
                Aero::Media::VisualTreeHelper::GetChild(visual, index);
            if (child == nullptr) continue;
            if (ToggleButton* found = self(self, *child)) return found;
        }
        return nullptr;
    };
    header = findToggle(findToggle, *host);
    CHECK(header != nullptr);
    CHECK(header->GetIsChecked().GetHasValue() &&
        header->GetIsChecked().GetValue());

    const Aero::Size headerSize = header->GetRenderSize();
    CHECK(headerSize.width > 0.0 && headerSize.height > 0.0);
    Point headerScreen{};
    CHECK(header->TryPointToScreen({8.0, headerSize.height * 0.5}, headerScreen));
    static_cast<void>(view.MouseMove(
        static_cast<int>(headerScreen.x), static_cast<int>(headerScreen.y)));
    Pump(view, 0.032);
    static_cast<void>(view.MouseButtonDown(
        static_cast<int>(headerScreen.x), static_cast<int>(headerScreen.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.016);
    static_cast<void>(view.MouseButtonUp(
        static_cast<int>(headerScreen.x), static_cast<int>(headerScreen.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.048);
    if (host->GetIsExpanded()) {
        const Aero::Nullable<bool> headerChecked = header->GetIsChecked();
        const bool checked = headerChecked.GetHasValue()
            ? headerChecked.GetValue()
            : true;
        std::fprintf(stderr,
            "unnamed expander header click did not collapse over=%d checked=%d "
            "header=(%.1fx%.1f)\n",
            header->GetIsMouseOver() ? 1 : 0,
            checked ? 1 : 0,
            headerSize.width, headerSize.height);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(!host->GetIsExpanded());
    return true;
}

bool TestExpanderResourceDictionaryHeaderClick() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({240.0, 200.0});

    constexpr char kTree[] =
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid.Resources>"
        "<ControlTemplate x:Key=\"ExpanderT\" TargetType=\"{x:Type Expander}\">"
        "<Grid>"
        "<Grid.RowDefinitions>"
        "<RowDefinition Height=\"Auto\"/>"
        "<RowDefinition Height=\"Auto\"/>"
        "</Grid.RowDefinitions>"
        "<Border x:Name=\"ContentBorder\" Grid.Row=\"1\" Height=\"40\" "
        "Background=\"#FFB8B8B8\" Visibility=\"Collapsed\">"
        "<ContentPresenter/>"
        "</Border>"
        "<Grid>"
        "<ToggleButton Height=\"24\" "
        "IsChecked=\"{Binding IsExpanded, RelativeSource={RelativeSource TemplatedParent}}\">"
        "<ToggleButton.Template>"
        "<ControlTemplate TargetType=\"{x:Type ToggleButton}\">"
        "<Border Background=\"#FFA0A0A0\"/>"
        "</ControlTemplate>"
        "</ToggleButton.Template>"
        "</ToggleButton>"
        "<ContentPresenter ContentSource=\"Header\" Margin=\"18,0,8,0\" "
        "IsHitTestVisible=\"False\" VerticalAlignment=\"Center\"/>"
        "</Grid>"
        "</Grid>"
        "<ControlTemplate.Triggers>"
        "<Trigger Property=\"IsExpanded\" Value=\"True\">"
        "<Setter TargetName=\"ContentBorder\" Property=\"Visibility\" Value=\"Visible\"/>"
        "</Trigger>"
        "</ControlTemplate.Triggers>"
        "</ControlTemplate>"
        "<Style TargetType=\"{x:Type Expander}\">"
        "<Setter Property=\"Template\" Value=\"{StaticResource ExpanderT}\"/>"
        "</Style>"
        "</Grid.Resources>"
        "<Expander x:Name=\"Host\" Header=\"Position\" IsExpanded=\"True\">"
        "<TextBlock Text=\"Body\"/>"
        "</Expander>"
        "</Grid>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "resource expander XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {240.0, 200.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    auto* host = root->FindName<Expander>(StringView("Host"));
    CHECK(host != nullptr);
    CHECK(host->GetIsExpanded());

    ToggleButton* header = nullptr;
    const auto findToggle = [&](auto& self, Aero::Media::Visual& visual)
        -> ToggleButton* {
        if (auto* toggle = TryCast<ToggleButton>(&visual)) return toggle;
        const std::uint32_t count =
            Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
        for (std::uint32_t index = 0U; index < count; ++index) {
            Aero::Media::Visual* child =
                Aero::Media::VisualTreeHelper::GetChild(visual, index);
            if (child == nullptr) continue;
            if (ToggleButton* found = self(self, *child)) return found;
        }
        return nullptr;
    };
    header = findToggle(findToggle, *host);
    CHECK(header != nullptr);
    header->SetIsChecked(Aero::Nullable<bool>{false});
    Pump(view, 0.048);
    if (host->GetIsExpanded()) {
        std::fprintf(stderr,
            "resource-dictionary Expander IsExpanded stayed true after uncheck\n");
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(!host->GetIsExpanded());
    return true;
}

bool TestControlTemplateHoverStoryboard() {
    ViewOptions options;
    options.automaticAnimationClock = false;
    LiveGui* live = NewLiveGui(options);
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({200.0, 80.0});

    constexpr char kTree[] =
        "<Button xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"Add\" Content=\"Add\" Width=\"120\" Height=\"32\">"
        "<Button.Template>"
        "<ControlTemplate TargetType=\"{x:Type Button}\">"
        "<ControlTemplate.Resources>"
        "<Storyboard x:Key=\"OverOn\">"
        "<DoubleAnimationUsingKeyFrames Storyboard.TargetProperty=\"(UIElement.Opacity)\" "
        "Storyboard.TargetName=\"BgOver\">"
        "<EasingDoubleKeyFrame KeyTime=\"0:0:0\" Value=\"1\"/>"
        "</DoubleAnimationUsingKeyFrames>"
        "</Storyboard>"
        "</ControlTemplate.Resources>"
        "<Grid>"
        "<Border x:Name=\"Bg\" Background=\"#FFA0A0A0\"/>"
        "<Border x:Name=\"BgOver\" Background=\"#FFFF0000\" Opacity=\"0\"/>"
        "<ContentPresenter HorizontalAlignment=\"Center\" VerticalAlignment=\"Center\"/>"
        "</Grid>"
        "<ControlTemplate.Triggers>"
        "<Trigger Property=\"IsMouseOver\" Value=\"True\">"
        "<Trigger.EnterActions>"
        "<BeginStoryboard Storyboard=\"{StaticResource OverOn}\"/>"
        "</Trigger.EnterActions>"
        "</Trigger>"
        "</ControlTemplate.Triggers>"
        "</ControlTemplate>"
        "</Button.Template>"
        "</Button>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "control template hover XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {200.0, 80.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);

    auto* add = TryCast<Button>(view.GetContent());
    CHECK(add != nullptr);
    const Aero::Size addSize = add->GetRenderSize();
    CHECK(addSize.width > 0.0 && addSize.height > 0.0);
    Point addScreen{};
    CHECK(add->TryPointToScreen(
        {addSize.width * 0.5, addSize.height * 0.5}, addScreen));
    static_cast<void>(view.MouseMove(
        static_cast<int>(addScreen.x), static_cast<int>(addScreen.y)));
    Pump(view, 0.048);
    CHECK(add->GetIsMouseOver());

    Border* over = nullptr;
    if (Aero::Media::VisualTreeHelper::GetChildrenCount(*add) > 0U) {
        Aero::Media::Visual* templateRoot =
            Aero::Media::VisualTreeHelper::GetChild(*add, 0U);
        if (templateRoot != nullptr) {
            const std::uint32_t nested =
                Aero::Media::VisualTreeHelper::GetChildrenCount(*templateRoot);
            for (std::uint32_t index = 0U; index < nested; ++index) {
                auto* border = TryCast<Border>(
                    Aero::Media::VisualTreeHelper::GetChild(*templateRoot, index));
                if (border == nullptr) continue;
                if (over == nullptr) {
                    over = border;
                    continue;
                }
                over = border;
                break;
            }
        }
    }
    CHECK(over != nullptr);
    if (over->GetOpacity() <= 0.5) {
        std::fprintf(stderr,
            "control template hover storyboard did not raise BgOver opacity=%.2f\n",
            over->GetOpacity());
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(over->GetOpacity() > 0.5);
    return true;
}

struct BlendAddClickProbe {
    std::uint32_t count = 0;
    void OnClick(Aero::Base::Object*, Aero::RoutedEventArgs&) noexcept {
        ++count;
    }
};

bool TestGridStarRowsSizeInStackPanel() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({240.0, 400.0});

    constexpr char kTree[] =
        "<StackPanel xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "Width=\"150\">"
        "<Grid x:Name=\"ColorGrid\">"
        "<Grid.RowDefinitions>"
        "<RowDefinition Height=\"*\"/>"
        "<RowDefinition Height=\"*\"/>"
        "<RowDefinition Height=\"*\"/>"
        "<RowDefinition Height=\"*\"/>"
        "</Grid.RowDefinitions>"
        "<Grid.ColumnDefinitions>"
        "<ColumnDefinition Width=\"1*\"/>"
        "<ColumnDefinition Width=\"2*\"/>"
        "</Grid.ColumnDefinitions>"
        "<Rectangle x:Name=\"Swatch\" Grid.RowSpan=\"4\" Fill=\"Red\" "
        "Stroke=\"Black\" Margin=\"0,0,5,0\"/>"
        "<Slider x:Name=\"R\" Grid.Column=\"1\" Height=\"18\" Margin=\"10,1,0,1\" "
        "Maximum=\"255\"/>"
        "</Grid>"
        "</StackPanel>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "star-grid XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {240.0, 400.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);
    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    Grid* grid = root->FindName<Grid>(StringView("ColorGrid"));
    Rectangle* swatch = root->FindName<Rectangle>(StringView("Swatch"));
    CHECK(grid != nullptr && swatch != nullptr);
    if (grid->GetRenderSize().height < 8.0 ||
        swatch->GetRenderSize().height < 8.0) {
        std::fprintf(stderr,
            "star Grid in StackPanel collapsed grid=(%.1fx%.1f) swatch=(%.1fx%.1f)\n",
            grid->GetRenderSize().width, grid->GetRenderSize().height,
            swatch->GetRenderSize().width, swatch->GetRenderSize().height);
    }
    CHECK(grid->GetRenderSize().height >= 8.0);
    CHECK(swatch->GetRenderSize().height >= 8.0);
    return true;
}

bool TestThemeTagAndIndentationAndColor() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({800.0, 600.0});

    constexpr char kXAML[] =
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<ComboBox x:Name=\"Picker\" Tag=\"Theme\">"
        "<ComboBox.Template>"
        "<ControlTemplate TargetType=\"ComboBox\">"
        "<ToggleButton x:Name=\"DropDownButton\">"
        "<StackPanel Orientation=\"Horizontal\">"
        "<TextBlock x:Name=\"SelectionText\" Text=\"{Binding Tag, RelativeSource={RelativeSource TemplatedParent}}\" Width=\"0\"/>"
        "</StackPanel>"
        "</ToggleButton>"
        "<ControlTemplate.Triggers>"
        "<Trigger Property=\"IsMouseOver\" Value=\"True\">"
        "<Trigger.EnterActions>"
        "<BeginStoryboard>"
        "<Storyboard>"
        "<DoubleAnimation Storyboard.TargetName=\"SelectionText\" Storyboard.TargetProperty=\"(FrameworkElement.Width)\" To=\"60\" Duration=\"0:0:0.1\"/>"
        "</Storyboard>"
        "</BeginStoryboard>"
        "</Trigger.EnterActions>"
        "</Trigger>"
        "</ControlTemplate.Triggers>"
        "</ControlTemplate>"
        "</ComboBox.Template>"
        "</ComboBox>"
        "<TreeView x:Name=\"Tree\">"
        "<TreeViewItem x:Name=\"ParentItem\" Header=\"Parent\" IsExpanded=\"True\">"
        "<TreeViewItem.Template>"
        "<ControlTemplate TargetType=\"TreeViewItem\">"
        "<Grid>"
        "<Grid.RowDefinitions><RowDefinition Height=\"Auto\"/><RowDefinition Height=\"*\"/></Grid.RowDefinitions>"
        "<ContentPresenter ContentSource=\"Header\"/>"
        "<ItemsPresenter x:Name=\"ItemsHost\" Grid.Row=\"1\" Tag=\"32\"/>"
        "</Grid>"
        "</ControlTemplate>"
        "</TreeViewItem.Template>"
        "<TreeViewItem x:Name=\"ChildItem\" Header=\"Child\">"
        "<TreeViewItem.Template>"
        "<ControlTemplate TargetType=\"TreeViewItem\">"
        "<StackPanel Orientation=\"Horizontal\">"
        "<Decorator x:Name=\"Indent\" Width=\"{Binding Tag, RelativeSource={RelativeSource AncestorType={x:Type ItemsPresenter}, AncestorLevel=1}}\"/>"
        "<ContentPresenter ContentSource=\"Header\"/>"
        "</StackPanel>"
        "</ControlTemplate>"
        "</TreeViewItem.Template>"
        "</TreeViewItem>"
        "</TreeViewItem>"
        "</TreeView>"
        "</Grid>";

    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(kXAML));
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {800.0, 600.0}));
    Pump(view, 0.016);
    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    ComboBox* picker = root->FindName<ComboBox>(StringView("Picker"));
    CHECK(picker != nullptr);

    std::function<TextBlock*(Aero::Media::Visual*)> findTb = [&](Aero::Media::Visual* node) -> TextBlock* {
        if (node == nullptr) return nullptr;
        if (auto* tb = ::Aero::TryCast<TextBlock>(node)) {
            return tb;
        }
        for (std::uint32_t i = 0; i < Aero::Media::VisualTreeHelper::GetChildrenCount(*node); ++i) {
            if (auto* res = findTb(Aero::Media::VisualTreeHelper::GetChild(*node, i))) return res;
        }
        return nullptr;
    };
    TextBlock* text = findTb(picker);
    if (text != nullptr) {
        std::printf("[DIAG] SelectionText Text='%s', Width=%f, ActualWidth=%f\n",
            text->GetText().Data(), text->GetWidth(), text->GetActualWidth());
    } else {
        std::printf("[DIAG] SelectionText not found in visual tree!\n");
    }

    // Now test hover animation on ComboBox
    const Aero::Size pickerSize = picker->GetRenderSize();
    Point pickerScreen{};
    if (picker->TryPointToScreen({pickerSize.width * 0.5, pickerSize.height * 0.5}, pickerScreen)) {
        static_cast<void>(view.MouseMove(static_cast<int>(pickerScreen.x), static_cast<int>(pickerScreen.y)));
    }
    Pump(view, 0.05);
    Pump(view, 0.15);
    if (text != nullptr) {
        std::printf("[DIAG] After hover: isMouseOver=%d, SelectionText Width=%f, ActualWidth=%f\n",
            picker->GetIsMouseOver() ? 1 : 0, text->GetWidth(), text->GetActualWidth());
    }

    // Now check TreeView indentation
    Aero::Controls::TreeViewItem* childItem = root->FindName<Aero::Controls::TreeViewItem>(StringView("ChildItem"));
    CHECK(childItem != nullptr);
    std::function<Aero::Controls::Decorator*(Aero::Media::Visual*)> findIndent = [&](Aero::Media::Visual* node) -> Aero::Controls::Decorator* {
        if (node == nullptr) return nullptr;
        if (auto* dec = ::Aero::TryCast<Aero::Controls::Decorator>(node)) {
            return dec;
        }
        for (std::uint32_t i = 0; i < Aero::Media::VisualTreeHelper::GetChildrenCount(*node); ++i) {
            if (auto* res = findIndent(Aero::Media::VisualTreeHelper::GetChild(*node, i))) return res;
        }
        return nullptr;
    };
    Aero::Controls::Decorator* indent = findIndent(childItem);
    if (indent != nullptr) {
        std::printf("[DIAG] Indent Decorator Width=%f, ActualWidth=%f\n",
            indent->GetWidth(), indent->GetActualWidth());
    } else {
        std::printf("[DIAG] Indent Decorator not found in ChildItem!\n");
    }
    return true;
}

bool TestGridSpanningAndStarMeasure() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({400.0, 400.0});

    // 1. Spanning child across Auto and Star: Auto row must not be inflated by spanning child
    {
        constexpr char kTree1[] =
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
            "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
            "Width=\"200\" Height=\"200\">"
            "<Grid.RowDefinitions>"
            "<RowDefinition Height=\"Auto\"/>"
            "<RowDefinition Height=\"*\"/>"
            "</Grid.RowDefinitions>"
            "<Border Grid.RowSpan=\"2\" Height=\"100\"/>"
            "<Button x:Name=\"HeaderBtn\" Grid.Row=\"0\" Height=\"30\"/>"
            "</Grid>";
        Aero::Markup::XamlReader reader(live->gui);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(kTree1));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {400.0, 400.0}));
        Pump(view, 0.016);
        FrameworkElement* root = view.GetContent();
        CHECK(root != nullptr);
        Button* btn = root->FindName<Button>(StringView("HeaderBtn"));
        CHECK(btn != nullptr);
        // Header button must be 30px, at Y = 0
        CHECK(std::abs(btn->GetActualHeight() - 30.0) < 0.1);
        CHECK(std::abs(btn->GetLayoutSlot().y - 0.0) < 0.1);
    }

    // 2. Grid with Star row and VerticalAlignment=Top: must not inflate desired height to window height
    {
        constexpr char kTree2[] =
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
            "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
            "Width=\"200\" Height=\"400\">"
            "<Grid x:Name=\"TopGrid\" VerticalAlignment=\"Top\">"
            "<Button Height=\"40\"/>"
            "</Grid>"
            "</Grid>";
        Aero::Markup::XamlReader reader(live->gui);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(kTree2));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {400.0, 400.0}));
        Pump(view, 0.016);
        FrameworkElement* root = view.GetContent();
        CHECK(root != nullptr);
        Grid* topGrid = root->FindName<Grid>(StringView("TopGrid"));
        CHECK(topGrid != nullptr);
        // TopGrid height must be 40, located at Y = 0 (not 400 or centered at 180)
        CHECK(std::abs(topGrid->GetActualHeight() - 40.0) < 0.1);
        CHECK(std::abs(topGrid->GetLayoutSlot().y - 0.0) < 0.1);
    }

    // 3. ScrollViewer layout pattern: Column 0 is *, Column 1 is Auto. Spanning content must not inflate Auto column.
    {
        constexpr char kTree3[] =
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
            "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
            "Width=\"200\" Height=\"200\">"
            "<Grid Width=\"200\" Height=\"200\">"
            "<Grid.ColumnDefinitions>"
            "<ColumnDefinition Width=\"*\"/>"
            "<ColumnDefinition Width=\"Auto\"/>"
            "</Grid.ColumnDefinitions>"
            "<Border Grid.ColumnSpan=\"2\" Width=\"180\"/>"
            "<Border x:Name=\"ScrollBar\" Grid.Column=\"1\" Width=\"14\"/>"
            "</Grid>"
            "</Grid>";
        Aero::Markup::XamlReader reader(live->gui);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(kTree3));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {400.0, 400.0}));
        Pump(view, 0.016);
        FrameworkElement* root = view.GetContent();
        CHECK(root != nullptr);
        Border* bar = root->FindName<Border>(StringView("ScrollBar"));
        CHECK(bar != nullptr);
        // ScrollBar must be 14px wide at X = 200 - 14 = 186 (not X = 125 or 100)
        CHECK(std::abs(bar->GetActualWidth() - 14.0) < 0.1);
        CHECK(std::abs(bar->GetLayoutSlot().x - 186.0) < 0.1);
    }

    return true;
}

bool TestBlendTutorialSidebarInteractions() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({800.0, 600.0});
    ViewViewport viewport;
    viewport.logicalSize = {800.0, 600.0};
    viewport.pixelWidth = 800U;
    viewport.pixelHeight = 600U;
    viewport.dpiScale = 1.0;
    static_cast<void>(view.SetViewport(viewport));

    constexpr char kTree[] =
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid.Resources>"
        "<SolidColorBrush x:Key=\"Bg\" Color=\"#FFA0A0A0\"/>"
        "<SolidColorBrush x:Key=\"Over\" Color=\"#FFFF0000\"/>"
        "<ControlTemplate x:Key=\"HoverBtn\" TargetType=\"{x:Type Button}\">"
        "<Grid>"
        "<Border x:Name=\"Bg\" Background=\"{StaticResource Bg}\" Height=\"24\"/>"
        "<Border x:Name=\"BgOver\" Background=\"{StaticResource Over}\" Opacity=\"0\"/>"
        "<ContentPresenter HorizontalAlignment=\"Center\" VerticalAlignment=\"Center\"/>"
        "</Grid>"
        "<ControlTemplate.Triggers>"
        "<Trigger Property=\"IsMouseOver\" Value=\"True\">"
        "<Setter TargetName=\"BgOver\" Property=\"Opacity\" Value=\"1\"/>"
        "</Trigger>"
        "</ControlTemplate.Triggers>"
        "</ControlTemplate>"
        "<ControlTemplate x:Key=\"ExpanderT\" TargetType=\"{x:Type Expander}\">"
        "<StackPanel>"
        "<ToggleButton x:Name=\"HeaderButton\" Height=\"24\" Background=\"#FFA0A0A0\" "
        "IsChecked=\"{Binding IsExpanded, RelativeSource={RelativeSource TemplatedParent}}\"/>"
        "<Border x:Name=\"ContentBorder\" Background=\"#FFB8B8B8\">"
        "<ContentPresenter/>"
        "</Border>"
        "</StackPanel>"
        "<ControlTemplate.Triggers>"
        "<Trigger Property=\"IsExpanded\" Value=\"False\">"
        "<Setter TargetName=\"ContentBorder\" Property=\"Visibility\" Value=\"Collapsed\"/>"
        "</Trigger>"
        "</ControlTemplate.Triggers>"
        "</ControlTemplate>"
        "<ControlTemplate x:Key=\"SliderT\" TargetType=\"{x:Type Slider}\">"
        "<Border Background=\"#FF696969\" Height=\"20\">"
        "<Track x:Name=\"PART_Track\">"
        "<Track.Thumb>"
        "<Thumb x:Name=\"thumb\" Width=\"10\" Height=\"16\" Background=\"White\"/>"
        "</Track.Thumb>"
        "</Track>"
        "</Border>"
        "</ControlTemplate>"
        "</Grid.Resources>"
        "<Grid Margin=\"10\">"
        "<Grid.RowDefinitions>"
        "<RowDefinition Height=\"369*\"/>"
        "<RowDefinition Height=\"631*\"/>"
        "</Grid.RowDefinitions>"
        "<Decorator x:Name=\"PropsBarMaxWidth\" Grid.Column=\"1\"/>"
        "</Grid>"
        "<DockPanel LastChildFill=\"True\">"
        "<Viewbox x:Name=\"PropsBar\" DockPanel.Dock=\"Right\" VerticalAlignment=\"Top\" "
        "Margin=\"0,10,10,10\" MaxWidth=\"{Binding ActualHeight, ElementName=PropsBarMaxWidth}\">"
        "<StackPanel Width=\"150\">"
        "<Button x:Name=\"AddButton\" Content=\"Add\" Height=\"24\" Margin=\"0,0,0,3\" "
        "Template=\"{StaticResource HoverBtn}\"/>"
        "<Expander x:Name=\"PositionExpander\" Header=\"Position\" IsExpanded=\"True\" "
        "Template=\"{StaticResource ExpanderT}\" Margin=\"0,0,0,10\">"
        "<Slider x:Name=\"PositionLeft\" Minimum=\"0\" Maximum=\"100\" Value=\"10\" "
        "Template=\"{StaticResource SliderT}\"/>"
        "</Expander>"
        "</StackPanel>"
        "</Viewbox>"
        "<Border x:Name=\"ContainerBorder\" Background=\"White\" Margin=\"10\" "
        "BorderBrush=\"Gray\" BorderThickness=\"1\">"
        "<Canvas x:Name=\"ContainerCanvas\"/>"
        "</Border>"
        "</DockPanel>"
        "</Grid>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "blend sidebar XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {800.0, 600.0}));
    Pump(view, 0.016);
    Pump(view, 0.048);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    Button* add = root->FindName<Button>(StringView("AddButton"));
    Expander* expander = root->FindName<Expander>(StringView("PositionExpander"));
    Slider* slider = root->FindName<Slider>(StringView("PositionLeft"));
    CHECK(add != nullptr && expander != nullptr && slider != nullptr);

    BlendAddClickProbe probe;
    add->Click().Add({&probe, &BlendAddClickProbe::OnClick});

    const Aero::Size addSize = add->GetRenderSize();
    CHECK(addSize.width > 0.0 && addSize.height > 0.0);
    Point addScreen{};
    CHECK(add->TryPointToScreen(
        {addSize.width * 0.5, addSize.height * 0.5}, addScreen));
    static_cast<void>(view.MouseMove(
        static_cast<int>(addScreen.x), static_cast<int>(addScreen.y)));
    Pump(view, 0.048);
    Border* over = nullptr;
    if (Aero::Media::VisualTreeHelper::GetChildrenCount(*add) > 0U) {
        Aero::Media::Visual* templateRoot =
            Aero::Media::VisualTreeHelper::GetChild(*add, 0U);
        if (templateRoot != nullptr) {
            const std::uint32_t nested =
                Aero::Media::VisualTreeHelper::GetChildrenCount(*templateRoot);
            for (std::uint32_t index = 0U; index < nested; ++index) {
                auto* border = TryCast<Border>(
                    Aero::Media::VisualTreeHelper::GetChild(*templateRoot, index));
                if (border == nullptr) continue;
                if (over == nullptr) {
                    over = border;
                    continue;
                }
                over = border;
                break;
            }
        }
    }
    if (!add->GetIsMouseOver()) {
        std::fprintf(stderr,
            "blend Add hover miss screen=(%.1f,%.1f) size=(%.1fx%.1f) "
            "slot=(%.1f,%.1f,%.1f,%.1f)\n",
            addScreen.x, addScreen.y, addSize.width, addSize.height,
            add->GetLayoutSlot().x, add->GetLayoutSlot().y,
            add->GetLayoutSlot().width, add->GetLayoutSlot().height);
    }
    CHECK(add->GetIsMouseOver());
    if (over != nullptr) {
        CHECK(over->GetOpacity() > 0.5);
    }

    static_cast<void>(view.MouseButtonDown(
        static_cast<int>(addScreen.x), static_cast<int>(addScreen.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.016);
    static_cast<void>(view.MouseButtonUp(
        static_cast<int>(addScreen.x), static_cast<int>(addScreen.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.032);
    if (probe.count == 0U) {
        std::fprintf(stderr,
            "blend Add click did not fire pressed=%d over=%d\n",
            add->GetIsPressed() ? 1 : 0,
            add->GetIsMouseOver() ? 1 : 0);
    }
    CHECK(probe.count >= 1U);

    CHECK(expander->GetIsExpanded());
    ToggleButton* header = nullptr;
    const auto findToggle = [&](auto& self, Aero::Media::Visual& visual)
        -> ToggleButton* {
        if (auto* toggle = TryCast<ToggleButton>(&visual)) return toggle;
        const std::uint32_t count =
            Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
        for (std::uint32_t index = 0U; index < count; ++index) {
            Aero::Media::Visual* child =
                Aero::Media::VisualTreeHelper::GetChild(visual, index);
            if (child == nullptr) continue;
            if (ToggleButton* found = self(self, *child)) return found;
        }
        return nullptr;
    };
    header = findToggle(findToggle, *expander);
    CHECK(header != nullptr);

    static_cast<void>(view.MouseMove(0, 0));
    Pump(view, 0.016);

    const Aero::Size headerSize = header->GetRenderSize();
    CHECK(headerSize.width > 0.0 && headerSize.height > 0.0);
    Point headerScreen{};
    Point headerOrigin{};
    CHECK(header->TryPointToScreen({0.0, 0.0}, headerOrigin));
    CHECK(header->TryPointToScreen(
        {headerSize.width * 0.5, headerSize.height * 0.5}, headerScreen));
    static_cast<void>(view.MouseMove(
        static_cast<int>(headerScreen.x), static_cast<int>(headerScreen.y)));
    Pump(view, 0.032);
    std::fprintf(stderr,
        "blend expander before-slider headerOver=%d expanderOver=%d "
        "sliderOver=%d addOver=%d headerScreen=(%.1f,%.1f) "
        "hdrOrigin=(%.1f,%.1f) captured=%d arrange=%d\n",
        header->GetIsMouseOver() ? 1 : 0,
        expander->GetIsMouseOver() ? 1 : 0,
        slider->GetIsMouseOver() ? 1 : 0,
        add->GetIsMouseOver() ? 1 : 0,
        headerScreen.x, headerScreen.y,
        headerOrigin.x, headerOrigin.y,
        Aero::Input::Mouse::Captured() != nullptr ? 1 : 0,
        header->GetIsArrangeValid() ? 1 : 0);
    static_cast<void>(view.MouseButtonDown(
        static_cast<int>(headerScreen.x), static_cast<int>(headerScreen.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.016);
    static_cast<void>(view.MouseButtonUp(
        static_cast<int>(headerScreen.x), static_cast<int>(headerScreen.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.048);
    if (expander->GetIsExpanded()) {
        const Aero::Nullable<bool> headerChecked = header->GetIsChecked();
        const bool checked = headerChecked.GetHasValue()
            ? headerChecked.GetValue()
            : true;
        std::fprintf(stderr,
            "blend Expander did not collapse after header click "
            "headerOver=%d expanderOver=%d addOver=%d checked=%d "
            "headerScreen=(%.1f,%.1f) addScreen=(%.1f,%.1f) "
            "headerSize=(%.1fx%.1f) expanderSize=(%.1fx%.1f)\n",
            header->GetIsMouseOver() ? 1 : 0,
            expander->GetIsMouseOver() ? 1 : 0,
            add->GetIsMouseOver() ? 1 : 0,
            checked ? 1 : 0,
            headerScreen.x, headerScreen.y,
            addScreen.x, addScreen.y,
            headerSize.width, headerSize.height,
            expander->GetRenderSize().width, expander->GetRenderSize().height);
    }
    CHECK(!expander->GetIsExpanded());
    expander->SetIsExpanded(true);
    Pump(view, 0.048);

    const double start = slider->GetValue();
    const Aero::Size sliderSize = slider->GetRenderSize();
    CHECK(sliderSize.width > 8.0);
    Point sliderScreen{};
    CHECK(slider->TryPointToScreen(
        {sliderSize.width * 0.85, sliderSize.height * 0.5}, sliderScreen));
    static_cast<void>(view.MouseMove(
        static_cast<int>(sliderScreen.x), static_cast<int>(sliderScreen.y)));
    Pump(view, 0.032);
    static_cast<void>(view.MouseButtonDown(
        static_cast<int>(sliderScreen.x), static_cast<int>(sliderScreen.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.016);
    static_cast<void>(view.MouseButtonUp(
        static_cast<int>(sliderScreen.x), static_cast<int>(sliderScreen.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.032);
    if (!(slider->GetValue() > start + 1.0)) {
        std::fprintf(stderr,
            "blend Slider did not follow click start=%.1f now=%.1f "
            "size=(%.1fx%.1f) over=%d screen=(%.1f,%.1f)\n",
            start, slider->GetValue(), sliderSize.width, sliderSize.height,
            slider->GetIsMouseOver() ? 1 : 0,
            sliderScreen.x, sliderScreen.y);
    }
    CHECK(slider->GetValue() > start + 1.0);
    return true;
}

bool TestLoadComponentUserControlInStackPanel() {
    LiveGui* live = NewTutorialLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({240.0, 160.0});

    Result<Ref<NumericUpDown>> control = MakeRef<NumericUpDown>();
    CHECK(control);
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> host = reader.Parse(StringView(
        "<StackPanel xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" Width=\"200\"/>"));
    CHECK(host);
    CHECK(view.SetContent(std::move(host).Value(), {240.0, 160.0}));
    Pump(view, 0.016);
    auto* stack = TryCast<StackPanel>(view.GetContent());
    CHECK(stack != nullptr);
    stack->GetChildren().Add(
        Ref<Aero::UIElement>(control.Value()));
    Pump(view, 0.032);
    CHECK(live->gui.LoadComponent(
        *control.Value(), "memory:///NumericUpDown.xaml"));
    Pump(view, 0.016);
    Pump(view, 0.032);
    if (control->GetRenderSize().height < 1.0) {
        Aero::Media::Visual* child =
            Aero::Media::VisualTreeHelper::GetChildrenCount(*control.Value()) > 0U
            ? Aero::Media::VisualTreeHelper::GetChild(*control.Value(), 0U)
            : nullptr;
        auto* childElement = TryCast<UIElement>(child);
        std::fprintf(stderr,
            "LoadComponent UserControl in StackPanel stayed 0 height "
            "size=(%.1fx%.1f) visualChildren=%u "
            "childLayout=%d childParent=%d childSize=(%.1fx%.1f)\n",
            control->GetRenderSize().width,
            control->GetRenderSize().height,
            Aero::Media::VisualTreeHelper::GetChildrenCount(*control.Value()),
            childElement != nullptr && childElement->GetIsLayoutAttached() ? 1 : 0,
            childElement != nullptr && childElement->LayoutParent() ==
                control.Value().Get() ? 1 : 0,
            childElement != nullptr ? childElement->GetRenderSize().width : 0.0,
            childElement != nullptr ? childElement->GetRenderSize().height : 0.0);
    }
    CHECK(control->GetRenderSize().height >= 1.0);
    CHECK(control->FindName("UpButton") != nullptr);
    return true;
}

bool TestLoadComponentStarGridUserControlInStackPanel() {
    LiveGui* live = NewTutorialLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({240.0, 200.0});

    Result<Ref<UserControl>> control = MakeRef<UserControl>();
    CHECK(control);
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> host = reader.Parse(StringView(
        "<StackPanel xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" Width=\"200\"/>"));
    CHECK(host);
    CHECK(view.SetContent(std::move(host).Value(), {240.0, 200.0}));
    Pump(view, 0.016);
    auto* stack = TryCast<StackPanel>(view.GetContent());
    CHECK(stack != nullptr);
    stack->GetChildren().Add(
        Ref<Aero::UIElement>(control.Value()));
    Pump(view, 0.032);
    CHECK(live->gui.LoadComponent(
        *control.Value(), "memory:///StarColorGrid.xaml"));
    Pump(view, 0.016);
    Pump(view, 0.032);
    if (control->GetRenderSize().height < 8.0) {
        std::fprintf(stderr,
            "LoadComponent star-grid UserControl stayed 0 height size=(%.1fx%.1f)\n",
            control->GetRenderSize().width,
            control->GetRenderSize().height);
    }
    CHECK(control->GetRenderSize().height >= 8.0);
    CHECK(control->FindName("R") != nullptr);
    return true;
}

TextBlock* FindTextBlock(Aero::Media::Visual& visual) noexcept {
    if (auto* text = TryCast<TextBlock>(&visual)) {
        return text;
    }
    const std::uint32_t count =
        Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
    for (std::uint32_t index = 0U; index < count; ++index) {
        Aero::Media::Visual* child =
            Aero::Media::VisualTreeHelper::GetChild(visual, index);
        if (child == nullptr) continue;
        if (TextBlock* found = FindTextBlock(*child)) {
            return found;
        }
    }
    return nullptr;
}

Path* FindFirstPath(Aero::Media::Visual& visual) {
    if (auto* path = TryCast<Path>(&visual)) {
        return path;
    }
    const std::uint32_t count =
        Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
    for (std::uint32_t index = 0U; index < count; ++index) {
        Aero::Media::Visual* child =
            Aero::Media::VisualTreeHelper::GetChild(visual, index);
        if (child == nullptr) continue;
        if (Path* found = FindFirstPath(*child)) {
            return found;
        }
    }
    return nullptr;
}

struct RepeatClickProbe {
    std::uint32_t count = 0;
    void OnClick(Aero::Base::Object*, Aero::RoutedEventArgs&) noexcept {
        ++count;
    }
};

bool TestMenuTooltipForegroundAndFocus() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({800.0, 600.0});

    constexpr char kTree[] =
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"LayoutRoot\" FocusManager.IsFocusScope=\"True\" Foreground=\"White\">"
        "<StackPanel>"
        "<ToggleButton x:Name=\"Start\" Content=\"START GAME\">"
        "<ToggleButton.Style>"
        "<Style TargetType=\"{x:Type ToggleButton}\">"
        "<Style.Triggers>"
        "<Trigger Property=\"IsKeyboardFocused\" Value=\"True\">"
        "<Setter Property=\"IsChecked\" Value=\"True\"/>"
        "</Trigger>"
        "</Style.Triggers>"
        "</Style>"
        "</ToggleButton.Style>"
        "</ToggleButton>"
        "<ContentControl x:Name=\"Tip\" Content=\"Dive straight into the adventure.\">"
        "<ContentControl.Style>"
        "<Style TargetType=\"{x:Type ContentControl}\">"
        "<Setter Property=\"OverridesDefaultStyle\" Value=\"True\"/>"
        "<Setter Property=\"Template\">"
        "<Setter.Value>"
        "<ControlTemplate TargetType=\"{x:Type ContentControl}\">"
        "<ContentPresenter x:Name=\"ContentHost\">"
        "<TextElement.Foreground>"
        "<SolidColorBrush Color=\"White\" Opacity=\"0\"/>"
        "</TextElement.Foreground>"
        "</ContentPresenter>"
        "</ControlTemplate>"
        "</Setter.Value>"
        "</Setter>"
        "</Style>"
        "</ContentControl.Style>"
        "</ContentControl>"
        "</StackPanel>"
        "</Grid>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "menu tooltip XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {800.0, 600.0}));
    Pump(view, 0.032);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    auto* layoutRoot = root->FindName<Grid>(StringView("LayoutRoot"));
    auto* start = root->FindName<ToggleButton>(StringView("Start"));
    auto* tip = root->FindName<ContentControl>(StringView("Tip"));
    CHECK(layoutRoot != nullptr && start != nullptr && tip != nullptr);
    static_cast<void>(tip->ApplyTemplate());
    Pump(view, 0.032);

    TextBlock* text = FindTextBlock(*tip);
    CHECK(text != nullptr);
    Ref<Aero::Media::Brush> foreground = text->GetForeground();
    CHECK(foreground);
    if (!(foreground->GetOpacity() < 0.05)) {
        std::fprintf(stderr,
            "tooltip TextElement.Foreground Opacity leaked through inherited "
            "Foreground opacity=%.3f\n",
            foreground->GetOpacity());
    }
    CHECK(foreground->GetOpacity() < 0.05);

    const bool scoped = layoutRoot->Focus();
    CHECK(scoped);
    Pump(view, 0.016);
    CHECK(start->GetIsKeyboardFocused());
    CHECK(!layoutRoot->GetIsKeyboardFocused());
    CHECK(start->GetIsChecked().GetHasValue());
    CHECK(start->GetIsChecked().GetValue());

    const bool layoutFocused = layoutRoot->Focus();
    CHECK(!layoutFocused);
    Pump(view, 0.016);
    CHECK(start->GetIsKeyboardFocused());
    CHECK(!layoutRoot->GetIsKeyboardFocused());
    return true;
}

bool TestViewboxTransform3DButtonHit() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({800.0, 600.0});

    constexpr char kTree[] =
        "<Viewbox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid x:Name=\"Host\" Width=\"400\" Height=\"300\" Background=\"#FF102030\">"
        "<Button x:Name=\"HitButton\" Width=\"200\" Height=\"80\" "
        "HorizontalAlignment=\"Center\" VerticalAlignment=\"Center\" "
        "Background=\"#FF3E4146\" Content=\"START GAME\"/>"
        "</Grid>"
        "</Viewbox>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    CHECK(document);
    Grid* host = document.Value().FindName<Grid>("Host");
    CHECK(host != nullptr);
    Result<Ref<CompositeTransform3D>> spin = MakeRef<CompositeTransform3D>();
    CHECK(spin);
    spin->SetRotationY(-8.0);
    host->SetTransform3D(spin.Value());
    CHECK(view.SetContent(std::move(document).Value(), {800.0, 600.0}));
    Pump(view, 0.032);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    Button* button = root->FindName<Button>(StringView("HitButton"));
    CHECK(button != nullptr);
    const Aero::Size buttonSize = button->GetRenderSize();
    CHECK(buttonSize.width > 0.0 && buttonSize.height > 0.0);

    bool hovered = false;
    for (int y = 40; y < 560 && !hovered; y += 16) {
        for (int x = 40; x < 760 && !hovered; x += 16) {
            static_cast<void>(view.MouseMove(x, y));
            Pump(view, 0.008);
            hovered = button->GetIsMouseOver();
        }
    }
    if (!hovered) {
        Point screen{};
        static_cast<void>(button->TryPointToScreen(
            {buttonSize.width * 0.5, buttonSize.height * 0.5}, screen));
        std::fprintf(stderr,
            "3D viewbox button was not hittable screen=(%.1f,%.1f) size=(%.1f,%.1f)\n",
            screen.x, screen.y, buttonSize.width, buttonSize.height);
    }
    CHECK(hovered);
    return true;
}

bool TestKeyboardNavigationIsTabStopTemplateTrigger() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({400.0, 200.0});

    constexpr char kTree[] =
        "<StackPanel xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel.Resources>"
        "<ControlTemplate x:Key=\"MenuTemplate\" TargetType=\"{x:Type ToggleButton}\">"
        "<Grid>"
        "<Path x:Name=\"CircledArrow\" Data=\"M0,0 L8,4 L0,8 z\" Width=\"16\" Height=\"16\" "
        "Fill=\"White\" Visibility=\"Visible\"/>"
        "</Grid>"
        "<ControlTemplate.Triggers>"
        "<Trigger Property=\"KeyboardNavigation.IsTabStop\" Value=\"False\">"
        "<Setter Property=\"Visibility\" TargetName=\"CircledArrow\" Value=\"Collapsed\"/>"
        "</Trigger>"
        "</ControlTemplate.Triggers>"
        "</ControlTemplate>"
        "</StackPanel.Resources>"
        "<ToggleButton x:Name=\"Menu\" Width=\"80\" Height=\"40\" "
        "Template=\"{StaticResource MenuTemplate}\"/>"
        "<ToggleButton x:Name=\"Setting\" Width=\"80\" Height=\"40\" IsTabStop=\"False\" "
        "Template=\"{StaticResource MenuTemplate}\"/>"
        "</StackPanel>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "IsTabStop trigger XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {400.0, 200.0}));
    Pump(view, 0.032);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    auto* menu = root->FindName<ToggleButton>(StringView("Menu"));
    auto* setting = root->FindName<ToggleButton>(StringView("Setting"));
    CHECK(menu != nullptr && setting != nullptr);
    static_cast<void>(menu->ApplyTemplate());
    static_cast<void>(setting->ApplyTemplate());
    Pump(view, 0.016);

    CHECK(menu->GetIsTabStop());
    CHECK(menu->GetValue(KeyboardNavigation::IsTabStopProperty));
    CHECK(!setting->GetIsTabStop());
    CHECK(!setting->GetValue(KeyboardNavigation::IsTabStopProperty));

    Path* menuArrow = FindFirstPath(*menu);
    Path* settingArrow = FindFirstPath(*setting);
    if (menuArrow == nullptr || settingArrow == nullptr) {
        std::fprintf(stderr,
            "CircledArrow path missing menuChildren=%u settingChildren=%u\n",
            Aero::Media::VisualTreeHelper::GetChildrenCount(*menu),
            Aero::Media::VisualTreeHelper::GetChildrenCount(*setting));
    }
    CHECK(menuArrow != nullptr && settingArrow != nullptr);
    if (menuArrow->GetVisibility() != Visibility::Visible) {
        std::fprintf(stderr,
            "menu CircledArrow collapsed despite IsTabStop=true\n");
    }
    CHECK(menuArrow->GetVisibility() == Visibility::Visible);
    CHECK(settingArrow->GetVisibility() == Visibility::Collapsed);
    return true;
}

double PathFillOpacity(Path& path) {
    Ref<Aero::Media::Brush> fill = path.GetFill();
    return fill ? fill->GetOpacity() : -1.0;
}

bool TestCheckedVisualStateRevertsArrowOpacity() {
    ViewOptions options;
    options.automaticAnimationClock = false;
    LiveGui* live = NewLiveGui(options);
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({400.0, 200.0});

    constexpr char kTree[] =
        "<StackPanel xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel.Resources>"
        "<ControlTemplate x:Key=\"MenuTemplate\" TargetType=\"{x:Type ToggleButton}\">"
        "<Grid>"
        "<VisualStateManager.VisualStateGroups>"
        "<VisualStateGroup x:Name=\"CheckStates\">"
        "<VisualStateGroup.Transitions>"
        "<VisualTransition From=\"Checked\" GeneratedDuration=\"0:0:0.2\"/>"
        "</VisualStateGroup.Transitions>"
        "<VisualState x:Name=\"Checked\">"
        "<Storyboard>"
        "<DoubleAnimationUsingKeyFrames Storyboard.TargetProperty=\"(Shape.Fill).(Brush.Opacity)\" "
        "Storyboard.TargetName=\"CircledArrow\">"
        "<EasingDoubleKeyFrame KeyTime=\"0\" Value=\"1\"/>"
        "</DoubleAnimationUsingKeyFrames>"
        "</Storyboard>"
        "</VisualState>"
        "<VisualState x:Name=\"Unchecked\"/>"
        "</VisualStateGroup>"
        "</VisualStateManager.VisualStateGroups>"
        "<Path x:Name=\"CircledArrow\" Data=\"M0,0 L8,4 L0,8 z\" Width=\"16\" Height=\"16\">"
        "<Path.Fill>"
        "<SolidColorBrush Color=\"White\" Opacity=\"0\"/>"
        "</Path.Fill>"
        "</Path>"
        "</Grid>"
        "</ControlTemplate>"
        "</StackPanel.Resources>"
        "<ToggleButton x:Name=\"Start\" Width=\"80\" Height=\"40\" "
        "Template=\"{StaticResource MenuTemplate}\">"
        "<ToggleButton.Style>"
        "<Style TargetType=\"{x:Type ToggleButton}\">"
        "<Style.Triggers>"
        "<Trigger Property=\"IsKeyboardFocused\" Value=\"True\">"
        "<Setter Property=\"IsChecked\" Value=\"True\"/>"
        "</Trigger>"
        "</Style.Triggers>"
        "</Style>"
        "</ToggleButton.Style>"
        "</ToggleButton>"
        "<ToggleButton x:Name=\"Settings\" Width=\"80\" Height=\"40\" "
        "Template=\"{StaticResource MenuTemplate}\">"
        "<ToggleButton.Style>"
        "<Style TargetType=\"{x:Type ToggleButton}\">"
        "<Style.Triggers>"
        "<Trigger Property=\"IsKeyboardFocused\" Value=\"True\">"
        "<Setter Property=\"IsChecked\" Value=\"True\"/>"
        "</Trigger>"
        "</Style.Triggers>"
        "</Style>"
        "</ToggleButton.Style>"
        "</ToggleButton>"
        "</StackPanel>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "checked VSM XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {400.0, 200.0}));
    Pump(view, 0.032);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    auto* start = root->FindName<ToggleButton>(StringView("Start"));
    auto* settings = root->FindName<ToggleButton>(StringView("Settings"));
    CHECK(start != nullptr && settings != nullptr);
    static_cast<void>(start->ApplyTemplate());
    static_cast<void>(settings->ApplyTemplate());
    Pump(view, 0.016);

    Path* startArrow = FindFirstPath(*start);
    Path* settingsArrow = FindFirstPath(*settings);
    CHECK(startArrow != nullptr && settingsArrow != nullptr);
    CHECK(PathFillOpacity(*startArrow) < 0.05);

    const bool focused = start->Focus();
    CHECK(focused);
    static_cast<void>(VisualStateManager::GoToState(*start, "Checked", false));
    Pump(view, 0.05);
    CHECK(start->GetIsKeyboardFocused());
    CHECK(start->GetIsChecked().GetHasValue() && start->GetIsChecked().GetValue());
    if (!(PathFillOpacity(*startArrow) > 0.8)) {
        std::fprintf(stderr, "checked arrow opacity stayed %.3f\n",
            PathFillOpacity(*startArrow));
    }
    CHECK(PathFillOpacity(*startArrow) > 0.8);

    const bool moved = settings->Focus();
    CHECK(moved);
    Pump(view, 0.35);
    CHECK(settings->GetIsKeyboardFocused());
    CHECK(!start->GetIsKeyboardFocused());
    if (start->GetIsChecked().GetHasValue() && start->GetIsChecked().GetValue()) {
        std::fprintf(stderr, "start IsChecked stayed true after focus moved\n");
    }
    CHECK(!(start->GetIsChecked().GetHasValue() && start->GetIsChecked().GetValue()));
    if (!(PathFillOpacity(*startArrow) < 0.15)) {
        std::fprintf(stderr,
            "unchecked arrow opacity stayed %.3f after leave\n",
            PathFillOpacity(*startArrow));
    }
    CHECK(PathFillOpacity(*startArrow) < 0.15);
    CHECK(PathFillOpacity(*settingsArrow) > 0.8);
    return true;
}

bool TestRepeatButtonPressClickAndScaleX() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({800.0, 600.0});

    constexpr char kTree[] =
        "<Viewbox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid x:Name=\"Host\" Width=\"400\" Height=\"80\" Background=\"#FF102030\">"
        "<RepeatButton x:Name=\"NextButton\" Width=\"80\" Height=\"48\" "
        "HorizontalAlignment=\"Right\" VerticalAlignment=\"Center\" "
        "Background=\"#FF3E4146\" RenderTransformOrigin=\"0.5,0.5\">"
        "<RepeatButton.RenderTransform>"
        "<ScaleTransform ScaleX=\"-1\"/>"
        "</RepeatButton.RenderTransform>"
        "</RepeatButton>"
        "</Grid>"
        "</Viewbox>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    CHECK(document);
    Grid* host = document.Value().FindName<Grid>("Host");
    CHECK(host != nullptr);
    Result<Ref<CompositeTransform3D>> spin = MakeRef<CompositeTransform3D>();
    CHECK(spin);
    spin->SetRotationY(-8.0);
    host->SetTransform3D(spin.Value());
    CHECK(view.SetContent(std::move(document).Value(), {800.0, 600.0}));
    Pump(view, 0.032);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    RepeatButton* button = root->FindName<RepeatButton>(StringView("NextButton"));
    CHECK(button != nullptr);
    CHECK(button->GetClickMode() == ClickMode::Press);

    RepeatClickProbe probe;
    button->Click().Add({&probe, &RepeatClickProbe::OnClick});

    bool hovered = false;
    int clickX = 0;
    int clickY = 0;
    for (int y = 40; y < 560 && !hovered; y += 8) {
        for (int x = 40; x < 760 && !hovered; x += 8) {
            static_cast<void>(view.MouseMove(x, y));
            Pump(view, 0.008);
            if (button->GetIsMouseOver()) {
                hovered = true;
                clickX = x;
                clickY = y;
            }
        }
    }
    if (!hovered) {
        const Aero::Size buttonSize = button->GetRenderSize();
        Point screen{};
        static_cast<void>(button->TryPointToScreen(
            {buttonSize.width * 0.5, buttonSize.height * 0.5}, screen));
        std::fprintf(stderr,
            "scaled RepeatButton was not hittable screen=(%.1f,%.1f) size=(%.1f,%.1f)\n",
            screen.x, screen.y, buttonSize.width, buttonSize.height);
    }
    CHECK(hovered);
    static_cast<void>(view.MouseButtonDown(
        clickX, clickY, Aero::Input::MouseButton::Left));
    Pump(view, 0.016);
    if (probe.count == 0U) {
        std::fprintf(stderr,
            "RepeatButton ClickMode=Press did not fire Click on MouseDown "
            "pressed=%d over=%d\n",
            button->GetIsPressed() ? 1 : 0,
            button->GetIsMouseOver() ? 1 : 0);
    }
    CHECK(probe.count >= 1U);
    static_cast<void>(view.MouseButtonUp(
        clickX, clickY, Aero::Input::MouseButton::Left));
    Pump(view, 0.016);
    return true;
}

bool TestComboBoxPopupItemClickSelects() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({800.0, 600.0});

    constexpr char kTree[] =
        "<Viewbox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid Width=\"400\" Height=\"300\">"
        "<Grid.Resources>"
        "<Style TargetType=\"{x:Type ComboBox}\">"
        "<Setter Property=\"Template\">"
        "<Setter.Value>"
        "<ControlTemplate TargetType=\"{x:Type ComboBox}\">"
        "<Grid>"
        "<ToggleButton IsChecked=\"{Binding IsDropDownOpen, Mode=TwoWay, "
        "RelativeSource={RelativeSource TemplatedParent}}\"/>"
        "<ContentPresenter x:Name=\"ContentSite\" ContentSource=\"SelectionBoxItem\" "
        "IsHitTestVisible=\"False\"/>"
        "<Popup x:Name=\"PART_Popup\" IsOpen=\"{Binding IsDropDownOpen, "
        "RelativeSource={RelativeSource TemplatedParent}}\" Placement=\"Bottom\" "
        "VerticalOffset=\"1\">"
        "<Border Background=\"#FF393B40\" MinWidth=\"{TemplateBinding ActualWidth}\">"
        "<ScrollViewer>"
        "<ItemsPresenter/>"
        "</ScrollViewer>"
        "</Border>"
        "</Popup>"
        "</Grid>"
        "</ControlTemplate>"
        "</Setter.Value>"
        "</Setter>"
        "</Style>"
        "</Grid.Resources>"
        "<ComboBox x:Name=\"HitCombo\" Width=\"140\" Height=\"28\" "
        "HorizontalAlignment=\"Left\" VerticalAlignment=\"Bottom\" "
        "Margin=\"8,0,0,8\">"
        "<ComboBoxItem Content=\"Alpha\"/>"
        "<ComboBoxItem Content=\"Beta\"/>"
        "<ComboBoxItem Content=\"Gamma\"/>"
        "</ComboBox>"
        "</Grid>"
        "</Viewbox>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "combo click XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {800.0, 600.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    ComboBox* combo = root->FindName<ComboBox>(StringView("HitCombo"));
    CHECK(combo != nullptr);
    static_cast<void>(combo->ApplyTemplate());
    combo->SetSelectedIndex(0U);
    Pump(view, 0.016);
    CHECK(combo->GetSelectedIndex() == 0U);

    combo->SetIsDropDownOpen(true);
    Pump(view, 0.064);
    Pump(view, 0.080);
    CHECK(combo->GetIsDropDownOpen());

    Panel* itemsHost = combo->GetItemsHost();
    CHECK(itemsHost != nullptr);
    CHECK(Aero::Media::VisualTreeHelper::GetChildrenCount(*itemsHost) >= 2U);
    Aero::Media::Visual* itemVisual =
        Aero::Media::VisualTreeHelper::GetChild(*itemsHost, 1U);
    auto* item = TryCast<ComboBoxItem>(itemVisual);
    CHECK(item != nullptr);
    const Aero::Size itemSize = item->GetRenderSize();
    CHECK(itemSize.width > 0.0 && itemSize.height > 0.0);
    Point itemScreen{};
    CHECK(item->TryPointToScreen(
        {itemSize.width * 0.5, itemSize.height * 0.5}, itemScreen));
    const int clickX = static_cast<int>(itemScreen.x);
    const int clickY = static_cast<int>(itemScreen.y);
    static_cast<void>(view.MouseMove(clickX, clickY));
    Pump(view, 0.032);
    if (!item->GetIsMouseOver()) {
        std::fprintf(stderr,
            "combo click hover miss screen=(%.1f,%.1f) click=(%d,%d)\n",
            itemScreen.x, itemScreen.y, clickX, clickY);
    }
    CHECK(item->GetIsMouseOver());
    static_cast<void>(view.MouseButtonDown(
        clickX, clickY, Aero::Input::MouseButton::Left));
    static_cast<void>(view.MouseButtonUp(
        clickX, clickY, Aero::Input::MouseButton::Left));
    Pump(view, 0.048);
    if (combo->GetSelectedIndex() != 1U) {
        std::fprintf(stderr,
            "combo click did not select index=%u dropDown=%d text=%.*s\n",
            combo->GetSelectedIndex(),
            combo->GetIsDropDownOpen() ? 1 : 0,
            static_cast<int>(combo->GetSelectionBoxText().SizeBytes()),
            combo->GetSelectionBoxText().Data());
    }
    CHECK(combo->GetSelectedIndex() == 1U);
    CHECK(!combo->GetIsDropDownOpen());
    return true;
}

bool TestComboBoxItemsSourcePopupClickSelects() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({800.0, 600.0});

    constexpr char kTree[] =
        "<Viewbox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid Width=\"400\" Height=\"300\">"
        "<Grid.Resources>"
        "<Style TargetType=\"{x:Type ComboBox}\">"
        "<Setter Property=\"Template\">"
        "<Setter.Value>"
        "<ControlTemplate TargetType=\"{x:Type ComboBox}\">"
        "<Grid>"
        "<ToggleButton IsChecked=\"{Binding IsDropDownOpen, Mode=TwoWay, "
        "RelativeSource={RelativeSource TemplatedParent}}\"/>"
        "<ContentPresenter x:Name=\"ContentSite\" ContentSource=\"SelectionBoxItem\" "
        "IsHitTestVisible=\"False\"/>"
        "<Popup x:Name=\"PART_Popup\" IsOpen=\"{Binding IsDropDownOpen, "
        "RelativeSource={RelativeSource TemplatedParent}}\" Placement=\"Bottom\" "
        "VerticalOffset=\"1\">"
        "<Border Background=\"#FF393B40\" MinWidth=\"{TemplateBinding ActualWidth}\">"
        "<ScrollViewer>"
        "<ItemsPresenter/>"
        "</ScrollViewer>"
        "</Border>"
        "</Popup>"
        "</Grid>"
        "</ControlTemplate>"
        "</Setter.Value>"
        "</Setter>"
        "</Style>"
        "</Grid.Resources>"
        "<ComboBox x:Name=\"HitCombo\" Width=\"140\" Height=\"28\" "
        "HorizontalAlignment=\"Left\" VerticalAlignment=\"Bottom\" "
        "Margin=\"8,0,0,8\"/>"
        "</Grid>"
        "</Viewbox>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "combo items-source click XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {800.0, 600.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    ComboBox* combo = root->FindName<ComboBox>(StringView("HitCombo"));
    CHECK(combo != nullptr);
    Result<Ref<ObservableObjectCollection>> teams =
        MakeRef<ObservableObjectCollection>();
    CHECK(teams);
    Aero::Controls::AddBoxedStringItem(*teams.Value(), "Overall");
    Aero::Controls::AddBoxedStringItem(*teams.Value(), "Alliance");
    Aero::Controls::AddBoxedStringItem(*teams.Value(), "Horde");
    combo->SetItemsSource(Ref<Aero::Base::Object>(teams.Value()));
    static_cast<void>(combo->ApplyTemplate());
    combo->SetSelectedIndex(0U);
    Pump(view, 0.048);
    CHECK(combo->GetSelectedIndex() == 0U);
    CHECK(combo->GetSelectionBoxText() == StringView("Overall"));

    combo->SetIsDropDownOpen(true);
    Pump(view, 0.064);
    Pump(view, 0.080);
    CHECK(combo->GetIsDropDownOpen());

    Panel* itemsHost = combo->GetItemsHost();
    CHECK(itemsHost != nullptr);
    CHECK(Aero::Media::VisualTreeHelper::GetChildrenCount(*itemsHost) >= 3U);
    Aero::Media::Visual* itemVisual =
        Aero::Media::VisualTreeHelper::GetChild(*itemsHost, 2U);
    auto* item = TryCast<ComboBoxItem>(itemVisual);
    CHECK(item != nullptr);
    const Aero::Size itemSize = item->GetRenderSize();
    CHECK(itemSize.width > 0.0 && itemSize.height > 0.0);
    Point itemScreen{};
    CHECK(item->TryPointToScreen(
        {itemSize.width * 0.5, itemSize.height * 0.5}, itemScreen));
    const int clickX = static_cast<int>(itemScreen.x);
    const int clickY = static_cast<int>(itemScreen.y);
    static_cast<void>(view.MouseMove(clickX, clickY));
    Pump(view, 0.096);
    CHECK(item->GetIsMouseOver());
    static_cast<void>(view.MouseButtonDown(
        clickX, clickY, Aero::Input::MouseButton::Left));
    static_cast<void>(view.MouseButtonUp(
        clickX, clickY, Aero::Input::MouseButton::Left));
    Pump(view, 0.112);
    if (combo->GetSelectedIndex() != 2U ||
        combo->GetSelectionBoxText() != StringView("Horde")) {
        std::fprintf(stderr,
            "combo items-source click index=%u dropDown=%d text=%.*s\n",
            combo->GetSelectedIndex(),
            combo->GetIsDropDownOpen() ? 1 : 0,
            static_cast<int>(combo->GetSelectionBoxText().SizeBytes()),
            combo->GetSelectionBoxText().Data());
    }
    CHECK(combo->GetSelectedIndex() == 2U);
    CHECK(combo->GetSelectionBoxText() == StringView("Horde"));
    CHECK(!combo->GetIsDropDownOpen());

    combo->SetIsDropDownOpen(true);
    Pump(view, 0.128);
    CHECK(combo->GetIsDropDownOpen());
    static_cast<void>(view.MouseButtonDown(10, 10, Aero::Input::MouseButton::Left));
    static_cast<void>(view.MouseButtonUp(10, 10, Aero::Input::MouseButton::Left));
    Pump(view, 0.144);
    CHECK(!combo->GetIsDropDownOpen());
    return true;
}

bool TestDataTemplateElementNameSelectedItemFilter() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({400.0, 300.0});

    constexpr char kTree[] =
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "Width=\"400\" Height=\"300\">"
        "<Grid.Resources>"
        "<DataTemplate x:Key=\"PlayerTemplate\">"
        "<Border x:Name=\"PlayerRow\" Height=\"24\" Background=\"#FF334455\"/>"
        "<DataTemplate.Triggers>"
        "<DataTrigger Binding=\"{Binding SelectedItem, ElementName=VisibleTeam}\" "
        "Value=\"Horde\">"
        "<Setter TargetName=\"PlayerRow\" Property=\"Visibility\" Value=\"Collapsed\"/>"
        "</DataTrigger>"
        "</DataTemplate.Triggers>"
        "</DataTemplate>"
        "</Grid.Resources>"
        "<StackPanel>"
        "<ComboBox x:Name=\"VisibleTeam\" Width=\"150\" Height=\"28\"/>"
        "<ItemsControl x:Name=\"Players\" ItemTemplate=\"{StaticResource PlayerTemplate}\"/>"
        "</StackPanel>"
        "</Grid>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "element-name filter XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {400.0, 300.0}));
    Pump(view, 0.016);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    ComboBox* combo = root->FindName<ComboBox>(StringView("VisibleTeam"));
    ItemsControl* players =
        root->FindName<ItemsControl>(StringView("Players"));
    CHECK(combo != nullptr);
    CHECK(players != nullptr);

    Result<Ref<ObservableObjectCollection>> teams =
        MakeRef<ObservableObjectCollection>();
    CHECK(teams);
    Aero::Controls::AddBoxedStringItem(*teams.Value(), "Overall");
    Aero::Controls::AddBoxedStringItem(*teams.Value(), "Alliance");
    Aero::Controls::AddBoxedStringItem(*teams.Value(), "Horde");
    combo->SetItemsSource(Ref<Aero::Base::Object>(teams.Value()));
    combo->SetSelectedIndex(0U);

    Result<Ref<ObservableCollection<BindingSlotItem>>> items =
        MakeRef<ObservableCollection<BindingSlotItem>>();
    Result<Ref<BindingSlotItem>> first = MakeRef<BindingSlotItem>();
    Result<Ref<BindingSlotItem>> second = MakeRef<BindingSlotItem>();
    CHECK(items && first && second);
    items->Add(first.Value());
    items->Add(second.Value());
    players->SetItemsSource(Ref<Aero::Base::Object>(items.Value()));
    static_cast<void>(combo->ApplyTemplate());
    static_cast<void>(players->ApplyTemplate());
    Pump(view, 0.032);
    Pump(view, 0.048);
    CHECK(players->GetRealizedItemCount() == 2U);

    const auto findRow = [](auto& self, Aero::Media::Visual& visual)
        -> Border* {
        if (auto* border = TryCast<Border>(&visual)) {
            if (Near(border->GetHeight(), 24.0, 0.5)) {
                return border;
            }
        }
        const std::uint32_t count =
            Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
        for (std::uint32_t index = 0U; index < count; ++index) {
            Aero::Media::Visual* child =
                Aero::Media::VisualTreeHelper::GetChild(visual, index);
            if (child == nullptr) continue;
            if (Border* found = self(self, *child)) return found;
        }
        return nullptr;
    };
    Border* row = findRow(findRow, *players);
    CHECK(row != nullptr);
    CHECK(row->GetVisibility() == Visibility::Visible);

    combo->SetSelectedIndex(2U);
    Pump(view, 0.080);
    Pump(view, 0.096);
    if (row->GetVisibility() != Visibility::Collapsed) {
        std::fprintf(stderr,
            "element-name filter did not collapse visibility=%u selected=%u\n",
            static_cast<unsigned>(row->GetVisibility()),
            combo->GetSelectedIndex());
    }
    CHECK(row->GetVisibility() == Visibility::Collapsed);
    return true;
}

bool TestDataTemplateMultiDataTriggerTeamFilter() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({400.0, 300.0});

    constexpr char kTree[] =
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "Width=\"400\" Height=\"300\">"
        "<Grid.Resources>"
        "<DataTemplate x:Key=\"PlayerTemplate\">"
        "<Border x:Name=\"PlayerRow\" Height=\"24\" Background=\"#FF334455\"/>"
        "<DataTemplate.Triggers>"
        "<MultiDataTrigger>"
        "<MultiDataTrigger.Conditions>"
        "<Condition Binding=\"{Binding SelectedItem, ElementName=VisibleTeam}\" "
        "Value=\"Horde\"/>"
        "<Condition Binding=\"{Binding Team}\" Value=\"Alliance\"/>"
        "</MultiDataTrigger.Conditions>"
        "<Setter TargetName=\"PlayerRow\" Property=\"Visibility\" Value=\"Collapsed\"/>"
        "</MultiDataTrigger>"
        "<MultiDataTrigger>"
        "<MultiDataTrigger.Conditions>"
        "<Condition Binding=\"{Binding SelectedItem, ElementName=VisibleTeam}\" "
        "Value=\"Alliance\"/>"
        "<Condition Binding=\"{Binding Team}\" Value=\"Horde\"/>"
        "</MultiDataTrigger.Conditions>"
        "<Setter TargetName=\"PlayerRow\" Property=\"Visibility\" Value=\"Collapsed\"/>"
        "</MultiDataTrigger>"
        "</DataTemplate.Triggers>"
        "</DataTemplate>"
        "</Grid.Resources>"
        "<StackPanel>"
        "<ItemsControl x:Name=\"Players\" ItemTemplate=\"{StaticResource PlayerTemplate}\"/>"
        "<ComboBox x:Name=\"VisibleTeam\" Width=\"150\" Height=\"28\"/>"
        "</StackPanel>"
        "</Grid>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "multi-data filter XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {400.0, 300.0}));
    Pump(view, 0.016);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    ComboBox* combo = root->FindName<ComboBox>(StringView("VisibleTeam"));
    ItemsControl* players =
        root->FindName<ItemsControl>(StringView("Players"));
    CHECK(combo != nullptr);
    CHECK(players != nullptr);

    Result<Ref<ObservableObjectCollection>> teams =
        MakeRef<ObservableObjectCollection>();
    CHECK(teams);
    Aero::Controls::AddBoxedStringItem(*teams.Value(), "Overall");
    Aero::Controls::AddBoxedStringItem(*teams.Value(), "Alliance");
    Aero::Controls::AddBoxedStringItem(*teams.Value(), "Horde");
    combo->SetItemsSource(Ref<Aero::Base::Object>(teams.Value()));
    combo->SetSelectedIndex(0U);

    Result<Ref<ObservableCollection<BindingSlotItem>>> items =
        MakeRef<ObservableCollection<BindingSlotItem>>();
    Result<Ref<BindingSlotItem>> alliance = MakeRef<BindingSlotItem>();
    Result<Ref<BindingSlotItem>> horde = MakeRef<BindingSlotItem>();
    CHECK(items && alliance && horde);
    String allianceTeam;
    String hordeTeam;
    CHECK(allianceTeam.Assign("Alliance"));
    CHECK(hordeTeam.Assign("Horde"));
    alliance->SetTeam(std::move(allianceTeam));
    horde->SetTeam(std::move(hordeTeam));
    items->Add(alliance.Value());
    items->Add(horde.Value());
    players->SetItemsSource(Ref<Aero::Base::Object>(items.Value()));
    static_cast<void>(combo->ApplyTemplate());
    static_cast<void>(players->ApplyTemplate());
    Pump(view, 0.032);
    Pump(view, 0.048);
    CHECK(players->GetRealizedItemCount() == 2U);

    Border* rows[2] = {nullptr, nullptr};
    std::uint32_t rowCount = 0U;
    const auto collectRows = [&](auto& self, Aero::Media::Visual& visual)
        -> void {
        if (rowCount >= 2U) return;
        if (auto* border = TryCast<Border>(&visual)) {
            if (Near(border->GetHeight(), 24.0, 0.5)) {
                rows[rowCount++] = border;
                return;
            }
        }
        const std::uint32_t count =
            Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
        for (std::uint32_t index = 0U; index < count; ++index) {
            Aero::Media::Visual* child =
                Aero::Media::VisualTreeHelper::GetChild(visual, index);
            if (child == nullptr) continue;
            self(self, *child);
        }
    };
    collectRows(collectRows, *players);
    CHECK(rowCount == 2U);
    CHECK(rows[0] != nullptr && rows[1] != nullptr);
    CHECK(rows[0]->GetVisibility() == Visibility::Visible);
    CHECK(rows[1]->GetVisibility() == Visibility::Visible);

    combo->SetSelectedIndex(2U);
    Pump(view, 0.064);
    Pump(view, 0.080);
    if (rows[0]->GetVisibility() != Visibility::Collapsed ||
        rows[1]->GetVisibility() != Visibility::Visible) {
        std::fprintf(stderr,
            "horde filter vis alliance=%u horde=%u selected=%u\n",
            static_cast<unsigned>(rows[0]->GetVisibility()),
            static_cast<unsigned>(rows[1]->GetVisibility()),
            combo->GetSelectedIndex());
    }
    CHECK(rows[0]->GetVisibility() == Visibility::Collapsed);
    CHECK(rows[1]->GetVisibility() == Visibility::Visible);

    combo->SetSelectedIndex(1U);
    Pump(view, 0.096);
    Pump(view, 0.112);
    CHECK(rows[0]->GetVisibility() == Visibility::Visible);
    CHECK(rows[1]->GetVisibility() == Visibility::Collapsed);
    return true;
}

bool TestDataTemplateItemHoverStoryboard() {
    ViewOptions options;
    options.automaticAnimationClock = false;
    LiveGui* live = NewLiveGui(options);
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({400.0, 300.0});

    constexpr char kTree[] =
        "<Viewbox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid Width=\"400\" Height=\"300\">"
        "<Grid.Resources>"
        "<DataTemplate x:Key=\"PlayerTemplate\">"
        "<DataTemplate.Resources>"
        "<Storyboard x:Key=\"OverOn\">"
        "<DoubleAnimationUsingKeyFrames "
        "Storyboard.TargetProperty=\"(UIElement.RenderTransform)."
        "(TransformGroup.Children)[3].(TranslateTransform.X)\" "
        "Storyboard.TargetName=\"PlayerRow\">"
        "<EasingDoubleKeyFrame KeyTime=\"0:0:0.15\" Value=\"8\"/>"
        "</DoubleAnimationUsingKeyFrames>"
        "</Storyboard>"
        "</DataTemplate.Resources>"
        "<Grid x:Name=\"PlayerRow\" Background=\"#FF334455\" Height=\"40\">"
        "<Grid.RenderTransform>"
        "<TransformGroup>"
        "<ScaleTransform/>"
        "<SkewTransform/>"
        "<RotateTransform/>"
        "<TranslateTransform/>"
        "</TransformGroup>"
        "</Grid.RenderTransform>"
        "<TextBlock Text=\"Alpha\" VerticalAlignment=\"Center\" Margin=\"8,0\"/>"
        "</Grid>"
        "<DataTemplate.Triggers>"
        "<Trigger Property=\"IsMouseOver\" Value=\"True\">"
        "<Trigger.EnterActions>"
        "<BeginStoryboard Storyboard=\"{StaticResource OverOn}\"/>"
        "</Trigger.EnterActions>"
        "</Trigger>"
        "</DataTemplate.Triggers>"
        "</DataTemplate>"
        "</Grid.Resources>"
        "<ItemsControl x:Name=\"Players\" ItemTemplate=\"{StaticResource PlayerTemplate}\">"
        "<ItemsControl.ItemsPanel>"
        "<ItemsPanelTemplate><StackPanel/></ItemsPanelTemplate>"
        "</ItemsControl.ItemsPanel>"
        "</ItemsControl>"
        "</Grid>"
        "</Viewbox>";

    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "data template hover XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {400.0, 300.0}));
    Pump(view, 0.016);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    ItemsControl* players =
        root->FindName<ItemsControl>(StringView("Players"));
    CHECK(players != nullptr);

    const DataTemplate* itemTemplate = players->GetItemTemplate().Get();
    CHECK(itemTemplate != nullptr);
    Aero::ResourceValue overOn =
        itemTemplate->GetResources().Lookup(StringView("OverOn"));
    if (overOn.IsUnset()) {
        std::fprintf(stderr,
            "DataTemplate.Resources missing OverOn\n");
    }
    CHECK(!overOn.IsUnset());

    Result<Ref<ObservableCollection<BindingSlotItem>>> items =
        MakeRef<ObservableCollection<BindingSlotItem>>();
    Result<Ref<BindingSlotItem>> item = MakeRef<BindingSlotItem>();
    CHECK(items && item);
    items->Add(item.Value());
    players->SetItemsSource(Ref<Aero::Base::Object>(items.Value()));
    static_cast<void>(players->ApplyTemplate());
    Pump(view, 0.032);
    Pump(view, 0.048);
    static_cast<void>(players->ApplyTemplate());
    Pump(view, 0.064);
    if (players->GetRealizedItemCount() != 1U) {
        std::fprintf(stderr,
            "data template hover realized=%u host=%d\n",
            players->GetRealizedItemCount(),
            players->GetItemsHost() != nullptr);
    }
    CHECK(players->GetRealizedItemCount() == 1U);

    const auto findRow = [](auto& self, Aero::Media::Visual& visual)
        -> Grid* {
        if (auto* grid = TryCast<Grid>(&visual)) {
            auto* group = TryCast<Aero::Media::TransformGroup>(
                grid->GetRenderTransform().Get());
            if (group != nullptr && group->GetChildren().Size() > 3U) {
                return grid;
            }
        }
        const std::uint32_t count =
            Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
        for (std::uint32_t index = 0U; index < count; ++index) {
            Aero::Media::Visual* child =
                Aero::Media::VisualTreeHelper::GetChild(visual, index);
            if (child == nullptr) continue;
            if (Grid* found = self(self, *child)) return found;
        }
        return nullptr;
    };
    Grid* row = findRow(findRow, *players);
    if (row == nullptr && root != nullptr) {
        row = findRow(findRow, *root);
    }
    if (row == nullptr) {
        std::fprintf(stderr, "data template hover PlayerRow was not realized\n");
    }
    CHECK(row != nullptr);

    const Aero::Size rowSize = row->GetRenderSize();
    CHECK(rowSize.width > 0.0 && rowSize.height > 0.0);
    Point rowScreen{};
    CHECK(row->TryPointToScreen(
        {rowSize.width * 0.5, rowSize.height * 0.5}, rowScreen));
    static_cast<void>(view.MouseMove(
        static_cast<int>(rowScreen.x),
        static_cast<int>(rowScreen.y)));
    Pump(view, 0.080);
    if (!row->GetIsMouseOver()) {
        std::fprintf(stderr,
            "data template hover miss screen=(%.1f,%.1f) size=(%.1f,%.1f) "
            "slot=(%.1f,%.1f,%.1f,%.1f)\n",
            rowScreen.x, rowScreen.y,
            rowSize.width, rowSize.height,
            row->GetLayoutSlot().x, row->GetLayoutSlot().y,
            row->GetLayoutSlot().width, row->GetLayoutSlot().height);
    }
    CHECK(row->GetIsMouseOver());

    Pump(view, 0.250);
    auto* group = TryCast<Aero::Media::TransformGroup>(
        row->GetRenderTransform().Get());
    CHECK(group != nullptr);
    CHECK(group->GetChildren().Size() > 3U);
    auto* translate = TryCast<Aero::Media::TranslateTransform>(
        group->GetChildren()[3].Get());
    CHECK(translate != nullptr);
    if (!(translate->GetX() > 1.0)) {
        std::fprintf(stderr,
            "data template hover storyboard did not run translate.x=%.3f "
            "over=%d\n",
            translate->GetX(),
            row->GetIsMouseOver() ? 1 : 0);
    }
    CHECK(translate->GetX() > 1.0);
    return true;
}

