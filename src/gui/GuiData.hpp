#pragma once

#include "gui/metadata/MetadataRuntime.hpp"
#include "gui/property/PropertyRuntime.hpp"
#include "gui/base/FreezableRuntime.hpp"
#include "gui/base/ElementRuntime.hpp"
#include "gui/base/RoutedEventRuntime.hpp"
#include "gui/input/InputRuntime.hpp"
#include "gui/layout/LayoutRuntime.hpp"
#include "gui/binding/BindingRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/resources/StyleRuntime.hpp"
#include "gui/controls/ControlRuntime.hpp"
#include "gui/controls/ItemsRuntime.hpp"
#include "gui/controls/TemplateRuntime.hpp"
#include "gui/markup/MarkupRuntime.hpp"
#include "gui/markup/MarkupWriterRuntime.hpp"
#include "gui/markup/XamlRuntime.hpp"
#include <Aero/Gui.hpp>
#include <Aero/Markup/XamlProvider.hpp>
#include <Aero/Media/FontProvider.hpp>
#include <Aero/Media/TextureProvider.hpp>
#include <Aero/Threading.hpp>

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
          xaml(schema, documents, xamlProviders),
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
    Markup::XamlRuntime xaml;
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
