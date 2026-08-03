#include "markup/MarkupPrivate.hpp"
#pragma once

#include <Aero/View.hpp>



namespace Aero {

struct Gui::Impl  : public Base::Object {
    struct XamlRoute {
        explicit XamlRoute(Base::IAllocator& allocator) noexcept
            : scheme(&allocator), assembly(&allocator) {}

        Integration::XamlProvider* provider = nullptr;
        Base::String scheme;
        Base::String assembly;
    };

    explicit Impl(Base::IAllocator& value) noexcept
        : allocator(&value),
          schema(&value),
          documents(&value),
          xamlProviders(&value) {}

    Base::IAllocator* allocator = nullptr;
    ModuleSet modules;
    GuiSchema schema;
    Markup::DocumentCache documents;
    Base::Vector<XamlRoute> xamlProviders;
    Integration::TextureProvider* textureProvider = nullptr;
    Integration::FontProvider* fontProvider = nullptr;
    bool initialized = false;
};

} // namespace Aero
