#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Core/Metadata/Activation.hpp>
#include <Aero/Markup/Compiled/XamlCompiledDocument.hpp>
#include <Aero/Markup/Runtime/XamlLoadResult.hpp>
#include <Aero/Markup/Parsing/XmlTokenizer.hpp>

#include <cstdint>

namespace Aero::Presentation {
class BindingManager;
}

namespace Aero::Markup {

class XamlSchemaContext;
class XamlDocumentCache;

namespace XamlLoaderDiagnosticCodes {
inline constexpr Core::DiagnosticCode InvalidUri =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 301U);
inline constexpr Core::DiagnosticCode SourceProviderNotFound =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 302U);
inline constexpr Core::DiagnosticCode SourceLoadFailed =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 303U);
inline constexpr Core::DiagnosticCode SourceRejected =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 304U);
inline constexpr Core::DiagnosticCode RecursiveLoad =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 305U);
inline constexpr Core::DiagnosticCode LoadComponentTypeMismatch =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 306U);
inline constexpr Core::DiagnosticCode ResourceDependencyFailed =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 307U);
} // namespace XamlLoaderDiagnosticCodes

struct XamlSource final {
    Base::ResourceUri uri;
    Base::Vector<std::uint8_t> bytes;
    std::uint64_t revision = 0U;

    Base::StringView Text() const noexcept {
        return Base::StringView(
            reinterpret_cast<const char*>(bytes.Data()),
            bytes.Size());
    }
};

class AERO_API IXamlSourceProvider {
public:
    virtual ~IXamlSourceProvider() = default;

    virtual Base::Result<XamlSource> Load(
        const Base::ResourceUri& uri) const noexcept = 0;
    virtual Base::Result<std::uint64_t> Revision(
        const Base::ResourceUri&) const noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML source provider does not expose revision probes");
    }
};

using XamlSourceLoadCallback = Base::Result<XamlSource> (*)(
    const Base::ResourceUri& uri,
    void* context) noexcept;
using XamlSourceRevisionCallback = Base::Result<std::uint64_t> (*)(
    const Base::ResourceUri& uri,
    void* context) noexcept;

// Data-driven adapter for module-provided XAML source capabilities. Hosts can
// register a callback/context pair without introducing another provider class;
// the existing IXamlSourceProvider boundary remains for source compatibility.
class AERO_API XamlSourceProviderFacet final
    : public IXamlSourceProvider {
public:
    XamlSourceProviderFacet() noexcept = default;
    XamlSourceProviderFacet(
        XamlSourceLoadCallback load,
        void* context = nullptr,
        XamlSourceRevisionCallback revision = nullptr) noexcept
        : load_(load), revision_(revision), context_(context) {}

    bool IsValid() const noexcept {
        return load_ != nullptr;
    }

    Base::Result<XamlSource> Load(
        const Base::ResourceUri& uri) const noexcept override {
        if (load_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML source provider facet has no load callback");
        }
        return load_(uri, context_);
    }
    Base::Result<std::uint64_t> Revision(
        const Base::ResourceUri& uri) const noexcept override {
        return revision_ != nullptr
            ? revision_(uri, context_)
            : IXamlSourceProvider::Revision(uri);
    }

private:
    XamlSourceLoadCallback load_ = nullptr;
    XamlSourceRevisionCallback revision_ = nullptr;
    void* context_ = nullptr;
};

struct XamlSourceProviderRegistration final {
    Base::String scheme;
    Base::String assembly;
    IXamlSourceProvider* provider = nullptr;
};

// Provider selection is deterministic:
// scheme+assembly, scheme, assembly, then default.
class AERO_API XamlSourceProviderRegistry final {
public:
    Base::Result<void> TryRegister(
        IXamlSourceProvider& provider,
        Base::StringView scheme = {},
        Base::StringView assembly = {}) noexcept;
    Base::Result<void> TryRegister(
        XamlSourceProviderFacet& facet,
        Base::StringView scheme = {},
        Base::StringView assembly = {}) noexcept {
        if (!facet.IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML source provider facet is invalid");
        }
        return TryRegister(
            static_cast<IXamlSourceProvider&>(facet),
            scheme,
            assembly);
    }
    Base::Result<IXamlSourceProvider*> Resolve(
        const Base::ResourceUri& uri) const noexcept;

    std::uint32_t ProviderCount() const noexcept {
        return registrations_.Size();
    }

private:
    Base::Vector<XamlSourceProviderRegistration> registrations_;
};

class AERO_API EmbeddedXamlSourceProvider final
    : public IXamlSourceProvider {
public:
    Base::Result<void> TryAdd(
        const Base::ResourceUri& uri,
        Base::Span<const std::uint8_t> bytes,
        std::uint64_t revision = 1U) noexcept;
    Base::Result<void> TryAddText(
        const Base::ResourceUri& uri,
        Base::StringView text,
        std::uint64_t revision = 1U) noexcept;
    Base::Result<void> Freeze() noexcept;

    Base::Result<XamlSource> Load(
        const Base::ResourceUri& uri) const noexcept override;
    Base::Result<std::uint64_t> Revision(
        const Base::ResourceUri& uri) const noexcept override;

    bool IsFrozen() const noexcept {
        return frozen_;
    }
    std::uint32_t SourceCount() const noexcept {
        return entries_.Size();
    }

private:
    struct Entry final {
        Base::ResourceUri uri;
        Base::Vector<std::uint8_t> bytes;
        std::uint64_t revision = 0U;
    };

    Base::Vector<Entry> entries_;
    bool frozen_ = false;
};

class AERO_API FileXamlSourceProvider final
    : public IXamlSourceProvider {
public:
    explicit FileXamlSourceProvider(
        std::uint64_t maxFileBytes =
            64ULL * 1024ULL * 1024ULL) noexcept
        : maxFileBytes_(maxFileBytes) {}

    Base::Result<XamlSource> Load(
        const Base::ResourceUri& uri) const noexcept override;
    Base::Result<std::uint64_t> Revision(
        const Base::ResourceUri& uri) const noexcept override;

private:
    std::uint64_t maxFileBytes_ = 0U;
};

struct XamlLoadPolicy final {
    bool allowNetwork = false;
    bool allowFile = true;
    bool allowPackApplication = true;
};

struct XamlLoadLimits final {
    XmlTokenizerLimits xml;
    XamlCompiledDocumentLimits compiled;
    std::uint64_t maxSourceBytes = 16ULL * 1024ULL * 1024ULL;
    std::uint32_t maxObjects = 100000U;
    std::uint32_t maxResources = 100000U;
    std::uint32_t maxDependencyDepth = 64U;
};

struct XamlLoadOptions final {
    XamlLoadPolicy policy;
    XamlLoadLimits limits;
    Base::ResourceUri baseUri;
    const ResourceDictionary* resources = nullptr;
    Core::EffectiveValueEngine* effectiveValues = nullptr;
    Presentation::BindingManager* bindings = nullptr;
    ResourceDictionary* fallbackResources = nullptr;
    XamlDocumentCache* documentCache = nullptr;
    Core::ActivationProviderRegistry* activationFacets = nullptr;
    const Core::ObjectActivationContext* activation = nullptr;
    Base::Object* templatedParent = nullptr;
};

class AERO_API XamlLoader final {
public:
    XamlLoader(
        XamlSchemaContext& schema,
        XamlSourceProviderRegistry& providers,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;

    Base::Result<XamlLoadResult> Load(
        Base::StringView uri,
        const XamlLoadOptions& options = {}) noexcept;
    Base::Result<XamlLoadResult> Load(
        const Base::ResourceUri& uri,
        const XamlLoadOptions& options = {}) noexcept;
    Base::Result<XamlLoadResult> Parse(
        Base::StringView text,
        const Base::ResourceUri& baseUri,
        const XamlLoadOptions& options = {}) noexcept;
    Base::Result<XamlLoadResult> LoadComponent(
        Base::Object& existingRoot,
        Base::StringView uri,
        const XamlLoadOptions& options = {}) noexcept;
    Base::Result<XamlLoadResult> LoadComponent(
        Base::Object& existingRoot,
        const Base::ResourceUri& uri,
        const XamlLoadOptions& options = {}) noexcept;
    Base::Result<XamlLoadResult> LoadCompiled(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        const XamlLoadOptions& options = {}) noexcept;

private:
    struct Operation;

    XamlSchemaContext* schema_ = nullptr;
    XamlSourceProviderRegistry* providers_ = nullptr;
    Core::IDiagnosticSink* diagnostics_ = nullptr;
};

} // namespace Aero::Markup
