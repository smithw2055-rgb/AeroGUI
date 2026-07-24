#include <Aero/Render/D3D11RenderBackend.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <new>

#include "AeroD3D11RenderPlanGlyphPixelShader.hpp"
#include "AeroD3D11RenderPlanGlyphVertexShader.hpp"
#include "AeroD3D11RenderPlanImagePixelShader.hpp"
#include "AeroD3D11RenderPlanImageVertexShader.hpp"
#include "AeroD3D11RenderPlanMeshPixelShader.hpp"
#include "AeroD3D11RenderPlanMeshVertexShader.hpp"
#include "AeroD3D11RenderPlanPixelShader.hpp"
#include "AeroD3D11RenderPlanVertexShader.hpp"

namespace Aero::Render {
namespace {

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotInitialized, message);
}

Base::Status OutOfMemory(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::OutOfMemory, message);
}

Rhi::ShaderDescriptor Shader(
    Rhi::ShaderStage stage,
    const std::uint8_t* bytes,
    std::uint32_t size,
    std::uint64_t stableId) noexcept {
    Rhi::ShaderDescriptor descriptor;
    descriptor.stage = stage;
    descriptor.language = Rhi::ShaderLanguage::Dxbc;
    descriptor.bytecode = bytes;
    descriptor.bytecodeSize = size;
    descriptor.entryPoint = Base::StringView(
        stage == Rhi::ShaderStage::Vertex ? "vs_main" : "ps_main");
    descriptor.stableId = stableId;
    return descriptor;
}

RendererShaderSet D3D11Shaders() noexcept {
    RendererShaderSet shaders;
    shaders.rectangle.vertex = Shader(
        Rhi::ShaderStage::Vertex,
        AeroD3D11RenderPlanVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderPlanVertexShader)),
        UINT64_C(0xD3111001));
    shaders.rectangle.fragment = Shader(
        Rhi::ShaderStage::Fragment,
        AeroD3D11RenderPlanPixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderPlanPixelShader)),
        UINT64_C(0xD3111002));
    shaders.image.vertex = Shader(
        Rhi::ShaderStage::Vertex,
        AeroD3D11RenderPlanImageVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderPlanImageVertexShader)),
        UINT64_C(0xD3111011));
    shaders.image.fragment = Shader(
        Rhi::ShaderStage::Fragment,
        AeroD3D11RenderPlanImagePixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderPlanImagePixelShader)),
        UINT64_C(0xD3111012));
    shaders.mesh.vertex = Shader(
        Rhi::ShaderStage::Vertex,
        AeroD3D11RenderPlanMeshVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderPlanMeshVertexShader)),
        UINT64_C(0xD3111021));
    shaders.mesh.fragment = Shader(
        Rhi::ShaderStage::Fragment,
        AeroD3D11RenderPlanMeshPixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderPlanMeshPixelShader)),
        UINT64_C(0xD3111022));
    shaders.glyph.vertex = Shader(
        Rhi::ShaderStage::Vertex,
        AeroD3D11RenderPlanGlyphVertexShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderPlanGlyphVertexShader)),
        UINT64_C(0xD3111031));
    shaders.glyph.fragment = Shader(
        Rhi::ShaderStage::Fragment,
        AeroD3D11RenderPlanGlyphPixelShader,
        static_cast<std::uint32_t>(sizeof(AeroD3D11RenderPlanGlyphPixelShader)),
        UINT64_C(0xD3111032));
    shaders.colorFormat = Rhi::GraphicsTextureFormat::Bgra8Unorm;
    return shaders;
}

} // namespace

D3D11RenderBackend::D3D11RenderBackend(
    Rhi::RhiDevice& device,
    Rhi::D3D11GraphicsBackend& graphics,
    Rhi::SurfaceSession& surface,
    Base::IAllocator* allocator) noexcept
    : device_(&device),
      graphics_(&graphics),
      surface_(&surface),
      allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()) {
    void* memory = allocator_->Allocate({
        sizeof(Renderer), alignof(Renderer), Base::MemoryTag::Render});
    if (memory != nullptr) {
        renderer_ = new (memory) Renderer(
            device, D3D11Shaders(), allocator_);
    }
}

D3D11RenderBackend::~D3D11RenderBackend() noexcept {
    Shutdown();
    if (renderer_ != nullptr) {
        renderer_->~Renderer();
        allocator_->Deallocate(renderer_, sizeof(Renderer), alignof(Renderer),
            Base::MemoryTag::Render);
        renderer_ = nullptr;
    }
}

Base::Result<void> D3D11RenderBackend::Initialize() noexcept {
    if (renderer_ == nullptr) {
        return OutOfMemory("Failed to allocate the D3D11 render adapter");
    }
    if (device_ == nullptr || graphics_ == nullptr || surface_ == nullptr ||
        !graphics_->IsInitialized() || graphics_->IsDeviceLost() ||
        surface_->State() != Rhi::SurfaceState::Ready) {
        return NotInitialized("D3D11 render adapter requires a ready backend");
    }
    return renderer_->Initialize();
}

void D3D11RenderBackend::Shutdown() noexcept {
    if (renderer_ != nullptr) {
        renderer_->Shutdown();
    }
    lastSubmittedFence_ = 0U;
}

Base::Result<void> D3D11RenderBackend::RegisterImage(
    Presentation::RenderImageId image,
    Rhi::ResourceHandle texture,
    Rhi::ResourceHandle sampler) noexcept {
    return renderer_ != nullptr
        ? renderer_->RegisterImage(image, texture, sampler)
        : Base::Result<void>(NotInitialized("D3D11 render adapter is unavailable"));
}

Base::Result<void> D3D11RenderBackend::UnregisterImage(
    Presentation::RenderImageId image) noexcept {
    return renderer_ != nullptr
        ? renderer_->UnregisterImage(image)
        : Base::Result<void>(NotInitialized("D3D11 render adapter is unavailable"));
}

Base::Result<void> D3D11RenderBackend::RegisterMesh(
    Presentation::RenderMeshId mesh,
    Rhi::ResourceHandle vertexBuffer,
    Rhi::ResourceHandle indexBuffer,
    std::uint32_t indexCount,
    Rhi::IndexType indexType) noexcept {
    return renderer_ != nullptr
        ? renderer_->RegisterMesh(
            mesh, vertexBuffer, indexBuffer, indexCount, indexType)
        : Base::Result<void>(NotInitialized("D3D11 render adapter is unavailable"));
}

Base::Result<void> D3D11RenderBackend::UnregisterMesh(
    Presentation::RenderMeshId mesh) noexcept {
    return renderer_ != nullptr
        ? renderer_->UnregisterMesh(mesh)
        : Base::Result<void>(NotInitialized("D3D11 render adapter is unavailable"));
}

Base::Result<void> D3D11RenderBackend::RegisterGlyphRun(
    Presentation::RenderGlyphRunId glyphRun,
    Rhi::ResourceHandle vertexBuffer,
    Rhi::ResourceHandle indexBuffer,
    std::uint32_t indexCount,
    Rhi::ResourceHandle atlasTexture,
    Rhi::ResourceHandle sampler,
    Rhi::IndexType indexType) noexcept {
    return renderer_ != nullptr
        ? renderer_->RegisterGlyphRun(glyphRun, vertexBuffer, indexBuffer,
            indexCount, atlasTexture, sampler, indexType)
        : Base::Result<void>(NotInitialized("D3D11 render adapter is unavailable"));
}

Base::Result<void> D3D11RenderBackend::UnregisterGlyphRun(
    Presentation::RenderGlyphRunId glyphRun) noexcept {
    return renderer_ != nullptr
        ? renderer_->UnregisterGlyphRun(glyphRun)
        : Base::Result<void>(NotInitialized("D3D11 render adapter is unavailable"));
}

Base::Result<void> D3D11RenderBackend::Submit(
    const Presentation::RenderPlan& plan) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("D3D11 render adapter is not initialized");
    }
    Base::Result<Rhi::SurfaceFrame> acquired = surface_->AcquireFrame();
    if (!acquired) {
        return acquired.GetStatus();
    }
    Rhi::SurfaceFrame frame = acquired.Value();
    RenderTarget target;
    target.color = frame.target.color;
    target.width = frame.target.width;
    target.height = frame.target.height;
    target.format = frame.target.colorFormat;
    target.sampleCount = frame.target.sampleCount;

    Base::Result<Rhi::GraphicsCommandBuffer> recorded =
        renderer_->Record(plan, target);
    if (!recorded) {
        static_cast<void>(surface_->DiscardFrame(frame));
        return recorded.GetStatus();
    }
    Base::Result<Rhi::FenceValue> submitted =
        device_->Submit(recorded.Value());
    if (!submitted) {
        static_cast<void>(surface_->DiscardFrame(frame));
        return submitted.GetStatus();
    }
    Base::Result<void> presented =
        surface_->Present(frame, submitted.Value());
    if (!presented) {
        return presented.GetStatus();
    }
    lastSubmittedFence_ = submitted.Value();
    return {};
}

bool D3D11RenderBackend::IsInitialized() const noexcept {
    return renderer_ != nullptr && renderer_->IsInitialized();
}

Rhi::FenceValue D3D11RenderBackend::LastSubmittedFence() const noexcept {
    return lastSubmittedFence_;
}

const RendererStatistics& D3D11RenderBackend::LastStatistics() const noexcept {
    static const RendererStatistics Empty;
    return renderer_ != nullptr ? renderer_->LastStatistics() : Empty;
}

} // namespace Aero::Render
