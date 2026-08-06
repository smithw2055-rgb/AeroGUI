#include <Aero/Markup.hpp>
#include <Aero/View.hpp>
#include "runtime/ViewAccess.hpp"

#include <utility>

namespace Aero::Markup {

Base::Result<XamlDocument> XamlReader::Load(
    Base::StringView uri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    return ::Aero::Runtime::Detail::ViewAccess::LoadDocument(
        *view_, uri, diagnostics, &settings);
}

Base::Result<XamlDocument> XamlReader::Load(
    Base::Stream& source,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    return ::Aero::Runtime::Detail::ViewAccess::ParseStreamDocument(
        *view_, source, baseUri, diagnostics, &settings);
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
        !::Aero::Runtime::Detail::ViewAccess::IsInstanceOf(
            *view_, *root, expectedRoot)) {
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
    return ::Aero::Runtime::Detail::ViewAccess::ParseDocument(
        *view_, source, baseUri, diagnostics, &settings);
}

Base::Result<XamlDocument> XamlReader::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri) noexcept {
    return ::Aero::Runtime::Detail::ViewAccess::LoadCompiledDocument(
        *view_, bytes, originUri);
}

Base::Result<void> XamlReader::Mount(
    Controls::ContentControl& host,
    XamlDocument&& document) noexcept {
    return ::Aero::Runtime::Detail::ViewAccess::MountContent(
        *view_, host, std::move(document));
}

Base::Result<void> XamlReader::Unmount(
    Controls::ContentControl& host) noexcept {
    return ::Aero::Runtime::Detail::ViewAccess::UnmountContent(
        *view_, host);
}

Base::Result<void> XamlReader::LoadResources(
    ResourceLayer layer,
    Base::StringView uri,
    ResourceLoadMode mode,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    return ::Aero::Runtime::Detail::ViewAccess::LoadResources(
        *view_, layer, uri, mode, diagnostics);
}

Base::Result<void> XamlReader::LoadCompiledResources(
    ResourceLayer layer,
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    ResourceLoadMode mode) noexcept {
    return ::Aero::Runtime::Detail::ViewAccess::LoadCompiledResources(
        *view_, layer, bytes, originUri, mode);
}

void XamlReader::SetResources(
    ResourceLayer layer,
    Aero::ResourceDictionary& dictionary,
    ResourceLoadMode mode) noexcept {
    ::Aero::Runtime::Detail::ViewAccess::SetResourceDictionary(
        *view_, layer, dictionary, mode);
}

Base::Result<void> XamlReader::LoadTheme(
    BuiltInTheme theme) noexcept {
    return ::Aero::Runtime::Detail::ViewAccess::LoadBuiltInTheme(
        *view_, theme);
}

} // namespace Aero::Markup
