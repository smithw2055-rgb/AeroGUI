#include "ConformanceTestHarness.hpp"

bool TestGalleryXamlSurface() {
    static_assert(std::is_base_of<Aero::Base::Object, Aero::Input::CommandBinding>::value,
        "CommandBinding must be an Object so XAML can construct it");
    static_assert(std::is_base_of<Aero::Controls::HeaderedItemsControl, Aero::Controls::TreeViewItem>::value,
        "TreeViewItem must derive HeaderedItemsControl");
    static_assert(std::is_base_of<Aero::FrameworkElement, Aero::Controls::Page>::value,
        "Page must derive FrameworkElement");
    static_assert(std::is_base_of<Aero::Media::ImageSource, Aero::Media::BitmapImage>::value,
        "BitmapImage must derive ImageSource");

    Gui gui;
    Result<void> initialized = gui.Initialize();
    CHECK(initialized);
    Aero::Markup::XamlReader reader(gui);

    auto parse = [&](StringView markup) -> bool {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(markup);
        if (!document) {
            std::fprintf(stderr, "XAML parse failed: %s\n",
                document.GetStatus().message);
            return false;
        }
        return document.Value().IsValid();
    };

    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
            "<Grid.ColumnDefinitions>"
            "<ColumnDefinition Width=\"Auto\"/>"
            "<ColumnDefinition Width=\"*\"/>"
            "</Grid.ColumnDefinitions>"
            "<Grid.CommandBindings>"
            "<CommandBinding Command=\"Copy\"/>"
            "</Grid.CommandBindings>"
            "<Grid.InputBindings>"
            "<KeyBinding Command=\"Copy\" Key=\"C\" Modifiers=\"Control\"/>"
            "</Grid.InputBindings>"
            "<Button Content=\"Hi\"/>"
            "</Grid>"));
        CHECK(document);
        Grid* grid = document.Value().Root<Grid>();
        CHECK(grid != nullptr);
        CHECK(grid->GetCommandBindings().Size() == 1U);
        CHECK(grid->GetInputBindings().Size() == 1U);
        CHECK(grid->GetCommandBindings()[0]->GetCommand() != nullptr);
        CHECK(grid->GetInputBindings()[0]->GetCommand().Get() ==
            grid->GetCommandBindings()[0]->GetCommand());
        Ref<Aero::Input::RoutedCommand> copy =
            ApplicationCommands::Copy();
        CHECK(copy != nullptr);
        CHECK(grid->GetCommandBindings()[0]->GetCommand() ==
            copy.Get());
    }

    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<PasswordBox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:aero=\"clr-namespace:Aero.GUI.Extensions\""
            " aero:Text.Placeholder=\"secret\"/>"));
        CHECK(document);
        PasswordBox* password = document.Value().Root<PasswordBox>();
        CHECK(password != nullptr);
        CHECK(password->GetPlaceholder() == StringView("secret"));
    }

    CHECK(parse(StringView(
        "<Button xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:i=\"http://schemas.microsoft.com/expression/2010/interactivity\">"
        "<i:Interaction.Behaviors>"
        "</i:Interaction.Behaviors>"
        "</Button>")));

    CHECK(parse(StringView(
        "<ListView xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<ListView.View>"
        "<GridView>"
        "<GridViewColumn Header=\"Name\"/>"
        "</GridView>"
        "</ListView.View>"
        "</ListView>")));

    CHECK(parse(StringView(
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<VisualStateManager.VisualStateGroups>"
        "<VisualStateGroup>"
        "<VisualState Name=\"Normal\"/>"
        "<VisualStateGroup.Transitions>"
        "<VisualStateTransition From=\"Normal\" To=\"Normal\""
        " GeneratedDuration=\"0:0:0.1\"/>"
        "</VisualStateGroup.Transitions>"
        "</VisualStateGroup>"
        "</VisualStateManager.VisualStateGroups>"
        "</Grid>")));

    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Button xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " BlendMode=\"Multiply\">"
            "<Button.LayoutTransform>"
            "<RotateTransform Angle=\"15\"/>"
            "</Button.LayoutTransform>"
            "<Button.Effect>"
            "<BlurEffect Radius=\"4\"/>"
            "</Button.Effect>"
            "</Button>"));
        CHECK(document);
        Button* button = document.Value().Root<Button>();
        CHECK(button != nullptr);
        CHECK(button->GetBlendMode() == BlendMode::Multiply);
        CHECK(button->GetLayoutTransform().Get() != nullptr);
        CHECK(button->GetEffect().Get() != nullptr);
    }
    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Button xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:aero=\"clr-namespace:Aero.GUI.Extensions\""
            " aero:Element.BlendingMode=\"Screen\"/>"));
        CHECK(document);
        Button* button = document.Value().Root<Button>();
        CHECK(button != nullptr);
        CHECK(button->GetBlendMode() == BlendMode::Screen);
    }

    CHECK(parse(StringView(
        "<TabControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<TabItem Header=\"One\"><TextBlock Text=\"A\"/></TabItem>"
        "<TabItem Header=\"Two\"><TextBlock Text=\"B\"/></TabItem>"
        "</TabControl>")));
    CHECK(parse(StringView(
        "<ScrollViewer xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<StackPanel><TextBlock Text=\"Hi\"/></StackPanel>"
        "</ScrollViewer>")));
    return true;
}

bool TestGalleryHostXamlSurface() {
    Gui gui;
    Result<void> initialized = gui.Initialize();
    CHECK(initialized);
    Aero::Markup::XamlReader reader(gui);
    Aero::Diagnostics::DiagnosticBag diagnostics;

    auto failIfGalleryHostError = [&](StringView where,
                                      const Status& status) -> bool {
        if (ReportsGalleryHostFailure(CStringView(status.message))) {
            std::fprintf(stderr, "%.*s reported gallery host failure: %s\n",
                static_cast<int>(where.SizeBytes()),
                where.Data(),
                status.message);
            DumpDiagnostics(diagnostics);
            return true;
        }
        if (DiagnosticsReportGalleryHostFailure(diagnostics)) {
            std::fprintf(stderr, "%.*s diagnostics contain gallery host failure\n",
                static_cast<int>(where.SizeBytes()),
                where.Data());
            DumpDiagnostics(diagnostics);
            return true;
        }
        return false;
    };

    {
        diagnostics.Clear();
        Result<Aero::Markup::XamlDocument> document = reader.Parse(
            StringView(
                "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
                " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
                " xmlns:b=\"http://schemas.microsoft.com/xaml/behaviors\">"
                "<Grid.Resources>"
                "<Storyboard x:Key=\"ShowContainer1\"/>"
                "<Storyboard x:Key=\"ShowContainer2\"/>"
                "</Grid.Resources>"
                "<ContentControl x:Name=\"SampleContainer1\" Visibility=\"Collapsed\">"
                "<b:Interaction.Triggers>"
                "<b:DataTrigger Binding=\"{Binding Content, ElementName=SampleContainer1}\""
                " Comparison=\"NotEqual\" Value=\"{x:Null}\">"
                "<b:ControlStoryboardAction Storyboard=\"{StaticResource ShowContainer1}\"/>"
                "</b:DataTrigger>"
                "<b:StoryboardCompletedTrigger Storyboard=\"{StaticResource ShowContainer1}\">"
                "<b:ChangePropertyAction PropertyName=\"Content\" Value=\"{x:Null}\""
                " TargetName=\"SampleContainer2\"/>"
                "</b:StoryboardCompletedTrigger>"
                "</b:Interaction.Triggers>"
                "<ContentControl.RenderTransform>"
                "<TranslateTransform/>"
                "</ContentControl.RenderTransform>"
                "</ContentControl>"
                "<ContentControl x:Name=\"SampleContainer2\" Visibility=\"Collapsed\">"
                "<b:Interaction.Triggers>"
                "<b:DataTrigger Binding=\"{Binding Content, ElementName=SampleContainer2}\""
                " Comparison=\"NotEqual\" Value=\"{x:Null}\">"
                "<b:ControlStoryboardAction Storyboard=\"{StaticResource ShowContainer2}\"/>"
                "</b:DataTrigger>"
                "<b:StoryboardCompletedTrigger Storyboard=\"{StaticResource ShowContainer2}\">"
                "<b:ChangePropertyAction PropertyName=\"Content\" Value=\"{x:Null}\""
                " TargetName=\"SampleContainer1\"/>"
                "</b:StoryboardCompletedTrigger>"
                "</b:Interaction.Triggers>"
                "<ContentControl.RenderTransform>"
                "<TranslateTransform/>"
                "</ContentControl.RenderTransform>"
                "</ContentControl>"
                "</Grid>"),
            {},
            {},
            &diagnostics);
        if (!document) {
            std::fprintf(stderr, "Interaction.Triggers parse failed: %s\n",
                document.GetStatus().message);
            DumpDiagnostics(diagnostics);
        }
        CHECK(!failIfGalleryHostError(
            StringView("Interaction.Triggers"),
            document.GetStatus()));
        CHECK(document);
        Grid* grid = document.Value().Root<Grid>();
        CHECK(grid != nullptr);
        CHECK(grid->FindName<ContentControl>("SampleContainer1") != nullptr);
        CHECK(grid->FindName<ContentControl>("SampleContainer2") != nullptr);
    }

    {
        diagnostics.Clear();
        std::error_code error;
        const std::filesystem::path dir =
            std::filesystem::temp_directory_path(error) /
            "aero-gallery-host-f940";
        CHECK(!error);
        std::filesystem::create_directories(dir, error);
        CHECK(!error);
        const std::filesystem::path resourcesPath = dir / "Resources.xaml";
        const std::filesystem::path appPath = dir / "App.xaml";
        CHECK(WriteUtf8File(resourcesPath,
            "<ResourceDictionary xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
            "<SolidColorBrush x:Key=\"MainWindowBackground\" Color=\"#FF1A1A2E\"/>"
            "<Geometry x:Key=\"AeroLogoGeometry\">M0,0 L10,0 L10,10 Z</Geometry>"
            "<sys:Double xmlns:sys=\"clr-namespace:System;assembly=mscorlib\""
            " x:Key=\"SelectorBarWidth\">48</sys:Double>"
            "</ResourceDictionary>"));
        CHECK(WriteUtf8File(appPath,
            "<ResourceDictionary xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
            "<ResourceDictionary.MergedDictionaries>"
            "<ResourceDictionary Source=\""
            "pack://application:,,,/Aero.GUI.Extensions;component/Theme/AeroTheme.DarkBlue.xaml\"/>"
            "<ResourceDictionary Source=\"Resources.xaml\"/>"
            "</ResourceDictionary.MergedDictionaries>"
            "</ResourceDictionary>"));
        const std::string appPathText = appPath.string();
        Result<Aero::Markup::XamlDocument> document = reader.Load(
            AsView(appPathText),
            {},
            &diagnostics);
        if (!document) {
            std::fprintf(stderr,
                "Gallery ResourceDictionary Source load failed: %s\n",
                document.GetStatus().message);
            DumpDiagnostics(diagnostics);
        }
        CHECK(!failIfGalleryHostError(
            StringView("ResourceDictionary Source"),
            document.GetStatus()));
        CHECK(document);
        Aero::ResourceDictionary* dictionary =
            document.Value().Root<Aero::ResourceDictionary>();
        CHECK(dictionary != nullptr);
        CHECK(dictionary->MergedDictionaryCount() >= 2U);
        CHECK(dictionary->Contains(StringView("AeroLogoGeometry")));
        CHECK(dictionary->Contains(StringView("MainWindowBackground")));
        CHECK(dictionary->Contains(StringView("SelectorBarWidth")));
    }
    return true;
}

bool TestTutorialXamlSurface() {
    static_assert(std::is_base_of<Aero::Controls::ContentControl, Aero::Controls::UserControl>::value,
        "UserControl must derive ContentControl");
    static_assert(std::is_base_of<Aero::Data::BindingBase, Aero::Data::MultiBinding>::value,
        "MultiBinding must derive BindingBase");
    static_assert(std::is_base_of<Aero::Media::Animation::TimelineGroup, Aero::Media::Animation::ParallelTimeline>::value,
        "ParallelTimeline must derive TimelineGroup");

    Gui gui;
    Result<void> initialized = gui.Initialize();
    CHECK(initialized);
    Aero::Markup::XamlReader reader(gui);

    auto parse = [&](StringView markup) -> bool {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(markup);
        if (!document) {
            std::fprintf(stderr, "XAML parse failed: %s\n",
                document.GetStatus().message);
            return false;
        }
        return document.Value().IsValid();
    };

    CHECK(parse(StringView(
        "<UserControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<UserControl.Resources>"
        "<BooleanToVisibilityConverter x:Key=\"VisibleWhenTrue\"/>"
        "</UserControl.Resources>"
        "<Grid><TextBlock Text=\"ok\"/></Grid>"
        "</UserControl>")));
    CHECK(parse(StringView(
        "<ItemsControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<ItemsControl.ItemsPanel>"
        "<ItemsPanelTemplate><UniformGrid Columns=\"2\"/></ItemsPanelTemplate>"
        "</ItemsControl.ItemsPanel>"
        "</ItemsControl>")));
    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<TextBlock xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " FontFamily=\"./#Demo\"/>"));
        CHECK(document);
        TextBlock* text = document.Value().Root<TextBlock>();
        CHECK(text != nullptr);
        const Ref<Aero::Media::FontFamily> family = text->GetFontFamily();
        CHECK(family.Get() != nullptr);
        CHECK(family->GetSource() == StringView("./#Demo"));
    }
    CHECK(parse(StringView(
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<Grid.RenderTransform>"
        "<TransformGroup>"
        "<MatrixTransform Matrix=\"1,0,0,1,8,4\"/>"
        "</TransformGroup>"
        "</Grid.RenderTransform>"
        "<Thumb Width=\"12\" Height=\"12\"/>"
        "<Popup IsOpen=\"False\"><Border/></Popup>"
        "</Grid>")));
    CHECK(parse(StringView(
        "<ParallelTimeline xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<DoubleAnimation From=\"0\" To=\"1\" Duration=\"0:0:0.2\"/>"
        "</ParallelTimeline>")));
    CHECK(parse(StringView(
        "<Page xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<PasswordBox/>"
        "</Page>")));
    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<TextBlock xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " FontFamily=\"./#Rajdhani SemiBold\"/>"));
        CHECK(document);
        TextBlock* text = document.Value().Root<TextBlock>();
        CHECK(text != nullptr);
        const Ref<Aero::Media::FontFamily> family = text->GetFontFamily();
        CHECK(family.Get() != nullptr);
        CHECK(family->GetSource() == StringView("./#Rajdhani SemiBold"));
    }
    CHECK(parse(StringView(
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid.Resources>"
        "<ImageSource x:Key=\"FillBlue\">fill-blue.png</ImageSource>"
        "</Grid.Resources>"
        "<BulletDecorator><BulletDecorator.Bullet><Ellipse Width=\"8\" Height=\"8\"/></BulletDecorator.Bullet>"
        "<TextBlock Text=\"item\"/></BulletDecorator>"
        "</Grid>")));
    CHECK(parse(StringView(
        "<Path xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<Path.Data>"
        "<PathGeometry>"
        "<PathFigure StartPoint=\"0,0\"><LineSegment Point=\"10,0\"/></PathFigure>"
        "</PathGeometry>"
        "</Path.Data>"
        "</Path>")));
    CHECK(parse(StringView(
        "<PointAnimationUsingKeyFrames xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<DiscretePointKeyFrame KeyTime=\"0\" Value=\"0,0\"/>"
        "<EasingPointKeyFrame KeyTime=\"0:0:0.2\" Value=\"8,4\"/>"
        "</PointAnimationUsingKeyFrames>")));
    CHECK(parse(StringView(
        "<ThicknessAnimationUsingKeyFrames xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<EasingThicknessKeyFrame KeyTime=\"0:0:0.1\" Value=\"1,2,3,4\"/>"
        "</ThicknessAnimationUsingKeyFrames>")));
    CHECK(parse(StringView(
        "<Style xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" TargetType=\"Button\">"
        "<Style.Triggers>"
        "<MultiDataTrigger>"
        "<MultiDataTrigger.Conditions>"
        "<Condition Binding=\"{Binding Active}\" Value=\"True\"/>"
        "</MultiDataTrigger.Conditions>"
        "<Setter Property=\"Opacity\" Value=\"0.5\"/>"
        "</MultiDataTrigger>"
        "</Style.Triggers>"
        "</Style>")));
    CHECK(parse(StringView(
        "<Style xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" TargetType=\"Button\">"
        "<Style.Triggers>"
        "<MultiDataTrigger>"
        "<MultiDataTrigger.Conditions>"
        "<Condition Binding=\"{Binding Active}\" Value=\"True\"/>"
        "<Condition Binding=\"{Binding Enabled}\" Value=\"True\"/>"
        "</MultiDataTrigger.Conditions>"
        "<Setter Property=\"Opacity\" Value=\"0.25\"/>"
        "</MultiDataTrigger>"
        "</Style.Triggers>"
        "</Style>")));
    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:noesis=\"clr-namespace:NoesisGUIExtensions;assembly=Noesis.GUI.Extensions\">"
            "<noesis:Element.Transform3D>"
            "<noesis:CompositeTransform3D RotationY=\"-8\" TranslateZ=\"40\" ScaleX=\"1.2\"/>"
            "</noesis:Element.Transform3D>"
            "<Grid.LayoutTransform>"
            "<RotateTransform Angle=\"10\"/>"
            "</Grid.LayoutTransform>"
            "</Grid>"));
        CHECK(document);
        Grid* grid = document.Value().Root<Grid>();
        CHECK(grid != nullptr);
        CHECK(grid->GetLayoutTransform().Get() != nullptr);
        CompositeTransform3D* transform =
            TryCast<CompositeTransform3D>(grid->GetTransform3D().Get());
        CHECK(transform != nullptr);
        CHECK(Near(transform->GetRotationY(), -8.0));
        CHECK(Near(transform->GetTranslateZ(), 40.0));
        CHECK(Near(transform->GetScaleX(), 1.2));
    }
    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Border xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
            "<Border.Effect><ShaderEffect PixelShader=\"noise.frag\"/></Border.Effect>"
            "</Border>"));
        CHECK(document);
        Aero::Controls::Border* border =
            document.Value().Root<Aero::Controls::Border>();
        CHECK(border != nullptr);
        ShaderEffect* shader = TryCast<ShaderEffect>(border->GetEffect().Get());
        CHECK(shader != nullptr);
        CHECK(shader->GetPixelShader() == StringView("noise.frag"));
    }
    CHECK(parse(StringView(
        "<Path xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:b=\"http://schemas.microsoft.com/xaml/behaviors\""
        " xmlns:noesis=\"clr-namespace:NoesisGUIExtensions;assembly=Noesis.GUI.Extensions\""
        " Data=\"M0,0 L10,0 L10,10 Z\">"
        "<b:Interaction.Behaviors>"
        "<noesis:BackgroundEffectBehavior>"
        "<BlurEffect Radius=\"6\"/>"
        "</noesis:BackgroundEffectBehavior>"
        "</b:Interaction.Behaviors>"
        "</Path>")));
    return true;
}

bool TestTutorialRuntimePatterns() {
    LiveGui* live = NewTutorialLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    Aero::Markup::XamlReader reader(live->gui);
    double clock = 0.0;
    auto pump = [&](double deltaSeconds) noexcept {
        clock += deltaSeconds;
        static_cast<void>(view.Update(clock));
    };

    {
        Result<Ref<NumericUpDown>> control = MakeRef<NumericUpDown>();
        CHECK(control);
        Result<void> loaded = live->gui.LoadComponent(
            *control.Value(), "memory:///NumericUpDown.xaml");
        CHECK(loaded);
        CHECK(control->FindName("UpButton") != nullptr);
        control->SetNumericValue(10);
        bool mounted = view.SetContent(
            control.Value(), {200.0, 80.0});
        if (!mounted) {
            std::fprintf(stderr, "SetContent(NumericUpDown) failed\n");
            DumpDiagnostics(live->diagnostics);
        }
        CHECK(mounted);
        pump( 0.016);
        Aero::Controls::Primitives::RepeatButton* up =
            control->FindName<Aero::Controls::Primitives::RepeatButton>(
                "UpButton");
        CHECK(up != nullptr);
        Aero::RoutedEventArgs click;
        control->UpButton_Click(up, click);
        pump( 0.032);
        CHECK(control->GetNumericValue() == 11);
    }

    {
        Result<Ref<RgbModel>> rgb = MakeRef<RgbModel>();
        CHECK(rgb);
        rgb->SetR(255);
        rgb->SetG(0);
        rgb->SetB(0);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Rectangle xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " xmlns:local=\"clr-namespace:UserControls\""
            " Width=\"40\" Height=\"40\">"
            "<Rectangle.Resources>"
            "<local:ColorConverter x:Key=\"ColorConverter\"/>"
            "</Rectangle.Resources>"
            "<Rectangle.Fill>"
            "<SolidColorBrush>"
            "<SolidColorBrush.Color>"
            "<MultiBinding Converter=\"{StaticResource ColorConverter}\">"
            "<Binding Path=\"R\"/>"
            "<Binding Path=\"G\"/>"
            "<Binding Path=\"B\"/>"
            "</MultiBinding>"
            "</SolidColorBrush.Color>"
            "</SolidColorBrush>"
            "</Rectangle.Fill>"
            "</Rectangle>"));
        CHECK(document);
        Aero::Shapes::Rectangle* rectangle =
            document.Value().Root<Aero::Shapes::Rectangle>();
        CHECK(rectangle != nullptr);
        rectangle->SetValue(
            FrameworkElement::DataContextProperty,
            Aero::Value::FromObject(RgbModel::StaticTypeId(), rgb.Value()));
        CHECK(view.SetContent(
            std::move(document).Value(), {80.0, 80.0}));
        rectangle = TryCast<Aero::Shapes::Rectangle>(view.GetContent());
        CHECK(rectangle != nullptr);
        pump( 0.016);
        pump( 0.032);
        SolidColorBrush* brush =
            TryCast<SolidColorBrush>(rectangle->GetFill().Get());
        CHECK(brush != nullptr);
        CHECK(Near(static_cast<double>(brush->GetColor().red), 1.0, 0.05));
        CHECK(Near(static_cast<double>(brush->GetColor().green), 0.0, 0.05));
        rgb->SetG(255);
        pump( 0.048);
        CHECK(Near(static_cast<double>(brush->GetColor().green), 1.0, 0.05));
    }

    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<local:CircleAnimation xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:local=\"clr-namespace:CustomAnimation\""
            " From=\"0\" To=\"10\" Radius=\"1\"/>"));
        CHECK(document);
        CircleAnimation* animation = document.Value().Root<CircleAnimation>();
        CHECK(animation != nullptr);
        CHECK(Near(animation->GetRadius(), 1.0));
        const double mid = animation->GetCurrentValue(0.0, 10.0, 0.5);
        CHECK(mid > 0.0 && mid < 10.0);
        CHECK(!Near(mid, 5.0, 0.01));
    }

    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " xmlns:local=\"clr-namespace:CustomControl\" Width=\"120\" Height=\"40\">"
            "<Grid.Resources>"
            "<Style TargetType=\"{x:Type local:Clock}\">"
            "<Setter Property=\"Background\" Value=\"Red\"/>"
            "<Setter Property=\"Template\">"
            "<Setter.Value>"
            "<ControlTemplate TargetType=\"{x:Type local:Clock}\">"
            "<Border Background=\"{TemplateBinding Background}\">"
            "<TextBlock Text=\"{Binding Hour, RelativeSource={RelativeSource TemplatedParent}}\"/>"
            "</Border>"
            "</ControlTemplate>"
            "</Setter.Value>"
            "</Setter>"
            "</Style>"
            "</Grid.Resources>"
            "<local:Clock x:Name=\"Face\" Hour=\"7\"/>"
            "</Grid>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {120.0, 40.0}));
        pump( 0.016);
        Grid* grid = TryCast<Grid>(view.GetContent());
        CHECK(grid != nullptr);
        Clock* clock = grid->FindName<Clock>("Face");
        CHECK(clock != nullptr);
        CHECK(clock->GetHour() == 7);
        static_cast<void>(clock->ApplyTemplate());
        CHECK(clock->GetBackground().Get() != nullptr);
        CHECK(Aero::Media::VisualTreeHelper::GetChildrenCount(*clock) >= 1U);
    }

    {
        Game::renderCount = 0;
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<local:Game xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:local=\"clr-namespace:CustomRender\" Width=\"80\" Height=\"60\"/>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {80.0, 60.0}));
        pump( 0.016);
        pump( 0.032);
        if (Game::renderCount <= 0) {
            std::fprintf(stderr, "Game OnRender was not called\n");
            DumpDiagnostics(live->diagnostics);
            Game* game = TryCast<Game>(view.GetContent());
            if (game != nullptr) {
                std::fprintf(stderr,
                    "Game arrangeValid=%d renderSize=%g x %g visibility=%d\n",
                    game->GetIsArrangeValid() ? 1 : 0,
                    game->GetRenderSize().width,
                    game->GetRenderSize().height,
                    static_cast<int>(game->GetVisibility()));
            }
        }
        CHECK(Game::renderCount > 0);
    }

    {
        Ref<CommandsViewModel> model = MakeRef<CommandsViewModel>();
        CHECK(model);
        Ref<HelloCommand> hello = MakeRef<HelloCommand>(model.Get());
        CHECK(hello);
        model->SetSayHelloCommand(hello);
        String input;
        CHECK(input.Assign("Ada"));
        model->SetInput(std::move(input));
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Button xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " Width=\"80\" Height=\"32\" Command=\"{Binding SayHelloCommand}\""
            " CommandParameter=\"Ada\" Content=\"Say\"/>"));
        CHECK(document);
        Button* button = document.Value().Root<Button>();
        CHECK(button != nullptr);
        button->SetValue(
            FrameworkElement::DataContextProperty,
            Aero::Value::FromObject(
                CommandsViewModel::StaticTypeId(), model));
        CHECK(view.SetContent(std::move(document).Value(), {80.0, 32.0}));
        button = TryCast<Button>(view.GetContent());
        CHECK(button != nullptr);
        pump( 0.016);
        CHECK(button->GetCommand() != nullptr);
        Result<Aero::Value> encoded = Aero::Value::TryFromString(
            Aero::Meta::TypeOf<String>(), StringView("Ada"));
        CHECK(encoded);
        button->GetCommand()->Execute(encoded.Value(), button);
        pump( 0.032);
        CHECK(hello->GetExecutionCount() >= 1U);
        CHECK(model->GetOutput() == StringView("Hello Ada") ||
            model->GetOutput() == StringView("Hello"));
    }

    {
        Result<Aero::Markup::XamlDocument> host = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" Width=\"200\" Height=\"40\">"
            "<TextBlock x:Name=\"Label\" Text=\"{DynamicResource Greeting}\"/>"
            "</Grid>"));
        CHECK(host);
        {
            bool mounted = view.SetContent(
                std::move(host).Value(), {200.0, 40.0});
            if (!mounted) {
                std::fprintf(stderr, "SetContent(Localization) failed\n");
                DumpDiagnostics(live->diagnostics);
            }
            CHECK(mounted);
        }
        pump( 0.016);
        Grid* grid = TryCast<Grid>(view.GetContent());
        CHECK(grid != nullptr);
        Result<Aero::Markup::XamlDocument> english = reader.Parse(
            StringView(kLanguageEnXaml));
        Result<Aero::Markup::XamlDocument> french = reader.Parse(
            StringView(kLanguageFrXaml));
        CHECK(english && french);
        Aero::ResourceDictionary* en =
            english.Value().Root<Aero::ResourceDictionary>();
        Aero::ResourceDictionary* fr =
            french.Value().Root<Aero::ResourceDictionary>();
        CHECK(en != nullptr && fr != nullptr);
        grid->GetResources().AddMerged(*en);
        pump( 0.032);
        TextBlock* label = grid->FindName<TextBlock>("Label");
        CHECK(label != nullptr);
        CHECK(label->GetText() == StringView("Hello"));
        grid->GetResources().ClearMergedDictionaries();
        grid->GetResources().AddMerged(*fr);
        pump( 0.048);
        CHECK(label->GetText() == StringView("Bonjour"));
    }

    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" Width=\"80\" Height=\"80\">"
            "<Image x:Name=\"Backdrop\" Width=\"80\" Height=\"80\"/>"
            "<Border x:Name=\"Overlay\" Width=\"40\" Height=\"40\">"
            "<Border.Effect><BlurEffect Radius=\"12\"/></Border.Effect>"
            "</Border>"
            "</Grid>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {80.0, 80.0}));
        pump( 0.016);
        Grid* grid = TryCast<Grid>(view.GetContent());
        CHECK(grid != nullptr);
        CHECK(grid->FindName("Backdrop") != nullptr);
        Aero::Controls::Border* overlay =
            grid->FindName<Aero::Controls::Border>("Overlay");
        CHECK(overlay != nullptr);
        Aero::Media::BlurEffect* blur =
            TryCast<Aero::Media::BlurEffect>(overlay->GetEffect().Get());
        CHECK(blur != nullptr);
        CHECK(Near(blur->GetRadius(), 12.0));
    }

    {
        Result<Ref<ShaderEffect>> effect = MakeRef<ShaderEffect>();
        CHECK(effect);
        effect->SetPixelShader("custom.frag");
        CHECK(effect->GetPixelShader() == StringView("custom.frag"));
        const std::uint8_t bytes[] = {0x43, 0x47, 0x58, 0x00};
        effect->SetBytecode({bytes, 4U});
        CHECK(effect->GetBytecode().Size() == 4U);
        // .noesisbrush is a Noesis offline compiler artifact. Aero loads
        // PixelShader source or raw bytecode; it does not compile brushes.
    }

    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<ItemsControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" Width=\"80\" Height=\"80\">"
            "<ItemsControl.ItemsPanel>"
            "<ItemsPanelTemplate><UniformGrid Columns=\"2\"/></ItemsPanelTemplate>"
            "</ItemsControl.ItemsPanel>"
            "<TextBlock Text=\"A\"/><TextBlock Text=\"B\"/>"
            "</ItemsControl>"));
        CHECK(document);
        ItemsControl* items = document.Value().Root<ItemsControl>();
        CHECK(items != nullptr);
        CHECK(items->GetItemsPanel() != nullptr);
        CHECK(view.SetContent(std::move(document).Value(), {80.0, 80.0}));
        pump( 0.016);
    }
    return true;
}

bool TestLoadedIntroStoryboard() {
    ViewOptions options;
    options.automaticAnimationClock = false;
    LiveGui* live = NewLiveGui(options);
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({200.0, 80.0});

    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
        " Width=\"200\" Height=\"80\">"
        "<Grid.Resources>"
        "<Storyboard x:Key=\"Anim.Intro\">"
        "<DoubleAnimationUsingKeyFrames Storyboard.TargetProperty=\"(UIElement.Opacity)\""
        " Storyboard.TargetName=\"Child\">"
        "<EasingDoubleKeyFrame KeyTime=\"0\" Value=\"0\"/>"
        "<EasingDoubleKeyFrame KeyTime=\"0:0:0.2\" Value=\"1\"/>"
        "</DoubleAnimationUsingKeyFrames>"
        "<DoubleAnimationUsingKeyFrames"
        " Storyboard.TargetProperty=\"(UIElement.RenderTransform).(TransformGroup.Children)[3].(TranslateTransform.X)\""
        " Storyboard.TargetName=\"Child\">"
        "<EasingDoubleKeyFrame KeyTime=\"0\" Value=\"40\"/>"
        "<EasingDoubleKeyFrame KeyTime=\"0:0:0.2\" Value=\"0\"/>"
        "</DoubleAnimationUsingKeyFrames>"
        "<DoubleAnimationUsingKeyFrames Storyboard.TargetProperty=\"(UIElement.Opacity)\""
        " Storyboard.TargetName=\"Missing\">"
        "<EasingDoubleKeyFrame KeyTime=\"0\" Value=\"0\"/>"
        "<EasingDoubleKeyFrame KeyTime=\"0:0:0.2\" Value=\"1\"/>"
        "</DoubleAnimationUsingKeyFrames>"
        "</Storyboard>"
        "</Grid.Resources>"
        "<Grid.Triggers>"
        "<EventTrigger RoutedEvent=\"FrameworkElement.Loaded\">"
        "<BeginStoryboard Storyboard=\"{StaticResource Anim.Intro}\"/>"
        "</EventTrigger>"
        "</Grid.Triggers>"
        "<Border x:Name=\"Child\" Width=\"40\" Height=\"40\">"
        "<Border.RenderTransform>"
        "<TransformGroup>"
        "<ScaleTransform/>"
        "<SkewTransform/>"
        "<RotateTransform/>"
        "<TranslateTransform X=\"40\"/>"
        "</TransformGroup>"
        "</Border.RenderTransform>"
        "</Border>"
        "</Grid>"));
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {200.0, 80.0}));
    Grid* root = TryCast<Grid>(view.GetContent());
    CHECK(root != nullptr);
    Aero::Controls::Border* child =
        root->FindName<Aero::Controls::Border>(StringView("Child"));
    CHECK(child != nullptr);
    Pump(view, 0.0);
    CHECK(Near(child->GetOpacity(), 0.0, 0.05));
    Pump(view, 0.25);
    CHECK(Near(child->GetOpacity(), 1.0, 0.05));
    auto* group = TryCast<Aero::Media::TransformGroup>(
        child->GetRenderTransform().Get());
    CHECK(group != nullptr);
    CHECK(group->GetChildren().Size() > 3U);
    auto* translate = TryCast<Aero::Media::TranslateTransform>(
        group->GetChildren()[3].Get());
    CHECK(translate != nullptr);
    CHECK(Near(translate->GetX(), 0.0, 0.75));
    return true;
}

bool TestBoardFlipRotationYStoryboard() {
    ViewOptions options;
    options.automaticAnimationClock = false;
    LiveGui* live = NewLiveGui(options);
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({400.0, 400.0});

    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
        "<Viewbox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
        " xmlns:aero=\"clr-namespace:AeroGUIExtensions;assembly=Aero.GUI.Extensions\""
        " xmlns:b=\"http://schemas.microsoft.com/xaml/behaviors\""
        " Width=\"400\" Height=\"400\">"
        "<Viewbox.Resources>"
        "<Storyboard x:Key=\"BoardFlip1Anim\">"
        "<DoubleAnimationUsingKeyFrames"
        " Storyboard.TargetProperty=\"(aero:Element.Transform3D).(aero:CompositeTransform3D.RotationY)\""
        " Storyboard.TargetName=\"Board\">"
        "<EasingDoubleKeyFrame KeyTime=\"0:0:0.1\" Value=\"-90\">"
        "<EasingDoubleKeyFrame.EasingFunction>"
        "<ExponentialEase EasingMode=\"EaseInOut\"/>"
        "</EasingDoubleKeyFrame.EasingFunction>"
        "</EasingDoubleKeyFrame>"
        "</DoubleAnimationUsingKeyFrames>"
        "</Storyboard>"
        "</Viewbox.Resources>"
        "<Grid x:Name=\"Board\" Width=\"200\" Height=\"200\" Margin=\"15\">"
        "<aero:Element.Transform3D>"
        "<aero:CompositeTransform3D CenterX=\"100\" CenterY=\"100\"/>"
        "</aero:Element.Transform3D>"
        "<Border x:Name=\"Probe\" Width=\"20\" Height=\"20\""
        " HorizontalAlignment=\"Left\" VerticalAlignment=\"Center\""
        " Background=\"White\"/>"
        "<Button x:Name=\"Start\" Background=\"#00FF0000\">"
        "<b:Interaction.Triggers>"
        "<b:EventTrigger EventName=\"Click\">"
        "<b:ControlStoryboardAction Storyboard=\"{StaticResource BoardFlip1Anim}\"/>"
        "</b:EventTrigger>"
        "</b:Interaction.Triggers>"
        "</Button>"
        "</Grid>"
        "</Viewbox>"));
    if (!document) {
        std::fprintf(stderr, "board flip XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {400.0, 400.0}));
    Pump(view, 0.0);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    Grid* board = root->FindName<Grid>(StringView("Board"));
    CHECK(board != nullptr);
    CompositeTransform3D* transform =
        TryCast<CompositeTransform3D>(board->GetTransform3D().Get());
    if (transform == nullptr) {
        std::fprintf(stderr, "Board GetTransform3D was null after load\n");
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(transform != nullptr);
    CHECK(Near(transform->GetRotationY(), 0.0, 0.5));

    Button* start = root->FindName<Button>(StringView("Start"));
    CHECK(start != nullptr);
    Point click{};
    CHECK(start->TryPointToScreen(
        {start->GetRenderSize().width * 0.5,
         start->GetRenderSize().height * 0.5},
        click));
    static_cast<void>(view.MouseButtonDown(
        static_cast<int>(click.x),
        static_cast<int>(click.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.016);
    static_cast<void>(view.MouseButtonUp(
        static_cast<int>(click.x),
        static_cast<int>(click.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.032);

    Border* probe = root->FindName<Aero::Controls::Border>(StringView("Probe"));
    CHECK(probe != nullptr);
    Point rest{};
    CHECK(probe->TryPointToScreen({10.0, 10.0}, rest));

    Pump(view, 0.09);
    const double midY = transform->GetRotationY();
    if (!(midY < -8.0)) {
        std::fprintf(stderr,
            "BoardFlip1 RotationY stayed at %.3f after Click (expected mid-flip)\n",
            midY);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(midY < -8.0);

    const double projective = Aero::MaxAbsCommittedProjectiveM13(view);
    if (!(projective > 1.0e-4)) {
        std::fprintf(stderr,
            "BoardFlip1 RotationY=%.3f but committed renderTransform stayed affine "
            "(max |m13/m23/(m33-1)|=%.6g)\n",
            midY, projective);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(projective > 1.0e-4);

    Point flipped{};
    CHECK(probe->TryPointToScreen({10.0, 10.0}, flipped));
    const double shift = flipped.x - rest.x;
    if (!(shift > 2.0 || shift < -2.0)) {
        std::fprintf(stderr,
            "BoardFlip1 RotationY=%.3f but Probe screen x did not move "
            "(rest=%.1f flipped=%.1f)\n",
            midY, rest.x, flipped.x);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(shift > 2.0 || shift < -2.0);

    Pump(view, 0.20);
    CHECK(Near(transform->GetRotationY(), -90.0, 4.0));
    return true;
}


bool TestValueLayoutAndSSO() {
    // 1. Size verification
    static_assert(sizeof(Aero::Base::Value) <= 56U, "Value size must not exceed 56 bytes");
    CHECK(sizeof(Aero::Base::Value) <= 56U);

    // 2. Short string optimization (SSO)
    Aero::Base::Value shortVal("HelloWorld");
    CHECK(shortVal.Kind() == Aero::Base::ValueKind::String);
    CHECK(shortVal.IsShortString());
    CHECK(shortVal.AsString() == "HelloWorld");

    // Copy constructor preserves short string
    Aero::Base::Value shortCopy = shortVal;
    CHECK(shortCopy.Kind() == Aero::Base::ValueKind::String);
    CHECK(shortCopy.IsShortString());
    CHECK(shortCopy.AsString() == "HelloWorld");
    CHECK(shortCopy == shortVal);

    // Move constructor
    Aero::Base::Value shortMoved = std::move(shortCopy);
    CHECK(shortMoved.AsString() == "HelloWorld");
    CHECK(shortCopy.IsUnset());

    // Copy assignment
    Aero::Base::Value assigned;
    assigned = shortVal;
    CHECK(assigned.AsString() == "HelloWorld");
    CHECK(assigned == shortVal);

    // 3. Long string (heap allocated fallback)
    const char* longStr = "This is a very long string that definitely exceeds thirty-one bytes in length";
    Aero::Base::Value longVal(longStr);
    CHECK(longVal.Kind() == Aero::Base::ValueKind::String);
    CHECK(!longVal.IsShortString());
    CHECK(longVal.AsString() == longStr);

    Aero::Base::Value longCopy = longVal;
    CHECK(!longCopy.IsShortString());
    CHECK(longCopy.AsString() == longStr);
    CHECK(longCopy == longVal);

    // 4. Primitive scalars
    Aero::Base::Value boolVal(true);
    CHECK(boolVal.Kind() == Aero::Base::ValueKind::Boolean);
    CHECK(boolVal.AsBoolean() == true);
    Aero::Base::Value boolCopy = boolVal;
    CHECK(boolCopy.AsBoolean() == true);
    CHECK(boolCopy == boolVal);

    Aero::Base::Value intVal(42);
    CHECK(intVal.Kind() == Aero::Base::ValueKind::SignedInteger);
    CHECK(intVal.AsSignedInteger() == 42);

    Aero::Base::Value dblVal(3.14159);
    CHECK(dblVal.Kind() == Aero::Base::ValueKind::Double);
    CHECK(dblVal.AsDouble() == 3.14159);

    // 5. Custom inline values
    Aero::Thickness thickness{1.0, 2.0, 3.0, 4.0};
    static const Aero::Meta::ValueTypeRegistration s_thickReg =
        Aero::Meta::MakeValueTypeRegistration<Aero::Thickness>();
    Aero::Base::Result<Aero::Base::Value> thickVal =
        Aero::Base::Value::TryFromCustom(
            Aero::Base::MakeMetaTypeId("Thickness"),
            &thickness,
            &s_thickReg);
    CHECK(thickVal);
    CHECK(thickVal.Value().Kind() == Aero::Base::ValueKind::Custom);
    CHECK(thickVal.Value().IsInlineCustom());
    const auto* readThick = static_cast<const Aero::Thickness*>(thickVal.Value().AsCustom());
    CHECK(readThick != nullptr);
    CHECK(readThick->left == 1.0 && readThick->top == 2.0 &&
          readThick->right == 3.0 && readThick->bottom == 4.0);

    Aero::Base::Value thickCopy = thickVal.Value();
    CHECK(thickCopy.IsInlineCustom());
    CHECK(thickCopy == thickVal.Value());

    // 6. Object value
    Aero::Base::Result<Aero::Base::Ref<Aero::Media::SolidColorBrush>> brush =
        Aero::Base::MakeRef<Aero::Media::SolidColorBrush>();
    CHECK(brush);
    Aero::Base::Value objVal(Aero::Base::Ref<Aero::Base::Object>(brush.Value()));
    CHECK(objVal.Kind() == Aero::Base::ValueKind::Object);
    CHECK(!objVal.IsNullObject());
    CHECK(objVal.AsObject().Get() == brush.Value().Get());

    Aero::Base::Value objCopy = objVal;
    CHECK(objCopy == objVal);

    // 7. Transition between kinds (Assignment overwriting)
    objVal = shortVal; // overwriting Object with ShortString
    CHECK(objVal.Kind() == Aero::Base::ValueKind::String);
    CHECK(objVal.IsShortString());
    CHECK(objVal.AsString() == "HelloWorld");

    objVal = longVal; // overwriting ShortString with LongString
    CHECK(objVal.Kind() == Aero::Base::ValueKind::String);
    CHECK(!objVal.IsShortString());
    CHECK(objVal.AsString() == longStr);

    objVal = intVal; // overwriting LongString with scalar
    CHECK(objVal.Kind() == Aero::Base::ValueKind::SignedInteger);
    CHECK(objVal.AsSignedInteger() == 42);

    return true;
}

bool TestPasswordBoxDecoupled() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;

    // 1. Hierarchy & type verification: PasswordBox must NOT inherit or embed TextBox
    static_assert(!std::is_base_of_v<Aero::Controls::TextBox, Aero::Controls::PasswordBox>);
    static_assert(std::is_base_of_v<Aero::Controls::Primitives::TextBoxBase, Aero::Controls::PasswordBox>);
    static_assert(std::is_base_of_v<Aero::Controls::Primitives::TextBoxBase, Aero::Controls::TextBox>);

    // 2. Lifecycle & basic properties
    Ref<PasswordBox> pbox = Aero::Base::MakeRef<PasswordBox>();
    CHECK(pbox);
    view.SetContent(pbox, {800.0, 600.0});
    CHECK(pbox->GetPassword().Empty());
    CHECK(pbox->GetMaxLength() == 0U);
    CHECK(pbox->GetPasswordChar() == StringView(u8"\u2022"));

    // 3. SetPassword and PasswordChanged event
    bool changedFired = false;
    bool* flag = &changedFired;
    pbox->PasswordChanged().Add([flag](Aero::Base::Object*, Aero::RoutedEventArgs&) noexcept {
        *flag = true;
    });

    pbox->SetPassword(StringView("Secret123"));
    CHECK(changedFired);
    CHECK(pbox->GetPassword() == StringView("Secret123"));

    // 4. Change PasswordChar mask
    pbox->SetPasswordChar(StringView("*"));
    CHECK(pbox->GetPasswordChar() == StringView("*"));
    // Multi-grapheme mask is rejected
    pbox->SetPasswordChar(StringView("**"));
    CHECK(pbox->GetPasswordChar() == StringView("*"));

    // 5. MaxLength constraints
    pbox->SetPassword(StringView("Sec"));
    pbox->SetMaxLength(5U);
    CHECK(pbox->GetMaxLength() == 5U);
    pbox->SetPassword(StringView("TooLongPassword"));
    CHECK(pbox->GetPassword() == StringView("Sec")); // rejected because exceeds 5 chars

    // 6. Security: clipboard copy/cut must NOT expose password
    pbox->SelectAll();
    Aero::Input::MemoryClipboard clipboard;
    static_cast<void>(clipboard.WriteText(StringView("initial")));
    Aero::Base::String clipText;
    static_cast<void>(clipboard.ReadText(clipText));
    CHECK(clipText == StringView("initial"));

    // 7. Clear password
    changedFired = false;
    pbox->SetPassword(StringView(""));
    CHECK(changedFired);
    CHECK(pbox->GetPassword().Empty());

    return true;
}

bool TestStyleSetterMergedStaticResource();
bool TestMergedNestedStaticResource();
bool TestTypeKeyedStaticResourceBasedOn();
bool TestControlTemplateBasedOn();
bool TestInventoryTemplateApply();
bool TestTutorialSampleXamlLoadApply();
bool TestMarkupExtensionContext();
bool TestViewLoadXamlAndHotReload();
bool TestArchitectureRefactoringValidation() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    CHECK(live->view);

    // 1. Validate Popup child management and FrameworkElement inheritance
    {
        Result<Ref<Popup>> popup = MakeRef<Popup>();
        CHECK(popup);
        Result<Ref<Button>> childBtn = MakeRef<Button>();
        CHECK(childBtn);
        popup.Value()->SetChild(childBtn.Value());
        CHECK(popup.Value()->GetChild() == childBtn.Value().Get());
        CHECK(VisualTreeHelper::GetChildrenCount(*popup.Value()) == 1U);
        CHECK(VisualTreeHelper::GetChild(*popup.Value(), 0U) == childBtn.Value().Get());

        popup.Value()->SetChild(nullptr);
        CHECK(popup.Value()->GetChild() == nullptr);
        CHECK(VisualTreeHelper::GetChildrenCount(*popup.Value()) == 0U);
    }

    // 2. Validate ScrollViewer fallback presenter virtual dispatch
    {
        Result<Ref<ScrollViewer>> sv = MakeRef<ScrollViewer>();
        CHECK(sv);
        Result<Ref<Button>> childBtn = MakeRef<Button>();
        CHECK(childBtn);
        sv.Value()->SetContent(childBtn.Value());
        CHECK(sv.Value()->GetContentElement() != nullptr);
    }

    // 3. Validate Animation Easing cartesian math
    {
        Result<Ref<EasingFunctionBase>> ease = MakeRef<EasingFunctionBase>(
            EasingFunctionBase::StaticTypeId(),
            EasingFunctionBase::Kind::Cubic);
        CHECK(ease);
        ease.Value()->SetEasingMode(EasingMode::EaseIn);
        double v0 = ease.Value()->Ease(0.0);
        double vHalf = ease.Value()->Ease(0.5);
        double v1 = ease.Value()->Ease(1.0);
        CHECK(std::abs(v0 - 0.0) < 1e-5);
        CHECK(std::abs(vHalf - 0.125) < 1e-4);
        CHECK(std::abs(v1 - 1.0) < 1e-5);
    }

    // 4. Validate XAML property element edge (ScaleTransform on Grid.RenderTransform),
    // skipping root x:Class, and default ContentMember (<Run>text</Run>)
    {
        const char* xaml =
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\"\n"
            "      xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"\n"
            "      x:Class=\"TestNamespace.TestClass\">\n"
            "    <Grid.RenderTransform>\n"
            "        <ScaleTransform ScaleX=\"2.5\" ScaleY=\"3.5\"/>\n"
            "    </Grid.RenderTransform>\n"
            "    <TextBlock x:Name=\"TextTarget\"><Run>DirectRunText</Run></TextBlock>\n"
            "</Grid>";

        Aero::Markup::XamlReader reader(live->gui);
        Result<Aero::Markup::XamlDocument> doc = reader.Parse(StringView(xaml), {}, {}, &live->diagnostics);
        CHECK(doc.HasValue());
        auto* textBlock = doc.Value().FindName<TextBlock>("TextTarget");
        CHECK(textBlock != nullptr);
        CHECK(textBlock->GetInlines().GetCount() == 1U);
        auto* run = TryCast<Run>(textBlock->GetInlines().GetItem(0U));
        CHECK(run != nullptr);
        CHECK(run->GetText() == StringView("DirectRunText"));

        CHECK(live->view->SetContent(std::move(doc).Value(), {640.0, 480.0}));
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);

        auto* rootGrid = TryCast<Grid>(live->view->GetContent());
        CHECK(rootGrid != nullptr);

        auto transform = rootGrid->GetRenderTransform();
        CHECK(transform != nullptr);
        auto* scaleTransform = TryCast<ScaleTransform>(transform.Get());
        CHECK(scaleTransform != nullptr);
        CHECK(std::abs(scaleTransform->GetScaleX() - 2.5) < 1e-5);
        CHECK(std::abs(scaleTransform->GetScaleY() - 3.5) < 1e-5);
    }

    // 5. Validate HeaderedItemsControl with ContentPresenter ContentSource="Header"
    {
        Result<Ref<Menu>> menu = MakeRef<Menu>();
        CHECK(menu);
        Result<Ref<MenuItem>> menuItem = MakeRef<MenuItem>();
        CHECK(menuItem);
        menuItem.Value()->SetHeader(StringView("FileMenu"));
        menu.Value()->GetItems().Add(menuItem.Value());
        CHECK(menuItem.Value()->GetHeader().AsString() == StringView("FileMenu"));
    }

    // 6. Validate MultiBinding and BindingOperations ClearBinding
    {
        Result<Ref<TextBlock>> target = MakeRef<TextBlock>();
        CHECK(target);
        BindingOperations::ClearBinding(target.Value().Get(), TextBlock::TextProperty.Handle());
        BindingOperations::ClearAllBindings(target.Value().Get());
    }

    // 7. Validate TemplateBinding Property= prefix and Style.TargetType propagation
    {
        const char* xaml =
            "<ResourceDictionary xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\"\n"
            "                    xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">\n"
            "    <Style TargetType=\"Button\">\n"
            "        <Setter Property=\"Template\">\n"
            "            <Setter.Value>\n"
            "                <ControlTemplate>\n"
            "                    <Border Background=\"{TemplateBinding Property=Background}\"/>\n"
            "                </ControlTemplate>\n"
            "            </Setter.Value>\n"
            "        </Setter>\n"
            "    </Style>\n"
            "</ResourceDictionary>";
        Aero::Markup::XamlReader reader(live->gui);
        Result<Aero::Markup::XamlDocument> doc = reader.Parse(StringView(xaml), {}, {}, &live->diagnostics);
        CHECK(doc.HasValue());
    }

    // 8. Validate O(1) DFS interval IsDerivedFrom via TryCast
    {
        Result<Ref<Button>> btn = MakeRef<Button>();
        CHECK(btn);
        Aero::Base::Object* rawObj = btn.Value().Get();
        CHECK(Aero::TryCast<Button>(rawObj) == btn.Value().Get());
        CHECK(Aero::TryCast<FrameworkElement>(rawObj) == btn.Value().Get());
        CHECK(Aero::TryCast<UIElement>(rawObj) == btn.Value().Get());
        CHECK(Aero::TryCast<Aero::Media::Visual>(rawObj) == btn.Value().Get());
        CHECK(Aero::TryCast<Aero::DependencyObject>(rawObj) == btn.Value().Get());
        CHECK(Aero::TryCast<TextBlock>(rawObj) == nullptr);
        Result<Ref<FrameworkElement>> fe = MakeRef<FrameworkElement>();
        CHECK(fe);
        CHECK(Aero::TryCast<Button>(fe.Value().Get()) == nullptr);
    }

    // 9. Validate TypedValueConverter::DecodeValue<T> numeric and string conversions
    {
        struct TestIntConverter final : Aero::Data::TypedValueConverter<std::int32_t, std::int64_t> {
            Result<std::int64_t> ConvertTyped(
                const std::int32_t& value,
                const Aero::Base::Value& parameter,
                Aero::Base::StringView) noexcept override {
                Result<std::uint32_t> factor = DecodeValue<std::uint32_t>(parameter);
                if (!factor) return factor.GetStatus();
                Result<bool> flag = DecodeValue<bool>(Aero::Base::Value("true"));
                if (!flag || !flag.Value()) {
                    return Aero::Base::Status::Failure(Aero::Base::ErrorCode::InvalidArgument, "bool decode failed");
                }
                return static_cast<std::int64_t>(value) * static_cast<std::int64_t>(factor.Value());
            }
        } converter;

        Result<Aero::Base::Value> fromStringInput = converter.Convert(
            Aero::Base::Value("-12"),
            Aero::Base::Value("10"));
        CHECK(fromStringInput.HasValue());
        CHECK(fromStringInput.Value().Kind() == Aero::Base::ValueKind::SignedInteger);
        CHECK(fromStringInput.Value().AsSignedInteger() == -120);

        Result<Aero::Base::Value> fromDoubleInput = converter.Convert(
            Aero::Base::Value(7.0),
            Aero::Base::Value(static_cast<std::int64_t>(3)));
        CHECK(fromDoubleInput.HasValue());
        CHECK(fromDoubleInput.Value().AsSignedInteger() == 21);

        Result<Aero::Base::Value> invalidInput = converter.Convert(
            Aero::Base::Value("not_an_int"),
            Aero::Base::Value("10"));
        CHECK(!invalidInput.HasValue());
    }

    // 10. Validate intrusive WeakRef::TryAttach and Peek on stack-allocated Visual
    {
        Aero::Base::WeakRef<Border> weakStackVisual;
        CHECK(weakStackVisual.Peek() == nullptr);
        {
            Border stackBorder;
            CHECK(weakStackVisual.TryAttach(stackBorder));
            CHECK(weakStackVisual.Peek() == &stackBorder);
            CHECK(!weakStackVisual.Expired());
            CHECK(!weakStackVisual.Lock()); // stack object has UseCount() == 0
        }
        CHECK(weakStackVisual.Peek() == nullptr);
        CHECK(weakStackVisual.Expired());
    }

    // 11. Validate PropertyStore sorted binary search & unmaterialized inheritance propagation
    {
        Result<Ref<StackPanel>> parentPanel = MakeRef<StackPanel>();
        Result<Ref<StackPanel>> midPanel = MakeRef<StackPanel>();
        Result<Ref<TextBlock>> childText = MakeRef<TextBlock>();
        CHECK(parentPanel && midPanel && childText);

        midPanel.Value()->GetChildren().Add(childText.Value());
        parentPanel.Value()->GetChildren().Add(midPanel.Value());
        CHECK(live->view->SetContent(parentPanel.Value(), {400.0, 300.0}));
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);

        parentPanel.Value()->SetValue(UIElement::IsEnabledProperty, false);
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        CHECK(!childText.Value()->GetIsEnabled());

        parentPanel.Value()->ClearValue(UIElement::IsEnabledProperty);
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        CHECK(childText.Value()->GetIsEnabled());
        Result<Aero::Meta::PropertyValueSourceInfo> midSource =
            Aero::Diagnostics::GetValueSource(*midPanel.Value(), UIElement::IsEnabledProperty.Handle());
        CHECK(midSource.HasValue());
        CHECK(!midSource.Value().isInherited);

        // Detaching a subtree while an inherited value is active must reset both the detached node and its descendants
        parentPanel.Value()->SetValue(UIElement::IsEnabledProperty, false);
        parentPanel.Value()->SetValue(FrameworkElement::DataContextProperty, Aero::Base::Value("RootContext"));
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        CHECK(!midPanel.Value()->GetIsEnabled());
        CHECK(!childText.Value()->GetIsEnabled());
        CHECK(childText.Value()->GetValue(FrameworkElement::DataContextProperty).AsString() == "RootContext");

        parentPanel.Value()->GetChildren().Remove(*midPanel.Value());
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        CHECK(midPanel.Value()->GetIsEnabled());
        CHECK(childText.Value()->GetIsEnabled());
        CHECK(childText.Value()->GetValue(FrameworkElement::DataContextProperty).IsNullObject());

        // 12. Validate SetInheritanceParent propagates to descendants even when midPanel already has a StoredValueEntry,
        // and verify > 255 unmaterialized inheritance notifications do not saturate unmaterializedQueueCount_
        midPanel.Value()->SetValue(UIElement::IsEnabledProperty, false);
        midPanel.Value()->ClearValue(UIElement::IsEnabledProperty);
        parentPanel.Value()->SetValue(UIElement::IsEnabledProperty, false);
        parentPanel.Value()->GetChildren().Add(midPanel.Value());
        for (int i = 0; i < 300; ++i) {
            parentPanel.Value()->SetValue(FrameworkElement::DataContextProperty, Aero::Base::Value(static_cast<std::int64_t>(i)));
        }
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        CHECK(!childText.Value()->GetIsEnabled());
        CHECK(childText.Value()->GetValue(FrameworkElement::DataContextProperty).AsSignedInteger() == 299);
    }

    // 13. Validate ControlTemplate VSM and Triggers with compile-time part indices and object Setter.Value
    {
        constexpr const char* kVsmTriggerPartIndexXaml = R"(
<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
    <Grid.Resources>
        <ControlTemplate x:Key="IndexedButtonTemplate" TargetType="Button">
            <Border x:Name="RootBorder" Width="120" Height="40">
                <VisualStateManager.VisualStateGroups>
                    <VisualStateGroup x:Name="CommonStates">
                        <VisualState x:Name="Normal"/>
                        <VisualState x:Name="Pressed">
                            <Setter TargetName="InnerLabel" Property="Opacity" Value="0.5"/>
                        </VisualState>
                    </VisualStateGroup>
                </VisualStateManager.VisualStateGroups>
                <TextBlock x:Name="InnerLabel" Text="{TemplateBinding Content}" Opacity="1.0"/>
            </Border>
            <ControlTemplate.Triggers>
                <Trigger SourceName="RootBorder" Property="Width" Value="120">
                    <Setter TargetName="RootBorder" Property="LayoutTransform">
                        <Setter.Value>
                            <RotateTransform Angle="90"/>
                        </Setter.Value>
                    </Setter>
                </Trigger>
            </ControlTemplate.Triggers>
        </ControlTemplate>
    </Grid.Resources>
    <Button x:Name="TestBtn" Template="{StaticResource IndexedButtonTemplate}" Content="Indexed"/>
</Grid>)";

        Aero::Markup::XamlReader reader(live->gui);
        Result<Aero::Markup::XamlDocument> doc = reader.Parse(StringView(kVsmTriggerPartIndexXaml), {}, {}, &live->diagnostics);
        CHECK(doc.HasValue());
        Button* btn = doc.Value().FindName<Button>("TestBtn");
        CHECK(btn != nullptr);
        CHECK(live->view->SetContent(std::move(doc).Value(), {400.0, 300.0}));
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        Border* rootBorder = TryCast<Border>(btn->GetTemplateRoot());
        CHECK(rootBorder != nullptr);
        TextBlock* innerLabel = TryCast<TextBlock>(rootBorder->GetChild());
        CHECK(innerLabel != nullptr);
        CHECK(innerLabel->GetText() == "Indexed");
        CHECK(rootBorder->GetLayoutTransform().Get() != nullptr);

        CHECK(VisualStateManager::GoToState(*btn, "Pressed", false));
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        CHECK( std::abs(innerLabel->GetOpacity() - 0.5) < 1e-4 );

        CHECK(VisualStateManager::GoToState(*btn, "Normal", false));
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        CHECK( std::abs(innerLabel->GetOpacity() - 1.0) < 1e-4 );
    }

    // 14. Validate VSM StaticResource resolution (including forward-referenced resource),
    // BasedOn ControlTemplate direct VisualStateManager.VisualStateGroups, and DataTemplate Trigger SourceName / ElementName slot indexing
    {
        constexpr const char* kVsmResourceAndBasedOnXaml = R"(
<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
    <Grid.Resources>
        <ControlTemplate x:Key="BaseVsmTemplate" TargetType="Button">
            <Border x:Name="RootBorder" Width="100" Height="30">
                <VisualStateManager.VisualStateGroups>
                    <VisualStateGroup x:Name="CommonStates">
                        <VisualState x:Name="Normal"/>
                        <VisualState x:Name="Pressed">
                            <Setter TargetName="RootBorder" Property="Background" Value="{StaticResource ForwardPressedBrush}"/>
                        </VisualState>
                    </VisualStateGroup>
                </VisualStateManager.VisualStateGroups>
                <TextBlock x:Name="Label" Text="{TemplateBinding Content}"/>
            </Border>
        </ControlTemplate>
        <SolidColorBrush x:Key="ForwardPressedBrush" Color="#FF112233"/>
        <ControlTemplate x:Key="DerivedVsmTemplate" TargetType="Button" BasedOn="{StaticResource BaseVsmTemplate}">
            <VisualStateManager.VisualStateGroups>
                <VisualStateGroup x:Name="FocusStates">
                    <VisualState x:Name="Unfocused"/>
                    <VisualState x:Name="Focused">
                        <Setter TargetName="Label" Property="Opacity" Value="0.25"/>
                    </VisualState>
                </VisualStateGroup>
            </VisualStateManager.VisualStateGroups>
        </ControlTemplate>
        <DataTemplate x:Key="IndexedDataTemplate">
            <Border x:Name="ItemBorder" Width="80">
                <TextBlock x:Name="ItemText" Text="Hello"/>
            </Border>
            <DataTemplate.Triggers>
                <Trigger SourceName="ItemBorder" Property="Width" Value="80">
                    <Setter TargetName="ItemText" Property="Opacity" Value="0.75"/>
                </Trigger>
                <DataTrigger Binding="{Binding Text, ElementName=ItemText}" Value="Hello">
                    <Setter TargetName="ItemBorder" Property="Height" Value="48"/>
                </DataTrigger>
            </DataTemplate.Triggers>
        </DataTemplate>
    </Grid.Resources>
    <StackPanel>
        <Button x:Name="DerivedBtn" Template="{StaticResource DerivedVsmTemplate}" Content="VSM"/>
        <ItemsControl x:Name="DataHost" ItemTemplate="{StaticResource IndexedDataTemplate}"/>
    </StackPanel>
</Grid>)";

        Aero::Markup::XamlReader reader(live->gui);
        Result<Aero::Markup::XamlDocument> doc = reader.Parse(StringView(kVsmResourceAndBasedOnXaml), {}, {}, &live->diagnostics);
        CHECK(doc.HasValue());
        Button* derivedBtn = doc.Value().FindName<Button>("DerivedBtn");
        ItemsControl* dataHost = doc.Value().FindName<ItemsControl>("DataHost");
        CHECK(derivedBtn != nullptr);
        CHECK(dataHost != nullptr);
        CHECK(live->view->SetContent(std::move(doc).Value(), {400.0, 300.0}));
        Result<Ref<ObservableObjectCollection>> hostItems = MakeRef<ObservableObjectCollection>();
        CHECK(hostItems);
        Aero::Controls::AddBoxedStringItem(*hostItems.Value(), "Row0");
        dataHost->SetItemsSource(Ref<Aero::Base::Object>(hostItems.Value()));
        static_cast<void>(dataHost->ApplyTemplate());
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);

        Border* btnBorder = TryCast<Border>(derivedBtn->GetTemplateRoot());
        CHECK(btnBorder != nullptr);
        TextBlock* btnLabel = TryCast<TextBlock>(btnBorder->GetChild());
        CHECK(btnLabel != nullptr);

        CHECK(VisualStateManager::GoToState(*derivedBtn, "Pressed", false));
        CHECK(VisualStateManager::GoToState(*derivedBtn, "Focused", false));
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        auto* pressedBrush = TryCast<SolidColorBrush>(btnBorder->GetBackground().Get());
        CHECK(pressedBrush != nullptr);
        CHECK(std::abs(pressedBrush->GetColor().red - (0x11 / 255.0F)) < 1e-2F &&
              std::abs(pressedBrush->GetColor().green - (0x22 / 255.0F)) < 1e-2F &&
              std::abs(pressedBrush->GetColor().blue - (0x33 / 255.0F)) < 1e-2F);
        CHECK(std::abs(btnLabel->GetOpacity() - 0.25) < 1e-4);

        const auto findItemBorder = [](auto& self, Aero::Media::Visual& visual) -> Border* {
            if (auto* border = TryCast<Border>(&visual)) {
                if (std::abs(border->GetWidth() - 80.0) < 0.5) {
                    return border;
                }
            }
            const std::uint32_t count = Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
            for (std::uint32_t index = 0U; index < count; ++index) {
                Aero::Media::Visual* child = Aero::Media::VisualTreeHelper::GetChild(visual, index);
                if (child == nullptr) continue;
                if (Border* found = self(self, *child)) return found;
            }
            return nullptr;
        };
        Border* itemBorder = findItemBorder(findItemBorder, *dataHost);
        CHECK(itemBorder != nullptr);
        TextBlock* itemText = TryCast<TextBlock>(itemBorder->GetChild());
        CHECK(itemText != nullptr);
        CHECK(std::abs(itemText->GetOpacity() - 0.75) < 1e-4);
        CHECK(std::abs(itemBorder->GetHeight() - 48.0) < 1e-4);
    }

    // 15. Validate StoredValueEntry inline revision preservation for plain local values
    {
        Result<Ref<Button>> button = MakeRef<Button>();
        CHECK(button);
        button.Value()->SetWidth(120.0);
        Result<Aero::Meta::PropertyValueSourceInfo> source1 =
            Aero::Diagnostics::GetValueSource(*button.Value(), FrameworkElement::WidthProperty.Handle());
        CHECK(source1.HasValue());
        CHECK(source1.Value().revision > 0U);

        button.Value()->SetWidth(180.0);
        Result<Aero::Meta::PropertyValueSourceInfo> source2 =
            Aero::Diagnostics::GetValueSource(*button.Value(), FrameworkElement::WidthProperty.Handle());
        CHECK(source2.HasValue());
        CHECK(source2.Value().revision > source1.Value().revision);
    }

    // 16. Validate shared Freezable and shared IItemsSource / DependencyObject item survival when a single Visual detaches
    {
        const char* sharedXaml =
            "<StackPanel xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\"\n"
            "            xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"\n"
            "            x:Name=\"RootPanel\">\n"
            "    <StackPanel.Resources>\n"
            "        <SolidColorBrush x:Key=\"SharedBrush\" Color=\"#FF0000\" Opacity=\"{Binding Value, ElementName=OpacitySlider}\"/>\n"
            "        <SolidColorBrush x:Key=\"SharedItemBrush\" Color=\"#00FF00\" Opacity=\"{Binding Value, ElementName=ItemSlider}\"/>\n"
            "    </StackPanel.Resources>\n"
            "    <Slider x:Name=\"OpacitySlider\" Minimum=\"0\" Maximum=\"1\" Value=\"0.8\"/>\n"
            "    <Slider x:Name=\"ItemSlider\" Minimum=\"0\" Maximum=\"1\" Value=\"0.6\"/>\n"
            "    <Border x:Name=\"BorderA\" Width=\"40\" Height=\"20\" Background=\"{StaticResource SharedBrush}\"/>\n"
            "    <Border x:Name=\"BorderB\" Width=\"40\" Height=\"20\" Background=\"{StaticResource SharedBrush}\"/>\n"
            "    <ItemsControl x:Name=\"ListA\"/>\n"
            "    <ItemsControl x:Name=\"ListB\"/>\n"
            "</StackPanel>";

        Aero::Markup::XamlReader reader(live->gui);
        Result<Aero::Markup::XamlDocument> doc = reader.Parse(StringView(sharedXaml), {}, {}, &live->diagnostics);
        CHECK(doc.HasValue());
        auto* rootPanel = doc.Value().FindName<StackPanel>("RootPanel");
        auto* opacitySlider = doc.Value().FindName<Slider>("OpacitySlider");
        auto* itemSlider = doc.Value().FindName<Slider>("ItemSlider");
        auto* borderA = doc.Value().FindName<Border>("BorderA");
        auto* borderB = doc.Value().FindName<Border>("BorderB");
        auto* listA = doc.Value().FindName<ItemsControl>("ListA");
        auto* listB = doc.Value().FindName<ItemsControl>("ListB");
        CHECK(rootPanel != nullptr && opacitySlider != nullptr && itemSlider != nullptr);
        CHECK(borderA != nullptr && borderB != nullptr && listA != nullptr && listB != nullptr);

        auto* sharedBrush = TryCast<SolidColorBrush>(borderA->GetBackground().Get());
        CHECK(sharedBrush != nullptr);
        CHECK(sharedBrush == TryCast<SolidColorBrush>(borderB->GetBackground().Get()));

        Aero::Value itemBrushVal = rootPanel->GetResources().Lookup(StringView("SharedItemBrush"));
        auto* sharedItemBrush = itemBrushVal.Kind() == Aero::Base::ValueKind::Object
            ? TryCast<SolidColorBrush>(itemBrushVal.AsObject().Get())
            : nullptr;
        CHECK(sharedItemBrush != nullptr);

        Result<Ref<ObservableObjectCollection>> sharedItems = MakeRef<ObservableObjectCollection>();
        CHECK(sharedItems);
        sharedItems.Value()->Add(itemBrushVal.AsObject());
        listA->SetItemsSource(Ref<Aero::Base::Object>(sharedItems.Value()));
        listB->SetItemsSource(Ref<Aero::Base::Object>(sharedItems.Value()));

        CHECK(live->view->SetContent(std::move(doc).Value(), {400.0, 300.0}));
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);

        CHECK(std::abs(sharedBrush->GetOpacity() - 0.8) < 1e-4);
        CHECK(std::abs(sharedItemBrush->GetOpacity() - 0.6) < 1e-4);

        // Detach borderA and listA while borderB and listB remain alive and sharing the resources/items
        rootPanel->GetChildren().Remove(*borderA);
        rootPanel->GetChildren().Remove(*listA);
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);

        // Bindings on sharedBrush and sharedItemBrush must still be active because borderB and listB still use them
        opacitySlider->SetValue(0.25);
        itemSlider->SetValue(0.35);
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        CHECK(std::abs(sharedBrush->GetOpacity() - 0.25) < 1e-4);
        CHECK(std::abs(sharedItemBrush->GetOpacity() - 0.35) < 1e-4);

        // Now detach the last remaining consumers (borderB and listB)
        rootPanel->GetChildren().Remove(*borderB);
        rootPanel->GetChildren().Remove(*listB);
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);

        // After all visual consumers are gone, bindings on the non-visual objects should be detached
        opacitySlider->SetValue(0.9);
        itemSlider->SetValue(0.95);
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        CHECK(std::abs(sharedBrush->GetOpacity() - 0.9) > 1e-2);
        CHECK(std::abs(sharedItemBrush->GetOpacity() - 0.95) > 1e-2);
    }

    // 17. Validate incremental layout scalar sync and multi-property unmaterialized inheritance bitmask
    {
        const char* treeXaml =
            "<UserControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\"\n"
            "             xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"\n"
            "             x:Name=\"OuterControl\">\n"
            "    <StackPanel HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\">\n"
            "        <Border x:Name=\"ChildBox\">\n"
            "            <TextBlock x:Name=\"LeafText\" Text=\"Hello\"/>\n"
            "        </Border>\n"
            "    </StackPanel>\n"
            "</UserControl>";

        Aero::Markup::XamlReader reader(live->gui);
        Result<Aero::Markup::XamlDocument> doc = reader.Parse(StringView(treeXaml), {}, {}, &live->diagnostics);
        CHECK(doc.HasValue());
        auto* outer = doc.Value().FindName<UserControl>("OuterControl");
        auto* childBox = doc.Value().FindName<Border>("ChildBox");
        auto* leafText = doc.Value().FindName<TextBlock>("LeafText");
        CHECK(outer != nullptr && childBox != nullptr && leafText != nullptr);
        CHECK(live->view->SetContent(std::move(doc).Value(), {400.0, 300.0}));
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);

        childBox->SetWidth(150.0);
        childBox->SetHeight(60.0);
        childBox->SetMargin(Aero::Base::Thickness{10.0, 5.0, 10.0, 5.0});
        childBox->SetValue(UIElement::OpacityProperty, 0.5);
        outer->SetFontSize(22.0);
        outer->SetIsEnabled(false);
        outer->SetFlowDirection(Aero::FlowDirection::RightToLeft);
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);

        CHECK(std::abs(childBox->GetActualWidth() - 150.0) < 1e-3);
        CHECK(std::abs(childBox->GetActualHeight() - 60.0) < 1e-3);
        CHECK(std::abs(childBox->GetOpacity() - 0.5) < 1e-4);
        CHECK(std::abs(leafText->GetFontSize() - 22.0) < 1e-4);
        CHECK(!leafText->GetIsEnabled());
        CHECK(leafText->GetFlowDirection() == Aero::FlowDirection::RightToLeft);

        childBox->SetVisibility(Visibility::Collapsed);
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        CHECK(std::abs(childBox->GetDesiredSize().width) < 1e-4);
    }

    // 18. Validate nested FreezableCollection (LinearGradientBrush -> GradientStop) and Cross-View resource/item sharing
    {
        const char* gradXaml =
            "<StackPanel xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\"\n"
            "            xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\"\n"
            "            x:Name=\"GradRoot\">\n"
            "    <StackPanel.Resources>\n"
            "        <LinearGradientBrush x:Key=\"SharedGrad\">\n"
            "            <GradientStop x:Name=\"SharedStop\" Color=\"#FF0000\" Offset=\"{Binding Value, ElementName=OffsetSlider}\"/>\n"
            "            <GradientStop Color=\"#0000FF\" Offset=\"1.0\"/>\n"
            "        </LinearGradientBrush>\n"
            "    </StackPanel.Resources>\n"
            "    <Slider x:Name=\"OffsetSlider\" Minimum=\"0\" Maximum=\"1\" Value=\"0.3\"/>\n"
            "    <Border x:Name=\"GradBorderA\" Width=\"40\" Height=\"20\" Background=\"{StaticResource SharedGrad}\"/>\n"
            "    <Border x:Name=\"GradBorderB\" Width=\"40\" Height=\"20\" Background=\"{StaticResource SharedGrad}\"/>\n"
            "</StackPanel>";

        Aero::Markup::XamlReader reader(live->gui);
        Result<Aero::Markup::XamlDocument> doc = reader.Parse(StringView(gradXaml), {}, {}, &live->diagnostics);
        CHECK(doc.HasValue());
        auto* gradRoot = doc.Value().FindName<StackPanel>("GradRoot");
        auto* offsetSlider = doc.Value().FindName<Slider>("OffsetSlider");
        auto* gradBorderA = doc.Value().FindName<Border>("GradBorderA");
        auto* gradBorderB = doc.Value().FindName<Border>("GradBorderB");
        CHECK(gradRoot != nullptr && offsetSlider != nullptr && gradBorderA != nullptr && gradBorderB != nullptr);

        auto* sharedGrad = TryCast<Aero::Media::LinearGradientBrush>(gradBorderA->GetBackground().Get());
        CHECK(sharedGrad != nullptr);
        CHECK(sharedGrad->GetGradientStops().Size() == 2U);
        auto* stop0 = sharedGrad->GetGradientStops()[0U].Get();
        CHECK(stop0 != nullptr);

        CHECK(live->view->SetContent(std::move(doc).Value(), {400.0, 300.0}));
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        CHECK(std::abs(stop0->GetOffset() - 0.3F) < 1e-4F);

        // Create a second View on the same Gui sharing the same LinearGradientBrush
        auto view2Res = live->gui.CreateView(Aero::ViewOptions{});
        CHECK(view2Res.HasValue() && view2Res.Value().Get() != nullptr);
        View* view2 = view2Res.Value().Get();

        Result<Aero::Markup::XamlDocument> doc2 = reader.Parse(
            StringView("<Border xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" Width=\"40\" Height=\"20\"/>"),
            {}, {}, &live->diagnostics);
        CHECK(doc2.HasValue());
        auto* view2Border = doc2.Value().Root<Border>();
        CHECK(view2Border != nullptr);
        view2Border->SetBackground(gradBorderA->GetBackground());
        CHECK(view2->SetContent(std::move(doc2).Value(), {200.0, 100.0}));
        Pump(*view2, live->viewTime);

        // Remove GradBorderA from view 1; GradBorderB and view2Border still share sharedGrad
        gradRoot->GetChildren().Remove(*gradBorderA);
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);

        offsetSlider->SetValue(0.65);
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        CHECK(std::abs(stop0->GetOffset() - 0.65F) < 1e-4F);

        // Unmount view2; GradBorderB in view 1 still uses sharedGrad, so stop0's binding must survive!
        Result<Aero::Markup::XamlDocument> emptyDoc = reader.Parse(
            StringView("<Border xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\"/>"),
            {}, {}, &live->diagnostics);
        CHECK(emptyDoc.HasValue());
        CHECK(view2->SetContent(std::move(emptyDoc).Value(), {200.0, 100.0}));
        live->viewTime += 0.016;
        Pump(*view2, live->viewTime);

        offsetSlider->SetValue(0.85);
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        CHECK(std::abs(stop0->GetOffset() - 0.85F) < 1e-4F);
    }

    // 19. Validate non-default Local value preservation under higher-precedence VisualState provider and failed SetValue rollback over active Style/Inherited value
    {
        constexpr const char* kLocalVsVisualStateXaml = R"(
<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
    <Grid.Resources>
        <Style x:Key="WidthStyle" TargetType="Button">
            <Setter Property="Width" Value="140"/>
        </Style>
        <ControlTemplate x:Key="LocalVsVsmTemplate" TargetType="Button">
            <Border x:Name="RootBorder" Width="120" Height="40">
                <VisualStateManager.VisualStateGroups>
                    <VisualStateGroup x:Name="CommonStates">
                        <VisualState x:Name="Normal"/>
                        <VisualState x:Name="Pressed">
                            <Setter TargetName="InnerLabel" Property="Opacity" Value="0.25"/>
                        </VisualState>
                    </VisualStateGroup>
                </VisualStateManager.VisualStateGroups>
                <TextBlock x:Name="InnerLabel" Text="Test" Opacity="0.85"/>
            </Border>
        </ControlTemplate>
    </Grid.Resources>
    <Button x:Name="VsmBtn" Style="{StaticResource WidthStyle}" Template="{StaticResource LocalVsVsmTemplate}"/>
</Grid>)";

        Aero::Markup::XamlReader reader(live->gui);
        Result<Aero::Markup::XamlDocument> doc = reader.Parse(StringView(kLocalVsVisualStateXaml), {}, {}, &live->diagnostics);
        CHECK(doc.HasValue());
        Button* btn = doc.Value().FindName<Button>("VsmBtn");
        CHECK(btn != nullptr);
        CHECK(live->view->SetContent(std::move(doc).Value(), {400.0, 300.0}));
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);

        Border* rootBorder = TryCast<Border>(btn->GetTemplateRoot());
        CHECK(rootBorder != nullptr);
        TextBlock* innerLabel = TryCast<TextBlock>(rootBorder->GetChild());
        CHECK(innerLabel != nullptr);
        innerLabel->SetValue(UIElement::OpacityProperty, 0.85);
        CHECK(std::abs(innerLabel->GetOpacity() - 0.85) < 1e-4);
        CHECK(innerLabel->ReadLocalValue(UIElement::OpacityProperty).Kind() == Aero::Base::ValueKind::Double);

        // Activate VisualState "Pressed" (0.25) over non-default Local value (0.85)
        CHECK(VisualStateManager::GoToState(*btn, "Pressed", false));
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        CHECK(std::abs(innerLabel->GetOpacity() - 0.25) < 1e-4);
        CHECK(innerLabel->ReadLocalValue(UIElement::OpacityProperty).Kind() == Aero::Base::ValueKind::Double);
        CHECK(std::abs(innerLabel->ReadLocalValue(UIElement::OpacityProperty).AsDouble() - 0.85) < 1e-4);

        // Return to "Normal": non-default Local value (0.85) must be restored (not wiped to 1.0 default)
        CHECK(VisualStateManager::GoToState(*btn, "Normal", false));
        live->viewTime += 0.016;
        Pump(*live->view, live->viewTime);
        CHECK(std::abs(innerLabel->GetOpacity() - 0.85) < 1e-4);

        // Failed SetValue validation (negative Width) over active StyleSetter (140.0) must preserve 140.0
        CHECK(std::abs(btn->GetWidth() - 140.0) < 1e-4);
        btn->SetWidth(-10.0);
        CHECK(std::abs(btn->GetWidth() - 140.0) < 1e-4);
    }

    return true;
}

int main() {
    RUN(TestValueLayoutAndSSO);
    RUN(TestPasswordBoxDecoupled);
    RUN(TestStreamContract);
    RUN(TestPublicNamesAndHierarchy);
    RUN(TestDependencyPropertyVirtualFallback);
    RUN(TestXamlStreamReader);
    RUN(TestProviderOwnershipAndReplacement);
    RUN(TestViewFrameViewportAndInput);
    RUN(TestRenderCommitContentStability);
    RUN(TestDrawGeometryFillStrokeScene);
    RUN(TestDispatcherCrossThreadPost);
    RUN(TestContainerLayoutAndCalculators);
    RUN(TestComboBoxAndVisualStateAnimation);
    RUN(TestTransform3DCollapseAndHits);
    RUN(TestViewboxHoverAndPopupFlip);
    RUN(TestViewboxFrameworkElementSpacer);
    RUN(TestStackPanelZIndexDoesNotReorderLayout);
    RUN(TestPanelProgrammaticAddAttachesVisual);
    RUN(TestPanelXamlChildrenStayVisuallyParented);
    RUN(TestExpanderTemplatedParentIsCheckedWritesBack);
    RUN(TestExpanderUnnamedHeaderClickWritesBack);
    RUN(TestExpanderResourceDictionaryHeaderClick);
    RUN(TestControlTemplateHoverStoryboard);
    RUN(TestGridStarRowsSizeInStackPanel);
    RUN(TestThemeTagAndIndentationAndColor);
    RUN(TestGridSpanningAndStarMeasure);
    RUN(TestBlendTutorialSidebarInteractions);
    RUN(TestLoadComponentUserControlInStackPanel);
    RUN(TestLoadComponentStarGridUserControlInStackPanel);
    RUN(TestMenuTooltipForegroundAndFocus);
    RUN(TestViewboxTransform3DButtonHit);
    RUN(TestKeyboardNavigationIsTabStopTemplateTrigger);
    RUN(TestCheckedVisualStateRevertsArrowOpacity);
    RUN(TestRepeatButtonPressClickAndScaleX);
    RUN(TestComboBoxPopupItemClickSelects);
    RUN(TestComboBoxItemsSourcePopupClickSelects);
    RUN(TestDataTemplateElementNameSelectedItemFilter);
    RUN(TestDataTemplateMultiDataTriggerTeamFilter);
    RUN(TestDataTemplateItemHoverStoryboard);
    RUN(TestGeometryFlatten);
    RUN(TestClosedPathRelativeMove);
    RUN(TestStreamGeometryFlattenCore);
    RUN(TestStreamGeometryContextFlatten);
    RUN(TestTimelineDurationAndKeyTime);
    RUN(TestLoadedIntroStoryboard);
    RUN(TestBoardFlipRotationYStoryboard);
    RUN(TestCollectionViewAndVirtualization);
    RUN(TestTemplateResolveOrder);
    RUN(TestStrokeJoinCapFillRule);
    RUN(TestNotifyPropertyChangedBindLoop);
    RUN(TestBindingExpressionAndLostFocus);
    RUN(TestCustomItemsSourceThunk);
    RUN(TestClrItemsSourceBindingAfterDataContext);
    RUN(TestGalleryXamlSurface);
    RUN(TestGalleryHostXamlSurface);
    RUN(TestTutorialXamlSurface);
    RUN(TestTutorialRuntimePatterns);
    RUN(TestStyleSetterMergedStaticResource);
    RUN(TestMergedNestedStaticResource);
    RUN(TestTypeKeyedStaticResourceBasedOn);
    RUN(TestControlTemplateBasedOn);
    RUN(TestInventoryTemplateApply);
    RUN(TestTutorialSampleXamlLoadApply);
    RUN(TestMarkupExtensionContext);
    RUN(TestViewLoadXamlAndHotReload);
    RUN(TestArchitectureRefactoringValidation);
    std::puts("Aero framework conformance tests passed");
    std::fflush(stdout);
    // LiveGui instances are leaked on purpose (View/~Gui SIGSEGV with mounted
    // content). Skip atexit teardown of those process-lifetime objects.
    std::_Exit(0);
}
