#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Stream.hpp>
#include <Aero/Markup/XamlDocument.hpp>
#include <Aero/Markup/ServiceProvider.hpp>

#include <cstdint>
#include <type_traits>

namespace Aero::Markup {
struct XmlTokenizerLimits {
    std::uint64_t maxInputBytes =
        16ULL * 1024ULL * 1024ULL;
    std::uint32_t maxDepth = 256U;
    std::uint32_t maxAttributesPerElement = 256U;
    std::uint32_t maxNameBytes = 1024U;
    std::uint32_t maxTextBytes = 1024U * 1024U;
};


struct CompiledDocumentLimits {
    std::uint32_t maxNodes = 100000U;
    std::uint32_t maxStringBytes = 16U * 1024U * 1024U;
    std::uint32_t maxDependencies = 4096U;
};


} // namespace Aero::Markup

namespace Aero::Markup {

struct XamlLoadPolicy {
    bool allowNetwork = false;
    bool allowFile = true;
    bool allowPackApplication = true;
};

struct XamlLoadLimits {
    XmlTokenizerLimits xml;
    CompiledDocumentLimits compiled;
    std::uint64_t maxSourceBytes =
        16ULL * 1024ULL * 1024ULL;
    std::uint32_t maxObjects = 100000U;
    std::uint32_t maxResources = 100000U;
    std::uint32_t maxDependencyDepth = 64U;
};

struct XamlReaderSettings {
    XamlLoadPolicy policy;
    XamlLoadLimits limits;
};

} // namespace Aero::Markup

namespace Aero {
class ResourceDictionary;
class Gui;
class View;
namespace Controls { class ContentControl; }
namespace Diagnostics { class IDiagnosticSink; }
namespace Markup { class XamlProvider; }
}

namespace Aero::Markup {

// XAML object-graph reader bound to the process-level Gui runtime. Loading is
// independent of any View; presentation-affine effects are bound when a
// document is mounted into a View.
class AERO_GUI_API XamlReader {
public:
    explicit XamlReader(Aero::Gui& gui) noexcept : gui_(&gui) {}

    Result<XamlDocument> Load(
        StringView uri,
        const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    // Resolves StaticResource references against an application/resource
    // environment while constructing the otherwise View-independent graph.
    Result<XamlDocument> Load(
        StringView uri,
        ResourceDictionary& resources,
        const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    template<class T>
    Result<XamlDocument> LoadComponent(
        StringView uri,
        const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept {
        static_assert(std::is_base_of_v<Base::Object, T>,
            "XamlReader::LoadComponent<T> requires an Aero object type");
        return LoadComponentCore(
            uri, Meta::TypeOf<T>(), nullptr, settings, diagnostics);
    }
    template<class T>
    Result<XamlDocument> LoadComponent(
        StringView uri,
        ResourceDictionary& resources,
        const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept {
        static_assert(std::is_base_of_v<Base::Object, T>,
            "XamlReader::LoadComponent<T> requires an Aero object type");
        return LoadComponentCore(
            uri, Meta::TypeOf<T>(), &resources, settings, diagnostics);
    }
    // Populates an already constructed code-behind root. The returned
    // document retains exactly this object; the root factory is not invoked.
    Result<XamlDocument> LoadComponentInto(
        Ref<Base::Object> existingRoot,
        StringView uri,
        const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr,
        ResourceDictionary* resources = nullptr) noexcept;
    Result<XamlDocument> Parse(
        StringView source,
        const Base::ResourceUri& baseUri = {},
        const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Result<XamlDocument> Load(
        Base::Stream& source,
        const Base::ResourceUri& baseUri = {},
        const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Result<XamlDocument> LoadCompiled(
        Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri = {}) noexcept;
    // Mounts an independently loaded XAML document beneath a ContentControl
    // that already belongs to a View. The View owns the mounted document until
    // the host is unmounted or replaced, keeping effects, bindings and names
    // alive for the complete fragment lifetime.
    Result<void> MountFragment(
        Aero::View& view,
        Aero::Controls::ContentControl& host,
        XamlDocument&& document) noexcept;
    Result<void> UnmountFragment(
        Aero::View& view,
        Aero::Controls::ContentControl& host) noexcept;

    Aero::Gui& GetGui() const noexcept { return *gui_; }

private:
    Result<XamlDocument> LoadComponentCore(
        StringView uri,
        Meta::TypeId expectedRoot,
        ResourceDictionary* resources,
        const XamlReaderSettings& settings,
        Diagnostics::IDiagnosticSink* diagnostics) noexcept;
    Aero::Gui* gui_ = nullptr;
};

} // namespace Aero::Markup
