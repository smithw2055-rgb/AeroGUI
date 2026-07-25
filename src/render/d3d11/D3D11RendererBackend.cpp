#include <Aero/Render/D3D11RendererBackend.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <new>

#include "AeroD3D11RenderPlanPixelShader.hpp"
#include "AeroD3D11RenderPlanVertexShader.hpp"
#include "AeroD3D11RenderPlanImagePixelShader.hpp"
#include "AeroD3D11RenderPlanImageVertexShader.hpp"
#include "AeroD3D11RenderPlanMeshPixelShader.hpp"
#include "AeroD3D11RenderPlanMeshVertexShader.hpp"
#include "AeroD3D11RenderPlanGlyphPixelShader.hpp"
#include "AeroD3D11RenderPlanGlyphVertexShader.hpp"

namespace Aero::Render {
namespace {

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotInitialized, message);
}

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

Base::Status OutOfMemory(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfMemory, message);
}

Rhi::ShaderDescriptor Shader(
    Rhi::ShaderStage stage,
    const std::uint8_t* bytecode,
    std::uint32_t bytecodeSize,
    std::uint64_t stableId,
    Base::StringView entryPoint) noexcept {
    Rhi::ShaderDescriptor descriptor;
    descriptor.stage = stage;
    descriptor.language = Rhi::ShaderLanguage::Dxbc;
    descriptor.bytecode = bytecode;
    descriptor.bytecodeSize = bytecodeSize;
    descriptor.entryPoint = entryPoint;
    descriptor.stableId = stableId;
    return descriptor;
}

RendererShaderSet MakeShaderSet() noexcept {
    RendererShaderSet shaders;
    shaders.rectangleVertex = Shader(
        Rhi::ShaderStage::Vertex,
        AeroD3D11RenderPlanVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderPlanVertexShader)),
        UINT64_C(0xD3111001), Base::StringView("vs_main"));
    shaders.rectangleFragment = Shader(
        Rhi::ShaderStage::Fragment,
        AeroD3D11RenderPlanPixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderPlanPixelShader)),
        UINT64_C(0xD3111002), Base::StringView("ps_main"));
    shaders.imageVertex = Shader(
        Rhi::ShaderStage::Vertex,
        AeroD3D11RenderPlanImageVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderPlanImageVertexShader)),
        UINT64_C(0xD3111011), Base::StringView("vs_main"));
    shaders.imageFragment = Shader(
        Rhi::ShaderStage::Fragment,
        AeroD3D11RenderPlanImagePixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderPlanImagePixelShader)),
        UINT64_C(0xD3111012), Base::StringView("ps_main"));
    shaders.meshVertex = Shader(
        Rhi::ShaderStage::Vertex,
        AeroD3D11RenderPlanMeshVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderPlanMeshVertexShader)),
        UINT64_C(0xD3111021), Base::StringView("vs_main"));
    shaders.meshFragment = Shader(
        Rhi::ShaderStage::Fragment,
        AeroD3D11RenderPlanMeshPixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderPlanMeshPixelShader)),
        UINT64_C(0xD3111022), Base::StringView("ps_main"));
    shaders.glyphVertex = Shader(
        Rhi::ShaderStage::Vertex,
        AeroD3D11RenderPlanGlyphVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderPlanGlyphVertexShader)),
        UINT64_C(0xD3111031), Base::StringView("vs_main"));
    shaders.glyphFragment = Shader(
        Rhi::ShaderStage::Fragment,
        AeroD3D11RenderPlanGlyphPixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderPlanGlyphPixelShader)),
        UINT64_C(0xD3111032), Base::StringView("ps_main"));
    shaders.colorFormat = Rhi::GraphicsTextureFormat::Bgra8Unorm;
    return shaders;
}

} // namespace

struct D3D11RenderPlanBackend::Impl final {
    Impl(
        Rhi::RhiDevice& device,
        Base::IAllocator* allocator) noexcept
        : renderer(device, MakeShaderSet(), allocator) {}

    Renderer renderer;
    Rhi::FenceValue lastSubmittedFence = 0U;
    bool initialized = false;
};

D3D11RenderPlanBackend::D3D11RenderPlanBackend(
    Rhi::RhiDevice& device,
    Rhi::D3D11SurfacePresenter& presenter,
    Base::IAllocator* allocator) noexcept
    : device_(&device),
      presenter_(&presenter),
      allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {}

D3D11RenderPlanBackend::~D3D11RenderPlanBackend() noexcept {
    Shutdown();
}

Base::Result<void> D3D11RenderPlanBackend::Initialize() noexcept {
    if (IsInitialized()) {
        return {};
    }
    if (device_ == nullptr || presenter_ == nullptr ||
        device_->Backend().IsDeviceLost()) {
        return NotInitialized(
            "D3D11 render adapter requires a ready device and presenter");
    }
    if (impl_ == nullptr) {
        void* memory = allocator_->Allocate({
            sizeof(Impl), alignof(Impl), Base::MemoryTag::Render});
        if (memory == nullptr) {
            return OutOfMemory(
                "Failed to allocate D3D11 render adapter state");
        }
        impl_ = new (memory) Impl(*device_, allocator_);
    }
    Base::Result<void> initialized = impl_->renderer.Initialize();
    if (!initialized) {
        Shutdown();
        return initialized;
    }
    impl_->initialized = true;
    return {};
}

void D3D11RenderPlanBackend::Shutdown() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    impl_->renderer.Shutdown();
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Render);
    impl_ = nullptr;
}

bool D3D11RenderPlanBackend::IsInitialized() const noexcept {
    return impl_ != nullptr && impl_->initialized &&
        impl_->renderer.IsInitialized();
}

Base::Result<void> D3D11RenderPlanBackend::RegisterImage(
    Presentation::RenderImageId image,
    Rhi::ResourceHandle texture,
    Rhi::ResourceHandle sampler) noexcept {
    return IsInitialized()
        ? impl_->renderer.RegisterImage(image, texture, sampler)
        : Base::Result<void>(NotInitialized(
            "D3D11 render adapter is not initialized"));
}

Base::Result<void> D3D11RenderPlanBackend::UnregisterImage(
    Presentation::RenderImageId image) noexcept {
    return IsInitialized()
        ? impl_->renderer.UnregisterImage(image)
        : Base::Result<void>(NotInitialized(
            "D3D11 render adapter is not initialized"));
}

Base::Result<void> D3D11RenderPlanBackend::RegisterMesh(
    Presentation::RenderMeshId mesh,
    Rhi::ResourceHandle vertexBuffer,
    Rhi::ResourceHandle indexBuffer,
    std::uint32_t indexCount,
    Rhi::IndexType indexType) noexcept {
    return IsInitialized()
        ? impl_->renderer.RegisterMesh(
            mesh, vertexBuffer, indexBuffer, indexCount, indexType)
        : Base::Result<void>(NotInitialized(
            "D3D11 render adapter is not initialized"));
}

Base::Result<void> D3D11RenderPlanBackend::UnregisterMesh(
    Presentation::RenderMeshId mesh) noexcept {
    return IsInitialized()
        ? impl_->renderer.UnregisterMesh(mesh)
        : Base::Result<void>(NotInitialized(
            "D3D11 render adapter is not initialized"));
}

Base::Result<void> D3D11RenderPlanBackend::RegisterGlyphRun(
    Presentation::RenderGlyphRunId glyphRun,
    Rhi::ResourceHandle vertexBuffer,
    Rhi::ResourceHandle indexBuffer,
    std::uint32_t indexCount,
    Rhi::ResourceHandle atlasTexture,
    Rhi::ResourceHandle sampler,
    Rhi::IndexType indexType) noexcept {
    return IsInitialized()
        ? impl_->renderer.RegisterGlyphRun(
            glyphRun, vertexBuffer, indexBuffer, indexCount,
            atlasTexture, sampler, indexType)
        : Base::Result<void>(NotInitialized(
            "D3D11 render adapter is not initialized"));
}

Base::Result<void> D3D11RenderPlanBackend::UnregisterGlyphRun(
    Presentation::RenderGlyphRunId glyphRun) noexcept {
    return IsInitialized()
        ? impl_->renderer.UnregisterGlyphRun(glyphRun)
        : Base::Result<void>(NotInitialized(
            "D3D11 render adapter is not initialized"));
}

Base::Result<void> D3D11RenderPlanBackend::Submit(
    const Presentation::RenderPlan& plan) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("D3D11 render adapter is not initialized");
    }
    if (device_->Backend().IsDeviceLost()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Cannot submit a RenderPlan to a lost D3D11 device");
    }

    Base::Result<Rhi::D3D11SurfaceFrame> acquired =
        presenter_->AcquireFrame();
    if (!acquired) {
        return acquired.GetStatus();
    }
    Rhi::D3D11SurfaceFrame frame = acquired.Value();
    const std::uint32_t width = frame.surface.target.width;
    const std::uint32_t height = frame.surface.target.height;
    if (width == 0U || height == 0U) {
        static_cast<void>(presenter_->DiscardFrame(frame));
        return InvalidArgument(
            "D3D11 surface frame has an empty render target");
    }

    Base::Result<Rhi::CommandList> recorded = impl_->renderer.Record(
        plan, {frame.renderTarget, width, height});
    if (!recorded) {
        static_cast<void>(presenter_->DiscardFrame(frame));
        return recorded.GetStatus();
    }
    Base::Result<Rhi::FenceValue> submitted =
        presenter_->SubmitAndPresent(frame, recorded.Value());
    if (!submitted) {
        return submitted.GetStatus();
    }
    impl_->lastSubmittedFence = submitted.Value();
    return {};
}

Rhi::FenceValue
D3D11RenderPlanBackend::LastSubmittedFence() const noexcept {
    return impl_ != nullptr ? impl_->lastSubmittedFence : 0U;
}

D3D11RenderPlanSubmitStatistics
D3D11RenderPlanBackend::LastSubmitStatistics() const noexcept {
    return impl_ != nullptr
        ? impl_->renderer.LastStatistics()
        : D3D11RenderPlanSubmitStatistics{};
}

} // namespace Aero::Render
