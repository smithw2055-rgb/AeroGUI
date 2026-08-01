#include <Aero/Markup/XamlReader.hpp>

#include <Aero/View.hpp>

namespace Aero::Markup {

Base::Result<UiDocument> XamlReader::Load(
    Base::StringView uri,
    Core::IDiagnosticSink* diagnostics) noexcept {
    return view_->LoadDocument(uri, diagnostics);
}

Base::Result<UiDocument> XamlReader::Parse(
    Base::StringView source,
    const Base::ResourceUri& baseUri,
    Core::IDiagnosticSink* diagnostics) noexcept {
    return view_->ParseDocument(source, baseUri, diagnostics);
}

Base::Result<UiDocument> XamlReader::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri) noexcept {
    return view_->LoadCompiledDocument(bytes, originUri);
}

Base::Result<void> XamlReader::RegisterSourceProvider(
    Integration::ISourceProvider& provider,
    Base::StringView scheme,
    Base::StringView assembly) noexcept {
    return view_->AddSourceProvider(provider, scheme, assembly);
}

} // namespace Aero::Markup
