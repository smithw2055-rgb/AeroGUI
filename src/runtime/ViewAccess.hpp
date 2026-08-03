#pragma once

#include <Aero/View.hpp>

#include <utility>

namespace Aero::Runtime::Detail {
struct ViewData;
}

namespace Aero {

struct Renderer::Impl {
    explicit Impl(View& owner, Base::IAllocator& selected) noexcept
        : allocator(&selected), view(&owner) {}

    Base::IAllocator* allocator = nullptr;
    View* view = nullptr;
    Base::Ref<Integration::RenderDevice> device;
    std::uint64_t updatedVersion = 0U;
    std::uint64_t renderedVersion = 0U;
    bool initialized = false;
    bool offscreenReady = false;
};

// Source-only access seam for the opaque View state.  The allocated object
// owns the source-side ViewData pointer; public headers only see Impl*.
struct View::Impl {
    Impl(
        View& owner,
        Base::IAllocator& selected,
        Base::Ref<Base::Object> guiState) noexcept
        : allocator(&selected),
          gui(std::move(guiState)),
          renderer(owner, selected) {}

    Base::IAllocator* allocator = nullptr;
    Base::Ref<Base::Object> gui;
    ::Aero::Runtime::Detail::ViewData* data = nullptr;
    Renderer renderer;

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
    static const ::Aero::Integration::RenderFrame* CurrentFrame(
        const ::Aero::View& view) noexcept;
};

} // namespace Aero
