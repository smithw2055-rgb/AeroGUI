#pragma once

#include <Aero/Integration/RenderDevice.hpp>
#include "render/RenderResources.hpp"
#include "render/RenderTree.hpp"

#include <utility>

namespace Aero::Integration::Detail {

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
    Aero::Render::Detail::RenderResources (*resources)(void*) noexcept = nullptr;
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

Base::Result<Base::Ref<::Aero::Integration::RenderDevice>> AdoptRenderDevice(
    ::Aero::Integration::RenderDeviceMode mode,
    void* state,
    const RenderDeviceFunctions* functions,
    Base::IAllocator* allocator = nullptr) noexcept;

template<class T>
Base::Result<Base::Ref<::Aero::Integration::RenderDevice>> AdoptRenderDevice(
    ::Aero::Integration::RenderDeviceMode mode,
    T* state,
    Base::IAllocator* allocator = nullptr) noexcept {
    return ::Aero::Integration::Detail::AdoptRenderDevice(
        mode,
        static_cast<void*>(state),
        &FunctionsFor<T>(),
        allocator);
}

} // namespace Aero::Integration::Detail

namespace Aero::Integration {

// Source-only access to the opaque backend state kept by RenderDevice.
struct RenderDevice::Impl {
    static const ::Aero::Integration::Detail::RenderDeviceFunctions* Functions(
        const RenderDevice& device) noexcept {
        return static_cast<
            const ::Aero::Integration::Detail::RenderDeviceFunctions*>(
                device.functions_);
    }

    static void* StateData(RenderDevice& device) noexcept {
        return device.stateData_;
    }

    static void SetBackend(
        RenderDevice& device,
        void* state,
        const ::Aero::Integration::Detail::RenderDeviceFunctions* functions) noexcept {
        device.stateData_ = state;
        device.functions_ = functions;
    }

    static Base::Result<Base::Ref<RenderDevice>> Create(
        RenderDeviceMode mode,
        void* state,
        const ::Aero::Integration::Detail::RenderDeviceFunctions* functions,
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
                mode,
                &selected);
        if (!made) {
            functions->destroy(state);
            return made.GetStatus();
        }
        SetBackend(*made.Value(), state, functions);
        return std::move(made).Value();
    }

    static ::Aero::Render::Detail::RenderResources Resources(
        RenderDevice& device) noexcept {
        const auto* functions = Functions(device);
        return device.stateData_ != nullptr && functions != nullptr
            ? functions->resources(device.stateData_)
            : ::Aero::Render::Detail::RenderResources{};
    }

    static Base::Result<void> Bind(
        RenderDevice& device,
        const void* owner) noexcept {
        return device.Bind(owner);
    }

    static void Unbind(
        RenderDevice& device,
        const void* owner) noexcept {
        device.Unbind(owner);
    }

    static Base::Result<void> Submit(
        RenderDevice& device,
        const RenderFrame& frame) noexcept {
        return device.Submit(frame);
    }

    static Base::Status FrameStatus(RenderDevice& device) noexcept {
        return device.GetFrameStatus();
    }
};

} // namespace Aero::Integration

namespace Aero::Integration::Detail {

Base::Result<Base::Ref<::Aero::Integration::RenderDevice>>
CreateHeadlessRenderDevice(
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Integration::Detail
