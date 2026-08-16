#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <AeroRender/Texture.hpp>

#include <cstdint>

namespace Aero {

class RenderTarget;
class RenderDevice;

// Texture formats enumeration (matching NoesisGUI NsRender/RenderDevice.h)
struct TextureFormat {
    enum Enum {
        RGBA8,
        RGBX8,
        R8,
        Count
    };
};

// Render device capabilities
struct DeviceCaps {
    float centerPixelOffset = 0.0f;
    bool linearRendering = false;
    bool subpixelRendering = false;
    bool depthRangeZeroToOne = true;
    bool clipSpaceYInverted = false;
};

// Shader effects
struct Shader {
    enum Enum {
        RGBA,
        Mask,
        Clear,
        Path_Solid,
        Path_Linear,
        Path_Radial,
        Path_Pattern,
        Path_Pattern_Clamp,
        Path_Pattern_Repeat,
        Path_Pattern_MirrorU,
        Path_Pattern_MirrorV,
        Path_Pattern_Mirror,
        Path_AA_Solid,
        Path_AA_Linear,
        Path_AA_Radial,
        Path_AA_Pattern,
        Path_AA_Pattern_Clamp,
        Path_AA_Pattern_Repeat,
        Path_AA_Pattern_MirrorU,
        Path_AA_Pattern_MirrorV,
        Path_AA_Pattern_Mirror,
        SDF_Solid,
        SDF_Linear,
        SDF_Radial,
        SDF_Pattern,
        SDF_Pattern_Clamp,
        SDF_Pattern_Repeat,
        SDF_Pattern_MirrorU,
        SDF_Pattern_MirrorV,
        SDF_Pattern_Mirror,
        SDF_LCD_Solid,
        SDF_LCD_Linear,
        SDF_LCD_Radial,
        SDF_LCD_Pattern,
        SDF_LCD_Pattern_Clamp,
        SDF_LCD_Pattern_Repeat,
        SDF_LCD_Pattern_MirrorU,
        SDF_LCD_Pattern_MirrorV,
        SDF_LCD_Pattern_Mirror,
        Opacity_Solid,
        Opacity_Linear,
        Opacity_Radial,
        Opacity_Pattern,
        Opacity_Pattern_Clamp,
        Opacity_Pattern_Repeat,
        Opacity_Pattern_MirrorU,
        Opacity_Pattern_MirrorV,
        Opacity_Pattern_Mirror,
        Upsample,
        Downsample,
        Shadow,
        Blur,
        Custom_Effect,
        Count
    };

    uint8_t v = 0;

    constexpr Shader() noexcept : v(0) {}
    constexpr Shader(Enum e) noexcept : v(static_cast<uint8_t>(e)) {}
    constexpr operator Enum() const noexcept { return static_cast<Enum>(v); }

    struct Vertex {
        enum Enum {
            Pos,
            PosColor,
            PosTex0,
            PosTex0Rect,
            PosTex0RectTile,
            PosColorCoverage,
            PosTex0Coverage,
            PosTex0CoverageRect,
            PosTex0CoverageRectTile,
            PosColorTex1_SDF,
            PosTex0Tex1_SDF,
            PosTex0Tex1Rect_SDF,
            PosTex0Tex1RectTile_SDF,
            PosColorTex1,
            PosTex0Tex1,
            PosTex0Tex1Rect,
            PosTex0Tex1RectTile,
            PosColorTex0Tex1,
            PosTex0Tex1_Downsample,
            PosColorTex1Rect,
            PosColorTex0RectImagePos,
            Count
        };

        struct Format {
            enum Enum {
                Pos,
                PosColor,
                PosTex0,
                PosTex0Rect,
                PosTex0RectTile,
                PosColorCoverage,
                PosTex0Coverage,
                PosTex0CoverageRect,
                PosTex0CoverageRectTile,
                PosColorTex1,
                PosTex0Tex1,
                PosTex0Tex1Rect,
                PosTex0Tex1RectTile,
                PosColorTex0Tex1,
                PosColorTex1Rect,
                PosColorTex0RectImagePos,
                Count
            };

            struct Attr {
                enum Enum {
                    Pos,
                    Color,
                    Tex0,
                    Tex1,
                    Coverage,
                    Rect,
                    Tile,
                    ImagePos,
                    Count
                };

                struct Type {
                    enum Enum {
                        Float,
                        Float2,
                        Float4,
                        UByte4Norm,
                        UShort4Norm,
                        Count
                    };
                };
            };
        };
    };
};

static constexpr const uint8_t VertexForShader[Shader::Count] = {
    0, 0, 0, 1, 2, 2, 2, 3, 4, 4, 4, 4, 5, 6, 6, 6, 7, 8, 8, 8, 8, 9, 10, 10, 10, 11, 12, 12, 12,
    12, 9, 10, 10, 10, 11, 12, 12, 12, 12, 13, 14, 14, 14, 15, 16, 16, 16, 16, 17, 18, 19, 13, 20
};

static constexpr const uint8_t FormatForVertex[Shader::Vertex::Count] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 9, 10, 11, 12, 13, 10, 14, 15
};

static constexpr const uint8_t SizeForFormat[Shader::Vertex::Format::Count] = {
    8, 12, 16, 24, 40, 16, 20, 28, 44, 20, 24, 32, 48, 28, 28, 44
};

static constexpr const uint8_t AttributesForFormat[Shader::Vertex::Format::Count] = {
    1, 3, 5, 37, 101, 19, 21, 53, 117, 11, 13, 45, 109, 15, 43, 167
};

static constexpr const uint8_t TypeForAttr[Shader::Vertex::Format::Attr::Count] = {
    1, 3, 1, 1, 0, 4, 2, 2
};

static constexpr const uint8_t SizeForType[Shader::Vertex::Format::Attr::Type::Count] = {
    4, 8, 16, 4, 8
};

// Alpha blending mode
struct RenderBlendMode {
    enum Enum {
        Src,
        SrcOver,
        SrcOver_Multiply,
        SrcOver_Screen,
        SrcOver_Additive,
        SrcOver_Dual,
        Count
    };
};

// Stencil buffer mode
struct StencilMode {
    enum Enum {
        Disabled,
        Equal_Keep,
        Equal_Incr,
        Equal_Decr,
        Clear,
        Disabled_ZTest,
        Equal_Keep_ZTest,
        Count
    };
};

// Render state bitfield union
union RenderState {
    struct {
        uint8_t colorEnable:1;
        uint8_t blendMode:3;
        uint8_t stencilMode:3;
        uint8_t wireframe:1;
    } f;
    uint8_t v;

    constexpr RenderState() noexcept : v(0) {}
    constexpr explicit RenderState(uint8_t val) noexcept : v(val) {}
};

// Texture wrapping mode
struct WrapMode {
    enum Enum {
        ClampToEdge,
        ClampToZero,
        Repeat,
        MirrorU,
        MirrorV,
        Mirror,
        Count
    };
};

// Texture minification and magnification filter
struct MinMagFilter {
    enum Enum {
        Nearest,
        Linear,
        Count
    };
};

// Texture Mipmap filter
struct MipFilter {
    enum Enum {
        Disabled,
        Nearest,
        Linear,
        Count
    };
};

// Texture sampler state
union SamplerState {
    struct {
        uint8_t wrapMode:3;
        uint8_t minmagFilter:1;
        uint8_t mipFilter:2;
        uint8_t unused:2;
    } f;
    uint8_t v;

    constexpr SamplerState() noexcept : v(0) {}
    constexpr explicit SamplerState(uint8_t val) noexcept : v(val) {}
};

// Uniform shader values with hash for caching
struct UniformData {
    const void* values = nullptr;
    uint32_t numDwords = 0U;
    uint32_t hash = 0U;
};

// A region of the render target
struct Tile {
    uint32_t x = 0U;
    uint32_t y = 0U;
    uint32_t width = 0U;
    uint32_t height = 0U;
};

// Render batch information
struct Batch {
    Shader shader{};
    RenderState renderState{};
    uint8_t stencilRef = 0U;
    bool singlePassStereo = false;

    uint32_t vertexOffset = 0U;
    uint32_t numVertices = 0U;
    uint32_t startIndex = 0U;
    uint32_t numIndices = 0U;

    Texture* pattern = nullptr;
    Texture* ramps = nullptr;
    Texture* image = nullptr;
    Texture* glyphs = nullptr;
    Texture* shadow = nullptr;

    SamplerState patternSampler{};
    SamplerState rampsSampler{};
    SamplerState imageSampler{};
    SamplerState glyphsSampler{};
    SamplerState shadowSampler{};

    UniformData vertexUniforms[2]{};
    UniformData pixelUniforms[2]{};

    void* pixelShader = nullptr;
};

#ifndef DYNAMIC_VB_SIZE
    #define DYNAMIC_VB_SIZE 512 * 1024
#endif
#ifndef DYNAMIC_IB_SIZE
    #define DYNAMIC_IB_SIZE 128 * 1024
#endif
#ifndef DYNAMIC_TEX_SIZE
    #define DYNAMIC_TEX_SIZE 128 * 1024
#endif

enum class RenderDeviceState : std::uint8_t {
    Ready = 0U,
    DeviceLost,
    Failed,
    Shutdown
};

enum class RenderBackendKind : std::uint8_t {
    Headless = 0U,
    D3D11,
    OpenGL33
};

enum class RenderBackendHealth : std::uint8_t {
    Ready = 0U,
    DeviceLost,
    Failed
};

namespace Diagnostics {
struct RenderDeviceStatistics;
struct RenderFrameStatistics;
AERO_GUI_API RenderDeviceStatistics GetRenderDeviceStatistics(
    const Aero::RenderDevice& device) noexcept;
AERO_GUI_API RenderFrameStatistics GetLastRenderFrameStatistics(
    const Aero::RenderDevice& device) noexcept;
}

namespace Render {
using BlendMode = Aero::RenderBlendMode;
using RenderDeviceBase = Aero::RenderDevice;

AERO_GUI_API Result<Ref<Aero::RenderDevice>> CreateHeadlessRenderDevice(
    Base::IAllocator* allocator = nullptr) noexcept;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Base class for implementing renderers (reference: NoesisGUI NsRender/RenderDevice.h)
////////////////////////////////////////////////////////////////////////////////////////////////////
class AERO_GUI_API RenderDevice : public Base::Object {
public:
    ~RenderDevice() noexcept override;

    RenderDevice(const RenderDevice&) = delete;
    RenderDevice& operator=(const RenderDevice&) = delete;

    RenderDeviceState State() const noexcept { return state_; }
    RenderBackendKind Backend() const noexcept { return backend_; }
    std::uint64_t Generation() const noexcept { return generation_; }

    void NotifyDeviceLost() noexcept;
    Result<void> Restore() noexcept;
    Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds = 5000U) noexcept;

    /// Retrieves device render capabilities
    virtual const DeviceCaps& GetCaps() const noexcept = 0;

    /// Creates render target surface with given dimensions, samples and optional stencil buffer
    virtual Ref<RenderTarget> CreateRenderTarget(
        const char* label, uint32_t width, uint32_t height,
        uint32_t sampleCount, bool needsStencil) noexcept = 0;

    /// Creates render target sharing transient (stencil, colorAA) buffers with the given surface
    virtual Ref<RenderTarget> CloneRenderTarget(
        const char* label, RenderTarget* surface) noexcept = 0;

    /// Creates texture with given dimensions and format
    virtual Ref<Texture> CreateTexture(
        const char* label, uint32_t width, uint32_t height,
        uint32_t numLevels, TextureFormat::Enum format, const void** data) noexcept = 0;

    /// Begins a block for uploading texture data
    virtual void BeginUpdatingTextures() noexcept;

    /// Updates a region of a texture mip level by copying the given data to the specified position
    virtual void UpdateTexture(
        Texture* texture, uint32_t level, uint32_t x, uint32_t y,
        uint32_t width, uint32_t height, const void* data) noexcept = 0;

    /// Marks the end of a texture update block
    virtual void EndUpdatingTextures(
        Texture** textures, uint32_t count) noexcept;

    /// Begins rendering offscreen commands
    virtual void BeginOffscreenRender() noexcept = 0;

    /// Ends rendering offscreen commands
    virtual void EndOffscreenRender() noexcept = 0;

    /// Begins rendering onscreen commands
    virtual void BeginOnscreenRender() noexcept = 0;

    /// Ends rendering onscreen commands
    virtual void EndOnscreenRender() noexcept = 0;

    /// Binds render target and sets viewport to cover the entire surface
    virtual void SetRenderTarget(RenderTarget* surface) noexcept = 0;

    /// Indicates that until the next call to EndTile(), drawing commands update given tile
    virtual void BeginTile(RenderTarget* surface, const Tile& tile) noexcept = 0;

    /// Completes rendering to the tile specified by BeginTile()
    virtual void EndTile(RenderTarget* surface) noexcept = 0;

    /// Resolves multisample render target
    virtual void ResolveRenderTarget(
        RenderTarget* surface, const Tile* tiles, uint32_t numTiles) noexcept = 0;

    /// Gets a pointer to stream vertices
    virtual void* MapVertices(uint32_t bytes) noexcept = 0;

    /// Invalidates the pointer previously mapped
    virtual void UnmapVertices() noexcept = 0;

    /// Gets a pointer to stream 16-bit indices
    virtual void* MapIndices(uint32_t bytes) noexcept = 0;

    /// Invalidates the pointer previously mapped
    virtual void UnmapIndices() noexcept = 0;

    /// Draws primitives for the given batch
    virtual void DrawBatch(const Batch& batch) noexcept = 0;

    /// Offscreen configuration
    void SetOffscreenWidth(uint32_t width) noexcept;
    uint32_t GetOffscreenWidth() const noexcept;
    void SetOffscreenHeight(uint32_t height) noexcept;
    uint32_t GetOffscreenHeight() const noexcept;
    void SetOffscreenSampleCount(uint32_t sampleCount) noexcept;
    uint32_t GetOffscreenSampleCount() const noexcept;
    void SetOffscreenDefaultNumSurfaces(uint32_t numSurfaces) noexcept;
    uint32_t GetOffscreenDefaultNumSurfaces() const noexcept;
    void SetOffscreenMaxNumSurfaces(uint32_t numSurfaces) noexcept;
    uint32_t GetOffscreenMaxNumSurfaces() const noexcept;

    /// Glyph cache configuration
    void SetGlyphCacheWidth(uint32_t width) noexcept;
    uint32_t GetGlyphCacheWidth() const noexcept;
    void SetGlyphCacheHeight(uint32_t height) noexcept;
    uint32_t GetGlyphCacheHeight() const noexcept;

    /// State validation helpers
    static bool IsValidState(Shader shader, RenderState state) noexcept;
    static bool IsValidBlendMode(Shader shader, RenderBlendMode::Enum blendMode) noexcept;
    static bool IsValidStencilMode(Shader shader, StencilMode::Enum stencilMode) noexcept;
    static bool IsValidColorEnable(Shader shader, bool colorEnable) noexcept;
    static bool IsValidWireframe(Shader shader, bool wireframe) noexcept;

protected:
    RenderDevice() noexcept = default;

    virtual RenderBackendKind BackendKind() const noexcept { return backend_; }
    virtual void NotifyBackendDeviceLost() noexcept {}
    virtual Result<void> RestoreBackendDevice() noexcept { return {}; }
    virtual Result<void> WaitBackendIdle(
        std::uint32_t timeoutMilliseconds) noexcept {
        static_cast<void>(timeoutMilliseconds);
        return {};
    }
    virtual RenderBackendHealth BackendHealth() const noexcept {
        return state_ == RenderDeviceState::Ready
            ? RenderBackendHealth::Ready
            : RenderBackendHealth::Failed;
    }

    RenderDeviceState state_ = RenderDeviceState::Ready;
    RenderBackendKind backend_ = RenderBackendKind::Headless;
    std::uint64_t generation_ = 1U;
    uint32_t offscreenWidth_ = 0U;
    uint32_t offscreenHeight_ = 0U;
    uint32_t offscreenSampleCount_ = 1U;
    uint32_t offscreenDefaultNumSurfaces_ = 0U;
    uint32_t offscreenMaxNumSurfaces_ = 0U;
    uint32_t glyphCacheWidth_ = 1024U;
    uint32_t glyphCacheHeight_ = 1024U;
};

} // namespace Aero
