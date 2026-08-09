#pragma once

#include "gui/MetadataRuntime.hpp"
#include "gui/PropertyRuntime.hpp"
#include "gui/FreezableRuntime.hpp"
#include "gui/ElementRuntime.hpp"
#include "gui/RoutedEventRuntime.hpp"
#include "gui/InputRuntime.hpp"
#include "gui/LayoutRuntime.hpp"
#include "gui/BindingRuntime.hpp"
#include "gui/AnimationRuntime.hpp"
#include "gui/StyleRuntime.hpp"
#include "gui/controls/ControlRuntime.hpp"
#include "gui/controls/ItemsRuntime.hpp"
#include "gui/controls/TemplateRuntime.hpp"
#include "gui/markup/MarkupRuntime.hpp"
#include "gui/markup/MarkupWriterRuntime.hpp"
#include "gui/markup/XamlRuntime.hpp"
#include "gui/PropertyRuntime.hpp"
#include <Aero/Gui.hpp>
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

struct GuiState final : public Base::Object {
    explicit GuiState(Base::IAllocator& value) noexcept
        : allocator(&value),
          schema(&value),
          documents(&value),
          xamlProviders(&value),
          pendingDocuments(&value),
          xaml(schema, documents, xamlProviders) {}

    Base::IAllocator* allocator = nullptr;
    ModuleSet modules;
    ::Aero::Threading::Dispatcher dispatcher;
    GuiSchema schema;
    Markup::DocumentCache documents;
    Markup::XamlProviderRegistry xamlProviders;
    Markup::EmbeddedXamlProvider embeddedXaml;
    Markup::FileXamlProvider fileXaml;
    Base::Vector<PendingXamlDocument> pendingDocuments;
    Markup::XamlRuntime xaml;
    Media::TextureProvider* textureProvider = nullptr;
    Text::FontProvider* fontProvider = nullptr;
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
