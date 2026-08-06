#include "D3D11Renderer.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <new>

#include "../MeshGpuResources.hpp"
#include "../ImageGpuResources.hpp"
#include "../TextGpuResources.hpp"

#include "AeroD3D11RenderFramePixelShader.hpp"
#include "AeroD3D11RenderFrameVertexShader.hpp"
#include "AeroD3D11RenderFrameImagePixelShader.hpp"
#include "AeroD3D11RenderFrameImageVertexShader.hpp"
#include "AeroD3D11RenderFrameMaskPixelShader.hpp"
#include "AeroD3D11RenderFrameMaskVertexShader.hpp"
#include "AeroD3D11RenderFrameEffectPixelShader.hpp"
#include "AeroD3D11RenderFrameEffectVertexShader.hpp"
#include "AeroD3D11RenderFrameMeshPixelShader.hpp"
#include "AeroD3D11RenderFrameMeshVertexShader.hpp"
#include "AeroD3D11RenderFrameGlyphPixelShader.hpp"
#include "AeroD3D11RenderFrameGlyphVertexShader.hpp"

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

Graphics::ShaderDescriptor Shader(
    Graphics::ShaderStage stage,
    const std::uint8_t* bytecode,
    std::uint32_t bytecodeSize,
    std::uint64_t stableId,
    Base::StringView entryPoint) noexcept {
    Graphics::ShaderDescriptor descriptor;
    descriptor.stage = stage;
    descriptor.language = Graphics::ShaderLanguage::Dxbc;
    descriptor.bytecode = bytecode;
    descriptor.bytecodeSize = bytecodeSize;
    descriptor.entryPoint = entryPoint;
    descriptor.stableId = stableId;
    return descriptor;
}

} // namespace

RendererShaderSet MakeD3D11RendererShaderSet() noexcept {
    RendererShaderSet shaders;
    shaders.rectangleVertex = Shader(
        Graphics::ShaderStage::Vertex,
        AeroD3D11RenderFrameVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameVertexShader)),
        UINT64_C(0xD3111001), Base::StringView("vs_main"));
    shaders.rectangleFragment = Shader(
        Graphics::ShaderStage::Fragment,
        AeroD3D11RenderFramePixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFramePixelShader)),
        UINT64_C(0xD3111002), Base::StringView("ps_main"));
    shaders.imageVertex = Shader(
        Graphics::ShaderStage::Vertex,
        AeroD3D11RenderFrameImageVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameImageVertexShader)),
        UINT64_C(0xD3111011), Base::StringView("vs_main"));
    shaders.imageFragment = Shader(
        Graphics::ShaderStage::Fragment,
        AeroD3D11RenderFrameImagePixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameImagePixelShader)),
        UINT64_C(0xD3111012), Base::StringView("ps_main"));
    shaders.maskVertex = Shader(
        Graphics::ShaderStage::Vertex,
        AeroD3D11RenderFrameMaskVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameMaskVertexShader)),
        UINT64_C(0xD3111013), Base::StringView("vs_main"));
    shaders.maskFragment = Shader(
        Graphics::ShaderStage::Fragment,
        AeroD3D11RenderFrameMaskPixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameMaskPixelShader)),
        UINT64_C(0xD3111014), Base::StringView("ps_main"));
    shaders.effectVertex = Shader(
        Graphics::ShaderStage::Vertex,
        AeroD3D11RenderFrameEffectVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameEffectVertexShader)),
        UINT64_C(0xD3111015), Base::StringView("vs_main"));
    shaders.effectFragment = Shader(
        Graphics::ShaderStage::Fragment,
        AeroD3D11RenderFrameEffectPixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameEffectPixelShader)),
        UINT64_C(0xD3111016), Base::StringView("ps_main"));
    shaders.meshVertex = Shader(
        Graphics::ShaderStage::Vertex,
        AeroD3D11RenderFrameMeshVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameMeshVertexShader)),
        UINT64_C(0xD3111021), Base::StringView("vs_main"));
    shaders.meshFragment = Shader(
        Graphics::ShaderStage::Fragment,
        AeroD3D11RenderFrameMeshPixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameMeshPixelShader)),
        UINT64_C(0xD3111022), Base::StringView("ps_main"));
    shaders.glyphVertex = Shader(
        Graphics::ShaderStage::Vertex,
        AeroD3D11RenderFrameGlyphVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameGlyphVertexShader)),
        UINT64_C(0xD3111031), Base::StringView("vs_main"));
    shaders.glyphFragment = Shader(
        Graphics::ShaderStage::Fragment,
        AeroD3D11RenderFrameGlyphPixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderFrameGlyphPixelShader)),
        UINT64_C(0xD3111032), Base::StringView("ps_main"));
    shaders.colorFormat = Graphics::GraphicsTextureFormat::Bgra8Unorm;
    return shaders;
}

struct D3D11Renderer::Impl {
    Impl(
        Graphics::GraphicsDevice& device,
        std::uint64_t generation,
        Base::IAllocator* allocator) noexcept
        : renderer(
              device,
              MakeD3D11RendererShaderSet(),
              allocator),
          textResources(
              device, renderer, generation, *allocator),
          meshResources(
              device, renderer, generation, *allocator),
          imageResources(
              device, renderer, generation, *allocator) {}

    FrameEncoder renderer;
    Detail::TextGpuResources textResources;
    Detail::MeshGpuResources meshResources;
    Detail::ImageGpuResources imageResources;
    Graphics::FenceValue lastSubmittedFence = 0U;
    bool initialized = false;
};

D3D11Renderer::D3D11Renderer(
    Graphics::GraphicsDevice& device,
    Graphics::D3D11SurfacePresenter& presenter,
    Base::IAllocator* allocator) noexcept
    : device_(&device),
      presenter_(&presenter),
      allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {}

D3D11Renderer::~D3D11Renderer() noexcept {
    Shutdown();
}

Base::Result<void> D3D11Renderer::Initialize() noexcept {
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
        ++textGeneration_;
        if (textGeneration_ == 0U) ++textGeneration_;
        impl_ = new (memory) Impl(
            *device_, textGeneration_, allocator_);
    }
    Base::Result<void> initialized = impl_->renderer.Initialize();
    if (!initialized) {
        Shutdown();
        return initialized;
    }
    impl_->renderer.SetBatchingEnabled(
        batchingEnabled_);
    impl_->initialized = true;
    return {};
}

void D3D11Renderer::Shutdown() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    impl_->textResources.Shutdown();
    impl_->meshResources.Shutdown();
    impl_->renderer.Shutdown();
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Render);
    impl_ = nullptr;
}

bool D3D11Renderer::IsInitialized() const noexcept {
    return impl_ != nullptr && impl_->initialized &&
        impl_->renderer.IsInitialized();
}

Base::Result<void> D3D11Renderer::RegisterImage(
    Render::RenderImageId image,
    Graphics::ResourceHandle texture,
    Graphics::ResourceHandle sampler) noexcept {
    return IsInitialized()
        ? impl_->renderer.RegisterImage(image, texture, sampler)
        : Base::Result<void>(NotInitialized(
            "D3D11 render adapter is not initialized"));
}

Base::Result<void> D3D11Renderer::UnregisterImage(
    Render::RenderImageId image) noexcept {
    return IsInitialized()
        ? impl_->renderer.UnregisterImage(image)
        : Base::Result<void>(NotInitialized(
            "D3D11 render adapter is not initialized"));
}

Base::Result<void> D3D11Renderer::RegisterMesh(
    Render::RenderMeshId mesh,
    Graphics::ResourceHandle vertexBuffer,
    Graphics::ResourceHandle indexBuffer,
    std::uint32_t indexCount,
    Graphics::IndexType indexType) noexcept {
    return IsInitialized()
        ? impl_->renderer.RegisterMesh(
            mesh, vertexBuffer, indexBuffer, indexCount, indexType)
        : Base::Result<void>(NotInitialized(
            "D3D11 render adapter is not initialized"));
}

Base::Result<void> D3D11Renderer::UnregisterMesh(
    Render::RenderMeshId mesh) noexcept {
    return IsInitialized()
        ? impl_->renderer.UnregisterMesh(mesh)
        : Base::Result<void>(NotInitialized(
            "D3D11 render adapter is not initialized"));
}

Base::Result<void> D3D11Renderer::RegisterGlyphRun(
    Render::RenderGlyphRunId glyphRun,
    Graphics::ResourceHandle vertexBuffer,
    Graphics::ResourceHandle indexBuffer,
    std::uint32_t indexCount,
    Graphics::ResourceHandle atlasTexture,
    Graphics::ResourceHandle sampler,
    Graphics::IndexType indexType) noexcept {
    return IsInitialized()
        ? impl_->renderer.RegisterGlyphRun(
            glyphRun, vertexBuffer, indexBuffer, indexCount,
            atlasTexture, sampler, indexType)
        : Base::Result<void>(NotInitialized(
            "D3D11 render adapter is not initialized"));
}

Base::Result<void> D3D11Renderer::UnregisterGlyphRun(
    Render::RenderGlyphRunId glyphRun) noexcept {
    return IsInitialized()
        ? impl_->renderer.UnregisterGlyphRun(glyphRun)
        : Base::Result<void>(NotInitialized(
            "D3D11 render adapter is not initialized"));
}

Aero::Render::Detail::TextResources* D3D11Renderer::GetTextResources() noexcept {
    return IsInitialized() ? &impl_->textResources.Table() : nullptr;
}

Aero::Render::Detail::MeshResources* D3D11Renderer::GetMeshResources() noexcept {
    return IsInitialized() ? &impl_->meshResources.Table() : nullptr;
}

Aero::Render::Detail::ImageResources* D3D11Renderer::GetImageResources() noexcept {
    return IsInitialized() ? &impl_->imageResources.Table() : nullptr;
}

Base::Result<void> D3D11Renderer::RenderOffscreen(
    const void* rendererToken,
    const Integration::RenderFrame& plan) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("D3D11 render adapter is not initialized");
    }
    if (device_->Backend().IsDeviceLost()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Cannot render offscreen on a lost D3D11 device");
    }
    Base::Result<Graphics::CommandList> recorded =
        impl_->renderer.RecordOffscreen(rendererToken, plan);
    if (!recorded) return recorded.GetStatus();
    if (recorded.Value().CommandCount() == 0U) return {};
    Base::Result<Graphics::FenceValue> submitted =
        device_->Submit(recorded.Value());
    if (!submitted) return submitted.GetStatus();
    impl_->lastSubmittedFence = submitted.Value();
    return {};
}

Base::Result<void> D3D11Renderer::Render(
    const void* rendererToken,
    const Integration::RenderFrame& plan,
    Graphics::LoadOperation load) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("D3D11 render adapter is not initialized");
    }
    if (device_->Backend().IsDeviceLost()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Cannot render a RenderFrame to a lost D3D11 device");
    }

    Base::Result<Graphics::D3D11SurfaceFrame> acquired =
        presenter_->AcquireFrame();
    if (!acquired) {
        return acquired.GetStatus();
    }
    Graphics::D3D11SurfaceFrame frame = acquired.Value();
    const std::uint32_t width = frame.surface.target.width;
    const std::uint32_t height = frame.surface.target.height;
    if (width == 0U || height == 0U) {
        static_cast<void>(presenter_->DiscardFrame(frame));
        return InvalidArgument(
            "D3D11 surface frame has an empty render target");
    }

    Base::Result<Graphics::CommandList> recorded =
        impl_->renderer.RecordOnscreen(
            rendererToken, plan,
            {frame.renderTarget, width, height, load});
    if (!recorded) {
        static_cast<void>(presenter_->DiscardFrame(frame));
        return recorded.GetStatus();
    }
    Base::Result<Graphics::FenceValue> submitted =
        presenter_->SubmitAndPresent(frame, recorded.Value());
    if (!submitted) {
        return submitted.GetStatus();
    }
    impl_->lastSubmittedFence = submitted.Value();
    return {};
}

void D3D11Renderer::ReleaseRenderer(
    const void* rendererToken) noexcept {
    if (impl_ != nullptr) {
        impl_->renderer.ReleaseRenderer(rendererToken);
    }
}

Graphics::FenceValue
D3D11Renderer::LastSubmittedFence() const noexcept {
    return impl_ != nullptr ? impl_->lastSubmittedFence : 0U;
}

D3D11RendererStatistics
D3D11Renderer::LastSubmitStatistics() const noexcept {
    return impl_ != nullptr
        ? impl_->renderer.LastStatistics()
        : D3D11RendererStatistics{};
}

void D3D11Renderer::SetBatchingEnabled(
    bool enabled) noexcept {
    batchingEnabled_ = enabled;
    if (impl_ != nullptr) {
        impl_->renderer.SetBatchingEnabled(
            enabled);
    }
}

} // namespace Aero::Render
