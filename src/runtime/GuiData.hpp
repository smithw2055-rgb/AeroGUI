#include "markup/MarkupInternal.hpp"
#pragma once

#include <Aero/View.hpp>



namespace Aero {

struct Gui::Impl  : public Base::Object {
    explicit Impl(Base::IAllocator& value) noexcept
        : allocator(&value), schema(&value), documents(&value) {}

    Base::IAllocator* allocator = nullptr;
    ModuleSet modules;
    GuiSchema schema;
    Markup::DocumentCache documents;
    bool initialized = false;
};

} // namespace Aero
