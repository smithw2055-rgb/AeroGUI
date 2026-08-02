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
#include <Aero/Value.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Styling.hpp>

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

namespace Aero::Internal {
class ResourcePrivate;
}

namespace Aero::Markup {

inline constexpr Base::StringView
LanguageNamespaceUri() noexcept {
    return Base::StringView(
        "http://schemas.microsoft.com/winfx/2006/xaml");
}

class AERO_API NamespaceScope {
public:
    using LookupCallback = Base::Result<Base::StringView> (*)(
        void* context,
        Base::StringView prefix) noexcept;

    NamespaceScope() noexcept = default;

    Base::Result<Base::StringView> Lookup(
        Base::StringView prefix) const noexcept;
    bool IsAvailable() const noexcept {
        return lookup_ != nullptr;
    }

private:
    friend class ::Aero::Internal::ResourcePrivate;

    NamespaceScope(
        LookupCallback lookup,
        void* context) noexcept
        : lookup_(lookup), context_(context) {}

    LookupCallback lookup_ = nullptr;
    void* context_ = nullptr;
};

class AERO_API ResourceResolver {
public:
    using LookupCallback =
        Base::Result<Aero::ResourceValue> (*)(
        void* context,
        Base::StringView key) noexcept;

    ResourceResolver() noexcept = default;

    Base::Result<Aero::ResourceValue> Lookup(
        Base::StringView key) const noexcept;
    bool IsAvailable() const noexcept {
        return lookup_ != nullptr;
    }

private:
    friend class ::Aero::Internal::ResourcePrivate;

    ResourceResolver(
        LookupCallback lookup,
        void* context) noexcept
        : lookup_(lookup), context_(context) {}

    LookupCallback lookup_ = nullptr;
    void* context_ = nullptr;
};

} // namespace Aero::Markup

namespace Aero {

class ResourceDictionary;

namespace Internal {
class XamlDocumentPrivate;
}

// Move-only ownership for one successfully loaded XAML document. The document
// keeps names, resources, dependency URIs, and the declaration/mount plan alive
// independently from a View until it is mounted or discarded.
namespace Markup {

class AERO_API XamlDocument {
public:
    XamlDocument() noexcept = default;
    ~XamlDocument() noexcept;

    XamlDocument(XamlDocument&& other) noexcept;
    XamlDocument& operator=(XamlDocument&& other) noexcept;

    XamlDocument(const XamlDocument&) = delete;
    XamlDocument& operator=(const XamlDocument&) = delete;

    bool IsValid() const noexcept;
    const Base::Ref<Base::Object>& Root() const noexcept;
    template<class T>
    T* Root() noexcept {
        static_assert(std::is_base_of_v<Base::Object, T>,
            "XamlDocument::Root<T> requires an Aero object type");
        return static_cast<T*>(RootObject(Meta::TypeOf<T>()));
    }
    template<class T>
    const T* Root() const noexcept {
        return const_cast<XamlDocument*>(this)->Root<T>();
    }
    Base::Object* FindName(
        Base::StringView name,
        Meta::TypeId expectedType = Meta::InvalidTypeId) noexcept;
    template<class T>
    T* FindName(Base::StringView name) noexcept {
        static_assert(std::is_base_of_v<Base::Object, T>,
            "XamlDocument::FindName<T> requires an Aero object type");
        return static_cast<T*>(FindName(name, Meta::TypeOf<T>()));
    }
    std::uint32_t NamedObjectCount() const noexcept;
    Aero::ResourceDictionary* Resources() noexcept;
    const Aero::ResourceDictionary* Resources() const noexcept;
    const Base::ResourceUri& CanonicalUri() const noexcept;
    Base::Span<const Base::ResourceUri> Dependencies() const noexcept;

private:
    friend class Aero::Internal::XamlDocumentPrivate;
    struct Impl;

    void Reset() noexcept;
    Base::Object* RootObject(Meta::TypeId expectedType) noexcept;

    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Markup

} // namespace Aero

namespace Aero {
class ResourceDictionary;
class View;
enum class BuiltInTheme : std::uint8_t;
enum class ResourceLayer : std::uint8_t;
enum class ResourceLoadMode : std::uint8_t;
namespace Diagnostics { class IDiagnosticSink; }
namespace Controls { class ContentControl; }
namespace Integration { class XamlProvider; }
}

namespace Aero::Markup {

// XAML and resource facade bound to a View. A reader uses the View's frozen
// schema, allocator and source providers while keeping loading concerns out of
// the frame/input/render API.
class AERO_API XamlReader {
public:
    explicit XamlReader(Aero::View& view) noexcept : view_(&view) {}

    Base::Result<XamlDocument> Load(
        Base::StringView uri,
        const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    template<class T>
    Base::Result<XamlDocument> LoadComponent(
        Base::StringView uri,
        const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept {
        static_assert(std::is_base_of_v<Base::Object, T>,
            "XamlReader::LoadComponent<T> requires an Aero object type");
        return LoadComponentCore(
            uri, Meta::TypeOf<T>(), settings, diagnostics);
    }
    Base::Result<XamlDocument> Parse(
        Base::StringView source,
        const Base::ResourceUri& baseUri = {},
        const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<XamlDocument> Load(
        Base::Stream& source,
        const Base::ResourceUri& baseUri = {},
        const XamlReaderSettings& settings = {},
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<XamlDocument> LoadCompiled(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri = {}) noexcept;
    Base::Result<void> RegisterXamlProvider(
        Integration::XamlProvider& provider,
        Base::StringView scheme = {},
        Base::StringView assembly = {}) noexcept;

    Base::Result<void> Mount(
        Controls::ContentControl& host,
        XamlDocument&& document) noexcept;
    Base::Result<void> Unmount(
        Controls::ContentControl& host) noexcept;
    Base::Result<void> LoadResources(
        ResourceLayer layer,
        Base::StringView uri,
        ResourceLoadMode mode,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<void> LoadCompiledResources(
        ResourceLayer layer,
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        ResourceLoadMode mode) noexcept;
    void SetResources(
        ResourceLayer layer,
        Aero::ResourceDictionary& dictionary,
        ResourceLoadMode mode) noexcept;
    Base::Result<void> LoadTheme(BuiltInTheme theme) noexcept;

    Aero::View& GetView() const noexcept { return *view_; }

private:
    Base::Result<XamlDocument> LoadComponentCore(
        Base::StringView uri,
        Meta::TypeId expectedRoot,
        const XamlReaderSettings& settings,
        Diagnostics::IDiagnosticSink* diagnostics) noexcept;
    Aero::View* view_ = nullptr;
};

} // namespace Aero::Markup
