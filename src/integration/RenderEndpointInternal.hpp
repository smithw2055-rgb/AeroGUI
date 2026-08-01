#pragma once

#include <Aero/Integration/RenderEndpoint.hpp>
#include "render/RenderTree.hpp"
#include "runtime/ImageResourceContract.hpp"
#include "runtime/MeshResourceContract.hpp"
#include "runtime/TextResourceContract.hpp"

namespace Aero::Integration::Detail {

class EndpointBackend {
public:
    virtual ~EndpointBackend() = default;

    virtual Base::Result<void> Submit(const Render::RenderFrame& frame) noexcept = 0;
    virtual Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept = 0;
    virtual void NotifySurfaceLost() noexcept = 0;
    virtual void NotifyDeviceLost() noexcept = 0;
    virtual Base::Result<void> Restore() noexcept = 0;
    virtual Base::Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds) noexcept = 0;
    virtual RenderFrameStatistics
        LastFrameStatistics() const noexcept {
        return {};
    }
    virtual void SetBatchingEnabled(
        bool) noexcept {}
    virtual Aero::Detail::TextBackendServices* TextServices() noexcept { return nullptr; }
    virtual Aero::Detail::MeshBackendServices* MeshServices() noexcept { return nullptr; }
    virtual Aero::Detail::ImageBackendServices* ImageServices() noexcept { return nullptr; }
};

class RenderEndpointAccess final {
public:
    static Base::Result<Base::Ref<RenderEndpoint>> Create(
        RenderEndpointMode mode,
        EndpointBackend* backend,
        Base::IAllocator* allocator = nullptr) noexcept;
    static Base::Result<Base::Ref<RenderEndpoint>> CreateHeadless(
        Base::IAllocator* allocator = nullptr) noexcept;

    static Base::Result<void> Bind(
        RenderEndpoint& endpoint,
        const void* owner) noexcept;
    static void Unbind(
        RenderEndpoint& endpoint,
        const void* owner) noexcept;
    static Base::Result<void> Submit(
        RenderEndpoint& endpoint,
        const Render::RenderFrame& plan) noexcept;
    static Base::Status FrameStatus(
        RenderEndpoint& endpoint) noexcept;
    static Aero::Detail::TextBackendServices* TextServices(RenderEndpoint& endpoint) noexcept;
    static Aero::Detail::MeshBackendServices* MeshServices(RenderEndpoint& endpoint) noexcept;
    static Aero::Detail::ImageBackendServices* ImageServices(RenderEndpoint& endpoint) noexcept;
};

} // namespace Aero::Integration::Detail
