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
#include "markup/XamlRuntime.hpp"
#include "gui/PropertyInternal.hpp"
#include <Aero/Gui.hpp>
#include <Aero/Threading.hpp>

#include <utility>

namespace Aero {

struct Gui::Impl  : public Base::Object {
    explicit Impl(Base::IAllocator& value) noexcept
        : allocator(&value),
          schema(&value),
          documents(&value),
          xamlProviders(&value),
          xaml(schema, documents, xamlProviders) {}

    Base::IAllocator* allocator = nullptr;
    ModuleSet modules;
    ::Aero::Threading::Dispatcher dispatcher;
    GuiSchema schema;
    Markup::DocumentCache documents;
    Markup::XamlProviderRegistry xamlProviders;
    Markup::EmbeddedXamlProvider embeddedXaml;
    Markup::FileXamlProvider fileXaml;
    Markup::Detail::XamlRuntime xaml;
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
    return ::Aero::GuiPrivate::Detail::NormalizeValueForProperty(
        metadata, property, std::move(value));
}

} // namespace Aero::Data
