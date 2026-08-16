#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <AeroRender/RenderDevice.hpp>
#include <AeroRender/RenderTarget.hpp>
#include <AeroRender/Texture.hpp>
#include <AeroRender/OpenGL33.hpp>

#include "OpenGL33.hpp"

namespace Aero::Render {

class OpenGL33Texture final : public Texture {
public:
    OpenGL33Texture(
        unsigned int textureId,
        uint32_t width,
        uint32_t height,
        bool hasMipMaps,
        bool hasAlpha) noexcept;
    ~OpenGL33Texture() noexcept override;

    uint32_t GetWidth() const noexcept override { return width_; }
    uint32_t GetHeight() const noexcept override { return height_; }
    bool HasMipMaps() const noexcept override { return hasMipMaps_; }
    bool IsInverted() const noexcept override { return true; }
    bool HasAlpha() const noexcept override { return hasAlpha_; }

    unsigned int GetNativeTexture() const noexcept { return textureId_; }

private:
    unsigned int textureId_ = 0;
    uint32_t width_ = 0U;
    uint32_t height_ = 0U;
    bool hasMipMaps_ = false;
    bool hasAlpha_ = true;
};

class OpenGL33RenderTarget final : public RenderTarget {
public:
    OpenGL33RenderTarget(
        Ref<RenderDevice> device,
        Ref<OpenGL33Texture> texture,
        unsigned int fboId,
        unsigned int rboId,
        uint32_t width,
        uint32_t height,
        bool defaultFbo = false) noexcept;
    ~OpenGL33RenderTarget() noexcept override;

    Texture* GetTexture() noexcept override { return texture_.Get(); }
    unsigned int GetFBO() const noexcept { return fboId_; }
    uint32_t GetWidth() const noexcept { return width_; }
    uint32_t GetHeight() const noexcept { return height_; }
    bool IsDefaultFBO() const noexcept { return defaultFbo_; }

private:
    Ref<OpenGL33Texture> texture_;
    unsigned int fboId_ = 0;
    unsigned int rboId_ = 0;
    uint32_t width_ = 0U;
    uint32_t height_ = 0U;
    bool defaultFbo_ = false;
};

class OpenGL33RenderDevice final : public Aero::Render::RenderDeviceBase {
public:
    explicit OpenGL33RenderDevice(
        const OpenGL33::DeviceOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept;
    ~OpenGL33RenderDevice() noexcept override;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    const DeviceCaps& GetCaps() const noexcept override { return caps_; }

    Ref<RenderTarget> CreateRenderTarget(
        const char* label, uint32_t width, uint32_t height,
        uint32_t sampleCount, bool needsStencil) noexcept override;

    Ref<RenderTarget> CloneRenderTarget(
        const char* label, RenderTarget* surface) noexcept override;

    Ref<Texture> CreateTexture(
        const char* label, uint32_t width, uint32_t height,
        uint32_t numLevels, TextureFormat::Enum format, const void** data) noexcept override;

    void BeginUpdatingTextures() noexcept override;
    void UpdateTexture(
        Texture* texture, uint32_t level, uint32_t x, uint32_t y,
        uint32_t width, uint32_t height, const void* data) noexcept override;
    void EndUpdatingTextures(
        Texture** textures, uint32_t count) noexcept override;

    void BeginOffscreenRender() noexcept override;
    void EndOffscreenRender() noexcept override;
    void BeginOnscreenRender() noexcept override;
    void EndOnscreenRender() noexcept override;

    void SetRenderTarget(RenderTarget* surface) noexcept override;
    void BeginTile(RenderTarget* surface, const Tile& tile) noexcept override;
    void EndTile(RenderTarget* surface) noexcept override;
    void ResolveRenderTarget(
        RenderTarget* surface, const Tile* tiles, uint32_t numTiles) noexcept override;

    void* MapVertices(uint32_t bytes) noexcept override;
    void UnmapVertices() noexcept override;
    void* MapIndices(uint32_t bytes) noexcept override;
    void UnmapIndices() noexcept override;

    void DrawBatch(const Batch& batch) noexcept override;

protected:
    RenderBackendKind BackendKind() const noexcept override {
        return RenderBackendKind::OpenGL33;
    }

private:
    OpenGL33::DeviceOptions options_;
    Base::IAllocator* allocator_ = nullptr;

    unsigned int vao_ = 0;
    unsigned int dynamicVB_ = 0;
    unsigned int dynamicIB_ = 0;

    uint8_t mappedVBMemory_[DYNAMIC_VB_SIZE]{};
    uint8_t mappedIBMemory_[DYNAMIC_IB_SIZE]{};

    DeviceCaps caps_{};
    OpenGL33RenderTarget* currentTarget_ = nullptr;
};

} // namespace Aero::Render
