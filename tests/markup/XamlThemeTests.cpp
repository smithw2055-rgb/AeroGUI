#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/RuntimeMetadata.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Markup/Metadata.hpp>
#include <Aero/Markup/XamlTheme.hpp>
#include <Aero/Presentation/Metadata.hpp>

#include <cstdio>
#include <fstream>
#include <memory>
#include <string>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Controls;
using namespace Aero::Markup;
using namespace Aero::Presentation;

#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed %d: %s\n", \
            __LINE__, #expression); \
        return false; \
    } \
} while (false)

std::string ReadTheme(const char* name) {
    std::string path(AERO_THEME_SOURCE_DIR);
    path += '/';
    path += name;
    std::ifstream stream(path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
}

bool SameColor(Color left, Color right) noexcept {
    return left.red == right.red &&
        left.green == right.green &&
        left.blue == right.blue &&
        left.alpha == right.alpha;
}

struct Fixture final {
    Dispatcher dispatcher;
    MetadataDomain metadata;
    std::unique_ptr<MetadataRuntime> runtime;
    std::unique_ptr<ObjectServicesScope> services;
    std::unique_ptr<EffectiveValueEngine> values;
    std::unique_ptr<ObjectTree> tree;
    std::unique_ptr<TemplateManager> templates;

    bool Build() {
        CHECK(TryRegisterBuiltInUiMetadata(metadata));
        CHECK(TryRegisterMarkupMetadata(metadata));
        CHECK(metadata.Seal());
        runtime = std::make_unique<MetadataRuntime>(metadata);
        CHECK(runtime->Freeze());
        services = std::make_unique<ObjectServicesScope>(
            dispatcher,
            metadata.DependencyProperties(),
            *runtime);
        values = std::make_unique<EffectiveValueEngine>(
            dispatcher, metadata.DependencyProperties());
        CHECK(values->Initialize());
        tree = std::make_unique<ObjectTree>(
            dispatcher, *values);
        CHECK(tree->Initialize());
        templates = std::make_unique<TemplateManager>(
            *tree, *values,
            metadata.DependencyProperties());
        return true;
    }
};

bool TestLightAndDarkThemeTemplates() {
    const std::string generic = ReadTheme("Generic.xaml");
    const std::string light = ReadTheme("Light.xaml");
    const std::string dark = ReadTheme("Dark.xaml");
    CHECK(!generic.empty() && !light.empty() && !dark.empty());

    Fixture fixture;
    CHECK(fixture.Build());
    Result<std::unique_ptr<XamlTheme>> lightResult =
        XamlTheme::Load(
            StringView(generic.data(),
                static_cast<std::uint32_t>(generic.size())),
            StringView(light.data(),
                static_cast<std::uint32_t>(light.size())),
            *fixture.runtime);
    if (!lightResult) {
        std::fprintf(stderr, "Theme load failed: %s\n",
            lightResult.GetStatus().message);
    }
    CHECK(lightResult);
    std::unique_ptr<XamlTheme> lightTheme =
        std::move(lightResult).Value();
    CHECK(lightTheme->Variant() == ThemeVariant::Light);
    CHECK(lightTheme->FindTemplate(Button::StaticTypeId()));
    CHECK(lightTheme->FindTemplate(RepeatButton::StaticTypeId()));
    CHECK(lightTheme->FindTemplate(ToggleButton::StaticTypeId()));
    CHECK(lightTheme->FindTemplate(CheckBox::StaticTypeId()));
    CHECK(lightTheme->FindTemplate(RadioButton::StaticTypeId()));
    CHECK(lightTheme->FindTemplate(ListBox::StaticTypeId()));
    CHECK(lightTheme->FindTemplate(ListBoxItem::StaticTypeId()));

    Button button;
    TextBlock buttonContent;
    CHECK(buttonContent.SetText("Run"));
    CHECK(button.SetContent(&buttonContent));
    CHECK(fixture.tree->SetRoot(&button));
    Result<TemplateHandle> applied =
        lightTheme->Apply(*fixture.templates, button);
    CHECK(applied);
    auto* chrome = static_cast<Border*>(
        fixture.templates->FindName(
            applied.Value(), "Chrome"));
    auto* focusCue = static_cast<Border*>(
        fixture.templates->FindName(
            applied.Value(), "FocusCue"));
    auto* presenter = static_cast<ContentPresenter*>(
        fixture.templates->FindName(
            applied.Value(), "ContentPresenter"));
    CHECK(chrome != nullptr && focusCue != nullptr &&
        presenter != nullptr);
    CHECK(presenter->Content() == &buttonContent);
    CHECK(buttonContent.LogicalParent() == &button);
    CHECK(buttonContent.VisualParent() == presenter);
    Result<Color> normal =
        lightTheme->ColorToken("ControlBackground");
    CHECK(normal);
    CHECK(SameColor(chrome->Background(), normal.Value()));

    VisualStateManager states(
        *fixture.values, *fixture.templates);
    CHECK(states.GoToState(
        button, "CommonStates", "Pressed").Value());
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::PropertyChanges));
    Result<Color> pressed =
        lightTheme->ColorToken(
            "ControlPressedBackground");
    CHECK(pressed);
    CHECK(SameColor(
        chrome->Background(), pressed.Value()));
    CHECK(states.GoToState(
        button, "FocusStates", "Focused").Value());
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::PropertyChanges));
    Result<Color> accent =
        lightTheme->ColorToken("AccentBorder");
    CHECK(accent);
    CHECK(SameColor(
        focusCue->BorderBrush(), accent.Value()));

    Result<std::unique_ptr<XamlTheme>> darkResult =
        XamlTheme::Load(
            StringView(generic.data(),
                static_cast<std::uint32_t>(generic.size())),
            StringView(dark.data(),
                static_cast<std::uint32_t>(dark.size())),
            *fixture.runtime);
    CHECK(darkResult);
    std::unique_ptr<XamlTheme> darkTheme =
        std::move(darkResult).Value();
    CHECK(darkTheme->Variant() == ThemeVariant::Dark);
    Result<Color> darkBackground =
        darkTheme->ColorToken("ControlBackground");
    CHECK(darkBackground);
    CHECK(!SameColor(
        normal.Value(), darkBackground.Value()));

    const TemplateHandle lightHandle = applied.Value();
    applied = darkTheme->Apply(*fixture.templates, button);
    CHECK(applied);
    CHECK(applied.Value().value != lightHandle.value);
    chrome = static_cast<Border*>(
        fixture.templates->FindName(
            applied.Value(), "Chrome"));
    presenter = static_cast<ContentPresenter*>(
        fixture.templates->FindName(
            applied.Value(), "ContentPresenter"));
    CHECK(chrome != nullptr && presenter != nullptr);
    CHECK(presenter->Content() == &buttonContent);
    CHECK(buttonContent.LogicalParent() == &button);
    CHECK(buttonContent.VisualParent() == presenter);
    CHECK(SameColor(
        chrome->Background(), darkBackground.Value()));
    CHECK(states.GoToState(
        button, "CommonStates", "Normal").Value());
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::PropertyChanges));

    CHECK(states.Clear(button));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::PropertyChanges));
    CHECK(fixture.templates->Clear(button).Value());
    CHECK(buttonContent.LogicalParent() == nullptr);
    CHECK(buttonContent.VisualParent() == nullptr);
    CHECK(fixture.tree->SetRoot(nullptr));
    CHECK(fixture.values->DetachObject(buttonContent));
    CHECK(fixture.values->DetachObject(button));

    CheckBox checkBox;
    CHECK(fixture.tree->SetRoot(&checkBox));
    applied = darkTheme->Apply(
        *fixture.templates, checkBox);
    CHECK(applied);
    auto* indicator = static_cast<Border*>(
        fixture.templates->FindName(
            applied.Value(), "Indicator"));
    CHECK(indicator != nullptr);
    CHECK(states.GoToState(
        checkBox, "CheckStates", "Checked").Value());
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::PropertyChanges));
    Result<Color> fill =
        darkTheme->ColorToken("AccentFill");
    CHECK(fill);
    CHECK(SameColor(
        indicator->Background(), fill.Value()));
    CHECK(states.Clear(checkBox));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::PropertyChanges));
    CHECK(fixture.templates->Clear(checkBox).Value());
    CHECK(fixture.tree->SetRoot(nullptr));
    CHECK(fixture.values->DetachObject(checkBox));

    ListBoxItem listBoxItem;
    TextBlock itemContent;
    CHECK(itemContent.SetText("Item"));
    CHECK(listBoxItem.SetContent(&itemContent));
    CHECK(fixture.tree->SetRoot(&listBoxItem));
    applied = darkTheme->Apply(
        *fixture.templates, listBoxItem);
    CHECK(applied);
    auto* selectionChrome = static_cast<Border*>(
        fixture.templates->FindName(
            applied.Value(), "SelectionChrome"));
    CHECK(selectionChrome != nullptr);
    CHECK(states.GoToState(
        listBoxItem,
        "SelectionStates",
        "Selected").Value());
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::PropertyChanges));
    CHECK(SameColor(
        selectionChrome->Background(),
        fill.Value()));
    CHECK(states.Clear(listBoxItem));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::PropertyChanges));
    CHECK(fixture.templates->Clear(
        listBoxItem).Value());
    CHECK(itemContent.LogicalParent() == nullptr);
    CHECK(itemContent.VisualParent() == nullptr);
    CHECK(fixture.tree->SetRoot(nullptr));
    CHECK(fixture.values->DetachObject(itemContent));
    CHECK(fixture.values->DetachObject(listBoxItem));
    return true;
}

bool TestThemeValidation() {
    const std::string generic = ReadTheme("Generic.xaml");
    Fixture fixture;
    CHECK(fixture.Build());
    const StringView missingTokenPalette(
        "<ResourceDictionary Variant=\"Light\">"
        "<Color Key=\"Transparent\" Value=\"#00000000\"/>"
        "</ResourceDictionary>");
    Result<std::unique_ptr<XamlTheme>> invalid =
        XamlTheme::Load(
            StringView(generic.data(),
                static_cast<std::uint32_t>(generic.size())),
            missingTokenPalette,
            *fixture.runtime);
    CHECK(!invalid);
    return true;
}

} // namespace

int main() {
    if (!TestLightAndDarkThemeTemplates()) return 1;
    if (!TestThemeValidation()) return 1;
    std::puts("Aero XAML theme tests passed");
    return 0;
}
