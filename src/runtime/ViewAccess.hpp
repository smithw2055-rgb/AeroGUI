#pragma once

#include <Aero/View.hpp>

namespace Aero::Runtime::Detail {
struct ViewData;
}

namespace Aero {

// Source-only access seam for the opaque View state.  The allocated object
// owns the source-side ViewData pointer; public headers only see Impl*.
struct View::Impl {
    ::Aero::Runtime::Detail::ViewData* data = nullptr;

    ::Aero::Runtime::Detail::ViewData* operator->() noexcept { return data; }
    const ::Aero::Runtime::Detail::ViewData* operator->() const noexcept {
        return data;
    }

    static bool IsInstanceOf(
        const ::Aero::View& view,
        const ::Aero::Base::Object& object,
        ::Aero::Meta::TypeId baseType) noexcept;
    static ::Aero::Base::Result<void> Unmount(
        ::Aero::View& view) noexcept;
};

} // namespace Aero
