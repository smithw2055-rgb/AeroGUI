#pragma once

#include <Aero/Integration/RenderSurface.hpp>
#include "integration/private/RenderDevice.hpp"

namespace Aero::Integration::Detail {

// Independently owned native surface contract. destroy is null only for the
// remaining borrowed single-window OpenGL migration path.
struct RenderSurfaceFunctions {
    void (*destroy)(void*) noexcept = nullptr;
    Base::Result<void> (*render)(
        void*, const void*, const Integration::RenderFrame&) noexcept = nullptr;
    Base::Result<void> (*resize)(
        void*, std::uint32_t, std::uint32_t) noexcept = nullptr;
    void (*surfaceLost)(void*) noexcept = nullptr;
    Base::Result<void> (*restoreSurface)(void*) noexcept = nullptr;
    SurfaceHealth (*health)(const void*) noexcept = nullptr;
};

template<class T>
const RenderSurfaceFunctions& OwnedSurfaceFunctionsFor() noexcept {
    static const RenderSurfaceFunctions functions{
        [](void* state) noexcept { delete static_cast<T*>(state); },
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

template<class T>
const RenderSurfaceFunctions& SurfaceFunctionsFor() noexcept {
    static const RenderSurfaceFunctions functions{
        nullptr,
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
            functions = Aero::RenderDevice::Impl::DefaultSurfaceFunctions(*device);
            RefreshHealth();
        }
    }

    ~Impl() noexcept { DestroyState(); }

    Base::IAllocator* allocator = nullptr;
    Base::Ref<Aero::RenderDevice> device;
    void* stateData = nullptr;
    const Detail::RenderSurfaceFunctions* functions = nullptr;
    RenderSurfaceKind kind = RenderSurfaceKind::Embedded;
    Detail::SurfaceHealth health = Detail::SurfaceHealth::Shutdown;

    void DestroyState() noexcept {
        if (stateData != nullptr && functions != nullptr &&
            functions->destroy != nullptr) {
            functions->destroy(stateData);
        }
        stateData = nullptr;
        functions = nullptr;
        health = Detail::SurfaceHealth::Shutdown;
    }

    void SetState(
        void* state,
        const Detail::RenderSurfaceFunctions* selectedFunctions) noexcept {
        DestroyState();
        stateData = state;
        functions = selectedFunctions;
        RefreshHealth();
    }

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

    static Base::Result<Base::Ref<RenderSurface>> CreateOwned(
        Base::Ref<Aero::RenderDevice> device,
        void* state,
        const Detail::RenderSurfaceFunctions* functions,
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

Base::Result<Base::Ref<RenderSurface>> AdoptOwnedRenderSurface(
    Base::Ref<Aero::RenderDevice> device,
    void* state,
    const RenderSurfaceFunctions* functions,
    RenderSurfaceKind kind,
    Base::IAllocator* allocator = nullptr) noexcept;

template<class T>
Base::Result<Base::Ref<RenderSurface>> AdoptOwnedRenderSurface(
    Base::Ref<Aero::RenderDevice> device,
    T* state,
    RenderSurfaceKind kind,
    Base::IAllocator* allocator = nullptr) noexcept {
    return AdoptOwnedRenderSurface(
        std::move(device),
        static_cast<void*>(state),
        &OwnedSurfaceFunctionsFor<T>(),
        kind,
        allocator);
}

} // namespace Detail
} // namespace Aero::Integration
