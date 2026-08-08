#pragma once

#include "gui/MetadataInternal.hpp"
#include "gui/PropertyInternal.hpp"
#include "gui/FreezableInternal.hpp"
#include "gui/ElementInternal.hpp"
#include "gui/RoutedEventInternal.hpp"
#include "gui/InputInternal.hpp"
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/AnimationInternal.hpp"
#include "gui/StyleInternal.hpp"
#include "controls/ControlInternal.hpp"
#include "controls/ItemsInternal.hpp"
#include "controls/TemplateInternal.hpp"
#include "markup/MarkupInternal.hpp"
#include "markup/MarkupWriterInternal.hpp"

#include <Aero/Gui/View.hpp>

namespace Aero::Markup::Detail {

// Gui-owned source-private XAML runtime. Schema, provider routing and compiled
// cache lifetime are process-level; LoadState remains supplied by each View so
// object creation, resource effects and name scopes stay presentation-affine.
class XamlRuntime final {
public:
    XamlRuntime(
        ::Aero::GuiSchema& schema,
        DocumentCache& documents,
        XamlProviderRegistry& providers) noexcept
        : schema_(&schema),
          documents_(&documents),
          providers_(&providers) {}

    XamlRuntime(const XamlRuntime&) = delete;
    XamlRuntime& operator=(const XamlRuntime&) = delete;

    ::Aero::GuiSchema& SchemaBundle() const noexcept {
        return *schema_;
    }
    DocumentCache& Documents() const noexcept {
        return *documents_;
    }
    XamlProviderRegistry& Providers() const noexcept {
        return *providers_;
    }

    Base::Result<XamlDocument> Load(
        XamlProviderRegistry& providers,
        const LoadState* state,
        Base::IAllocator* allocator,
        Base::StringView uri,
        const XamlReaderSettings& settings,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept {
        Loader loader(
            schema_->Schema(), providers, diagnostics, allocator, state);
        return loader.Load(uri, settings);
    }

    Base::Result<XamlDocument> LoadComponentInto(
        XamlProviderRegistry& providers,
        const LoadState* state,
        Base::IAllocator* allocator,
        Base::Object& existingRoot,
        Base::StringView uri,
        const XamlReaderSettings& settings,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept {
        Loader loader(
            schema_->Schema(), providers, diagnostics, allocator, state);
        return loader.LoadComponent(existingRoot, uri, settings);
    }

    Base::Result<XamlDocument> Parse(
        XamlProviderRegistry& providers,
        const LoadState* state,
        Base::IAllocator* allocator,
        Base::StringView source,
        const Base::ResourceUri& baseUri,
        const XamlReaderSettings& settings,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept {
        Loader loader(
            schema_->Schema(), providers, diagnostics, allocator, state);
        return loader.Parse(source, baseUri, settings);
    }

    Base::Result<XamlDocument> Parse(
        XamlProviderRegistry& providers,
        const LoadState* state,
        Base::IAllocator* allocator,
        Base::Stream& source,
        const Base::ResourceUri& baseUri,
        const XamlReaderSettings& settings,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept {
        Loader loader(
            schema_->Schema(), providers, diagnostics, allocator, state);
        return loader.Parse(source, baseUri, settings);
    }

    Base::Result<XamlDocument> LoadCompiled(
        XamlProviderRegistry& providers,
        const LoadState* state,
        Base::IAllocator* allocator,
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        const XamlReaderSettings& settings) noexcept {
        Loader loader(
            schema_->Schema(), providers, nullptr, allocator, state);
        return loader.LoadCompiled(bytes, originUri, settings);
    }

    Base::Result<void> QuerySource(
        XamlProviderRegistry& providers,
        const Base::ResourceUri& uri,
        std::uint64_t& sourceIdentity,
        std::uint64_t& revision) noexcept {
        if (uri.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML source URI is empty");
        }
        Base::Result<XamlProviderResolution> resolved =
            providers.ResolveDetailed(uri);
        if (!resolved) return resolved.GetStatus();
        if (resolved.Value().provider == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML source provider is unavailable");
        }
        sourceIdentity = resolved.Value().cacheIdentity;

        Base::Result<std::uint64_t> probed =
            resolved.Value().provider->Revision(uri);
        if (probed && probed.Value() != 0U) {
            revision = probed.Value();
            return {};
        }
        Base::Result<::Aero::Markup::StreamResourceInfo> source =
            resolved.Value().provider->Open(uri);
        if (!source) return source.GetStatus();
        if (source.Value().revision != 0U) {
            revision = source.Value().revision;
            return {};
        }
        if (!source.Value().stream) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML source stream is invalid");
        }

        constexpr Base::HashCode OffsetBasis =
            UINT64_C(14695981039346656037);
        constexpr Base::HashCode Prime = UINT64_C(1099511628211);
        Base::HashCode hash = OffsetBasis ^ Base::MixHash64(0U);
        std::uint64_t size = 0U;
        std::uint8_t buffer[4096];
        for (;;) {
            Base::Result<std::uint32_t> read =
                source.Value().stream->Read({buffer, sizeof(buffer)});
            if (!read) return read.GetStatus();
            if (read.Value() == 0U) break;
            for (std::uint32_t index = 0U;
                 index < read.Value(); ++index) {
                hash ^= static_cast<Base::HashCode>(buffer[index]);
                hash *= Prime;
            }
            size += read.Value();
        }
        revision = Base::MixHash64(hash ^ size);
        return {};
    }

    bool TryGetCachedRevision(
        const Base::ResourceUri& uri,
        std::uint64_t sourceIdentity,
        std::uint64_t& revision) noexcept {
        return documents_ != nullptr &&
            documents_->GetSourceRevision(
                uri, sourceIdentity, revision);
    }

    Base::Result<std::uint32_t> Invalidate(
        const Base::ResourceUri& uri,
        bool includeDependents) noexcept {
        return documents_ != nullptr
            ? documents_->Invalidate(uri, includeDependents)
            : Base::Result<std::uint32_t>(std::uint32_t{0U});
    }

private:
    ::Aero::GuiSchema* schema_ = nullptr;
    DocumentCache* documents_ = nullptr;
    XamlProviderRegistry* providers_ = nullptr;
};

} // namespace Aero::Markup::Detail
