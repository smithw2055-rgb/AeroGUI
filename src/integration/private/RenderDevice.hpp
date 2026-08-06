#pragma once

#include <Aero/RenderDevice.hpp>
#include "render/RenderResources.hpp"
#include "render/RenderTree.hpp"

#include <utility>

namespace Aero::Integration::Detail {

enum class RenderDeviceMode : std::uint8_t {
    Headless = 0U,
    Embedded,
    Window
};

enum class BackendHealth : std::uint8_t {
    Ready = 0U,
    SurfaceLost,
    DeviceLost,
    Failed
};

enum class SurfaceHealth : std::uint8_t {
    Ready = 0U,
    Lost,
    Failed,
    Shutdown
};

// One immutable function table connects RenderDevice to a concrete native
// implementation. Backends still own their native surface during this stage,
// while the public API addresses that surface through RenderSurface.
struct RenderDeviceFunctions {
    void (*destroy)(void*) noexcept = nullptr;
    Base::Result<void> (*renderOffscreen)(
        void*, const void*, const Integration::RenderFrame&) noexcept = nullptr;
    Base::Result<void> (*render)(
        void*, const void*, const Integration::RenderFrame&) noexcept = nullptr;
    void (*releaseRenderer)(void*, const void*) noexcept = nullptr;
    Base::Result<void> (*resize)(
        void*, std::uint32_t, std::uint32_t) noexcept = nullptr;
    void (*surfaceLost)(void*) noexcept = nullptr;
    void (*deviceLost)(void*) noexcept = nullptr;
    Base::Result<void> (*restore)(void*) noexcept = nullptr;
    Base::Result<void> (*waitIdle)(void*, std::uint32_t) noexcept = nullptr;
    BackendHealth (*health)(const void*) noexcept = nullptr;
    ::Aero::RenderFrameStatistics (*statistics)(const void*) noexcept = nullptr;
    Aero::Render::Detail::RenderResources (*resources)(void*) noexcept = nullptr;
};

class RenderDeviceFactory {
public:
    static Base::Result<Base::Ref<::Aero::RenderDevice>> Adopt(
        RenderDeviceMode mode,
        void* state,
        const RenderDeviceFunctions* functions,
        Base::IAllocator* allocator = nullptr) noexcept;
};

template<class T>
const RenderDeviceFunctions& FunctionsFor() noexcept {
    static const RenderDeviceFunctions functions{
        [](void* state) noexcept { delete static_cast<T*>(state); },
        [](void* state, const void* renderer,
           const Integration::RenderFrame& frame) noexcept {
            return static_cast<T*>(state)->RenderOffscreen(renderer, frame);
        },
        [](void* state, const void* renderer,
           const Integration::RenderFrame& frame) noexcept {
            return static_cast<T*>(state)->Render(renderer, frame);
        },
        [](void* state, const void* renderer) noexcept {
            static_cast<T*>(state)->ReleaseRenderer(renderer);
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
            return static_cast<const T*>(state)->Health();
        },
        [](const void* state) noexcept {
            return static_cast<const T*>(state)->LastFrameStatistics();
        },
        [](void* state) noexcept {
            return static_cast<T*>(state)->Resources();
        }};
    return functions;
}

Base::Result<Base::Ref<::Aero::RenderDevice>> AdoptRenderDevice(
    RenderDeviceMode mode,
    void* state,
    const RenderDeviceFunctions* functions,
    Base::IAllocator* allocator = nullptr) noexcept;

template<class T>
Base::Result<Base::Ref<::Aero::RenderDevice>> AdoptRenderDevice(
    RenderDeviceMode mode,
    T* state,
    Base::IAllocator* allocator = nullptr) noexcept {
    return AdoptRenderDevice(
        mode,
        static_cast<void*>(state),
        &FunctionsFor<T>(),
        allocator);
}

} // namespace Aero::Integration::Detail

namespace Aero {

struct RenderDevice::Impl {
    explicit Impl(Base::IAllocator& selectedAllocator) noexcept
        : allocator(&selectedAllocator) {}

    Base::IAllocator* allocator = nullptr;
    void* stateData = nullptr;
    const Integration::Detail::RenderDeviceFunctions* functions = nullptr;
    Integration::Detail::RenderDeviceMode mode =
        Integration::Detail::RenderDeviceMode::Headless;
    RenderDeviceState state = RenderDeviceState::Ready;
    Integration::Detail::SurfaceHealth surface =
        Integration::Detail::SurfaceHealth::Ready;
    RenderDeviceStatistics statistics;
    RenderFrameStatistics lastFrameStatistics;

    static const Integration::Detail::RenderDeviceFunctions* Functions(
        const RenderDevice& device) noexcept {
        return device.impl_ != nullptr ? device.impl_->functions : nullptr;
    }

    static void SetBackend(
        RenderDevice& device,
        Integration::Detail::RenderDeviceMode mode,
        void* state,
        const Integration::Detail::RenderDeviceFunctions* functions) noexcept {
        device.impl_->mode = mode;
        device.impl_->stateData = state;
        device.impl_->functions = functions;
        device.impl_->surface = mode == Integration::Detail::RenderDeviceMode::Headless
            ? Integration::Detail::SurfaceHealth::Shutdown
            : Integration::Detail::SurfaceHealth::Ready;
    }

    static Base::Result<Base::Ref<RenderDevice>> Create(
        Integration::Detail::RenderDeviceMode mode,
        void* state,
        const Integration::Detail::RenderDeviceFunctions* functions,
        Base::IAllocator* allocator) noexcept {
        if (state == nullptr || functions == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Render device implementation is required");
        }
        Base::IAllocator& selected = allocator != nullptr
            ? *allocator
            : Base::GetDefaultAllocator();
        Base::Result<Base::Ref<RenderDevice>> made =
            Base::MakeRefWithAllocator<RenderDevice>(
                selected,
                RenderDevice::ConstructionToken{},
                &selected);
        if (!made) {
            functions->destroy(state);
            return made.GetStatus();
        }
        SetBackend(*made.Value(), mode, state, functions);
        return std::move(made).Value();
    }

    static ::Aero::Render::Detail::RenderResources Resources(
        RenderDevice& device) noexcept {
        const auto* functions = Functions(device);
        return device.impl_ != nullptr &&
            device.impl_->stateData != nullptr && functions != nullptr
            ? functions->resources(device.impl_->stateData)
            : ::Aero::Render::Detail::RenderResources{};
    }

    static Base::Result<void> ResizeSurface(
        RenderDevice& device,
        std::uint32_t width,
        std::uint32_t height) noexcept;
    static void NotifySurfaceLost(RenderDevice& device) noexcept;
    static Base::Result<void> RestoreSurface(RenderDevice& device) noexcept;
    static Integration::Detail::SurfaceHealth SurfaceState(
        const RenderDevice& device) noexcept;
    static Base::Status SurfaceStatus(RenderDevice& device) noexcept;

    static Base::Result<void> RenderOffscreen(
        RenderDevice& device,
        const void* rendererToken,
        const Integration::RenderFrame& frame) noexcept {
        return device.RenderOffscreen(rendererToken, frame);
    }

    static Base::Result<void> Render(
        RenderDevice& device,
        const void* rendererToken,
        const Integration::RenderFrame& frame) noexcept {
        return device.Render(rendererToken, frame);
    }

    static void ReleaseRenderer(
        RenderDevice& device,
        const void* rendererToken) noexcept {
        device.ReleaseRenderer(rendererToken);
    }

    static Base::Status FrameStatus(RenderDevice& device) noexcept {
        return device.GetFrameStatus();
    }
};

} // namespace Aero

namespace Aero::Integration::Detail {

Base::Result<Base::Ref<::Aero::RenderDevice>>
CreateHeadlessRenderDevice(
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Integration::Detail
