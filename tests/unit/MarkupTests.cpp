#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include "TestAllocatorScope.hpp"

#include <cstdio>

namespace {
using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Markup;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

class RejectAllocator final : public IAllocator {
public:
    void* Allocate(const AllocationRequest&) noexcept override {
        return nullptr;
    }

    void Deallocate(
        void*,
        std::size_t,
        std::size_t,
        MemoryTag) noexcept override {}
};

bool ReadToken(
    IXmlTokenizer& tokenizer,
    XmlToken& token,
    XmlTokenKind expected) {
    Result<XmlTokenKind> result = tokenizer.Read(token);
    CHECK(result);
    CHECK(result.Value() == expected);
    CHECK(token.Kind() == expected);
    return true;
}

bool ReadNode(
    XamlNodeReader& reader,
    XamlNode& node,
    XamlNodeKind expected) {
    Result<XamlNodeKind> result = reader.Read(node);
    CHECK(result);
    CHECK(result.Value() == expected);
    CHECK(node.Kind() == expected);
    return true;
}

bool TestTokenizerSequenceAndEntities() {
    const StringView input(
        "<?xml version=\"1.0\"?>"
        "<Window xmlns=\"urn:aero\" Title=\"A &amp; B\">"
        "<TextBlock Text=\"Hi &#x1F642;\"/>"
        "A &lt; B"
        "<![CDATA[ & raw]]>"
        "</Window>");

    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(input, &diagnostics));
    XmlToken token;

    CHECK(ReadToken(tokenizer, token, XmlTokenKind::StartElement));
    CHECK(token.Name() == StringView("Window"));
    CHECK(!token.IsEmptyElement());
    CHECK(token.Attributes().Size() == 2U);
    CHECK(token.Attributes()[0].Name() == StringView("xmlns"));
    CHECK(token.Attributes()[0].Value() == StringView("urn:aero"));
    CHECK(token.Attributes()[1].Name() == StringView("Title"));
    CHECK(token.Attributes()[1].Value() == StringView("A & B"));
    CHECK(tokenizer.Depth() == 1U);

    CHECK(ReadToken(tokenizer, token, XmlTokenKind::StartElement));
    CHECK(token.Name() == StringView("TextBlock"));
    CHECK(token.IsEmptyElement());
    CHECK(token.Attributes().Size() == 1U);
    CHECK(token.Attributes()[0].Value() == StringView("Hi \xF0\x9F\x99\x82"));
    CHECK(tokenizer.Depth() == 1U);

    CHECK(ReadToken(tokenizer, token, XmlTokenKind::Text));
    CHECK(token.Text() == StringView("A < B"));

    CHECK(ReadToken(tokenizer, token, XmlTokenKind::Text));
    CHECK(token.Text() == StringView(" & raw"));

    CHECK(ReadToken(tokenizer, token, XmlTokenKind::EndElement));
    CHECK(token.Name() == StringView("Window"));
    CHECK(tokenizer.Depth() == 0U);

    CHECK(ReadToken(tokenizer, token, XmlTokenKind::EndOfDocument));
    CHECK(ReadToken(tokenizer, token, XmlTokenKind::EndOfDocument));
    CHECK(diagnostics.Size() == 0U);
    return true;
}

bool TestSourceSpans() {
    const StringView input("<Root>\n  <Child a=\"v\"/>\n</Root>");
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(input));
    XmlToken token;

    CHECK(ReadToken(tokenizer, token, XmlTokenKind::StartElement));
    CHECK(token.Source().begin.line == 1U);
    CHECK(token.Source().begin.column == 1U);
    CHECK(token.NameSource().begin.column == 2U);

    CHECK(ReadToken(tokenizer, token, XmlTokenKind::Text));
    CHECK(token.Text() == StringView("\n  "));

    CHECK(ReadToken(tokenizer, token, XmlTokenKind::StartElement));
    CHECK(token.Source().begin.line == 2U);
    CHECK(token.Source().begin.column == 3U);
    CHECK(token.Attributes().Size() == 1U);
    CHECK(token.Attributes()[0].NameSource().begin.line == 2U);
    CHECK(token.Attributes()[0].ValueSource().begin.line == 2U);
    CHECK(token.Attributes()[0].ValueSource().end.byteOffset >
        token.Attributes()[0].ValueSource().begin.byteOffset);
    return true;
}

bool TestXmlLineEndingNormalization() {
    const StringView input("<A v=\"x\ty\r\nz\">a\r\nb</A>");
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(input));
    XmlToken token;

    CHECK(ReadToken(tokenizer, token, XmlTokenKind::StartElement));
    CHECK(token.Attributes().Size() == 1U);
    CHECK(token.Attributes()[0].Value() == StringView("x y z"));
    CHECK(ReadToken(tokenizer, token, XmlTokenKind::Text));
    CHECK(token.Text() == StringView("a\nb"));
    CHECK(ReadToken(tokenizer, token, XmlTokenKind::EndElement));
    CHECK(ReadToken(tokenizer, token, XmlTokenKind::EndOfDocument));
    return true;
}

bool TestXamlNodeStreamNamespacesAndSelfClosing() {
    const StringView input(
        "<Window xmlns=\"urn:aero\" xmlns:x=\"urn:x\" x:Name=\"root\">"
        "<TextBlock Text=\"Hello\"/>"
        "</Window>");

    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(input, &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlNode node;

    CHECK(ReadNode(reader, node, XamlNodeKind::NamespaceDeclaration));
    CHECK(node.NamespacePrefix().Empty());
    CHECK(node.NamespaceUri() == StringView("urn:aero"));

    CHECK(ReadNode(reader, node, XamlNodeKind::NamespaceDeclaration));
    CHECK(node.NamespacePrefix() == StringView("x"));
    CHECK(node.NamespaceUri() == StringView("urn:x"));

    CHECK(ReadNode(reader, node, XamlNodeKind::StartObject));
    CHECK(node.Name().Prefix().Empty());
    CHECK(node.Name().LocalName() == StringView("Window"));
    CHECK(node.Name().NamespaceUri() == StringView("urn:aero"));

    CHECK(ReadNode(reader, node, XamlNodeKind::StartMember));
    CHECK(node.IsFromAttribute());
    CHECK(node.Name().Prefix() == StringView("x"));
    CHECK(node.Name().LocalName() == StringView("Name"));
    CHECK(node.Name().NamespaceUri() == StringView("urn:x"));

    CHECK(ReadNode(reader, node, XamlNodeKind::Value));
    CHECK(node.IsFromAttribute());
    CHECK(node.Value() == StringView("root"));

    CHECK(ReadNode(reader, node, XamlNodeKind::EndMember));
    CHECK(node.Name().LocalName() == StringView("Name"));

    CHECK(ReadNode(reader, node, XamlNodeKind::StartObject));
    CHECK(node.Name().LocalName() == StringView("TextBlock"));
    CHECK(node.Name().NamespaceUri() == StringView("urn:aero"));

    CHECK(ReadNode(reader, node, XamlNodeKind::StartMember));
    CHECK(node.Name().Prefix().Empty());
    CHECK(node.Name().LocalName() == StringView("Text"));
    CHECK(node.Name().NamespaceUri().Empty());

    CHECK(ReadNode(reader, node, XamlNodeKind::Value));
    CHECK(node.Value() == StringView("Hello"));
    CHECK(node.IsFromAttribute());

    CHECK(ReadNode(reader, node, XamlNodeKind::EndMember));
    CHECK(ReadNode(reader, node, XamlNodeKind::EndObject));
    CHECK(node.Name().LocalName() == StringView("TextBlock"));
    CHECK(node.Name().NamespaceUri() == StringView("urn:aero"));

    CHECK(ReadNode(reader, node, XamlNodeKind::EndObject));
    CHECK(node.Name().LocalName() == StringView("Window"));
    CHECK(node.Name().NamespaceUri() == StringView("urn:aero"));

    CHECK(ReadNode(reader, node, XamlNodeKind::EndOfDocument));
    CHECK(ReadNode(reader, node, XamlNodeKind::EndOfDocument));
    CHECK(diagnostics.Size() == 0U);
    return true;
}

bool TestNamespaceOverrideScope() {
    const StringView input(
        "<Root xmlns=\"urn:root\" xmlns:p=\"urn:outer\">"
        "<p:Child xmlns:p=\"urn:inner\" p:Value=\"1\"/>"
        "</Root>");

    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(input));
    XamlNodeReader reader(tokenizer);
    XamlNode node;

    CHECK(ReadNode(reader, node, XamlNodeKind::NamespaceDeclaration));
    CHECK(ReadNode(reader, node, XamlNodeKind::NamespaceDeclaration));
    CHECK(ReadNode(reader, node, XamlNodeKind::StartObject));
    CHECK(node.Name().NamespaceUri() == StringView("urn:root"));

    CHECK(ReadNode(reader, node, XamlNodeKind::NamespaceDeclaration));
    CHECK(node.NamespacePrefix() == StringView("p"));
    CHECK(node.NamespaceUri() == StringView("urn:inner"));

    CHECK(ReadNode(reader, node, XamlNodeKind::StartObject));
    CHECK(node.Name().Prefix() == StringView("p"));
    CHECK(node.Name().NamespaceUri() == StringView("urn:inner"));

    CHECK(ReadNode(reader, node, XamlNodeKind::StartMember));
    CHECK(node.Name().NamespaceUri() == StringView("urn:inner"));
    CHECK(ReadNode(reader, node, XamlNodeKind::Value));
    CHECK(ReadNode(reader, node, XamlNodeKind::EndMember));
    CHECK(ReadNode(reader, node, XamlNodeKind::EndObject));
    CHECK(node.Name().NamespaceUri() == StringView("urn:inner"));

    CHECK(ReadNode(reader, node, XamlNodeKind::EndObject));
    CHECK(node.Name().NamespaceUri() == StringView("urn:root"));
    CHECK(ReadNode(reader, node, XamlNodeKind::EndOfDocument));
    return true;
}

bool TestTokenizerFailuresAndLimits() {
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    XmlToken token;

    CHECK(tokenizer.Reset(
        StringView("<!DOCTYPE Window><Window/>"),
        &diagnostics));
    Result<XmlTokenKind> declaration = tokenizer.Read(token);
    CHECK(!declaration);
    CHECK(declaration.GetStatus().code == ErrorCode::Unsupported);
    CHECK(diagnostics.Size() == 1U);
    CHECK(diagnostics.Items()[0].Code() ==
        XmlDiagnosticCodes::UnsupportedDeclaration);

    diagnostics.Clear();
    CHECK(tokenizer.Reset(StringView("<A><B></A>"), &diagnostics));
    CHECK(ReadToken(tokenizer, token, XmlTokenKind::StartElement));
    CHECK(ReadToken(tokenizer, token, XmlTokenKind::StartElement));
    Result<XmlTokenKind> mismatch = tokenizer.Read(token);
    CHECK(!mismatch);
    CHECK(mismatch.GetStatus().code == ErrorCode::ValidationFailed);
    CHECK(diagnostics.Size() == 1U);
    CHECK(diagnostics.Items()[0].Code() ==
        XmlDiagnosticCodes::MismatchedEndElement);

    XmlTokenizerLimits limits;
    limits.maxDepth = 1U;
    DiagnosticBag limitDiagnostics;
    Utf8XmlTokenizer limited(limits);
    CHECK(limited.Reset(StringView("<A><B/></A>"), &limitDiagnostics));
    CHECK(ReadToken(limited, token, XmlTokenKind::StartElement));
    Result<XmlTokenKind> depth = limited.Read(token);
    CHECK(!depth);
    CHECK(depth.GetStatus().code == ErrorCode::OutOfRange);
    CHECK(limitDiagnostics.Items()[0].Code() ==
        XmlDiagnosticCodes::DepthLimitExceeded);
    return true;
}

bool TestNamespaceFailures() {
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView("<p:Window/>"), &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlNode node;

    Result<XamlNodeKind> unbound = reader.Read(node);
    CHECK(!unbound);
    CHECK(unbound.GetStatus().code == ErrorCode::NotFound);
    CHECK(diagnostics.Size() == 1U);
    CHECK(diagnostics.Items()[0].Code() ==
        XamlNodeDiagnosticCodes::UnboundNamespacePrefix);

    diagnostics.Clear();
    CHECK(tokenizer.Reset(
        StringView("<Window xmlns:xml=\"urn:not-xml\"/>"),
        &diagnostics));
    reader.Reset();
    Result<XamlNodeKind> invalidNamespace = reader.Read(node);
    CHECK(!invalidNamespace);
    CHECK(invalidNamespace.GetStatus().code == ErrorCode::ValidationFailed);
    CHECK(diagnostics.Size() == 1U);
    CHECK(diagnostics.Items()[0].Code() ==
        XamlNodeDiagnosticCodes::InvalidNamespaceDeclaration);
    return true;
}

bool TestInvalidUtf8AndOutOfMemory() {
    const char invalidBytes[] = {
        '<', 'A', '>', static_cast<char>(0xC3), '(', '<', '/', 'A', '>'};
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    Result<void> invalid = tokenizer.Reset(
        StringView(invalidBytes, static_cast<std::uint32_t>(sizeof(invalidBytes))),
        &diagnostics);
    CHECK(!invalid);
    CHECK(invalid.GetStatus().code == ErrorCode::InvalidUtf8);
    CHECK(diagnostics.Size() == 1U);
    CHECK(diagnostics.Items()[0].Code() == XmlDiagnosticCodes::InvalidUtf8);

    RejectAllocator reject;
    Aero::Tests::ScopedDefaultAllocator allocatorScope(reject);
    Utf8XmlTokenizer rejecting;
    CHECK(rejecting.Reset(StringView("<ElementNameLongerThanInlineStorage/>")));
    XmlToken token;
    Result<XmlTokenKind> allocation = rejecting.Read(token);
    CHECK(!allocation);
    CHECK(allocation.GetStatus().code == ErrorCode::OutOfMemory);
    return true;
}


Result<void> RegisterCustomTextMetadata(
    MetaRegistrationContext& context,
    void*) noexcept {
    const StringView ns("urn:custom-values");
    return context.types.TryRegisterType({
        ns,
        StringView("CustomLength"),
        InvalidTypeId,
        TypeFlags::ValueType | TypeFlags::Sealed,
        nullptr})
        ? Result<void>()
        : Result<void>(Status::Failure(
            ErrorCode::InternalError,
            "CustomLength registration failed"));
}

Result<XamlValue> ConvertCustomLength(
    TypeId targetType,
    StringView text,
    void*) noexcept {
    if (text != StringView("Auto")) {
        return Status::Failure(
            ErrorCode::ValidationFailed,
            "CustomLength expects Auto");
    }
    return XamlValue::FromDouble(targetType, -1.0);
}

bool TestCustomTextConverter() {
    const StringView ns("urn:custom-values");
    const TypeId customLength = MakeTypeId(ns, StringView("CustomLength"));
    MetadataDomain metadata;
    const StringView moduleName("Tests.CustomText");
    CHECK(metadata.TryRegisterModule({
        MakeMetadataModuleId(moduleName),
        moduleName,
        1U,
        &RegisterCustomTextMetadata,
        nullptr}));
    CHECK(metadata.Seal());
    MetadataRuntime runtime(metadata);
    CHECK(runtime.Freeze());

    XamlSchemaContext schema(metadata, runtime);
    CHECK(schema.TryRegisterTextConverter({
        customLength,
        &ConvertCustomLength,
        nullptr}));
    CHECK(schema.Freeze());

    Result<XamlValue> converted = schema.ConvertText(
        customLength,
        StringView("Auto"));
    CHECK(converted);
    CHECK(converted.Value().Type() == customLength);
    CHECK(converted.Value().Kind() == XamlValueKind::Double);
    CHECK(converted.Value().AsDouble() == -1.0);

    Result<XamlValue> invalid = schema.ConvertText(
        customLength,
        StringView("12"));
    CHECK(!invalid);
    CHECK(invalid.GetStatus().code == ErrorCode::ValidationFailed);
    return true;
}

} // namespace

int main() {
    if (!TestTokenizerSequenceAndEntities()) return 1;
    if (!TestSourceSpans()) return 1;
    if (!TestXmlLineEndingNormalization()) return 1;
    if (!TestXamlNodeStreamNamespacesAndSelfClosing()) return 1;
    if (!TestNamespaceOverrideScope()) return 1;
    if (!TestTokenizerFailuresAndLimits()) return 1;
    if (!TestNamespaceFailures()) return 1;
    if (!TestInvalidUtf8AndOutOfMemory()) return 1;
    if (!TestCustomTextConverter()) return 1;
    std::puts("Aero markup tests passed");
    return 0;
}
