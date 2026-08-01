#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Value.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Styling.hpp>

#include <cstdint>
#include <type_traits>

namespace Aero::Markup {
struct XmlTokenizerLimits final {
    std::uint64_t maxInputBytes =
        16ULL * 1024ULL * 1024ULL;
    std::uint32_t maxDepth = 256U;
    std::uint32_t maxAttributesPerElement = 256U;
    std::uint32_t maxNameBytes = 1024U;
    std::uint32_t maxTextBytes = 1024U * 1024U;
};


struct CompiledDocumentLimits final {
    std::uint32_t maxNodes = 100000U;
    std::uint32_t maxStringBytes = 16U * 1024U * 1024U;
    std::uint32_t maxDependencies = 4096U;
};


} // namespace Aero::Markup

namespace Aero::Markup {

namespace Detail {
class LoadOptionsPrivate;
}

struct LoadPolicy final {
    bool allowNetwork = false;
    bool allowFile = true;
    bool allowPackApplication = true;
};

struct LoadLimits final {
    XmlTokenizerLimits xml;
    CompiledDocumentLimits compiled;
    std::uint64_t maxSourceBytes =
        16ULL * 1024ULL * 1024ULL;
    std::uint32_t maxObjects = 100000U;
    std::uint32_t maxResources = 100000U;
    std::uint32_t maxDependencyDepth = 64U;
};

struct LoadOptions final {
    LoadPolicy policy;
    LoadLimits limits;
    Base::ResourceUri baseUri;

private:
    friend class Detail::LoadOptionsPrivate;
    const void* context_ = nullptr;
};

} // namespace Aero::Markup

namespace Aero::Markup {

namespace Detail {
class ResourcePrivate;
}

inline constexpr Base::StringView
LanguageNamespaceUri() noexcept {
    return Base::StringView(
        "http://schemas.microsoft.com/winfx/2006/xaml");
}

class AERO_API NamespaceScope final {
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
    friend class Detail::ResourcePrivate;

    NamespaceScope(
        LookupCallback lookup,
        void* context) noexcept
        : lookup_(lookup), context_(context) {}

    LookupCallback lookup_ = nullptr;
    void* context_ = nullptr;
};

class AERO_API ResourceResolver final {
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
    friend class Detail::ResourcePrivate;

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

namespace Detail {
class XamlDocumentPrivate;
}

// Move-only ownership for one successfully loaded XAML document. The document
// keeps names, resources, dependency URIs, and the declaration/mount plan alive
// independently from a View until it is mounted or discarded.
class AERO_API UiDocument final {
public:
    UiDocument() noexcept = default;
    ~UiDocument() noexcept;

    UiDocument(UiDocument&& other) noexcept;
    UiDocument& operator=(UiDocument&& other) noexcept;

    UiDocument(const UiDocument&) = delete;
    UiDocument& operator=(const UiDocument&) = delete;

    bool IsValid() const noexcept;
    const Base::Ref<Base::Object>& Root() const noexcept;
    template<class T>
    T* Root() noexcept {
        static_assert(std::is_base_of_v<Base::Object, T>,
            "UiDocument::Root<T> requires an Aero object type");
        return static_cast<T*>(RootObject(Core::TypeOf<T>()));
    }
    template<class T>
    const T* Root() const noexcept {
        return const_cast<UiDocument*>(this)->Root<T>();
    }
    Base::Object* FindName(
        Base::StringView name,
        Core::TypeId expectedType = Core::InvalidTypeId) noexcept;
    template<class T>
    T* FindName(Base::StringView name) noexcept {
        static_assert(std::is_base_of_v<Base::Object, T>,
            "UiDocument::FindName<T> requires an Aero object type");
        return static_cast<T*>(FindName(name, Core::TypeOf<T>()));
    }
    std::uint32_t NamedObjectCount() const noexcept;
    Aero::ResourceDictionary* Resources() noexcept;
    const Aero::ResourceDictionary* Resources() const noexcept;
    const Base::ResourceUri& CanonicalUri() const noexcept;
    Base::Span<const Base::ResourceUri> Dependencies() const noexcept;

private:
    friend class Aero::Detail::XamlDocumentPrivate;
    struct Impl;

    void Reset() noexcept;
    Base::Object* RootObject(Core::TypeId expectedType) noexcept;

    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero

namespace Aero {
class ResourceDictionary;
class View;
enum class BuiltInTheme : std::uint8_t;
enum class ResourceLayer : std::uint8_t;
enum class ResourceLoadMode : std::uint8_t;
namespace Core { class IDiagnosticSink; }
namespace Controls { class ContentControl; }
namespace Integration { class ISourceProvider; }
}

namespace Aero::Markup {

// XAML and resource facade bound to a View. A reader uses the View's frozen
// schema, allocator and source providers while keeping loading concerns out of
// the frame/input/render API.
class AERO_API XamlReader final {
public:
    explicit XamlReader(Aero::View& view) noexcept : view_(&view) {}

    Base::Result<UiDocument> Load(
        Base::StringView uri,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    template<class T>
    Base::Result<UiDocument> LoadComponent(
        Base::StringView uri,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept {
        static_assert(std::is_base_of_v<Base::Object, T>,
            "XamlReader::LoadComponent<T> requires an Aero object type");
        return LoadComponentCore(
            uri, Core::TypeOf<T>(), diagnostics);
    }
    Base::Result<UiDocument> Parse(
        Base::StringView source,
        const Base::ResourceUri& baseUri = {},
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<UiDocument> LoadCompiled(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri = {}) noexcept;
    Base::Result<void> RegisterSourceProvider(
        Integration::ISourceProvider& provider,
        Base::StringView scheme = {},
        Base::StringView assembly = {}) noexcept;

    Base::Result<void> Mount(
        Controls::ContentControl& host,
        UiDocument&& document) noexcept;
    Base::Result<void> Unmount(
        Controls::ContentControl& host) noexcept;
    Base::Result<void> LoadResources(
        ResourceLayer layer,
        Base::StringView uri,
        ResourceLoadMode mode,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<void> LoadCompiledResources(
        ResourceLayer layer,
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        ResourceLoadMode mode) noexcept;
    Base::Result<void> SetResources(
        ResourceLayer layer,
        Aero::ResourceDictionary& dictionary,
        ResourceLoadMode mode) noexcept;
    Base::Result<void> LoadTheme(BuiltInTheme theme) noexcept;

    Aero::View& GetView() const noexcept { return *view_; }

private:
    Base::Result<UiDocument> LoadComponentCore(
        Base::StringView uri,
        Core::TypeId expectedRoot,
        Core::IDiagnosticSink* diagnostics) noexcept;
    Aero::View* view_ = nullptr;
};

} // namespace Aero::Markup
