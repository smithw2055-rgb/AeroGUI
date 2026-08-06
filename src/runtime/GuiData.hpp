#pragma once

#include "markup/MarkupPrivate.hpp"
#include "markup/XamlRuntime.hpp"
#include <Aero/View.hpp>

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
    GuiSchema schema;
    Markup::DocumentCache documents;
    Markup::XamlProviderRegistry xamlProviders;
    Markup::Detail::XamlRuntime xaml;
    Integration::TextureProvider* textureProvider = nullptr;
    Integration::FontProvider* fontProvider = nullptr;
    bool initialized = false;
};

} // namespace Aero
