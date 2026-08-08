#pragma once

#include <Aero/Diagnostics/Rendering.hpp>
#include <Aero/Platform/NativeWindow.hpp>
#include <Aero/RenderDevice.hpp>
#include <Aero/RenderTarget.hpp>
#define AERO_RENDER_BACKEND_IMPLEMENTATION 1
#include <Aero/Render/D3D11.hpp>
#include <Aero/Render/OpenGL33.hpp>
#undef AERO_RENDER_BACKEND_IMPLEMENTATION
#include "render/RenderResources.hpp"
#include "render/RenderBatch.hpp"
#include "render/RenderTree.hpp"
#include "render/WindowRenderContext.hpp"

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
    virtual Base::Result<::Aero::Graphics::FenceValue> DrawBatch(
        ::Aero::Render::Detail::RenderBatch&& batch) noexcept = 0;
    virtual void ReleaseRenderer(const void* rendererToken) noexcept = 0;
    virtual void NotifyDeviceLost() noexcept = 0;
    virtual Base::Result<void> RestoreDevice() noexcept = 0;
    virtual Base::Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds) noexcept = 0;
    virtual ::Aero::Render::Detail::BackendHealth
        GetDeviceHealth() const noexcept = 0;
    virtual RenderFrameStatistics LastFrameStatistics() const noexcept = 0;
    virtual ::Aero::Render::Detail::RenderResources Resources() noexcept = 0;

    // UI resource and command services consumed by Renderer. These live on
    // the real RenderDevice implementation instead of a second generic device
    // object and an abstract backend lifetime.
    Base::Result<void> InitializeResources() noexcept;
    void ShutdownResources() noexcept;
    bool AreResourcesReady() const noexcept;
    const ::Aero::Graphics::DeviceCapabilities& Capabilities() const noexcept {
        return capabilities_;
    }
    ::Aero::Graphics::GraphicsCapabilities GraphicsCapabilities() const noexcept {
        return QueryNativeGraphicsCapabilities();
    }
    Base::Result<::Aero::Graphics::ResourceHandle> CreateBuffer(
        const ::Aero::Graphics::BufferDescriptor& descriptor) noexcept;
    Base::Result<::Aero::Graphics::ResourceHandle> CreateTexture(
        const ::Aero::Graphics::TextureResourceDescriptor& descriptor) noexcept;
    Base::Result<::Aero::Graphics::ResourceHandle> CreateExternalTexture(
        const ::Aero::Graphics::TextureResourceDescriptor& descriptor) noexcept;
    Base::Result<::Aero::Graphics::ResourceHandle> CreateRenderTarget(
        const ::Aero::Graphics::TextureResourceDescriptor& descriptor) noexcept;
    Base::Result<::Aero::Graphics::ResourceHandle> CreateExternalRenderTarget(
        const ::Aero::Graphics::TextureResourceDescriptor& descriptor) noexcept;
    Base::Result<::Aero::Graphics::ResourceHandle> CreateSampler(
        const ::Aero::Graphics::SamplerDescriptor& descriptor) noexcept;
    Base::Result<::Aero::Graphics::ResourceHandle> CreatePipeline(
        const ::Aero::Graphics::PipelineDescriptor& descriptor) noexcept;
    Base::Result<void> DestroyResource(
        ::Aero::Graphics::ResourceHandle handle,
        ::Aero::Graphics::FenceValue retireAfter = 0U) noexcept;
    bool IsAlive(::Aero::Graphics::ResourceHandle handle) const noexcept;
    Base::Result<::Aero::Graphics::FenceValue> SubmitCommands(
        const ::Aero::Graphics::CommandList& commands) noexcept;
    Base::Result<std::uint32_t> CollectGarbage() noexcept;
    ::Aero::Graphics::FenceValue LastSubmittedFence() const noexcept {
        return lastSubmittedFence_;
    }
    std::uint32_t LiveResourceCount() const noexcept;
    std::uint32_t PendingDestroyCount() const noexcept {
        return deferred_.Size();
    }
    bool IsNativeDeviceLost() const noexcept {
        return NativeDeviceLost();
    }

    std::uint64_t BackendGeneration() const noexcept {
        return backendGeneration_;
    }

    Base::IAllocator* allocator = nullptr;
    RenderDeviceState state = RenderDeviceState::Ready;
    RenderDeviceStatistics statistics;
    RenderFrameStatistics lastFrameStatistics;

    virtual ::Aero::Graphics::DeviceCapabilities
        QueryNativeDeviceCapabilities() const noexcept = 0;
    virtual ::Aero::Graphics::NativeRenderBackendKind
        NativeBackendKind() const noexcept = 0;
    virtual ::Aero::Graphics::GraphicsCapabilities
        QueryNativeGraphicsCapabilities() const noexcept = 0;
    virtual Base::Result<void> CreateNativeResource(
        ::Aero::Graphics::ResourceHandle handle,
        const ::Aero::Graphics::ResourceDescriptor& descriptor) noexcept = 0;
    virtual void DestroyNativeResource(
        ::Aero::Graphics::ResourceHandle handle) noexcept = 0;
    virtual Base::Result<void> ConfigureNativeTexture(
        ::Aero::Graphics::ResourceHandle handle,
        const ::Aero::Graphics::TextureResourceDescriptor& descriptor) noexcept = 0;
    virtual Base::Result<void> ConfigureNativeSampler(
        ::Aero::Graphics::ResourceHandle handle,
        const ::Aero::Graphics::SamplerDescriptor& descriptor) noexcept = 0;
    virtual Base::Result<void> ConfigureNativePipeline(
        ::Aero::Graphics::ResourceHandle handle,
        const ::Aero::Graphics::PipelineDescriptor& descriptor) noexcept = 0;
    virtual Base::Result<void> SubmitNativeCommands(
        const ::Aero::Graphics::CommandList& commands,
        ::Aero::Graphics::FenceValue signalFence) noexcept = 0;
    virtual ::Aero::Graphics::FenceValue
        NativeLastSubmittedFence() const noexcept = 0;
    virtual ::Aero::Graphics::FenceValue
        NativeCompletedFence() const noexcept = 0;
    virtual bool NativeDeviceLost() const noexcept = 0;

    static Impl* BackendState(RenderDevice& device) noexcept {
        return device.impl_;
    }
    static const Impl* BackendState(const RenderDevice& device) noexcept {
        return device.impl_;
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
    struct ResourceSlot {
        ::Aero::Graphics::ResourceDescriptor descriptor;
        std::uint32_t generation = 1U;
        bool alive = false;
    };
    struct DeferredDestroy {
        ::Aero::Graphics::ResourceHandle handle;
        ::Aero::Graphics::FenceValue retireAfter = 0U;
    };

    Base::Result<void> VerifyResourcesReady() const noexcept;
    Base::Result<void> ValidateResourceDescriptor(
        const ::Aero::Graphics::ResourceDescriptor& descriptor) const noexcept;
    Base::Result<::Aero::Graphics::ResourceHandle> CreateResource(
        const ::Aero::Graphics::ResourceDescriptor& descriptor) noexcept;
    void RollbackResource(
        ::Aero::Graphics::ResourceHandle handle) noexcept;

    ::Aero::Graphics::DeviceCapabilities capabilities_;
    Base::Vector<ResourceSlot> resourceSlots_{allocator};
    Base::Vector<DeferredDestroy> deferred_{allocator};
    ::Aero::Graphics::FenceValue lastSubmittedFence_ = 0U;
    bool resourcesInitialized_ = false;
    std::uint64_t backendGeneration_ = 0U;
};

// Compile-time forwarding only: the concrete RenderDevice state and its native
// command queue remain one lifetime. There is no generic backend object.
template<class TCommandQueue>
struct CommandQueueRenderDevice : RenderDevice::Impl {
    explicit CommandQueueRenderDevice(Base::IAllocator& allocator) noexcept
        : RenderDevice::Impl(allocator) {}

    void BindCommandQueue(TCommandQueue* queue) noexcept {
        commandQueue_ = queue;
    }

    ::Aero::Graphics::DeviceCapabilities
    QueryNativeDeviceCapabilities() const noexcept override {
        return commandQueue_ != nullptr
            ? commandQueue_->Capabilities()
            : ::Aero::Graphics::DeviceCapabilities{};
    }
    ::Aero::Graphics::NativeRenderBackendKind
    NativeBackendKind() const noexcept override {
        return commandQueue_ != nullptr
            ? commandQueue_->Kind()
            : ::Aero::Graphics::NativeRenderBackendKind::Invalid;
    }
    ::Aero::Graphics::GraphicsCapabilities
    QueryNativeGraphicsCapabilities() const noexcept override {
        return commandQueue_ != nullptr
            ? commandQueue_->QueryGraphicsCapabilities()
            : ::Aero::Graphics::GraphicsCapabilities{};
    }
    Base::Result<void> CreateNativeResource(
        ::Aero::Graphics::ResourceHandle handle,
        const ::Aero::Graphics::ResourceDescriptor& descriptor) noexcept override {
        return commandQueue_->CreateResource(handle, descriptor);
    }
    void DestroyNativeResource(
        ::Aero::Graphics::ResourceHandle handle) noexcept override {
        if (commandQueue_ != nullptr) commandQueue_->DestroyResource(handle);
    }
    Base::Result<void> ConfigureNativeTexture(
        ::Aero::Graphics::ResourceHandle handle,
        const ::Aero::Graphics::TextureResourceDescriptor& descriptor) noexcept override {
        return commandQueue_->ConfigureTexture(handle, descriptor);
    }
    Base::Result<void> ConfigureNativeSampler(
        ::Aero::Graphics::ResourceHandle handle,
        const ::Aero::Graphics::SamplerDescriptor& descriptor) noexcept override {
        return commandQueue_->ConfigureSampler(handle, descriptor);
    }
    Base::Result<void> ConfigureNativePipeline(
        ::Aero::Graphics::ResourceHandle handle,
        const ::Aero::Graphics::PipelineDescriptor& descriptor) noexcept override {
        return commandQueue_->ConfigurePipeline(handle, descriptor);
    }
    Base::Result<void> SubmitNativeCommands(
        const ::Aero::Graphics::CommandList& commands,
        ::Aero::Graphics::FenceValue signalFence) noexcept override {
        return commandQueue_->Submit(commands, signalFence);
    }
    ::Aero::Graphics::FenceValue
    NativeLastSubmittedFence() const noexcept override {
        return commandQueue_ != nullptr
            ? commandQueue_->LastSubmittedFence() : 0U;
    }
    ::Aero::Graphics::FenceValue
    NativeCompletedFence() const noexcept override {
        return commandQueue_ != nullptr
            ? commandQueue_->CompletedFence() : 0U;
    }
    bool NativeDeviceLost() const noexcept override {
        return commandQueue_ == nullptr || commandQueue_->IsDeviceLost();
    }

private:
    TCommandQueue* commandQueue_ = nullptr;
};

} // namespace Aero

namespace Aero::Render::Detail {

Base::Result<Base::Ref<Aero::RenderDevice>> AdoptRenderDevice(
    Aero::RenderDevice::Impl* backend,
    Base::IAllocator* allocator = nullptr) noexcept;

Base::Result<Base::Ref<Aero::RenderDevice>>
CreateHeadlessRenderDevice(
    Base::IAllocator* allocator = nullptr) noexcept;

struct D3D11EmbeddedTargetOptions {
    ::Aero::Render::D3D11TargetCallback acquireTarget = nullptr;
    void* callbackContext = nullptr;
};

struct D3D11WindowTargetOptions {
    Platform::NativeWindowHandle window;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    ::Aero::Graphics::PresentMode presentMode =
        ::Aero::Graphics::PresentMode::Fifo;
};

struct OpenGL33EmbeddedTargetOptions {
    ::Aero::Render::OpenGL33TargetCallback acquireTarget = nullptr;
    void* callbackContext = nullptr;
    void* targetContext = nullptr;
};

struct OpenGL33WindowTargetOptions {
    Platform::NativeWindowHandle window;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    ::Aero::Graphics::PresentMode presentMode =
        ::Aero::Graphics::PresentMode::Fifo;
    bool enableDebugContext = false;
};

struct WindowRenderPair {
    Base::Ref<Aero::RenderDevice> device;
    Base::Ref<Aero::RenderTarget> target;
};

Base::Result<Base::Ref<Aero::RenderDevice>> CreateD3D11Device(
    const ::Aero::Render::D3D11DeviceOptions& options = {},
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderTarget>> CreateD3D11EmbeddedTarget(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11EmbeddedTargetOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderTarget>> CreateD3D11WindowTarget(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11WindowTargetOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

Base::Result<Base::Ref<Aero::RenderDevice>> CreateOpenGL33Device(
    const ::Aero::Render::OpenGL33DeviceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderTarget>> CreateOpenGL33EmbeddedTarget(
    Base::Ref<Aero::RenderDevice> device,
    const OpenGL33EmbeddedTargetOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<WindowRenderPair> CreateOpenGL33WindowRenderPair(
    const OpenGL33WindowTargetOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Render::Detail
