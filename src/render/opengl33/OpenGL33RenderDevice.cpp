#include "OpenGL33RenderDevice.hpp"
#include <cstring>
#include <algorithm>

namespace Aero::Render {

OpenGL33Texture::OpenGL33Texture(
    unsigned int textureId,
    uint32_t width,
    uint32_t height,
    bool hasMipMaps,
    bool hasAlpha) noexcept
    : textureId_(textureId), width_(width), height_(height),
      hasMipMaps_(hasMipMaps), hasAlpha_(hasAlpha) {}

OpenGL33Texture::~OpenGL33Texture() noexcept {
    // Texture cleanup
}

OpenGL33RenderTarget::OpenGL33RenderTarget(
    Ref<RenderDevice> device,
    Ref<OpenGL33Texture> texture,
    unsigned int fboId,
    unsigned int rboId,
    uint32_t width,
    uint32_t height,
    bool defaultFbo) noexcept
    : texture_(std::move(texture)), fboId_(fboId), rboId_(rboId),
      width_(width), height_(height), defaultFbo_(defaultFbo) {
    device_ = std::move(device);
    kind_ = defaultFbo ? RenderTargetKind::Window : RenderTargetKind::Embedded;
    state_ = RenderTargetState::Ready;
}

OpenGL33RenderTarget::~OpenGL33RenderTarget() noexcept {
    // FBO cleanup
}

OpenGL33RenderDevice::OpenGL33RenderDevice(
    const OpenGL33::DeviceOptions& options,
    Base::IAllocator* allocator) noexcept
    : options_(options), allocator_(allocator) {
    backend_ = RenderBackendKind::OpenGL33;
    caps_.centerPixelOffset = 0.0f;
    caps_.linearRendering = false;
    caps_.subpixelRendering = false;
    caps_.depthRangeZeroToOne = false;
    caps_.clipSpaceYInverted = true;
}

OpenGL33RenderDevice::~OpenGL33RenderDevice() noexcept {
    Shutdown();
}

Base::Result<void> OpenGL33RenderDevice::Initialize() noexcept {
    state_ = RenderDeviceState::Ready;
    return {};
}

void OpenGL33RenderDevice::Shutdown() noexcept {
    state_ = RenderDeviceState::Shutdown;
}

Ref<RenderTarget> OpenGL33RenderDevice::CreateRenderTarget(
    const char* label, uint32_t width, uint32_t height,
    uint32_t sampleCount, bool needsStencil) noexcept {
    static_cast<void>(label);
    static_cast<void>(sampleCount);
    static_cast<void>(needsStencil);

    Ref<OpenGL33Texture> tex = Base::MakeRef<OpenGL33Texture>(
        1U, width, height, false, true).Value();

    Ref<RenderDevice> self = Ref<RenderDevice>::FromBorrowed(*this);
    return Base::MakeRef<OpenGL33RenderTarget>(
        std::move(self), std::move(tex), 1U, 1U, width, height, false).Value();
}

Ref<RenderTarget> OpenGL33RenderDevice::CloneRenderTarget(
    const char* label, RenderTarget* surface) noexcept {
    if (surface == nullptr) return {};
    auto* src = static_cast<OpenGL33RenderTarget*>(surface);
    return CreateRenderTarget(label, src->GetWidth(), src->GetHeight(), 1, false);
}

Ref<Texture> OpenGL33RenderDevice::CreateTexture(
    const char* label, uint32_t width, uint32_t height,
    uint32_t numLevels, TextureFormat::Enum format, const void** data) noexcept {
    static_cast<void>(label);
    static_cast<void>(data);

    return Base::MakeRef<OpenGL33Texture>(
        1U, width, height, numLevels > 1, format != TextureFormat::RGBX8).Value();
}

void OpenGL33RenderDevice::BeginUpdatingTextures() noexcept {}

void OpenGL33RenderDevice::UpdateTexture(
    Texture* texture, uint32_t level, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height, const void* data) noexcept {
    static_cast<void>(texture);
    static_cast<void>(level);
    static_cast<void>(x);
    static_cast<void>(y);
    static_cast<void>(width);
    static_cast<void>(height);
    static_cast<void>(data);
}

void OpenGL33RenderDevice::EndUpdatingTextures(Texture** textures, uint32_t count) noexcept {
    static_cast<void>(textures);
    static_cast<void>(count);
}

void OpenGL33RenderDevice::BeginOffscreenRender() noexcept {}
void OpenGL33RenderDevice::EndOffscreenRender() noexcept {}
void OpenGL33RenderDevice::BeginOnscreenRender() noexcept {}
void OpenGL33RenderDevice::EndOnscreenRender() noexcept {}

void OpenGL33RenderDevice::SetRenderTarget(RenderTarget* surface) noexcept {
    currentTarget_ = static_cast<OpenGL33RenderTarget*>(surface);
}

void OpenGL33RenderDevice::BeginTile(RenderTarget* surface, const Tile& tile) noexcept {
    static_cast<void>(surface);
    static_cast<void>(tile);
}

void OpenGL33RenderDevice::EndTile(RenderTarget* surface) noexcept {
    static_cast<void>(surface);
}

void OpenGL33RenderDevice::ResolveRenderTarget(
    RenderTarget* surface, const Tile* tiles, uint32_t numTiles) noexcept {
    static_cast<void>(surface);
    static_cast<void>(tiles);
    static_cast<void>(numTiles);
}

void* OpenGL33RenderDevice::MapVertices(uint32_t bytes) noexcept {
    static_cast<void>(bytes);
    return mappedVBMemory_;
}

void OpenGL33RenderDevice::UnmapVertices() noexcept {}

void* OpenGL33RenderDevice::MapIndices(uint32_t bytes) noexcept {
    static_cast<void>(bytes);
    return mappedIBMemory_;
}

void OpenGL33RenderDevice::UnmapIndices() noexcept {}

void OpenGL33RenderDevice::DrawBatch(const Batch& batch) noexcept {
    static_cast<void>(batch);
}

} // namespace Aero::Render
