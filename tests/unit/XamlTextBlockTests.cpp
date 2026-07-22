#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/Controls.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Core/TypeRegistry.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XamlTextBlock.hpp>
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
    XamlTextBlockExtension textBlock;
    TypeId objectType = InvalidTypeId;
    TypeId stringType = InvalidTypeId;
    TypeId textBlockType = InvalidTypeId;

    static Result<Ref<Object>> Activate(
        TypeId type,
        const XamlActivationContext& context,
        IAllocator& allocator,
        void*) noexcept {
        if (context.dispatcher == nullptr ||
            context.dependencyProperties == nullptr) {
            return Status::Failure(ErrorCode::InvalidArgument,
                "TextBlock activation services are missing");
        }
        Result<Ref<TextBlock>> made = MakeRefWithAllocator<TextBlock>(
            allocator,
            *context.dispatcher,
            *context.dependencyProperties,
            type,
            &allocator);
        if (!made) return made.GetStatus();
        Ref<TextBlock> text = std::move(made).Value();
        return Ref<Object>(std::move(text));
    }

    static TextBlock* Cast(Object& object, void*) noexcept {
        return &static_cast<TextBlock&>(object);
    }

    bool Build() {
        const StringView ns("urn:xaml-text-block");
        objectType = MakeTypeId(ns, StringView("Object"));
        stringType = MakeTypeId(ns, StringView("String"));
        textBlockType = MakeTypeId(ns, StringView("TextBlock"));
        CHECK(types.TryRegisterType({ns, StringView("Object"), InvalidTypeId,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("String"), InvalidTypeId,
            TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("TextBlock"), objectType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterProperty(textBlockType, {
            StringView("Text"), stringType, PropertyFlags::None}));
        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        CHECK(schema.TryRegisterScalarType(stringType, XamlScalarKind::String));
        CHECK(textBlock.TryRegisterType({textBlockType, &Cast, nullptr}));
        CHECK(textBlock.Register(schema));
        CHECK(activation.TryRegister({textBlockType, &Activate, nullptr}));
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
        "<TextBlock xmlns=\"urn:xaml-text-block\" Text=\"Hello, 世界\"/>"),
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
