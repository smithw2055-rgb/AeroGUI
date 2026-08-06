#pragma once

#include <Aero/RenderDevice.hpp>
#include "render/RenderResources.hpp"
#include "render/RenderTree.hpp"

#include <utility>

namespace Aero::Render::Detail {

enum class RenderBackendKind : std::uint8_t {
    Unknown = 0U,
    Headless,
    D3D11,
    OpenGL33
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

class NativeRenderTarget;

// One real private backend object replaces the former void* plus function-table
// gateway. It owns the backend-neutral Graphics::Device and DeviceRenderer for
// its API implementation; Graphics::Device is no longer a peer lifecycle.
class NativeRenderDevice {
public:
    virtual ~NativeRenderDevice() noexcept = default;

    virtual RenderBackendKind Backend() const noexcept = 0;
    virtual Base::Result<void> RenderOffscreen(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept = 0;
    virtual void ReleaseRenderer(const void* rendererToken) noexcept = 0;
    virtual void NotifyDeviceLost() noexcept = 0;
    virtual Base::Result<void> RestoreDevice() noexcept = 0;
    virtual Base::Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds) noexcept = 0;
    virtual BackendHealth GetDeviceHealth() const noexcept = 0;
    virtual ::Aero::RenderFrameStatistics
        LastFrameStatistics() const noexcept = 0;
    virtual Aero::Render::Detail::RenderResources Resources() noexcept = 0;

    // Only the legacy combined OpenGL window object supplies a borrowed target.
    // Shared-device D3D11 and embedded OpenGL use independently owned targets.
    virtual NativeRenderTarget* DefaultTarget() noexcept { return nullptr; }
};

class RenderDeviceFactory {
public:
    static Base::Result<Base::Ref<Aero::RenderDevice>> Adopt(
        NativeRenderDevice* backend,
        Base::IAllocator* allocator = nullptr) noexcept;
};

Base::Result<Base::Ref<Aero::RenderDevice>> AdoptRenderDevice(
    NativeRenderDevice* backend,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Render::Detail

namespace Aero {

struct RenderDevice::Impl {
    explicit Impl(Base::IAllocator& selectedAllocator) noexcept
        : allocator(&selectedAllocator) {}

    Base::IAllocator* allocator = nullptr;
    ::Aero::Render::Detail::NativeRenderDevice* native = nullptr;
    RenderDeviceState state = RenderDeviceState::Ready;
    RenderDeviceStatistics statistics;
    RenderFrameStatistics lastFrameStatistics;

    static ::Aero::Render::Detail::NativeRenderDevice* NativeBackend(
        RenderDevice& device) noexcept {
        return device.impl_ != nullptr ? device.impl_->native : nullptr;
    }

    static const ::Aero::Render::Detail::NativeRenderDevice* NativeBackend(
        const RenderDevice& device) noexcept {
        return device.impl_ != nullptr ? device.impl_->native : nullptr;
    }

    static ::Aero::Render::Detail::NativeRenderTarget* DefaultTarget(
        RenderDevice& device) noexcept {
        return device.impl_ != nullptr && device.impl_->native != nullptr
            ? device.impl_->native->DefaultTarget()
            : nullptr;
    }

    static ::Aero::Render::Detail::RenderBackendKind Backend(
        const RenderDevice& device) noexcept {
        const auto* native = NativeBackend(device);
        return native != nullptr
            ? native->Backend()
            : ::Aero::Render::Detail::RenderBackendKind::Unknown;
    }

    static Base::Result<Base::Ref<RenderDevice>> Create(
        ::Aero::Render::Detail::NativeRenderDevice* backend,
        Base::IAllocator* allocator) noexcept {
        if (backend == nullptr) {
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
            delete backend;
            return made.GetStatus();
        }
        made.Value()->impl_->native = backend;
        return std::move(made).Value();
    }

    static ::Aero::Render::Detail::RenderResources Resources(
        RenderDevice& device) noexcept {
        auto* native = NativeBackend(device);
        return native != nullptr
            ? native->Resources()
            : ::Aero::Render::Detail::RenderResources{};
    }

    static Base::Result<void> RenderOffscreen(
        RenderDevice& device,
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept {
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
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept;
    static void CompleteSurfaceFrame(
        RenderDevice& device,
        const ::Aero::Render::Detail::RenderFrame& frame,
        RenderFrameStatistics& statistics) noexcept;
    static void RecordSurfaceFailure(RenderDevice& device) noexcept;
    static void RefreshHealth(RenderDevice& device) noexcept;
};

} // namespace Aero

namespace Aero::Render::Detail {

Base::Result<Base::Ref<Aero::RenderDevice>>
CreateHeadlessRenderDevice(
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Render::Detail
