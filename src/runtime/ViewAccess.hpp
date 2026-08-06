#pragma once

#include <Aero/View.hpp>

#include <utility>

namespace Aero::Runtime::Detail {

// Single source-private gateway for repository-owned XAML, reload and desktop
// hosting code. View remains the owner of its runtime state, while callers no
// longer acquire individual friendship or grow a parallel service-locator API.
class ViewAccess final {
public:
    static bool IsInitialized(const View& view) noexcept {
        return view.IsInitialized();
    }

    static bool IsMounted(const View& view) noexcept {
        return view.IsMounted();
    }

    static Base::Result<Markup::XamlDocument> LoadDocument(
        View& view,
        Base::StringView uri,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr,
        const Markup::XamlReaderSettings* settings = nullptr) noexcept {
        return view.LoadDocument(uri, diagnostics, settings);
    }

    static Base::Result<Markup::XamlDocument> ParseDocument(
        View& view,
        Base::StringView source,
        const Base::ResourceUri& baseUri = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr,
        const Markup::XamlReaderSettings* settings = nullptr) noexcept {
        return view.ParseDocument(source, baseUri, diagnostics, settings);
    }

    static Base::Result<Markup::XamlDocument> ParseStreamDocument(
        View& view,
        Base::Stream& source,
        const Base::ResourceUri& baseUri = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr,
        const Markup::XamlReaderSettings* settings = nullptr) noexcept {
        return view.ParseStreamDocument(
            source, baseUri, diagnostics, settings);
    }

    static Base::Result<Markup::XamlDocument> LoadCompiledDocument(
        View& view,
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri = {}) noexcept {
        return view.LoadCompiledDocument(bytes, originUri);
    }

    static Base::Result<void> MountContent(
        View& view,
        Controls::ContentControl& host,
        Markup::XamlDocument&& document) noexcept {
        return view.MountContent(host, std::move(document));
    }

    static Base::Result<void> UnmountContent(
        View& view,
        Controls::ContentControl& host) noexcept {
        return view.UnmountContent(host);
    }

    static Base::Result<void> LoadResources(
        View& view,
        ResourceLayer layer,
        Base::StringView uri,
        ResourceLoadMode mode = ResourceLoadMode::Replace,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept {
        return view.LoadResources(layer, uri, mode, diagnostics);
    }

    static Base::Result<void> LoadCompiledResources(
        View& view,
        ResourceLayer layer,
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        ResourceLoadMode mode = ResourceLoadMode::Replace) noexcept {
        return view.LoadCompiledResources(layer, bytes, originUri, mode);
    }

    static void SetResourceDictionary(
        View& view,
        ResourceLayer layer,
        ResourceDictionary& dictionary,
        ResourceLoadMode mode = ResourceLoadMode::Replace) noexcept {
        view.SetResourceDictionary(layer, dictionary, mode);
    }

    static Base::Result<void> LoadBuiltInTheme(
        View& view,
        BuiltInTheme theme) noexcept {
        return view.LoadBuiltInTheme(theme);
    }

    static Base::Result<void> Mount(
        View& view,
        Markup::XamlDocument&& document,
        Size availableSize) noexcept {
        return view.Mount(std::move(document), availableSize);
    }

    static Base::Result<void> ReplaceMountedDocument(
        View& view,
        Markup::XamlDocument&& document,
        Size availableSize) noexcept {
        return view.ReplaceMountedDocument(
            std::move(document), availableSize);
    }

    static Base::Result<void> Unmount(View& view) noexcept {
        return view.Unmount();
    }

    static Base::Object* FindNamedObject(
        View& view,
        Base::StringView name,
        Meta::TypeId expectedType = Meta::InvalidTypeId) noexcept {
        return view.FindNamedObject(name, expectedType);
    }

    static std::uint32_t NamedObjectCount(const View& view) noexcept {
        return view.NamedObjectCount();
    }

    static Base::Result<void> QueryReloadSource(
        View& view,
        const Base::ResourceUri& uri,
        std::uint64_t& sourceIdentity,
        std::uint64_t& revision) noexcept {
        return view.QueryReloadSource(uri, sourceIdentity, revision);
    }

    static bool TryGetCachedReloadRevision(
        View& view,
        const Base::ResourceUri& uri,
        std::uint64_t sourceIdentity,
        std::uint64_t& revision) noexcept {
        return view.TryGetCachedReloadRevision(
            uri, sourceIdentity, revision);
    }

    static Base::Result<std::uint32_t> InvalidateReloadDocuments(
        View& view,
        const Base::ResourceUri& uri,
        bool includeDependents) noexcept {
        return view.InvalidateReloadDocuments(uri, includeDependents);
    }

    static bool IsInstanceOf(
        const View& view,
        const Base::Object& object,
        Meta::TypeId baseType) noexcept {
        return view.IsInstanceOf(object, baseType);
    }
};

} // namespace Aero::Runtime::Detail
