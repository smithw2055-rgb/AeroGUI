#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Markup/Runtime/XamlLoader.hpp>
#include <Aero/RuntimeHost.hpp>
#include <Aero/Presentation/Resources.hpp>

#include <cstdio>
#include <string>

namespace {

using namespace Aero;
using namespace Aero::Base;
using namespace Aero::Controls;
using namespace Aero::Core;
using namespace Aero::Presentation;

#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed %d: %s\n", \
            __LINE__, #expression); \
        return false; \
    } \
} while (false)

std::string ThemePath(const char* name) {
    std::string path(AERO_THEME_SOURCE_DIR);
    path += '/';
    path += name;
    return path;
}

StringView View(const std::string& value) noexcept {
    return {
        value.data(),
        static_cast<std::uint32_t>(value.size())};
}

bool SameColor(Color left, Color right) noexcept {
    return left.red == right.red &&
        left.green == right.green &&
        left.blue == right.blue &&
        left.alpha == right.alpha;
}

bool LoadAndVerifyTheme(
    BuiltInTheme theme,
    Color& background) {
    RuntimeHost runtime;
    CHECK(runtime.Initialize());
    CHECK(runtime.EmbeddedXamlSources() != nullptr);
    CHECK(runtime.EmbeddedXamlSources()->SourceCount() >= 3U);
    CHECK(runtime.LoadBuiltInTheme(theme));

    ResourceDictionary* resources =
        runtime.ThemeResources();
    CHECK(resources != nullptr);
    CHECK(resources->Contains(Button::StaticTypeId()));
    CHECK(resources->Contains(RepeatButton::StaticTypeId()));
    CHECK(resources->Contains(ToggleButton::StaticTypeId()));
    CHECK(resources->Contains(CheckBox::StaticTypeId()));
    CHECK(resources->Contains(RadioButton::StaticTypeId()));
    CHECK(resources->Contains(ListBox::StaticTypeId()));
    CHECK(resources->Contains(ListBoxItem::StaticTypeId()));
    Result<ResourceValue> backgroundValue =
        resources->Lookup("ControlBackground");
    CHECK(backgroundValue);
    CHECK(backgroundValue.Value().Kind() ==
        ValueKind::Custom);
    CHECK(backgroundValue.Value().Type() ==
        TypeOf<Color>());
    background = *static_cast<const Color*>(
        backgroundValue.Value().AsCustom());

    Result<Ref<Object>> loaded = runtime.ParseXaml(
        "<Grid xmlns=\"urn:aero\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Button x:Name=\"Button\">"
        "<TextBlock x:Name=\"Content\" Text=\"Run\"/>"
        "</Button>"
        "</Grid>");
    CHECK(loaded);
    CHECK(runtime.Mount({320.0, 180.0}));
    Button* button = runtime.FindNamed<Button>("Button");
    TextBlock* content =
        runtime.FindNamed<TextBlock>("Content");
    CHECK(button != nullptr && content != nullptr);
    CHECK(runtime.Styles()->AppliedStyle(*button) != nullptr);
    const TemplateHandle handle =
        runtime.Templates()->AppliedHandle(*button);
    CHECK(handle.IsValid());
    const ControlTemplate* appliedTemplate =
        runtime.Templates()->AppliedTemplate(handle);
    CHECK(appliedTemplate != nullptr);
    CHECK(appliedTemplate->IsSealed());
    auto* chrome = static_cast<Border*>(
        runtime.Templates()->FindName(handle, "Chrome"));
    auto* presenter = static_cast<ContentPresenter*>(
        runtime.Templates()->FindName(
            handle, "ContentPresenter"));
    CHECK(chrome != nullptr && presenter != nullptr);
    CHECK(SameColor(chrome->Background(), background));
    CHECK(presenter->Content() == content);
    CHECK(content->LogicalParent() == button);
    CHECK(content->VisualParent() == presenter);

    CHECK(runtime.VisualStates()
        ->GoToState(
            *button, "CommonStates", "Pressed")
        .Value());
    CHECK(runtime.RunFrame());
    Result<ResourceValue> pressedValue =
        resources->Lookup("ControlPressedBackground");
    CHECK(pressedValue);
    const Color pressed =
        *static_cast<const Color*>(
            pressedValue.Value().AsCustom());
    CHECK(SameColor(chrome->Background(), pressed));

    CHECK(runtime.Unmount());
    runtime.Shutdown();
    return true;
}

bool TestLightAndDarkThemeTemplates() {
    Color light;
    Color dark;
    CHECK(LoadAndVerifyTheme(BuiltInTheme::Light, light));
    CHECK(LoadAndVerifyTheme(BuiltInTheme::Dark, dark));
    CHECK(!SameColor(light, dark));
    return true;
}

bool TestThemeValidation() {
    const std::string generic = ThemePath("Generic.xaml");
    RuntimeHost missingTokens;
    CHECK(missingTokens.Initialize());
    Result<void> merged =
        missingTokens.LoadResources(
            RuntimeResourceLayer::Theme,
            View(generic),
            RuntimeResourceLoadMode::Merge);
    CHECK(!merged);
    Result<void> invalid =
        missingTokens.LoadBuiltInTheme(
            static_cast<BuiltInTheme>(255U));
    CHECK(!invalid &&
        invalid.GetStatus().code ==
            ErrorCode::InvalidArgument);
    missingTokens.Shutdown();
    return true;
}

} // namespace

int main() {
    if (!TestLightAndDarkThemeTemplates()) return 1;
    if (!TestThemeValidation()) return 1;
    std::puts("Aero unified XAML theme tests passed");
    return 0;
}
