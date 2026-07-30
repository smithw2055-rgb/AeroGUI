#pragma once

#include <Aero/Integration/RenderEndpoint.hpp>
#include "presentation/RenderingInternal.hpp"

namespace Aero::Integration::Detail {

class EndpointDriver {
public:
    virtual ~EndpointDriver() = default;

    virtual Base::Result<void> Submit(
        const Presentation::RenderPlan& plan) noexcept = 0;
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
    virtual void* QueryInternalService(
        std::uint64_t service) noexcept {
        (void)service;
        return nullptr;
    }
    virtual bool SupportsDedicatedThread() const noexcept {
        return true;
    }
};

class RenderEndpointAccess final {
public:
    static Base::Result<Base::Ref<RenderEndpoint>> Create(
        RenderEndpointMode mode,
        RenderSubmissionMode submissionMode,
        EndpointDriver* driver,
        Base::IAllocator* allocator = nullptr) noexcept;
    static Base::Result<Base::Ref<RenderEndpoint>> CreateHeadless(
        RenderSubmissionMode submissionMode =
            RenderSubmissionMode::Immediate,
        Base::IAllocator* allocator = nullptr) noexcept;

    static Base::Result<void> Bind(
        RenderEndpoint& endpoint,
        const void* owner) noexcept;
    static void Unbind(
        RenderEndpoint& endpoint,
        const void* owner) noexcept;
    static Base::Result<void> Submit(
        RenderEndpoint& endpoint,
        const Presentation::RenderPlan& plan) noexcept;
    static Base::Status FrameStatus(
        RenderEndpoint& endpoint) noexcept;
    static void* QueryInternalService(
        RenderEndpoint& endpoint,
        std::uint64_t service) noexcept;
};

} // namespace Aero::Integration::Detail
