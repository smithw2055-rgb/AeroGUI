#pragma once

#include <Aero/Integration/RenderSurface.hpp>
#include "integration/private/RenderDevice.hpp"

namespace Aero::Integration::Detail {

// Surface-only backend contract. The state pointer is borrowed from the device
// during this migration stage; the contract is ready for independently owned
// native surface states in the next stage.
struct RenderSurfaceFunctions {
    Base::Result<void> (*render)(
        void*, const void*, const Integration::RenderFrame&) noexcept = nullptr;
    Base::Result<void> (*resize)(
        void*, std::uint32_t, std::uint32_t) noexcept = nullptr;
    void (*surfaceLost)(void*) noexcept = nullptr;
    Base::Result<void> (*restoreSurface)(void*) noexcept = nullptr;
    SurfaceHealth (*health)(const void*) noexcept = nullptr;
};

template<class T>
const RenderSurfaceFunctions& SurfaceFunctionsFor() noexcept {
    static const RenderSurfaceFunctions functions{
        [](void* state, const void* renderer,
           const Integration::RenderFrame& frame) noexcept {
            return static_cast<T*>(state)->Render(renderer, frame);
        },
        [](void* state, std::uint32_t width, std::uint32_t height) noexcept {
            return static_cast<T*>(state)->Resize(width, height);
        },
        [](void* state) noexcept {
            static_cast<T*>(state)->NotifySurfaceLost();
        },
        [](void* state) noexcept {
            return static_cast<T*>(state)->RestoreSurface();
        },
        [](const void* state) noexcept {
            return static_cast<const T*>(state)->GetSurfaceHealth();
        }};
    return functions;
}

} // namespace Aero::Integration::Detail

namespace Aero::Integration {

struct RenderSurface::Impl {
    Impl(
        Base::Ref<Aero::RenderDevice> selectedDevice,
        RenderSurfaceKind selectedKind,
        Base::IAllocator& selectedAllocator) noexcept
        : allocator(&selectedAllocator),
          device(std::move(selectedDevice)),
          kind(selectedKind) {
        if (device) {
            stateData = Aero::RenderDevice::Impl::NativeState(*device);
            functions = Aero::RenderDevice::Impl::SurfaceFunctions(*device);
            RefreshHealth();
        }
    }

    Base::IAllocator* allocator = nullptr;
    Base::Ref<Aero::RenderDevice> device;
    void* stateData = nullptr;
    const Detail::RenderSurfaceFunctions* functions = nullptr;
    RenderSurfaceKind kind = RenderSurfaceKind::Embedded;
    Detail::SurfaceHealth health = Detail::SurfaceHealth::Shutdown;

    Detail::SurfaceHealth RefreshHealth() noexcept {
        health = stateData != nullptr && functions != nullptr &&
                functions->health != nullptr
            ? functions->health(stateData)
            : Detail::SurfaceHealth::Shutdown;
        return health;
    }

    static Base::Result<Base::Ref<RenderSurface>> Create(
        Base::Ref<Aero::RenderDevice> device,
        RenderSurfaceKind kind,
        Base::IAllocator* allocator = nullptr) noexcept;

    static Base::Result<void> Render(
        RenderSurface& surface,
        const void* rendererToken,
        const Integration::RenderFrame& frame) noexcept;
};

namespace Detail {

Base::Result<Base::Ref<RenderSurface>> AdoptRenderSurface(
    Base::Ref<Aero::RenderDevice> device,
    RenderSurfaceKind kind,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Detail
} // namespace Aero::Integration
