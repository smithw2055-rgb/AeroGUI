#include <Aero/Controls/Bars.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/ContentControls.hpp>
#include <Aero/Controls/ListView.hpp>
#include <Aero/Controls/Menus.hpp>
#include <Aero/Controls/Scroll.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/Trees.hpp>
#include <Aero/Integration/RenderEndpoint.hpp>
#include <Aero/Integration/ViewHost.hpp>
#include <Aero/Presentation/Animation.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Presentation/Transforms.hpp>
#include <Aero/RuntimeEnvironment.hpp>

#include "presentation/BatchPlanner.hpp"
#include "runtime/ViewAccess.hpp"

#include <cmath>
#include <cstdio>
#include <type_traits>

namespace {

using namespace Aero::Base;
using namespace Aero::Controls;
using namespace Aero::Presentation;

#define CHECK(expression)                                                   \
    do {                                                                    \
        if (!(expression)) {                                                \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n",           \
                __FILE__, __LINE__, #expression);                           \
            return false;                                                   \
        }                                                                   \
    } while (false)

TextBox* FindTextBoxDescendant(
    Visual& root) noexcept {
    for (Visual* child : root.VisualChildren()) {
        if (child == nullptr) {
            continue;
        }
        if (child->RuntimeType() ==
            TextBox::StaticTypeId()) {
            return static_cast<TextBox*>(child);
        }
        if (TextBox* nested =
                FindTextBoxDescendant(*child)) {
            return nested;
        }
    }
    return nullptr;
}

bool TestGeometryAndGridLengths() {
    CHECK(IsValidLayoutRect({0.0, 0.0, 64.0, 32.0}));
    CHECK(!IsValidLayoutSize({-1.0, 32.0}));
    CHECK(GridLength::Auto().unit == GridUnitType::Auto);
    CHECK(GridLength::Pixel(24.0).value == 24.0);
    CHECK(GridLength::Star(2.0).unit == GridUnitType::Star);
    Border border;
    CHECK(border.Background().alpha == 0.0F);
    const CornerRadius radius =
        border.GetCornerRadius();
    CHECK(radius.topLeft == 0.0);
    CHECK(radius.topRight == 0.0);
    CHECK(radius.bottomRight == 0.0);
    CHECK(radius.bottomLeft == 0.0);
    TreeViewItem treeItem;
    CHECK(treeItem.Icon().Empty());
    Path path;
    CHECK(path.Data().Empty());
    CHECK(path.GetStretch() == Stretch::Uniform);
    CHECK(path.GeometryBounds().width == 0.0);
    TextBlock text;
    CHECK(text.FontFamily().Empty());
    TextBox textBox;
    CHECK(textBox.MaximumLength() == 0U);
    CHECK(textBox.MaxLines() == 0U);
    CHECK(textBox.MinLines() == 1U);
    CHECK(TextBox::MaxLengthProperty.Name() ==
        "MaxLength");
    CHECK(TextBox::MaximumLengthProperty.Handle() ==
        TextBox::MaxLengthProperty.Handle());
    ScrollViewer scrollViewer;
    CHECK(scrollViewer.HorizontalScrollBarVisibility() ==
        ScrollBarVisibility::Disabled);
    CHECK(scrollViewer.VerticalScrollBarVisibility() ==
        ScrollBarVisibility::Visible);
    Slider slider;
    CHECK(slider.GetTickPlacement() ==
        TickPlacement::None);
    CHECK(slider.TickFrequency() == 1.0);
    CHECK(slider.Ticks().Empty());
    CHECK(!slider.IsSnapToTickEnabled());
    CHECK(!slider.IsDirectionReversed());
    CHECK(!slider.IsMoveToPointEnabled());
    SolidColorBrush solid;
    CHECK(solid.Opacity() == 1.0);
    CHECK(solid.GetColor().alpha == 1.0F);
    LinearGradientBrush gradient;
    CHECK(gradient.GradientStops().Empty());
    return true;
}

bool TestDisplayListOrdering() {
    DisplayListBuilder builder;
    CHECK(builder.PushOpacity(0.5));
    CHECK(builder.PushClip({0.0, 0.0, 32.0, 32.0}));
    CHECK(builder.FillRect(
        {0.0, 0.0, 16.0, 16.0},
        {1.0F, 0.0F, 0.0F, 1.0F}));
    CHECK(builder.PopClip());
    CHECK(builder.PopOpacity());
    Result<DisplayList> finished = builder.Finish();
    CHECK(finished);
    const Aero::Base::Span<const RenderCommand> commands =
        finished.Value().Commands();
    CHECK(commands.Size() == 5U);
    CHECK(commands[0].kind == RenderCommandKind::PushOpacity);
    CHECK(commands[1].kind == RenderCommandKind::PushClip);
    CHECK(commands[2].kind == RenderCommandKind::FillRect);
    CHECK(commands[3].kind == RenderCommandKind::PopClip);
    CHECK(commands[4].kind == RenderCommandKind::PopOpacity);
    return true;
}

bool TestControlHierarchy() {
    static_assert(std::is_base_of_v<Control, TextBoxBase>);
    static_assert(std::is_base_of_v<TextBoxBase, TextBox>);
    static_assert(std::is_base_of_v<TextBoxBase, PasswordBox>);
    static_assert(std::is_base_of_v<Control, RangeBase>);
    static_assert(std::is_base_of_v<RangeBase, ScrollBar>);
    static_assert(std::is_base_of_v<RangeBase, Slider>);
    static_assert(std::is_base_of_v<RangeBase, ProgressBar>);
    static_assert(std::is_base_of_v<ContentControl, ScrollViewer>);
    static_assert(std::is_base_of_v<FrameworkElement, Path>);
    static_assert(std::is_base_of_v<HeaderedContentControl, GroupBox>);
    static_assert(std::is_base_of_v<HeaderedContentControl, Expander>);
    static_assert(std::is_base_of_v<HeaderedContentControl, TabItem>);
    static_assert(std::is_base_of_v<ContentControl, Popup>);
    static_assert(std::is_base_of_v<Selector, ComboBox>);
    static_assert(std::is_base_of_v<ItemContainer, ComboBoxItem>);
    static_assert(std::is_base_of_v<ItemsControl, TreeView>);
    static_assert(std::is_base_of_v<ItemContainer, TreeViewItem>);
    static_assert(std::is_base_of_v<ItemsControl, Menu>);
    static_assert(std::is_base_of_v<TreeViewItem, MenuItem>);
    static_assert(std::is_base_of_v<Menu, ContextMenu>);
    static_assert(std::is_base_of_v<
        Aero::Base::Object,
        ContextMenuService>);
    static_assert(std::is_base_of_v<ListBox, ListView>);
    static_assert(std::is_base_of_v<ListBoxItem, ListViewItem>);
    static_assert(std::is_base_of_v<ItemsControl, ToolBar>);
    static_assert(std::is_base_of_v<ItemsControl, StatusBar>);
    static_assert(std::is_base_of_v<ItemContainer, StatusBarItem>);
    static_assert(std::is_base_of_v<Popup, ToolTip>);
    static_assert(std::is_base_of_v<
        Aero::Base::Object,
        ToolTipService>);
    static_assert(std::is_trivially_copyable_v<
        Aero::Integration::RenderFrameStatistics>);
    return true;
}

bool TestBatchPlanner() {
    DisplayListBuilder builder;
    CHECK(builder.FillRect(
        {0.0, 0.0, 10.0, 10.0},
        {1.0F, 0.0F, 0.0F, 1.0F}));
    CHECK(builder.FillRect(
        {10.0, 0.0, 10.0, 10.0},
        {0.0F, 1.0F, 0.0F, 1.0F}));
    CHECK(builder.FillRect(
        {20.0, 0.0, 10.0, 10.0},
        {0.0F, 0.0F, 1.0F, 1.0F}));
    Result<DisplayList> finished = builder.Finish();
    CHECK(finished);

    Aero::Presentation::Detail::BatchPlanner planner;
    Result<Aero::Presentation::Detail::BatchPlan> enabled =
        planner.BuildCommandsForTesting(
            finished.Value().Commands(), true, 64U);
    CHECK(enabled);
    CHECK(enabled.Value().Statistics().drawPacketCount == 3U);
    CHECK(enabled.Value().Statistics().batchCount == 1U);
    CHECK(enabled.Value().Statistics().mergedPacketCount == 2U);

    Result<Aero::Presentation::Detail::BatchPlan> disabled =
        planner.BuildCommandsForTesting(
            finished.Value().Commands(), false, 64U);
    CHECK(disabled);
    CHECK(disabled.Value().Statistics().batchCount == 3U);
    CHECK(disabled.Value().Statistics().mergedPacketCount == 0U);

    Result<Aero::Presentation::Detail::BatchPlan> capacityFlush =
        planner.BuildCommandsForTesting(
            finished.Value().Commands(), true, 2U);
    CHECK(capacityFlush);
    CHECK(capacityFlush.Value().Statistics().batchCount == 2U);

    Result<Aero::Presentation::Detail::BatchPlan> invalidCapacity =
        planner.BuildCommandsForTesting(
            finished.Value().Commands(), true, 0U);
    CHECK(!invalidCapacity);
    CHECK(invalidCapacity.GetStatus().code ==
        ErrorCode::InvalidArgument);

    DisplayListBuilder barrierBuilder;
    CHECK(barrierBuilder.FillRect(
        {0.0, 0.0, 10.0, 10.0},
        {1.0F, 1.0F, 1.0F, 1.0F}));
    CHECK(barrierBuilder.PushOpacity(0.5));
    CHECK(barrierBuilder.FillRect(
        {10.0, 0.0, 10.0, 10.0},
        {1.0F, 1.0F, 1.0F, 1.0F}));
    CHECK(barrierBuilder.PopOpacity());
    Result<DisplayList> barrierList =
        barrierBuilder.Finish();
    CHECK(barrierList);
    Result<Aero::Presentation::Detail::BatchPlan> barrierPlan =
        planner.BuildCommandsForTesting(
            barrierList.Value().Commands());
    CHECK(barrierPlan);
    CHECK(barrierPlan.Value().Statistics().batchCount == 2U);
    CHECK(barrierPlan.Value().Statistics().barrierCount == 2U);

    DisplayListBuilder resourceBuilder;
    CHECK(resourceBuilder.DrawImage(
        1U,
        {0.0, 0.0, 10.0, 10.0},
        {0.0, 0.0, 1.0, 1.0}));
    CHECK(resourceBuilder.DrawImage(
        2U,
        {10.0, 0.0, 10.0, 10.0},
        {0.0, 0.0, 1.0, 1.0}));
    CHECK(resourceBuilder.DrawGlyphRun(7U));
    CHECK(resourceBuilder.DrawGlyphRun(7U));
    CHECK(resourceBuilder.DrawGlyphRun(8U));
    Result<DisplayList> resourceList =
        resourceBuilder.Finish();
    CHECK(resourceList);
    Result<Aero::Presentation::Detail::BatchPlan>
        resourcePlan =
            planner.BuildCommandsForTesting(
                resourceList.Value().Commands());
    CHECK(resourcePlan);
    CHECK(resourcePlan.Value().Statistics().batchCount == 4U);
    CHECK(resourcePlan.Value().Statistics().mergedPacketCount == 1U);

    class FailingAllocator final : public IAllocator {
    public:
        void* Allocate(
            const AllocationRequest&) noexcept override {
            return nullptr;
        }
        void Deallocate(
            void*,
            std::size_t,
            std::size_t,
            MemoryTag) noexcept override {}
    } failingAllocator;
    Aero::Presentation::Detail::BatchPlanner
        failingPlanner(&failingAllocator);
    Result<Aero::Presentation::Detail::BatchPlan>
        outOfMemory =
            failingPlanner.BuildCommandsForTesting(
                finished.Value().Commands());
    CHECK(!outOfMemory);
    CHECK(outOfMemory.GetStatus().code ==
        ErrorCode::OutOfMemory);
    return true;
}

bool TestGridResponsiveMeasure() {
    Aero::RuntimeEnvironment environment;
    CHECK(environment.Initialize());
    Result<Aero::Base::Ref<Aero::View>> made =
        Aero::Integration::ViewHost::CreateView(
            environment, {});
    CHECK(made);
    Aero::Base::Ref<Aero::View> view =
        std::move(made).Value();
    CHECK(view->LoadBuiltInTheme(
        Aero::BuiltInTheme::Light));
    static constexpr char Source[] =
        "<Grid xmlns=\"urn:aero\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<TextBlock x:Name=\"ResponsiveText\" "
        "FontSize=\"16\" TextWrapping=\"Wrap\" "
        "Text=\"Star tracks must remeasure wrapped text when the viewport becomes narrower.\"/>"
        "</Grid>";
    Result<Aero::UiDocument> document = view->Parse({
        Source,
        static_cast<std::uint32_t>(
            sizeof(Source) - 1U)});
    CHECK(document);
    CHECK(view->SetContent(
        std::move(document).Value(),
        {420.0, 160.0}));
    CHECK(view->RunFrame());
    TextBlock* text =
        view->FindNamed<TextBlock>("ResponsiveText");
    CHECK(text != nullptr);
    const double wideHeight =
        text->DesiredSize().height;
    CHECK(view->Resize({140.0, 160.0}));
    CHECK(view->RunFrame());
    const double narrowHeight =
        text->DesiredSize().height;
    CHECK(narrowHeight > wideHeight);
    CHECK(view->Unmount());
    return true;
}

bool TestContentFragmentMounting() {
    Aero::RuntimeEnvironment environment;
    CHECK(environment.Initialize());
    Result<Aero::Base::Ref<Aero::View>> made =
        Aero::Integration::ViewHost::CreateView(
            environment, {});
    CHECK(made);
    Aero::Base::Ref<Aero::View> view =
        std::move(made).Value();
    static constexpr char HostSource[] =
        "<Grid xmlns=\"urn:aero\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Button x:Name=\"PageHost\"/>"
        "</Grid>";
    Result<Aero::UiDocument> hostDocument = view->Parse({
        HostSource,
        static_cast<std::uint32_t>(sizeof(HostSource) - 1U)});
    CHECK(hostDocument);
    CHECK(view->SetContent(
        std::move(hostDocument).Value(), {320.0, 180.0}));
    Button* host = view->FindNamed<Button>("PageHost");
    CHECK(host != nullptr);

    static constexpr char FirstPage[] =
        "<Border xmlns=\"urn:aero\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"FirstPage\"><TextBlock Text=\"First\"/>"
        "</Border>";
    Result<Aero::UiDocument> first = view->Parse({
        FirstPage,
        static_cast<std::uint32_t>(sizeof(FirstPage) - 1U)});
    CHECK(first);
    CHECK(view->MountContent(*host, std::move(first).Value()));
    CHECK(host->Content() != nullptr);
    CHECK(host->Content()->RuntimeType() == Border::StaticTypeId());
    CHECK(view->RunFrame());

    static constexpr char SecondPage[] =
        "<Border xmlns=\"urn:aero\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"SecondPage\"><TextBlock Text=\"Second\"/>"
        "</Border>";
    Result<Aero::UiDocument> second = view->Parse({
        SecondPage,
        static_cast<std::uint32_t>(sizeof(SecondPage) - 1U)});
    CHECK(second);
    CHECK(view->MountContent(*host, std::move(second).Value()));
    CHECK(host->Content() != nullptr);
    CHECK(host->Content()->RuntimeType() == Border::StaticTypeId());
    CHECK(view->RunFrame());
    CHECK(view->UnmountContent(*host));
    CHECK(host->Content() == nullptr);
    CHECK(view->Unmount());
    return true;
}

bool TestAnimationRuntime() {
    EasingFunction quadratic;
    quadratic.kind = EasingFunctionKind::Quadratic;
    quadratic.mode = EasingMode::EaseIn;
    CHECK(std::abs(AnimationManager::Ease(0.5, quadratic) - 0.25) <
        0.000001);
    quadratic.mode = EasingMode::EaseOut;
    CHECK(std::abs(AnimationManager::Ease(0.5, quadratic) - 0.75) <
        0.000001);

    Aero::RuntimeEnvironment environment;
    CHECK(environment.Initialize());
    Aero::Integration::ViewHostOptions viewOptions;
    viewOptions.automaticAnimationClock = false;
    Result<Aero::Base::Ref<Aero::View>> made =
        Aero::Integration::ViewHost::CreateView(
            environment, viewOptions);
    CHECK(made);
    Aero::Base::Ref<Aero::View> view =
        std::move(made).Value();
    CHECK(view->LoadBuiltInTheme(
        Aero::BuiltInTheme::Light));
    LinearGradientBrush gradient;
    Result<Ref<GradientStop>> first =
        MakeRef<GradientStop>();
    Result<Ref<GradientStop>> middle =
        MakeRef<GradientStop>();
    Result<Ref<GradientStop>> last =
        MakeRef<GradientStop>();
    CHECK(first && middle && last);
    CHECK(first.Value()->SetColor(
        {1.0F, 0.0F, 0.0F, 1.0F}));
    CHECK(middle.Value()->SetOffset(0.05));
    CHECK(middle.Value()->SetColor(
        {0.0F, 1.0F, 0.0F, 1.0F}));
    CHECK(last.Value()->SetOffset(1.0));
    CHECK(last.Value()->SetColor(
        {0.0F, 0.0F, 1.0F, 1.0F}));
    CHECK(gradient.AddGradientStop(
        Ref<GradientStop>(
            std::move(first).Value())));
    CHECK(gradient.AddGradientStop(
        Ref<GradientStop>(
            std::move(middle).Value())));
    CHECK(gradient.AddGradientStop(
        Ref<GradientStop>(
            std::move(last).Value())));
    const Color midpoint =
        gradient.Sample(0.525);
    CHECK(std::fabs(midpoint.red) < 0.001F);
    CHECK(std::fabs(midpoint.green - 0.5F) <
        0.001F);
    CHECK(std::fabs(midpoint.blue - 0.5F) <
        0.001F);
    static constexpr char Source[] =
        "<StackPanel xmlns=\"urn:aero\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "xmlns:aero=\"clr-namespace:AeroGUIExtensions;"
        "assembly=Aero.GUI.Extensions\" "
        "Orientation=\"Vertical\">"
        "<Border "
        "x:Name=\"AnimatedBorder\" MinWidth=\"7\" MaxHeight=\"10\" "
        "Background=\"#FF000000\">"
        "<Border.Triggers>"
        "<EventTrigger RoutedEvent=\"Loaded\">"
        "<BeginStoryboard><Storyboard>"
        "<DoubleAnimation "
        "Storyboard.TargetName=\"AnimatedBorder\" "
        "Storyboard.TargetProperty=\"MaxHeight\" "
        "From=\"10\" To=\"110\" Duration=\"0:0:1\" />"
        "<ColorAnimation "
        "Storyboard.TargetName=\"AnimatedBorder\" "
        "Storyboard.TargetProperty=\"Background\" "
        "From=\"#FF000000\" To=\"#FFFF0000\" Duration=\"0:0:1\" />"
        "</Storyboard></BeginStoryboard>"
        "</EventTrigger>"
        "</Border.Triggers>"
        "</Border>"
        "<Rectangle x:Name=\"AnimatedRectangle\" Width=\"60\" Height=\"60\" "
        "Fill=\"Turquoise\">"
        "<Rectangle.RenderTransform><TranslateTransform/>"
        "</Rectangle.RenderTransform>"
        "<Rectangle.Triggers><EventTrigger RoutedEvent=\"Loaded\">"
        "<BeginStoryboard><Storyboard>"
        "<DoubleAnimation Storyboard.TargetName=\"AnimatedRectangle\" "
        "Storyboard.TargetProperty=\"RenderTransform.X\" "
        "From=\"0\" To=\"300\" Duration=\"0:0:1\" />"
        "<DoubleAnimationUsingKeyFrames "
        "Storyboard.TargetName=\"AnimatedRectangle\" "
        "Storyboard.TargetProperty=\"RenderTransform.Y\">"
        "<LinearDoubleKeyFrame KeyTime=\"0:0:1\" Value=\"100\"/>"
        "</DoubleAnimationUsingKeyFrames>"
        "<ColorAnimationUsingKeyFrames "
        "Storyboard.TargetName=\"AnimatedRectangle\" "
        "Storyboard.TargetProperty=\"Fill.Color\" Duration=\"0:0:1\">"
        "<DiscreteColorKeyFrame KeyTime=\"0\" Value=\"#FFFF0000\"/>"
        "<LinearColorKeyFrame KeyTime=\"0:0:1\" Value=\"#FF0000FF\"/>"
        "</ColorAnimationUsingKeyFrames>"
        "<ObjectAnimationUsingKeyFrames "
        "Storyboard.TargetName=\"AnimatedRectangle\" "
        "Storyboard.TargetProperty=\"Visibility\">"
        "<DiscreteObjectKeyFrame KeyTime=\"0\" "
        "Value=\"{x:Static Visibility.Visible}\"/>"
        "<DiscreteObjectKeyFrame KeyTime=\"0:0:0.2\" "
        "Value=\"{x:Static Visibility.Hidden}\"/>"
        "<DiscreteObjectKeyFrame KeyTime=\"0:0:0.3\" "
        "Value=\"{x:Static Visibility.Visible}\"/>"
        "</ObjectAnimationUsingKeyFrames>"
        "</Storyboard></BeginStoryboard>"
        "</EventTrigger></Rectangle.Triggers>"
        "</Rectangle>"
        "<Rectangle x:Name=\"LayoutAnimatedRectangle\" "
        "Width=\"40\" Height=\"20\" HorizontalAlignment=\"Left\" "
        "Fill=\"Orange\">"
        "<Rectangle.LayoutTransform><ScaleTransform/>"
        "</Rectangle.LayoutTransform>"
        "<Rectangle.Triggers><EventTrigger RoutedEvent=\"Loaded\">"
        "<BeginStoryboard><Storyboard>"
        "<DoubleAnimation "
        "Storyboard.TargetName=\"LayoutAnimatedRectangle\" "
        "Storyboard.TargetProperty=\"LayoutTransform.ScaleX\" "
        "From=\"1\" To=\"2\" Duration=\"0:0:1\"/>"
        "<DoubleAnimation "
        "Storyboard.TargetName=\"LayoutAnimatedRectangle\" "
        "Storyboard.TargetProperty=\""
        "(FrameworkElement.LayoutTransform).(ScaleTransform.ScaleY)\" "
        "From=\"1\" To=\"1.5\" Duration=\"0:0:1\"/>"
        "</Storyboard></BeginStoryboard>"
        "</EventTrigger></Rectangle.Triggers>"
        "</Rectangle>"
        "<Rectangle x:Name=\"IndexedLayoutAnimatedRectangle\" "
        "Width=\"30\" Height=\"10\" HorizontalAlignment=\"Left\" "
        "Fill=\"Purple\">"
        "<Rectangle.LayoutTransform><TransformGroup>"
        "<ScaleTransform/><TranslateTransform X=\"3\"/>"
        "</TransformGroup></Rectangle.LayoutTransform>"
        "<Rectangle.Triggers><EventTrigger RoutedEvent=\"Loaded\">"
        "<BeginStoryboard><Storyboard>"
        "<DoubleAnimation "
        "Storyboard.TargetName=\"IndexedLayoutAnimatedRectangle\" "
        "Storyboard.TargetProperty=\""
        "(FrameworkElement.LayoutTransform)."
        "(TransformGroup.Children)[0].(ScaleTransform.ScaleX)\" "
        "From=\"1\" To=\"3\" Duration=\"0:0:1\"/>"
        "</Storyboard></BeginStoryboard>"
        "</EventTrigger></Rectangle.Triggers>"
        "</Rectangle>"
        "<Path x:Name=\"AnimatedStrokePath\" Width=\"180\" Height=\"40\" "
        "Data=\"M0,20 C45,0 90,40 180,20\" "
        "Fill=\"Transparent\" Stroke=\"Red\" StrokeThickness=\"8\" "
        "TrimEnd=\"0\">"
        "<Path.Triggers><EventTrigger RoutedEvent=\"Loaded\">"
        "<BeginStoryboard><Storyboard>"
        "<DoubleAnimation Storyboard.TargetName=\"AnimatedStrokePath\" "
        "Storyboard.TargetProperty=\"(Path.TrimEnd)\" "
        "From=\"0\" To=\"1\" Duration=\"0:0:1\"/>"
        "</Storyboard></BeginStoryboard>"
        "</EventTrigger></Path.Triggers>"
        "</Path>"
        "<Button x:Name=\"LiteralContentButton\" "
        "Content=\"Literal content\"/>"
        "<Button x:Name=\"LateTemplateButton\">"
        "<Button.Template>"
        "<ControlTemplate TargetType=\"Button\">"
        "<ControlTemplate.VisualTree>"
        "<Border x:Name=\"LateTemplateChrome\" Background=\"#FF202A31\">"
        "<ContentPresenter x:Name=\"LateTemplatePresenter\"/>"
        "</Border>"
        "</ControlTemplate.VisualTree>"
        "</ControlTemplate>"
        "</Button.Template>"
        "<Grid x:Name=\"LateTemplateContent\">"
        "<Border x:Name=\"LateTemplateNestedBorder\" "
        "Width=\"20\" Height=\"10\" Background=\"#FF2AA6E2\"/>"
        "<TextBlock Text=\"late template content\"/>"
        "</Grid>"
        "</Button>"
        "<ListBox x:Name=\"StaticItemsList\" Height=\"80\">"
        "<TextBlock Text=\"Mercury\"/>"
        "<TextBlock Text=\"Venus\"/>"
        "<TextBlock Text=\"Earth\"/>"
        "</ListBox>"
        "<TextBlock x:Name=\"InlineText\" FontSize=\"18\">"
        "<Run x:Name=\"InlineLead\" FontWeight=\"Bold\">Lead</Run>"
        "<LineBreak/>"
        "<Span x:Name=\"InlineSpan\">Text "
        "<Bold x:Name=\"InlineBold\">bold</Bold>, "
        "<Italic x:Name=\"InlineItalic\">italic</Italic>, "
        "<Underline x:Name=\"InlineUnderline\">underlined</Underline>."
        "</Span>"
        "<Hyperlink x:Name=\"InlineLink\" "
        "NavigateUri=\"https://example.com\">link</Hyperlink>"
        "</TextBlock>"
        "<TextBlock x:Name=\"InlineWrappedText\" "
        "Width=\"120\" TextWrapping=\"Wrap\">"
        "<Run>Inline text </Run>"
        "<Bold>must wrap across multiple lines when its parent constrains the available width.</Bold>"
        "</TextBlock>"
        "<TextBox x:Name=\"WrappedTextBox\" "
        "Width=\"120\" TextWrapping=\"Wrap\" "
        "Text=\"Editable text must wrap across multiple lines when constrained.\"/>"
        "<TextBox x:Name=\"NoWrapTextBox\" "
        "Width=\"120\" TextWrapping=\"NoWrap\" "
        "Text=\"Editable text must wrap across multiple lines when constrained.\"/>"
        "<TextBox x:Name=\"DependencyPropertyTextBox\" "
        "aero:Text.Placeholder=\"Enter your name\" "
        "MaxLength=\"3\" MaxLines=\"5\" MinLines=\"2\" "
        "ScrollViewer.VerticalScrollBarVisibility=\"Auto\"/>"
        "<ScrollViewer x:Name=\"VisibilityScrollViewer\" "
        "HorizontalScrollBarVisibility=\"Auto\" "
        "VerticalScrollBarVisibility=\"Hidden\">"
        "<Border Width=\"300\" Height=\"40\"/>"
        "</ScrollViewer>"
        "<ComboBox x:Name=\"EditableComboBox\" "
        "Width=\"160\" IsEditable=\"True\" Text=\"Custom value\">"
        "<ComboBoxItem Content=\"Alpha\"/>"
        "<ComboBoxItem Content=\"Beta\"/>"
        "</ComboBox>"
        "<ToggleButton x:Name=\"InitialCheckedToggle\" "
        "IsChecked=\"True\">"
        "<ToggleButton.Template>"
        "<ControlTemplate TargetType=\"ToggleButton\">"
        "<ControlTemplate.VisualTree>"
        "<Grid>"
        "<Border x:Name=\"InitialCheckedChrome\" "
        "Width=\"80\" Height=\"30\" Background=\"#FF30363A\"/>"
        "<Border x:Name=\"InitialCheckedSpace\" "
        "Width=\"30\" Height=\"20\" Background=\"#00000000\">"
        "<Border.LayoutTransform>"
        "<TransformGroup>"
        "<ScaleTransform ScaleX=\"0\" ScaleY=\"1\"/>"
        "<TranslateTransform/>"
        "</TransformGroup>"
        "</Border.LayoutTransform>"
        "</Border>"
        "</Grid>"
        "</ControlTemplate.VisualTree>"
        "<ControlTemplate.VisualStateGroups>"
        "<VisualStateGroup Name=\"CheckStates\">"
        "<VisualState Name=\"Checked\">"
        "<Setter TargetName=\"InitialCheckedChrome\" "
        "Property=\"Background\" Value=\"#FF0CC736\"/>"
        "<Storyboard>"
        "<DoubleAnimationUsingKeyFrames "
        "Storyboard.TargetName=\"InitialCheckedSpace\" "
        "Storyboard.TargetProperty=\"(FrameworkElement.LayoutTransform)."
        "(TransformGroup.Children)[0].(ScaleTransform.ScaleX)\">"
        "<EasingDoubleKeyFrame KeyTime=\"0\" Value=\"1\"/>"
        "</DoubleAnimationUsingKeyFrames>"
        "</Storyboard>"
        "</VisualState>"
        "<VisualState Name=\"Unchecked\"/>"
        "</VisualStateGroup>"
        "</ControlTemplate.VisualStateGroups>"
        "</ControlTemplate>"
        "</ToggleButton.Template>"
        "</ToggleButton>"
        "<Button x:Name=\"TransitionButton\">"
        "<Button.Template>"
        "<ControlTemplate TargetType=\"Button\">"
        "<ControlTemplate.VisualTree>"
        "<Border x:Name=\"TransitionChrome\" "
        "MinWidth=\"10\" MinHeight=\"20\" Background=\"#FF000000\"/>"
        "</ControlTemplate.VisualTree>"
        "<ControlTemplate.VisualStateGroups>"
        "<VisualStateGroup Name=\"TestStates\">"
        "<VisualStateGroup.Transitions>"
        "<VisualTransition From=\"Resting\" To=\"Active\" "
        "GeneratedDuration=\"0:0:1\">"
        "<VisualTransition.GeneratedEasingFunction>"
        "<QuadraticEase EasingMode=\"EaseIn\"/>"
        "</VisualTransition.GeneratedEasingFunction>"
        "</VisualTransition>"
        "<VisualTransition From=\"Active\" To=\"Resting\">"
        "<Storyboard>"
        "<DoubleAnimation Storyboard.TargetName=\"TransitionChrome\" "
        "Storyboard.TargetProperty=\"MinWidth\" "
        "From=\"110\" To=\"10\" Duration=\"0:0:0.5\"/>"
        "</Storyboard>"
        "</VisualTransition>"
        "</VisualStateGroup.Transitions>"
        "<VisualState Name=\"Resting\">"
        "<Setter TargetName=\"TransitionChrome\" "
        "Property=\"MinWidth\" Value=\"10\"/>"
        "</VisualState>"
        "<VisualState Name=\"Active\">"
        "<Setter TargetName=\"TransitionChrome\" "
        "Property=\"MinWidth\" Value=\"110\"/>"
        "<Storyboard>"
        "<DoubleAnimationUsingKeyFrames "
        "Storyboard.TargetName=\"TransitionChrome\" "
        "Storyboard.TargetProperty=\"MinHeight\">"
        "<DiscreteDoubleKeyFrame KeyTime=\"0\" Value=\"20\"/>"
        "<LinearDoubleKeyFrame KeyTime=\"0:0:1\" Value=\"120\"/>"
        "</DoubleAnimationUsingKeyFrames>"
        "</Storyboard>"
        "</VisualState>"
        "</VisualStateGroup>"
        "</ControlTemplate.VisualStateGroups>"
        "</ControlTemplate>"
        "</Button.Template>"
        "</Button>"
        "</StackPanel>";
    Result<Aero::UiDocument> document = view->Parse({
        Source, static_cast<std::uint32_t>(sizeof(Source) - 1U)});
    if (!document) {
        std::fprintf(stderr, "Animation XAML parse failed: %s\n",
            document.GetStatus().message);
    }
    CHECK(document);
    CHECK(view->SetContent(
        std::move(document).Value(), {320.0, 200.0}));
    Border* border = view->FindNamed<Border>("AnimatedBorder");
    CHECK(border != nullptr);
    Rectangle* rectangle =
        view->FindNamed<Rectangle>("AnimatedRectangle");
    CHECK(rectangle != nullptr);
    Rectangle* layoutRectangle =
        view->FindNamed<Rectangle>(
            "LayoutAnimatedRectangle");
    CHECK(layoutRectangle != nullptr);
    Rectangle* indexedLayoutRectangle =
        view->FindNamed<Rectangle>(
            "IndexedLayoutAnimatedRectangle");
    CHECK(indexedLayoutRectangle != nullptr);
    Path* strokePath =
        view->FindNamed<Path>("AnimatedStrokePath");
    CHECK(strokePath != nullptr);
    Button* literalContentButton =
        view->FindNamed<Button>("LiteralContentButton");
    CHECK(literalContentButton != nullptr);
    CHECK(literalContentButton->Content() != nullptr);
    CHECK(literalContentButton->Content()->RuntimeType() ==
        TextBlock::StaticTypeId());
    CHECK(static_cast<TextBlock*>(
        literalContentButton->Content())->Text() ==
        "Literal content");
    Button* lateTemplateButton =
        view->FindNamed<Button>("LateTemplateButton");
    CHECK(lateTemplateButton != nullptr);
    Grid* lateTemplateContent =
        view->FindNamed<Grid>("LateTemplateContent");
    CHECK(lateTemplateContent != nullptr);
    Border* lateTemplateNestedBorder =
        view->FindNamed<Border>(
            "LateTemplateNestedBorder");
    CHECK(lateTemplateNestedBorder != nullptr);
    ListBox* staticItemsList =
        view->FindNamed<ListBox>("StaticItemsList");
    CHECK(staticItemsList != nullptr);
    TextBlock* inlineText =
        view->FindNamed<TextBlock>("InlineText");
    CHECK(inlineText != nullptr);
    TextBlock* inlineWrappedText =
        view->FindNamed<TextBlock>(
            "InlineWrappedText");
    CHECK(inlineWrappedText != nullptr);
    TextBox* wrappedTextBox =
        view->FindNamed<TextBox>(
            "WrappedTextBox");
    CHECK(wrappedTextBox != nullptr);
    TextBox* noWrapTextBox =
        view->FindNamed<TextBox>(
            "NoWrapTextBox");
    CHECK(noWrapTextBox != nullptr);
    TextBox* dependencyPropertyTextBox =
        view->FindNamed<TextBox>(
            "DependencyPropertyTextBox");
    CHECK(dependencyPropertyTextBox != nullptr);
    ScrollViewer* visibilityScrollViewer =
        view->FindNamed<ScrollViewer>(
            "VisibilityScrollViewer");
    CHECK(visibilityScrollViewer != nullptr);
    ComboBox* editableComboBox =
        view->FindNamed<ComboBox>(
            "EditableComboBox");
    CHECK(editableComboBox != nullptr);
    CHECK(editableComboBox->IsEditable());
    CHECK(editableComboBox->Text() ==
        "Custom value");
    CHECK(wrappedTextBox->TextWrapping() ==
        Aero::Text::TextWrapping::Wrap);
    CHECK(dependencyPropertyTextBox->Placeholder() ==
        "Enter your name");
    CHECK(dependencyPropertyTextBox->MaximumLength() == 3U);
    CHECK(dependencyPropertyTextBox->MaxLines() == 5U);
    CHECK(dependencyPropertyTextBox->MinLines() == 2U);
    CHECK(ScrollViewer::GetVerticalScrollBarVisibility(
        *dependencyPropertyTextBox) ==
        ScrollBarVisibility::Auto);
    CHECK(visibilityScrollViewer->
        HorizontalScrollBarVisibility() ==
        ScrollBarVisibility::Auto);
    CHECK(visibilityScrollViewer->
        VerticalScrollBarVisibility() ==
        ScrollBarVisibility::Hidden);
    CHECK(visibilityScrollViewer->
        ComputedVerticalScrollBarVisibility() ==
        Visibility::Collapsed);
    CHECK(ScrollViewer::SetVerticalScrollBarVisibility(
        *visibilityScrollViewer,
        ScrollBarVisibility::Visible));
    CHECK(visibilityScrollViewer->
        ComputedVerticalScrollBarVisibility() ==
        Visibility::Visible);
    CHECK(dependencyPropertyTextBox->SetText(
        "programmatic text may exceed MaxLength"));
    CHECK(dependencyPropertyTextBox->Text() ==
        "programmatic text may exceed MaxLength");
    CHECK(inlineText->InlineCount() == 4U);
    Run* inlineLead =
        view->FindNamed<Run>("InlineLead");
    CHECK(inlineLead != nullptr);
    CHECK(inlineLead->Text() == "Lead");
    CHECK(inlineLead->GetFontWeight() ==
        FontWeight::Bold);
    CHECK(view->FindNamed<Bold>("InlineBold") != nullptr);
    CHECK(view->FindNamed<Italic>("InlineItalic") != nullptr);
    CHECK(view->FindNamed<Underline>(
        "InlineUnderline") != nullptr);
    CHECK(view->FindNamed<Hyperlink>(
        "InlineLink") != nullptr);
    Button* transitionButton =
        view->FindNamed<Button>("TransitionButton");
    CHECK(transitionButton != nullptr);
    ToggleButton* initialCheckedToggle =
        view->FindNamed<ToggleButton>(
            "InitialCheckedToggle");
    CHECK(initialCheckedToggle != nullptr);
    CHECK(initialCheckedToggle->IsChecked());
    CHECK(view->RunFrame());
    TextBox* editableComboText =
        FindTextBoxDescendant(
            *editableComboBox);
    CHECK(editableComboText != nullptr);
    CHECK(editableComboText->GetVisibility() ==
        Visibility::Visible);
    CHECK(editableComboText->Text() ==
        "Custom value");
    CHECK(inlineText->DesiredSize().height > 18.0);
    CHECK(inlineWrappedText->DesiredSize().height >
        30.0);
    CHECK(wrappedTextBox->DesiredSize().height >
        noWrapTextBox->DesiredSize().height);
    CHECK(staticItemsList->RealizedItemCount() > 0U);
    CHECK(staticItemsList->SetHeight(20.0));
    CHECK(view->RunFrame());
    CHECK(staticItemsList->SetHeight(80.0));
    CHECK(view->RunFrame());
    Ref<Transform> renderTransform =
        rectangle->RenderTransform();
    CHECK(renderTransform);
    CHECK(renderTransform->RuntimeType() ==
        TranslateTransform::StaticTypeId());
    auto* translate =
        static_cast<TranslateTransform*>(renderTransform.Get());
    Ref<Transform> layoutTransform =
        layoutRectangle->LayoutTransform();
    CHECK(layoutTransform);
    CHECK(layoutTransform->RuntimeType() ==
        ScaleTransform::StaticTypeId());
    auto* layoutScale =
        static_cast<ScaleTransform*>(
            layoutTransform.Get());
    Ref<Transform> indexedLayoutTransform =
        indexedLayoutRectangle->LayoutTransform();
    CHECK(indexedLayoutTransform);
    CHECK(indexedLayoutTransform->RuntimeType() ==
        TransformGroup::StaticTypeId());
    auto* layoutGroup =
        static_cast<TransformGroup*>(
            indexedLayoutTransform.Get());
    CHECK(layoutGroup->Children().Size() == 2U);
    CHECK(layoutGroup->Children()[0U]->RuntimeType() ==
        ScaleTransform::StaticTypeId());
    auto* indexedLayoutScale =
        static_cast<ScaleTransform*>(
            layoutGroup->Children()[0U].Get());
    AnimationManager* animations =
        Aero::Detail::ViewAccess::Animations(*view);
    CHECK(animations != nullptr);
    VisualStateManager* visualStates =
        Aero::Detail::ViewAccess::VisualStates(*view);
    CHECK(visualStates != nullptr);
    TemplateManager* templates =
        Aero::Detail::ViewAccess::Templates(*view);
    CHECK(templates != nullptr);
    CHECK(visualStates->CurrentState(
        *initialCheckedToggle,
        "CheckStates") == "Checked");
    const TemplateHandle initialCheckedHandle =
        templates->AppliedHandle(*initialCheckedToggle);
    CHECK(initialCheckedHandle.IsValid());
    auto* initialCheckedChrome =
        static_cast<Border*>(templates->FindName(
            initialCheckedHandle,
            "InitialCheckedChrome"));
    CHECK(initialCheckedChrome != nullptr);
    CHECK(initialCheckedChrome->Background().green >
        0.7F);
    auto* initialCheckedSpace =
        static_cast<Border*>(templates->FindName(
            initialCheckedHandle,
            "InitialCheckedSpace"));
    CHECK(initialCheckedSpace != nullptr);
    Ref<Transform> initialCheckedTransform =
        initialCheckedSpace->LayoutTransform();
    CHECK(initialCheckedTransform);
    CHECK(initialCheckedTransform->RuntimeType() ==
        TransformGroup::StaticTypeId());
    auto* initialCheckedGroup =
        static_cast<TransformGroup*>(
            initialCheckedTransform.Get());
    CHECK(initialCheckedGroup->Children().Size() == 2U);
    CHECK(initialCheckedGroup->Children()[0U]->
        RuntimeType() ==
        ScaleTransform::StaticTypeId());
    CHECK(view->AdvanceAnimationTime(1U));
    CHECK(view->RunFrame());
    CHECK(std::fabs(static_cast<ScaleTransform*>(
        initialCheckedGroup->Children()[0U].Get())->
            ScaleX() - 1.0) < 0.001);
    const TemplateHandle lateTemplateHandle =
        templates->AppliedHandle(*lateTemplateButton);
    CHECK(lateTemplateHandle.IsValid());
    const ControlTemplate* lateTemplate =
        templates->AppliedTemplate(lateTemplateHandle);
    CHECK(lateTemplate != nullptr);
    CHECK(templates->Clear(*lateTemplateButton));
    Result<TemplateHandle> reappliedLateTemplate =
        templates->Apply(
            *lateTemplateButton,
            *lateTemplate);
    CHECK(reappliedLateTemplate);
    CHECK(view->RunFrame());
    CHECK(lateTemplateContent->NodeId() !=
        Aero::Presentation::InvalidRenderNodeId);
    CHECK(lateTemplateNestedBorder->NodeId() !=
        Aero::Presentation::InvalidRenderNodeId);
    const TemplateHandle transitionTemplate =
        templates->AppliedHandle(*transitionButton);
    CHECK(transitionTemplate.IsValid());
    auto* transitionChrome = static_cast<Border*>(
        templates->FindName(
            transitionTemplate,
            "TransitionChrome"));
    CHECK(transitionChrome != nullptr);
    Result<bool> resting = visualStates->GoToState(
        *transitionButton,
        "TestStates",
        "Resting",
        false);
    CHECK(resting && resting.Value());
    Result<PropertyValue> transitionHeight =
        transitionChrome->GetValue(
            FrameworkElement::MinHeightProperty.Handle());
    CHECK(transitionHeight);
    if (transitionHeight.Value().Kind() !=
        ValueKind::Double) {
        std::fprintf(
            stderr,
            "Transition height kind=%u type=%llu expected=%llu\n",
            static_cast<unsigned>(
                transitionHeight.Value().Kind()),
            static_cast<unsigned long long>(
                transitionHeight.Value().Type()),
            static_cast<unsigned long long>(
                TypeOf<double>()));
    }
    CHECK(transitionHeight.Value().Kind() ==
        ValueKind::Double);
    CHECK(transitionHeight.Value().Type() ==
        TypeOf<double>());
    Result<bool> active = visualStates->GoToState(
        *transitionButton,
        "TestStates",
        "Active",
        true);
    if (!active) {
        std::fprintf(
            stderr,
            "VisualTransition activation failed: %s\n",
            active.GetStatus().message);
    }
    CHECK(active && active.Value());

    DoubleAnimation animation;
    animation.from = 0.0;
    animation.to = 100.0;
    animation.timing.durationMicroseconds = 1000000U;
    animation.timing.fillBehavior = FillBehavior::HoldEnd;
    Result<AnimationHandle> handle = animations->Begin(
        *border, FrameworkElement::MinWidthProperty, animation);
    CHECK(handle);
    Result<std::uint32_t> firstAdvance =
        view->AdvanceTime(250U);
    if (!firstAdvance) {
        std::fprintf(
            stderr,
            "Animation advance failed: %s\n",
            firstAdvance.GetStatus().message);
    }
    CHECK(firstAdvance);
    CHECK(view->RunFrame());
    Result<double> quarter =
        border->GetValue(FrameworkElement::MinWidthProperty);
    CHECK(quarter);
    CHECK(quarter.Value() > 24.0 && quarter.Value() < 26.0);
    Result<double> transitionWidth =
        transitionChrome->GetValue(
            FrameworkElement::MinWidthProperty);
    CHECK(transitionWidth);
    CHECK(transitionWidth.Value() > 15.0 &&
        transitionWidth.Value() < 17.0);
    Result<double> transitionMinHeight =
        transitionChrome->GetValue(
            FrameworkElement::MinHeightProperty);
    CHECK(transitionMinHeight);
    CHECK(std::abs(transitionMinHeight.Value() - 20.0) <
        0.001);
    Result<double> xamlQuarter =
        border->GetValue(FrameworkElement::MaxHeightProperty);
    CHECK(xamlQuarter);
    CHECK(xamlQuarter.Value() > 34.0 && xamlQuarter.Value() < 36.0);
    CHECK(border->Background().red > 0.24F &&
        border->Background().red < 0.26F);
    CHECK(translate->X() > 74.0 && translate->X() < 76.0);
    CHECK(translate->Y() > 24.0 && translate->Y() < 26.0);
    CHECK(layoutScale->ScaleX() > 1.24 &&
        layoutScale->ScaleX() < 1.26);
    CHECK(layoutScale->ScaleY() > 1.12 &&
        layoutScale->ScaleY() < 1.13);
    CHECK(layoutRectangle->DesiredSize().width > 49.0 &&
        layoutRectangle->DesiredSize().width < 51.0);
    CHECK(layoutRectangle->DesiredSize().height > 22.0 &&
        layoutRectangle->DesiredSize().height < 23.0);
    CHECK(indexedLayoutScale->ScaleX() > 1.49 &&
        indexedLayoutScale->ScaleX() < 1.51);
    CHECK(indexedLayoutRectangle->DesiredSize().width > 44.0 &&
        indexedLayoutRectangle->DesiredSize().width < 46.0);
    CHECK(rectangle->Fill().red > 0.74F &&
        rectangle->Fill().red < 0.76F);
    CHECK(rectangle->Fill().blue > 0.24F &&
        rectangle->Fill().blue < 0.26F);
    CHECK(rectangle->GetVisibility() == Visibility::Hidden);
    CHECK(strokePath->TrimEnd() > 0.24 &&
        strokePath->TrimEnd() < 0.26);
    CHECK(animations->Diagnostics().activeCount >= 7U);
    CHECK(animations->State(handle.Value()) ==
        AnimationState::Active);

    CHECK(animations->Pause(handle.Value()));
    const double pausedValue = quarter.Value();
    CHECK(view->AdvanceTime(250U));
    Result<double> stillPaused =
        border->GetValue(FrameworkElement::MinWidthProperty);
    CHECK(stillPaused);
    CHECK(std::abs(stillPaused.Value() - pausedValue) < 0.001);
    CHECK(animations->Resume(handle.Value()));
    CHECK(view->AdvanceTime(750U));
    Result<double> filled =
        border->GetValue(FrameworkElement::MinWidthProperty);
    CHECK(filled);
    CHECK(std::abs(filled.Value() - 100.0) < 0.001);
    CHECK(animations->State(handle.Value()) ==
        AnimationState::Filling);
    transitionMinHeight =
        transitionChrome->GetValue(
            FrameworkElement::MinHeightProperty);
    CHECK(transitionMinHeight);
    CHECK(transitionMinHeight.Value() > 44.0 &&
        transitionMinHeight.Value() < 46.0);
    CHECK(animations->Remove(handle.Value()));
    Result<double> restored =
        border->GetValue(FrameworkElement::MinWidthProperty);
    CHECK(restored);
    CHECK(std::abs(restored.Value() - 7.0) < 0.001);

    DoubleKeyFrame frames[] = {
        {250000U, 10.0, DoubleKeyFrameInterpolation::Discrete},
        {500000U, 40.0, DoubleKeyFrameInterpolation::Linear}};
    DoubleKeyFrameAnimation keyAnimation;
    keyAnimation.baseValue = 0.0;
    keyAnimation.timing.durationMicroseconds = 500000U;
    keyAnimation.timing.autoReverse = true;
    keyAnimation.timing.repeat = RepeatBehavior::Count(2.0);
    keyAnimation.timing.fillBehavior = FillBehavior::Stop;
    keyAnimation.keyFrames = {frames, 2U};
    Result<AnimationHandle> keyHandle = animations->Begin(
        *border, FrameworkElement::MinWidthProperty, keyAnimation);
    CHECK(keyHandle);
    CHECK(view->AdvanceTime(125U));
    Result<double> discrete =
        border->GetValue(FrameworkElement::MinWidthProperty);
    CHECK(discrete);
    CHECK(std::abs(discrete.Value()) < 0.001);
    CHECK(view->AdvanceTime(1875U));
    CHECK(animations->State(keyHandle.Value()) ==
        AnimationState::Stopped);
    restored = border->GetValue(FrameworkElement::MinWidthProperty);
    CHECK(restored);
    CHECK(std::abs(restored.Value() - 7.0) < 0.001);
    resting = visualStates->GoToState(
        *transitionButton,
        "TestStates",
        "Resting",
        true);
    CHECK(resting && resting.Value());
    CHECK(view->AdvanceTime(250U));
    transitionWidth =
        transitionChrome->GetValue(
            FrameworkElement::MinWidthProperty);
    CHECK(transitionWidth);
    CHECK(transitionWidth.Value() > 59.0 &&
        transitionWidth.Value() < 61.0);
    CHECK(view->Unmount());
    return true;
}

} // namespace

int main() {
    if (!TestGeometryAndGridLengths() ||
        !TestDisplayListOrdering() ||
        !TestControlHierarchy() ||
        !TestBatchPlanner() ||
        !TestGridResponsiveMeasure() ||
        !TestContentFragmentMounting() ||
        !TestAnimationRuntime()) {
        return 1;
    }
    std::puts("Aero framework conformance tests passed");
    return 0;
}
