#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include "render/GraphicsDevice.hpp"

#include <cstddef>
#include <cstdint>

namespace Aero::Graphics {

constexpr std::uint32_t SurfaceAbiVersion = 1U;

enum class SurfaceKind : std::uint8_t {
    Invalid = 0U,
    Headless,
    D3D11Window,
    WglWindow,
    GlxWindow,
    EglWindow,
    WebGL2Canvas,
    ExternalRenderTarget
};

using SurfaceKindFlags = std::uint32_t;

constexpr SurfaceKindFlags SurfaceKindBit(
    SurfaceKind kind) noexcept {
    return kind == SurfaceKind::Invalid
        ? 0U
        : (UINT32_C(1) << static_cast<std::uint32_t>(kind));
}

constexpr bool SupportsSurfaceKind(
    SurfaceKindFlags available,
    SurfaceKind kind) noexcept {
    return (available & SurfaceKindBit(kind)) != 0U;
}

enum class SurfaceOwnership : std::uint8_t {
    Borrowed = 0U,
    Owned
};

enum class SurfaceState : std::uint8_t {
    Uninitialized = 0U,
    Ready,
    Lost,
    Destroyed
};

enum class PresentMode : std::uint8_t {
    Immediate = 0U,
    Fifo,
    Mailbox
};

struct D3D11SurfaceNative  {
    std::uintptr_t window = 0U;
    std::uintptr_t device = 0U;
    std::uintptr_t immediateContext = 0U;
    std::uintptr_t swapChain = 0U;
};

struct WglSurfaceNative  {
    std::uintptr_t window = 0U;
    std::uintptr_t deviceContext = 0U;
    std::uintptr_t renderContext = 0U;
};

struct GlxSurfaceNative  {
    std::uintptr_t display = 0U;
    std::uintptr_t drawable = 0U;
    std::uintptr_t context = 0U;
    std::int32_t screen = 0;
};

struct EglSurfaceNative  {
    std::uintptr_t display = 0U;
    std::uintptr_t surface = 0U;
    std::uintptr_t context = 0U;
};

struct WebGL2SurfaceNative  {
    std::uint32_t contextHandle = 0U;
    std::uint64_t canvasId = 0U;
    bool offscreenCanvas = false;
};

struct ExternalSurfaceNative  {
    std::uintptr_t colorTarget = 0U;
    std::uintptr_t depthStencilTarget = 0U;
    bool defaultFramebuffer = false;
};

struct NativeSurfaceDescriptor  {
    std::uint32_t abiVersion = SurfaceAbiVersion;
    SurfaceKind kind = SurfaceKind::Invalid;
    SurfaceOwnership ownership = SurfaceOwnership::Borrowed;
    PresentMode presentMode = PresentMode::Fifo;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    GraphicsTextureFormat colorFormat = GraphicsTextureFormat::Bgra8Unorm;
    GraphicsTextureFormat depthStencilFormat =
        GraphicsTextureFormat::Depth24Stencil8;
    std::uint8_t sampleCount = 1U;
    std::uint64_t stableId = 0U;
    D3D11SurfaceNative d3d11;
    WglSurfaceNative wgl;
    GlxSurfaceNative glx;
    EglSurfaceNative egl;
    WebGL2SurfaceNative webgl2;
    ExternalSurfaceNative external;
};

struct SurfaceCapabilities  {
    std::uint32_t abiVersion = SurfaceAbiVersion;
    SurfaceKindFlags supportedKinds = 0U;
    std::uint32_t maxWidth = 16384U;
    std::uint32_t maxHeight = 16384U;
    bool supportsResize = false;
    bool supportsPresent = false;
    bool supportsContextLossRecovery = false;
    bool supportsExternalRenderTargets = false;
};

struct ExternalRenderTargetDescriptor  {
    std::uintptr_t colorTarget = 0U;
    std::uintptr_t depthStencilTarget = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    GraphicsTextureFormat colorFormat = GraphicsTextureFormat::Bgra8Unorm;
    GraphicsTextureFormat depthStencilFormat =
        GraphicsTextureFormat::Depth24Stencil8;
    std::uint8_t sampleCount = 1U;
    bool defaultFramebuffer = false;
    std::uint64_t stableId = 0U;
};

struct SurfaceFrame  {
    std::uint64_t surfaceGeneration = 0U;
    std::uint64_t frameSerial = 0U;
    ExternalRenderTargetDescriptor target;
};

AERO_API Base::Result<void> ValidateNativeSurfaceDescriptor(
    const NativeSurfaceDescriptor& descriptor,
    const SurfaceCapabilities& capabilities) noexcept;

AERO_API Base::Result<void>
ValidateExternalRenderTargetDescriptor(
    const ExternalRenderTargetDescriptor& descriptor) noexcept;

class AERO_API ISurfaceBackend {
public:
    virtual ~ISurfaceBackend() = default;

    virtual SurfaceCapabilities
    QuerySurfaceCapabilities() const noexcept = 0;
    virtual Base::Result<void> CreateSurface(
        const NativeSurfaceDescriptor& descriptor) noexcept = 0;
    virtual void DestroySurface() noexcept = 0;
    virtual Base::Result<void> ResizeSurface(
        std::uint32_t width,
        std::uint32_t height) noexcept = 0;
    virtual Base::Result<ExternalRenderTargetDescriptor>
    AcquireSurfaceTarget(std::uint64_t frameSerial) noexcept = 0;
    virtual Base::Result<void> PresentSurface(
        std::uint64_t frameSerial,
        FenceValue signalFence) noexcept = 0;
    virtual void DiscardSurfaceFrame(std::uint64_t frameSerial) noexcept = 0;
    virtual void NotifySurfaceLost() noexcept = 0;
    virtual Base::Result<void> RestoreSurface(
        const NativeSurfaceDescriptor& descriptor) noexcept = 0;
    virtual bool IsSurfaceLost() const noexcept = 0;
};

struct SurfaceFrameCapture  {
    GraphicsBackendKind backend = GraphicsBackendKind::Invalid;
    FenceValue signalFence = 0U;
    std::uint64_t surfaceGeneration = 0U;
    std::uint64_t frameSerial = 0U;
    std::uint64_t targetStableId = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint32_t commandCount = 0U;
    std::uint32_t uploadByteCount = 0U;
    std::uint64_t commandHash = 0U;
    bool presented = false;
};

} // namespace Aero::Graphics
