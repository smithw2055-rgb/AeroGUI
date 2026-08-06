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
    DeviceLost,
    Failed
};

enum class SurfaceHealth : std::uint8_t {
    Ready = 0U,
    Lost,
    Failed,
    Shutdown
};

struct RenderSurfaceFunctions;

// Device-only backend contract. Presentation, resize and surface recovery are
// deliberately absent and live in RenderSurfaceFunctions.
struct RenderDeviceFunctions {
    void (*destroy)(void*) noexcept = nullptr;
    Base::Result<void> (*renderOffscreen)(
        void*, const void*, const Integration::RenderFrame&) noexcept = nullptr;
    void (*releaseRenderer)(void*, const void*) noexcept = nullptr;
    void (*deviceLost)(void*) noexcept = nullptr;
    Base::Result<void> (*restoreDevice)(void*) noexcept = nullptr;
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
        const RenderSurfaceFunctions* surfaceFunctions,
        Base::IAllocator* allocator = nullptr) noexcept;
};

template<class T>
const RenderDeviceFunctions& DeviceFunctionsFor() noexcept {
    static const RenderDeviceFunctions functions{
        [](void* state) noexcept { delete static_cast<T*>(state); },
        [](void* state, const void* renderer,
           const Integration::RenderFrame& frame) noexcept {
            return static_cast<T*>(state)->RenderOffscreen(renderer, frame);
        },
        [](void* state, const void* renderer) noexcept {
            static_cast<T*>(state)->ReleaseRenderer(renderer);
        },
        [](void* state) noexcept {
            static_cast<T*>(state)->NotifyDeviceLost();
        },
        [](void* state) noexcept {
            return static_cast<T*>(state)->RestoreDevice();
        },
        [](void* state, std::uint32_t timeout) noexcept {
            return static_cast<T*>(state)->WaitIdle(timeout);
        },
        [](const void* state) noexcept {
            return static_cast<const T*>(state)->GetDeviceHealth();
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
const RenderSurfaceFunctions& SurfaceFunctionsFor() noexcept;

Base::Result<Base::Ref<::Aero::RenderDevice>> AdoptRenderDevice(
    RenderDeviceMode mode,
    void* state,
    const RenderDeviceFunctions* functions,
    const RenderSurfaceFunctions* surfaceFunctions,
    Base::IAllocator* allocator = nullptr) noexcept;

template<class T>
Base::Result<Base::Ref<::Aero::RenderDevice>> AdoptRenderDevice(
    RenderDeviceMode mode,
    T* state,
    Base::IAllocator* allocator = nullptr) noexcept {
    return AdoptRenderDevice(
        mode,
        static_cast<void*>(state),
        &DeviceFunctionsFor<T>(),
        &SurfaceFunctionsFor<T>(),
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
    const Integration::Detail::RenderSurfaceFunctions* surfaceFunctions = nullptr;
    Integration::Detail::RenderDeviceMode mode =
        Integration::Detail::RenderDeviceMode::Headless;
    RenderDeviceState state = RenderDeviceState::Ready;
    RenderDeviceStatistics statistics;
    RenderFrameStatistics lastFrameStatistics;

    static const Integration::Detail::RenderDeviceFunctions* Functions(
        const RenderDevice& device) noexcept {
        return device.impl_ != nullptr ? device.impl_->functions : nullptr;
    }

    static const Integration::Detail::RenderSurfaceFunctions* SurfaceFunctions(
        const RenderDevice& device) noexcept {
        return device.impl_ != nullptr ? device.impl_->surfaceFunctions : nullptr;
    }

    static void* NativeState(RenderDevice& device) noexcept {
        return device.impl_ != nullptr ? device.impl_->stateData : nullptr;
    }

    static void SetBackend(
        RenderDevice& device,
        Integration::Detail::RenderDeviceMode mode,
        void* state,
        const Integration::Detail::RenderDeviceFunctions* functions,
        const Integration::Detail::RenderSurfaceFunctions* surfaceFunctions) noexcept {
        device.impl_->mode = mode;
        device.impl_->stateData = state;
        device.impl_->functions = functions;
        device.impl_->surfaceFunctions = surfaceFunctions;
    }

    static Base::Result<Base::Ref<RenderDevice>> Create(
        Integration::Detail::RenderDeviceMode mode,
        void* state,
        const Integration::Detail::RenderDeviceFunctions* functions,
        const Integration::Detail::RenderSurfaceFunctions* surfaceFunctions,
        Base::IAllocator* allocator) noexcept {
        if (state == nullptr || functions == nullptr || surfaceFunctions == nullptr) {
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
        SetBackend(*made.Value(), mode, state, functions, surfaceFunctions);
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

    static Base::Result<void> RenderOffscreen(
        RenderDevice& device,
        const void* rendererToken,
        const Integration::RenderFrame& frame) noexcept {
        return device.RenderOffscreen(rendererToken, frame);
    }

    static void ReleaseRenderer(
        RenderDevice& device,
        const void* rendererToken) noexcept {
        device.ReleaseRenderer(rendererToken);
    }

    static Base::Status FrameStatus(RenderDevice& device) noexcept {
        return device.GetFrameStatus();
    }

    static Base::Result<RenderFrameStatistics> BeginSurfaceFrame(
        RenderDevice& device,
        const Integration::RenderFrame& frame) noexcept;
    static void CompleteSurfaceFrame(
        RenderDevice& device,
        const Integration::RenderFrame& frame,
        RenderFrameStatistics& statistics) noexcept;
    static void RecordSurfaceFailure(RenderDevice& device) noexcept;
    static void RefreshHealth(RenderDevice& device) noexcept;
};

} // namespace Aero

namespace Aero::Integration::Detail {

Base::Result<Base::Ref<::Aero::RenderDevice>>
CreateHeadlessRenderDevice(
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Integration::Detail
