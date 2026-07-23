#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/Controls.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Core/Presentation.hpp>
#include <Aero/Core/TypeRegistry.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlDependencyProperty.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <cstdio>
#include <utility>

namespace {
using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Markup;

#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "CHECK failed at %s:%d: %s\\n", __FILE__, __LINE__, #expression); \
    return false; } } while (false)

struct Fixture final {
    Dispatcher dispatcher;
    TypeRegistry types;
    DependencyPropertyRegistry properties{types};
    XamlSchemaContext schema{types};
    XamlActivationProviderRegistry activation{schema};
    XamlDependencyPropertyBridge dependencyProperties{schema, properties};
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
        Ref<TextBlock> text = std::move(made).Value();
        return Ref<Object>(std::move(text));
    }

    bool Build() {
        Result<CorePresentationMetadata> metadata =
            TryRegisterCorePresentationMetadata(types, properties);
        CHECK(metadata);
        objectType = metadata.Value().objectType;
        stringType = metadata.Value().stringType;
        textBlockType = metadata.Value().textBlockType;
        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        CHECK(activation.TryRegister({textBlockType, &Activate, nullptr}));
        CHECK(TryRegisterCorePresentationXaml(dependencyProperties));
        CHECK(schema.Freeze());
        CHECK(activation.Freeze());
        return true;
    }

    XamlActivationContext Activation() noexcept {
        XamlActivationContext context = XamlActivationContext::Create();
        context.dispatcher = &dispatcher;
        context.dependencyProperties = &properties;
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
    XamlObjectWriter writer(fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlWithActivation(
        writer, reader, fixture.activation, fixture.Activation());
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
