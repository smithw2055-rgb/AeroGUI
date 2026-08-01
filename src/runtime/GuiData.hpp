#pragma once

#include <Aero/View.hpp>
#include "markup/Loader.hpp"
#include "markup/GuiSchema.hpp"

namespace Aero {

struct GUI::Impl final : public Base::Object {
    explicit Impl(Base::IAllocator& value) noexcept
        : allocator(&value), schema(&value), documents(&value) {}

    Base::IAllocator* allocator = nullptr;
    ModuleSet modules;
    GuiSchema schema;
    Markup::DocumentCache documents;
    bool initialized = false;
};

} // namespace Aero
