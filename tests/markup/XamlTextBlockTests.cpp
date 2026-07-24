#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Controls/RuntimeMetadata.hpp>
#include <Aero/Presentation/Metadata.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlDependencyProperty.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <cstdio>
#include <memory>
#include <utility>

namespace {
using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Presentation;
using namespace Aero::Controls;
using namespace Aero::Markup;

#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "CHECK failed at %s:%d: %s\\n", __FILE__, __LINE__, #expression); \
    return false; } } while (false)

struct Fixture final {
    Dispatcher dispatcher;
    MetadataDomain metadata;
    std::unique_ptr<MetadataRuntime> runtime;
    std::unique_ptr<XamlSchemaContext> schema;
    std::unique_ptr<XamlActivationProviderRegistry> activation;
    std::unique_ptr<XamlDependencyPropertyBridge> dependencyProperties;
    TypeId objectType = InvalidTypeId;
    TypeId stringType = InvalidTypeId;
    TypeId textBlockType = InvalidTypeId;

    static Result<Ref<Object>> Activate(
        TypeId type,
        const XamlActivationContext& context,
        void*) noexcept {
        if (context.dispatcher == nullptr ||
            context.dependencyProperties == nullptr) {
            return Status::Failure(ErrorCode::InvalidArgument,
                "TextBlock activation services are missing");
        }
        if (type != TextBlock::StaticTypeId()) {
            return Status::Failure(ErrorCode::InvalidArgument,
                "Activation type is not TextBlock");
        }
        Result<Ref<TextBlock>> made = MakeRef<TextBlock>();
        if (!made) return made.GetStatus();
        Ref<TextBlock> typed = std::move(made).Value();
        return Ref<Object>(std::move(typed));
    }

    bool Build() {
        CHECK(Aero::Controls::TryRegisterBuiltInUiMetadata(metadata));
        CHECK(metadata.Seal());
        objectType = BuiltinTypes::Object;
        stringType = BuiltinTypes::String;
        textBlockType = BuiltinTypes::TextBlock;
        runtime = std::make_unique<MetadataRuntime>(metadata);
        schema = std::make_unique<XamlSchemaContext>(metadata, *runtime);
        activation = std::make_unique<XamlActivationProviderRegistry>(*schema);
        dependencyProperties = std::make_unique<XamlDependencyPropertyBridge>(
            *schema, metadata.DependencyProperties());
        CHECK(activation->TryRegister({textBlockType, &Activate, nullptr}));
        CHECK(TryRegisterAeroPresentationXaml(*dependencyProperties));
        CHECK(runtime->Freeze());
        CHECK(schema->Freeze());
        CHECK(activation->Freeze());
        return true;
    }

    XamlActivationContext Activation() noexcept {
        XamlActivationContext context = XamlActivationContext::Create();
        context.dispatcher = &dispatcher;
        context.dependencyProperties = &metadata.DependencyProperties();
        return context;
    }
};

bool TestTextAttributeActivatesCoreTextBlock() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<TextBlock xmlns=\"urn:aero\" Text=\"Hello, 世界\"/>"),
        &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlWithActivation(
        writer, reader, *fixture.activation, fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    TextBlock* text = static_cast<TextBlock*>(loaded.Value().Get());
    CHECK(text != nullptr && text->Text() == StringView("Hello, 世界"));
    return true;
}

} // namespace

int main() {
    if (!TestTextAttributeActivatesCoreTextBlock()) return 1;
    std::puts("Aero XAML TextBlock tests passed");
    return 0;
}
