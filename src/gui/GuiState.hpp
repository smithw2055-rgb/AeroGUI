#pragma once

// Gui process/module state (type GuiState). Filename matches the type;
// former GuiData.hpp name was misleading — this is not a DTO bag.

#include "gui/meta/MetadataState.hpp"
#include "gui/core/state/ElementTree.hpp"
#include "gui/core/state/LayoutEngine.hpp"
#include "gui/core/state/FreezableState.hpp"
#include "gui/core/state/EffectiveValueEngine.hpp"
#include "gui/core/state/RoutedEvents.hpp"
#include "gui/core/state/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/input/InputState.hpp" 
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include "gui/markup/MarkupState.hpp"
#include "gui/markup/MarkupWriterState.hpp"
#include <Aero/Gui.hpp>
#include <Aero/Markup/XamlProvider.hpp>
#include <Aero/Media/FontProvider.hpp>
#include <Aero/Media/TextureProvider.hpp>
#include <Aero/Threading.hpp>
#include <Aero/Base/Hash.hpp>

#include <cstdint>
#include <utility>

namespace Aero {

struct PendingXamlDocument {
    Markup::LoaderResult document;
    // LoaderResult can own the root through more than its root field. This
    // baseline separates those internal references from caller-held Refs.
    std::uint32_t internalRootReferences = 0U;
};

struct XamlProviderChangeRecord {
    Base::ResourceUri uri;
    std::uint64_t generation = 0U;
};

struct GuiState final : public Base::Object {
    explicit GuiState(Base::IAllocator& value) noexcept
        : allocator(&value),
          schema(&value),
          documents(&value),
          builtinXamlProviders(&value),
          xamlProviders(&builtinXamlProviders, &value),
          subscribedXamlProviders(&value),
          xamlChanges(&value),
          textureChanges(&value),
          pendingDocuments(&value),
          xamlChanged(this, &GuiState::OnXamlChanged),
          textureChanged(this, &GuiState::OnTextureChanged),
          fontChanged(this, &GuiState::OnFontChanged) {}

    ~GuiState() noexcept override {
        for (const Base::Ref<Markup::XamlProvider>& provider :
             subscribedXamlProviders) {
            if (provider) {
                static_cast<void>(provider->RemoveChangedHandler(xamlChanged));
            }
        }
        if (textureProvider) {
            static_cast<void>(textureProvider->RemoveChangedHandler(
                textureChanged));
        }
        if (fontProvider) {
            static_cast<void>(fontProvider->RemoveChangedHandler(fontChanged));
        }
    }

    void OnXamlChanged(const Base::ResourceUri& uri) noexcept {
        if (!dispatcher.CheckAccess()) return;
        if (uri.Empty()) {
            documents.Clear();
        } else {
            static_cast<void>(documents.Invalidate(uri, true));
        }
        XamlProviderChangeRecord record;
        record.uri = uri;
        record.generation = ++xamlChangeGeneration;
        if (!xamlChanges.PushBack(std::move(record))) {
            xamlChanges.Clear();
            xamlChangesLost = true;
        }
    }

    void OnTextureChanged(const Base::ResourceUri& uri) noexcept {
        if (!dispatcher.CheckAccess()) return;
        XamlProviderChangeRecord record;
        record.uri = uri;
        record.generation = ++textureChangeGeneration;
        if (!textureChanges.PushBack(std::move(record))) {
            textureChanges.Clear();
            textureChangesLost = true;
        }
    }

    void OnFontChanged(const Media::FontProviderChange& change) noexcept {
        if (!dispatcher.CheckAccess()) return;
        fontChangedBaseUri = change.baseUri;
        static_cast<void>(fontChangedFamily.Assign(change.familyName));
        ++fontChangeGeneration;
    }

    Base::IAllocator* allocator = nullptr;
    ModuleSet modules;
    ::Aero::Threading::Dispatcher dispatcher;
    GuiSchema schema;
    Markup::DocumentCache documents;
    Markup::XamlProviderRegistry builtinXamlProviders;
    Markup::XamlProviderRegistry xamlProviders;
    Ref<Markup::EmbeddedXamlProvider> embeddedXaml;
    Ref<Markup::FileXamlProvider> fileXaml;
    Base::Vector<Ref<Markup::XamlProvider>> subscribedXamlProviders;
    Base::Vector<XamlProviderChangeRecord> xamlChanges;
    Base::Vector<XamlProviderChangeRecord> textureChanges;
    Base::Vector<PendingXamlDocument> pendingDocuments;

    Base::Result<Markup::XamlDocument> Load(
        const Markup::LoadState* state,
        Base::StringView uri,
        const Markup::XamlReaderSettings& settings,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept {
        Markup::Loader loader(
            schema.Schema(), xamlProviders, diagnostics, allocator, state);
        return loader.Load(uri, settings);
    }

    Base::Result<Markup::XamlDocument> LoadComponentInto(
        const Markup::LoadState* state,
        Base::Object& existingRoot,
        Base::StringView uri,
        const Markup::XamlReaderSettings& settings,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept {
        Markup::Loader loader(
            schema.Schema(), xamlProviders, diagnostics, allocator, state);
        return loader.LoadComponent(existingRoot, uri, settings);
    }

    Base::Result<Markup::XamlDocument> Parse(
        const Markup::LoadState* state,
        Base::StringView source,
        const Base::ResourceUri& baseUri,
        const Markup::XamlReaderSettings& settings,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept {
        Markup::Loader loader(
            schema.Schema(), xamlProviders, diagnostics, allocator, state);
        return loader.Parse(source, baseUri, settings);
    }

    Base::Result<Markup::XamlDocument> Parse(
        const Markup::LoadState* state,
        Base::Stream& source,
        const Base::ResourceUri& baseUri,
        const Markup::XamlReaderSettings& settings,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept {
        Markup::Loader loader(
            schema.Schema(), xamlProviders, diagnostics, allocator, state);
        return loader.Parse(source, baseUri, settings);
    }

    Base::Result<Markup::XamlDocument> LoadCompiled(
        const Markup::LoadState* state,
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        const Markup::XamlReaderSettings& settings) noexcept {
        Markup::Loader loader(
            schema.Schema(), xamlProviders, nullptr, allocator, state);
        return loader.LoadCompiled(bytes, originUri, settings);
    }

    Base::Result<void> QuerySource(
        const Base::ResourceUri& uri,
        std::uint64_t& sourceIdentity,
        std::uint64_t& revision) noexcept {
        if (uri.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML source URI is empty");
        }
        Base::Result<Markup::XamlProviderResolution> resolved =
            xamlProviders.ResolveDetailed(uri);
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
        return documents.GetSourceRevision(
            uri, sourceIdentity, revision);
    }

    Base::Result<std::uint32_t> Invalidate(
        const Base::ResourceUri& uri,
        bool includeDependents) noexcept {
        return documents.Invalidate(uri, includeDependents);
    }

    Ref<Media::TextureProvider> textureProvider;
    Ref<Media::FontProvider> fontProvider;
    Markup::XamlProviderChangedHandler xamlChanged;
    Media::TextureProviderChangedHandler textureChanged;
    Media::FontProviderChangedHandler fontChanged;
    Base::ResourceUri fontChangedBaseUri;
    String fontChangedFamily;
    std::uint64_t xamlChangeGeneration = 0U;
    std::uint64_t textureChangeGeneration = 0U;
    std::uint64_t fontChangeGeneration = 0U;
    bool xamlChangesLost = false;
    bool textureChangesLost = false;
    bool initialized = false;
};

} // namespace Aero

namespace Aero::Data {

// Source-only bridge used by ChangePropertyAction. Dependency-property value
// normalization already has one canonical implementation in the Gui property
// engine; do not duplicate binding conversion rules in View.
inline Base::Result<Meta::PropertyValue> CoerceBindingTargetValue(
    Meta::Registry* metadata,
    const Meta::DependencyProperty& property,
    Meta::PropertyValue value) noexcept {
    return ::Aero::NormalizeValueForProperty(
        metadata, property, std::move(value));
}

} // namespace Aero::Data
