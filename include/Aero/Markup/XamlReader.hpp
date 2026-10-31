#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Stream.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Meta.hpp>

#include <cstdint>
#include <type_traits>

namespace Aero {
class ResourceDictionary;
class Gui;
class View;
namespace Controls { class ContentControl; }
namespace Diagnostics { class IDiagnosticSink; }
namespace Markup { class XamlProvider; }
} // namespace Aero

namespace Aero::Markup {

struct XamlDocumentState;

// Move-only ownership for one successfully loaded XAML document. The document
// keeps names, resources, dependency URIs, and the declaration/mount plan alive
// independently from a View until it is mounted or discarded.
class AERO_GUI_API XamlDocument {
public:
    XamlDocument() noexcept = default;
    ~XamlDocument() noexcept;

    XamlDocument(XamlDocument&& other) noexcept;
    XamlDocument& operator=(XamlDocument&& other) noexcept;

    XamlDocument(const XamlDocument&) = delete;
    XamlDocument& operator=(const XamlDocument&) = delete;

    bool IsValid() const noexcept;
    const Ref<Base::Object>& Root() const noexcept;
    template<class T> T* Root() noexcept {
        static_assert(std::is_base_of_v<Base::Object, T>, "XamlDocument::Root<T> requires an Aero object type");
        return static_cast<T*>(RootObject(Meta::TypeOf<T>()));
    }
    template<class T> const T* Root() const noexcept { return const_cast<XamlDocument*>(this)->Root<T>(); }
    Base::Object* FindName(StringView name, Meta::TypeId expectedType = Meta::InvalidTypeId) noexcept;
    template<class T> T* FindName(StringView name) noexcept {
        static_assert(std::is_base_of_v<Base::Object, T>, "XamlDocument::FindName<T> requires an Aero object type");
        return static_cast<T*>(FindName(name, Meta::TypeOf<T>()));
    }
    std::uint32_t NamedObjectCount() const noexcept;
    Aero::ResourceDictionary* Resources() noexcept;
    const Aero::ResourceDictionary* Resources() const noexcept;
    const Base::ResourceUri& CanonicalUri() const noexcept;
    Span<const Base::ResourceUri> Dependencies() const noexcept;

private:
    friend struct XamlDocumentState;

    void Reset() noexcept;
    Base::Object* RootObject(Meta::TypeId expectedType) noexcept;

    Base::IAllocator* allocator_ = nullptr;
    void* state_ = nullptr;
};

struct XmlTokenizerLimits {
    std::uint64_t maxInputBytes = 16ULL * 1024ULL * 1024ULL;
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

struct XamlLoadPolicy {
    bool allowNetwork = false;
    bool allowFile = true;
    bool allowPackApplication = true;
};

struct XamlLoadLimits {
    XmlTokenizerLimits xml;
    CompiledDocumentLimits compiled;
    std::uint64_t maxSourceBytes = 16ULL * 1024ULL * 1024ULL;
    std::uint32_t maxObjects = 100000U;
    std::uint32_t maxResources = 100000U;
    std::uint32_t maxDependencyDepth = 64U;
};

struct XamlReaderSettings {
    XamlLoadPolicy policy;
    XamlLoadLimits limits;
};

// XAML object-graph reader bound to the process-level Gui runtime. Loading is
// independent of any View; presentation-affine effects are bound when a
// document is mounted into a View.
class AERO_GUI_API XamlReader {
public:
    explicit XamlReader(Aero::Gui& gui) noexcept : gui_(&gui) {}

    Result<XamlDocument> Load(StringView uri, const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Result<XamlDocument> Load(StringView uri, ResourceDictionary& resources, const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    template<class T> Result<XamlDocument> LoadComponent(StringView uri, const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept {
        static_assert(std::is_base_of_v<Base::Object, T>, "XamlReader::LoadComponent<T> requires an Aero object type");
        return LoadComponentCore(uri, Meta::TypeOf<T>(), nullptr, settings, diagnostics);
    }
    template<class T> Result<XamlDocument> LoadComponent(StringView uri,
        ResourceDictionary& resources, const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept {
        static_assert(std::is_base_of_v<Base::Object, T>, "XamlReader::LoadComponent<T> requires an Aero object type");
        return LoadComponentCore(uri, Meta::TypeOf<T>(), &resources, settings, diagnostics);
    }
    Result<XamlDocument> LoadComponentInto(Ref<Base::Object> existingRoot,
        StringView uri, const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr,
        ResourceDictionary* resources = nullptr) noexcept;
    Result<XamlDocument> Parse(StringView source,
        const Base::ResourceUri& baseUri = {}, const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Result<XamlDocument> Load(Base::Stream& source,
        const Base::ResourceUri& baseUri = {}, const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Result<XamlDocument> LoadCompiled(Span<const std::uint8_t> bytes, const Base::ResourceUri& originUri = {}) noexcept;
    Result<void> MountFragment(Aero::View& view, Aero::Controls::ContentControl& host,
        XamlDocument&& document) noexcept;
    Result<void> UnmountFragment(Aero::View& view, Aero::Controls::ContentControl& host) noexcept;

    Aero::Gui& GetGui() const noexcept { return *gui_; }

private:
    Result<XamlDocument> LoadComponentCore(StringView uri, Meta::TypeId expectedRoot,
        ResourceDictionary* resources, const XamlReaderSettings& settings,
        Diagnostics::IDiagnosticSink* diagnostics) noexcept;
    Aero::Gui* gui_ = nullptr;
};

struct ReloadResult {
    bool changed = false;
    bool reloaded = false;
    std::uint32_t invalidatedDocuments = 0U;
    std::uint64_t generation = 0U;
    Base::ResourceUri changedUri;
};

// Development-time full-document reload coordinator. It polls source-provider
// revisions, invalidates the shared document cache through the reverse
// dependency graph, loads a replacement XamlDocument off to the side, and swaps
// it into a mounted View only after the replacement is valid. All calls must
// occur on the View owner thread.
class AERO_GUI_API ReloadCoordinator {
public:
    explicit ReloadCoordinator(View& view, Base::IAllocator* allocator = nullptr) noexcept;
    ~ReloadCoordinator() noexcept;

    ReloadCoordinator(ReloadCoordinator&& other) noexcept;
    ReloadCoordinator& operator=(ReloadCoordinator&& other) noexcept;

    ReloadCoordinator(const ReloadCoordinator&) = delete;
    ReloadCoordinator& operator=(const ReloadCoordinator&) = delete;

    Result<void> Start(StringView rootUri, Aero::Base::Size availableSize,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    void Stop() noexcept;

    Result<ReloadResult> Poll(Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Result<ReloadResult> Reload(Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Result<ReloadResult> NotifySourceChanged(const Base::ResourceUri& changedUri,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;

    bool IsActive() const noexcept;
    const Base::ResourceUri& RootUri() const noexcept;
    std::uint64_t Generation() const noexcept;
    std::uint32_t TrackedSourceCount() const noexcept;

private:
    Base::IAllocator* allocator_ = nullptr;
    void* state_ = nullptr;
};

} // namespace Aero::Markup
