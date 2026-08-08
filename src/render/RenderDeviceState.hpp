#pragma once

#include <Aero/Diagnostics/Rendering.hpp>
#include <Aero/Platform/NativeWindow.hpp>
#include <Aero/Render/RenderDevice.hpp>
#include <Aero/Render/RenderTarget.hpp>
#include <Aero/Render/D3D11.hpp>
#include <Aero/Render/OpenGL33.hpp>
#include "render/RenderResources.hpp"
#include "render/RenderBatch.hpp"
#include "render/RenderTree.hpp"

#include <utility>

namespace Aero {
using RenderDeviceStatistics = Diagnostics::RenderDeviceStatistics;
using RenderFrameStatistics = Diagnostics::RenderFrameStatistics;
}

namespace Aero::Render {

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

} // namespace Aero::Render

namespace Aero {

// Source-private backend base. RenderDevice owns exactly one Access; native
// backends derive from this type directly instead of sitting behind a second
// extra native-device lifetime/factory layer.
struct RenderDevice::Access {
    explicit Access(Base::IAllocator& selectedAllocator) noexcept
        : allocator(&selectedAllocator) {}
    virtual ~Access() noexcept = default;

    virtual ::Aero::Render::RenderBackendKind
        Backend() const noexcept = 0;
    virtual Base::Result<::Aero::Graphics::FenceValue> DrawBatch(
        ::Aero::Render::RenderBatch&& batch) noexcept = 0;
    virtual void NotifyDeviceLost() noexcept = 0;
    virtual Base::Result<void> RestoreDevice() noexcept = 0;
    virtual Base::Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds) noexcept = 0;
    virtual ::Aero::Render::BackendHealth
        GetDeviceHealth() const noexcept = 0;

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
    Base::Result<void> DestroyResource(
        ::Aero::Graphics::ResourceHandle handle,
        ::Aero::Graphics::FenceValue retireAfter = 0U) noexcept;
    bool IsAlive(::Aero::Graphics::ResourceHandle handle) const noexcept;
    Base::Result<::Aero::Graphics::FenceValue> SubmitBatch(
        const ::Aero::Render::RenderBatch& commands) noexcept;
    Base::Result<void> UpdateBuffer(
        ::Aero::Graphics::ResourceHandle buffer,
        std::uint64_t destinationOffset,
        Base::Span<const std::uint8_t> data) noexcept {
        return UpdateNativeBuffer(buffer, destinationOffset, data);
    }
    Base::Result<void> UpdateTexture(
        ::Aero::Graphics::ResourceHandle texture,
        const ::Aero::Graphics::TextureRegion& region,
        Base::Span<const std::uint8_t> data) noexcept {
        return UpdateNativeTexture(texture, region, data);
    }
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
        ::Aero::Render::UiPipelineKey key) noexcept = 0;
    virtual Base::Result<void> SubmitNativeBatch(
        const ::Aero::Render::RenderBatch& commands,
        ::Aero::Graphics::ResourceHandle pipeline,
        ::Aero::Graphics::FenceValue signalFence) noexcept = 0;
    virtual Base::Result<void> UpdateNativeBuffer(
        ::Aero::Graphics::ResourceHandle buffer,
        std::uint64_t destinationOffset,
        Base::Span<const std::uint8_t> data) noexcept = 0;
    virtual Base::Result<void> UpdateNativeTexture(
        ::Aero::Graphics::ResourceHandle texture,
        const ::Aero::Graphics::TextureRegion& region,
        Base::Span<const std::uint8_t> data) noexcept = 0;
    virtual ::Aero::Graphics::FenceValue
        NativeLastSubmittedFence() const noexcept = 0;
    virtual ::Aero::Graphics::FenceValue
        NativeCompletedFence() const noexcept = 0;
    virtual bool NativeDeviceLost() const noexcept = 0;

    static Access* BackendState(RenderDevice& device) noexcept {
        return device.impl_;
    }
    static const Access* BackendState(const RenderDevice& device) noexcept {
        return device.impl_;
    }

    static ::Aero::Render::RenderBackendKind Backend(
        const RenderDevice& device) noexcept {
        return device.impl_ != nullptr
            ? device.impl_->Backend()
            : ::Aero::Render::RenderBackendKind::Unknown;
    }

    static Base::Result<Base::Ref<RenderDevice>> Create(
        Access* backend,
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

    static std::uint64_t BackendGeneration(
        const RenderDevice& device) noexcept {
        return device.impl_ != nullptr
            ? device.impl_->BackendGeneration()
            : 0U;
    }

    static Base::Result<::Aero::Graphics::FenceValue> DrawBatch(
        RenderDevice& device,
        ::Aero::Render::RenderBatch&& batch) noexcept {
        if (device.impl_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Render device is not initialized");
        }
        return device.impl_->DrawBatch(std::move(batch));
    }

    static Base::Status FrameStatus(RenderDevice& device) noexcept {
        return device.GetFrameStatus();
    }

    static Base::Result<RenderFrameStatistics> BeginSurfaceFrame(
        RenderDevice& device,
        const ::Aero::Render::RenderFrame& frame) noexcept;
    static void CompleteSurfaceFrame(
        RenderDevice& device,
        const ::Aero::Render::RenderFrame& frame,
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
    Base::Result<::Aero::Graphics::ResourceHandle> CreatePipeline(
        ::Aero::Render::UiPipelineKey key) noexcept;
    Base::Result<::Aero::Graphics::ResourceHandle> ResolvePipeline(
        ::Aero::Render::UiPipelineKey key) noexcept;
    void RollbackResource(
        ::Aero::Graphics::ResourceHandle handle) noexcept;

    ::Aero::Graphics::DeviceCapabilities capabilities_;
    Base::Vector<ResourceSlot> resourceSlots_{allocator};
    Base::Vector<DeferredDestroy> deferred_{allocator};
    static constexpr std::uint32_t UiShaderCount = 6U;
    static constexpr std::uint32_t UiBlendCount = 6U;
    ::Aero::Graphics::ResourceHandle
        uiPipelines_[UiShaderCount * UiBlendCount]{};
    ::Aero::Graphics::FenceValue lastSubmittedFence_ = 0U;
    bool resourcesInitialized_ = false;
    std::uint64_t backendGeneration_ = 0U;
};

} // namespace Aero

namespace Aero::Render {

Base::Result<Base::Ref<Aero::RenderDevice>> AdoptRenderDevice(
    Aero::RenderDevice::Access* backend,
    Base::IAllocator* allocator = nullptr) noexcept;

Base::Result<Base::Ref<Aero::RenderDevice>>
CreateHeadlessRenderDevice(
    Base::IAllocator* allocator = nullptr) noexcept;

struct D3D11EmbeddedTargetOptions {
    ::Aero::Render::D3D11TargetCallback acquireTarget = nullptr;
    void* callbackContext = nullptr;
    bool clearBeforeRender = false;
};

struct OpenGL33EmbeddedTargetOptions {
    ::Aero::Render::OpenGL33TargetCallback acquireTarget = nullptr;
    void* callbackContext = nullptr;
    void* targetContext = nullptr;
    bool clearBeforeRender = false;
};

Base::Result<Base::Ref<Aero::RenderTarget>> CreateD3D11EmbeddedTarget(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11EmbeddedTargetOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderTarget>> CreateOpenGL33EmbeddedTarget(
    Base::Ref<Aero::RenderDevice> device,
    const OpenGL33EmbeddedTargetOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
} // namespace Aero::Render
