#include <Aero/Markup/XamlReader.hpp>

#include <Aero/View.hpp>

#include <utility>

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

Base::Result<void> XamlReader::Mount(
    Controls::ContentControl& host,
    UiDocument&& document) noexcept {
    return view_->MountContent(host, std::move(document));
}

Base::Result<void> XamlReader::Unmount(
    Controls::ContentControl& host) noexcept {
    return view_->UnmountContent(host);
}

Base::Result<void> XamlReader::LoadResources(
    ResourceLayer layer,
    Base::StringView uri,
    ResourceLoadMode mode,
    Core::IDiagnosticSink* diagnostics) noexcept {
    return view_->LoadResources(layer, uri, mode, diagnostics);
}

Base::Result<void> XamlReader::LoadCompiledResources(
    ResourceLayer layer,
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    ResourceLoadMode mode) noexcept {
    return view_->LoadCompiledResources(
        layer, bytes, originUri, mode);
}

Base::Result<void> XamlReader::SetResources(
    ResourceLayer layer,
    Aero::ResourceDictionary& dictionary,
    ResourceLoadMode mode) noexcept {
    return view_->SetResourceDictionary(layer, dictionary, mode);
}

Base::Result<void> XamlReader::LoadTheme(
    BuiltInTheme theme) noexcept {
    return view_->LoadBuiltInTheme(theme);
}

} // namespace Aero::Markup
