#include <Aero/Markup.hpp>
#include <Aero/View.hpp>

#include <utility>

namespace Aero::Markup {

Base::Result<XamlDocument> XamlReader::Load(
    Base::StringView uri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    return view_->LoadDocument(uri, diagnostics, &settings);
}

Base::Result<XamlDocument> XamlReader::Load(
    Base::Stream& source,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    return view_->ParseStreamDocument(
        source, baseUri, diagnostics, &settings);
}

Base::Result<XamlDocument> XamlReader::LoadComponentCore(
    Base::StringView uri,
    Meta::TypeId expectedRoot,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    Base::Result<XamlDocument> loaded = Load(
        uri, settings, diagnostics);
    if (!loaded) return loaded.GetStatus();
    const Base::Ref<Base::Object>& root = loaded.Value().Root();
    if (!root || expectedRoot == Meta::InvalidTypeId ||
        !view_->IsInstanceOf(*root, expectedRoot)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML component root is incompatible with the requested type");
    }
    return std::move(loaded).Value();
}

Base::Result<XamlDocument> XamlReader::Parse(
    Base::StringView source,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    return view_->ParseDocument(source, baseUri, diagnostics, &settings);
}

Base::Result<XamlDocument> XamlReader::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri) noexcept {
    return view_->LoadCompiledDocument(bytes, originUri);
}

Base::Result<void> XamlReader::Mount(
    Controls::ContentControl& host,
    XamlDocument&& document) noexcept {
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
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
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

void XamlReader::SetResources(
    ResourceLayer layer,
    Aero::ResourceDictionary& dictionary,
    ResourceLoadMode mode) noexcept {
    view_->SetResourceDictionary(layer, dictionary, mode);
}

Base::Result<void> XamlReader::LoadTheme(
    BuiltInTheme theme) noexcept {
    return view_->LoadBuiltInTheme(theme);
}

} // namespace Aero::Markup
