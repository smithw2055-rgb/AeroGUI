#include <Aero/Markup/Runtime/XamlLoadSession.hpp>

namespace Aero::Markup {

XamlObjectWriter::XamlObjectWriter(
    XamlSchemaContext& schema,
    Core::IDiagnosticSink* diagnostics) noexcept
    : schema_(&schema),
      diagnostics_(diagnostics) {}

Base::Result<XamlLoadResult> XamlObjectWriter::LoadDocument(
    XamlNodeReader& reader,
    const XamlLoadContext& context) noexcept {
    XamlLoadSession session(*this);
    return session.Load(reader, context);
}

Base::Result<XamlLoadResult> XamlObjectWriter::LoadDocument(
    XamlNodeReader& reader) noexcept {
    XamlLoadSession session(*this);
    return session.Load(reader);
}

Base::Result<XamlLoadResult> XamlObjectWriter::LoadDocument(
    const XamlCompiledDocument& document,
    const XamlLoadContext& context) noexcept {
    XamlLoadSession session(*this);
    return session.Load(document, context);
}

Base::Result<XamlLoadResult> XamlObjectWriter::LoadDocument(
    const XamlCompiledDocument& document) noexcept {
    XamlLoadSession session(*this);
    return session.Load(document);
}

} // namespace Aero::Markup
