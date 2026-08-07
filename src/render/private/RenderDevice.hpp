#pragma once

#include <Aero/Diagnostics/Rendering.hpp>
#include <Aero/RenderDevice.hpp>
#include <Aero/RenderTarget.hpp>
#include "render/RenderResources.hpp"
#include "render/RenderTree.hpp"

#include <utility>

namespace Aero {
using RenderDeviceStatistics = Diagnostics::RenderDeviceStatistics;
using RenderFrameStatistics = Diagnostics::RenderFrameStatistics;
}

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

} // namespace Aero::Render::Detail

namespace Aero {

// Source-private backend base. RenderDevice owns exactly one Impl; native
// backends derive from this type directly instead of sitting behind a second
// extra native-device lifetime/factory layer.
struct RenderDevice::Impl {
    explicit Impl(Base::IAllocator& selectedAllocator) noexcept
        : allocator(&selectedAllocator) {}
    virtual ~Impl() noexcept = default;

    virtual ::Aero::Render::Detail::RenderBackendKind
        Backend() const noexcept = 0;
    virtual Base::Result<void> RenderOffscreen(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept = 0;
    virtual void ReleaseRenderer(const void* rendererToken) noexcept = 0;
    virtual void NotifyDeviceLost() noexcept = 0;
    virtual Base::Result<void> RestoreDevice() noexcept = 0;
    virtual Base::Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds) noexcept = 0;
    virtual ::Aero::Render::Detail::BackendHealth
        GetDeviceHealth() const noexcept = 0;
    virtual RenderFrameStatistics LastFrameStatistics() const noexcept = 0;
    virtual ::Aero::Render::Detail::RenderResources Resources() noexcept = 0;
    virtual RenderTarget::Impl* DefaultTarget() noexcept { return nullptr; }

    std::uint64_t BackendGeneration() const noexcept {
        return backendGeneration_;
    }

    Base::IAllocator* allocator = nullptr;
    RenderDeviceState state = RenderDeviceState::Ready;
    RenderDeviceStatistics statistics;
    RenderFrameStatistics lastFrameStatistics;

    static Impl* BackendState(RenderDevice& device) noexcept {
        return device.impl_;
    }
    static const Impl* BackendState(const RenderDevice& device) noexcept {
        return device.impl_;
    }

    static RenderTarget::Impl* DefaultTarget(RenderDevice& device) noexcept {
        return device.impl_ != nullptr
            ? device.impl_->DefaultTarget()
            : nullptr;
    }

    static ::Aero::Render::Detail::RenderBackendKind Backend(
        const RenderDevice& device) noexcept {
        return device.impl_ != nullptr
            ? device.impl_->Backend()
            : ::Aero::Render::Detail::RenderBackendKind::Unknown;
    }

    static Base::Result<Base::Ref<RenderDevice>> Create(
        Impl* backend,
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
                backend);
        if (!made) {
            delete backend;
            return made.GetStatus();
        }
        return std::move(made).Value();
    }

    static ::Aero::Render::Detail::RenderResources Resources(
        RenderDevice& device) noexcept {
        return device.impl_ != nullptr
            ? device.impl_->Resources()
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

protected:
    Base::Result<std::uint64_t> AdvanceGeneration() noexcept {
        if (backendGeneration_ == UINT64_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Render device generation space is exhausted");
        }
        return ++backendGeneration_;
    }

private:
    std::uint64_t backendGeneration_ = 0U;
};

} // namespace Aero

namespace Aero::Render::Detail {

Base::Result<Base::Ref<Aero::RenderDevice>> AdoptRenderDevice(
    Aero::RenderDevice::Impl* backend,
    Base::IAllocator* allocator = nullptr) noexcept;

Base::Result<Base::Ref<Aero::RenderDevice>>
CreateHeadlessRenderDevice(
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Render::Detail
