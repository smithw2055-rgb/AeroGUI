#pragma once

#include <Aero/Integration/RenderDevice.hpp>
#include "render/RenderResources.hpp"
#include "render/RenderTree.hpp"

namespace Aero::Internal {

// One immutable function table connects RenderDevice to a concrete native
// implementation. It avoids a second public object model and virtual hierarchy.
struct RenderDeviceFunctions {
    void (*destroy)(void*) noexcept = nullptr;
    Base::Result<void> (*submit)(
        void*, const Integration::RenderFrame&) noexcept = nullptr;
    Base::Result<void> (*resize)(
        void*, std::uint32_t, std::uint32_t) noexcept = nullptr;
    void (*surfaceLost)(void*) noexcept = nullptr;
    void (*deviceLost)(void*) noexcept = nullptr;
    Base::Result<void> (*restore)(void*) noexcept = nullptr;
    Base::Result<void> (*waitIdle)(void*, std::uint32_t) noexcept = nullptr;
    RenderFrameStatistics (*statistics)(const void*) noexcept = nullptr;
    Aero::Internal::RenderResources (*resources)(void*) noexcept = nullptr;
};

// The public RenderDevice only exposes the host-facing object. Construction
// and backend adoption stay behind this source-side factory friend.
class RenderDeviceFactory {
public:
    static Base::Result<Base::Ref<::Aero::Integration::RenderDevice>> Adopt(
        ::Aero::Integration::RenderDeviceMode mode,
        void* state,
        const RenderDeviceFunctions* functions,
        Base::IAllocator* allocator = nullptr) noexcept;
};

template<class T>
const RenderDeviceFunctions& FunctionsFor() noexcept {
    static const RenderDeviceFunctions functions{
        [](void* state) noexcept { delete static_cast<T*>(state); },
        [](void* state, const Integration::RenderFrame& frame) noexcept {
            return static_cast<T*>(state)->Submit(frame);
        },
        [](void* state, std::uint32_t width, std::uint32_t height) noexcept {
            return static_cast<T*>(state)->Resize(width, height);
        },
        [](void* state) noexcept {
            static_cast<T*>(state)->NotifySurfaceLost();
        },
        [](void* state) noexcept {
            static_cast<T*>(state)->NotifyDeviceLost();
        },
        [](void* state) noexcept {
            return static_cast<T*>(state)->Restore();
        },
        [](void* state, std::uint32_t timeout) noexcept {
            return static_cast<T*>(state)->WaitIdle(timeout);
        },
        [](const void* state) noexcept {
            return static_cast<const T*>(state)->LastFrameStatistics();
        },
        [](void* state) noexcept {
            return static_cast<T*>(state)->Resources();
        }};
    return functions;
}

template<class T>
Base::Result<Base::Ref<::Aero::Integration::RenderDevice>> AdoptRenderDevice(
    ::Aero::Integration::RenderDeviceMode mode,
    T* state,
    Base::IAllocator* allocator = nullptr) noexcept {
    return ::Aero::Internal::AdoptRenderDevice(
        mode,
        static_cast<void*>(state),
        &FunctionsFor<T>(),
        allocator);
}

} // namespace Aero::Internal
