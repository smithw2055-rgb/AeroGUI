#pragma once

// Gui process/module state (type GuiState). Filename matches the type;
// former GuiData.hpp name was misleading — this is not a DTO bag.

#include "gui/meta/MetadataState.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/input/InputState.hpp" 
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
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

    void OnXamlChanged(const Base::ResourceUri& uri) noexcept;
    void OnTextureChanged(const Base::ResourceUri& uri) noexcept;
    void OnFontChanged(const Media::FontProviderChange& change) noexcept;

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
        std::uint64_t& revision) noexcept;

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
