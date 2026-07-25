#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/RuntimeMetadata.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>
#include <Aero/Presentation/Metadata.hpp>

#include <cstdio>
#include <memory>
#include <utility>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Controls;
using namespace Aero::Markup;

#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed %d: %s\n", \
            __LINE__, #expression); \
        return false; \
    } \
} while (false)

struct Fixture final {
    Dispatcher dispatcher;
    MetadataDomain metadata;
    std::unique_ptr<MetadataRuntime> runtime;
    std::unique_ptr<XamlSchemaContext> schema;
    std::unique_ptr<XamlActivationProviderRegistry> activation;

    bool Build() {
        CHECK(TryRegisterBuiltInUiMetadata(metadata));
        CHECK(metadata.Seal());
        runtime = std::make_unique<MetadataRuntime>(metadata);
        schema = std::make_unique<XamlSchemaContext>(
            metadata, *runtime);
        activation =
            std::make_unique<XamlActivationProviderRegistry>(
                *schema);
        CHECK(runtime->Freeze());
        CHECK(schema->Freeze());
        CHECK(activation->Freeze());
        return true;
    }

    XamlActivationContext Activation() noexcept {
        XamlActivationContext context =
            XamlActivationContext::Create();
        context.dispatcher = &dispatcher;
        context.dependencyProperties =
            &metadata.DependencyProperties();
        return context;
    }
};

Result<Ref<Object>> Load(
    Fixture& fixture,
    StringView xaml,
    DiagnosticBag& diagnostics) noexcept {
    Utf8XmlTokenizer tokenizer;
    Result<void> reset = tokenizer.Reset(xaml, &diagnostics);
    if (!reset) return reset.GetStatus();
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    return LoadXamlWithActivation(
        writer, reader, *fixture.activation,
        fixture.Activation());
}

bool TestTogglePropertiesFromXaml() {
    Fixture fixture;
    CHECK(fixture.Build());

    DiagnosticBag checkDiagnostics;
    Result<Ref<Object>> checked = Load(
        fixture,
        "<CheckBox xmlns=\"urn:aero\" "
        "IsChecked=\"True\" IsThreeState=\"True\"/>",
        checkDiagnostics);
    CHECK(checked && checkDiagnostics.Size() == 0U);
    auto* checkBox =
        static_cast<CheckBox*>(checked.Value().Get());
    CHECK(checkBox->IsChecked());
    CHECK(checkBox->IsThreeState());
    CHECK(!checkBox->IsIndeterminate());

    DiagnosticBag radioDiagnostics;
    Result<Ref<Object>> radioValue = Load(
        fixture,
        "<RadioButton xmlns=\"urn:aero\" "
        "GroupName=\"primary\" IsChecked=\"True\"/>",
        radioDiagnostics);
    CHECK(radioValue && radioDiagnostics.Size() == 0U);
    auto* radio =
        static_cast<RadioButton*>(radioValue.Value().Get());
    CHECK(radio->GroupName() == StringView("primary"));
    CHECK(radio->IsChecked());

    DiagnosticBag readOnlyDiagnostics;
    Result<Ref<Object>> invalid = Load(
        fixture,
        "<CheckBox xmlns=\"urn:aero\" "
        "IsIndeterminate=\"True\"/>",
        readOnlyDiagnostics);
    CHECK(!invalid);
    CHECK(readOnlyDiagnostics.Size() > 0U);
    return true;
}

bool TestTextBoxPropertiesFromXaml() {
    Fixture fixture;
    CHECK(fixture.Build());

    DiagnosticBag diagnostics;
    Result<Ref<Object>> loaded = Load(
        fixture,
        "<TextBox xmlns=\"urn:aero\" "
        "Text=\"hello\" MaximumLength=\"12\" "
        "AcceptsReturn=\"True\" IsReadOnly=\"True\"/>",
        diagnostics);
    CHECK(loaded && diagnostics.Size() == 0U);
    auto* textBox =
        static_cast<TextBox*>(
            loaded.Value().Get());
    CHECK(textBox->Text() ==
        StringView("hello"));
    CHECK(textBox->MaximumLength() == 12U);
    CHECK(textBox->AcceptsReturn());
    CHECK(textBox->IsReadOnly());
    return true;
}

} // namespace

int main() {
    if (!TestTogglePropertiesFromXaml()) return 1;
    if (!TestTextBoxPropertiesFromXaml()) return 1;
    std::puts("Aero XAML button tests passed");
    return 0;
}
