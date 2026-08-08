#pragma once

#include <Aero/Base/Result.hpp>
#include "render/RenderCommands.hpp"

#include <cstdint>

namespace Aero::Graphics {

constexpr std::uint32_t WindowRenderContextAbiVersion = 1U;

enum class WindowRenderContextKind : std::uint8_t {
    Invalid = 0U,
    D3D11,
    Wgl,
    Glx
};

using WindowRenderContextKindFlags = std::uint32_t;

constexpr WindowRenderContextKindFlags WindowRenderContextKindBit(
    WindowRenderContextKind kind) noexcept {
    return kind == WindowRenderContextKind::Invalid
        ? 0U
        : (UINT32_C(1) << static_cast<std::uint32_t>(kind));
}

constexpr bool SupportsWindowRenderContextKind(
    WindowRenderContextKindFlags available,
    WindowRenderContextKind kind) noexcept {
    return (available & WindowRenderContextKindBit(kind)) != 0U;
}

enum class WindowRenderContextOwnership : std::uint8_t {
    Borrowed = 0U,
    Owned
};

enum class PresentMode : std::uint8_t {
    Immediate = 0U,
    Fifo,
    Mailbox
};

struct D3D11WindowContextNative {
    std::uintptr_t window = 0U;
    std::uintptr_t device = 0U;
    std::uintptr_t immediateContext = 0U;
    std::uintptr_t swapChain = 0U;
};

struct WglWindowContextNative {
    std::uintptr_t window = 0U;
    std::uintptr_t deviceContext = 0U;
    std::uintptr_t renderContext = 0U;
};

struct GlxWindowContextNative {
    std::uintptr_t display = 0U;
    std::uintptr_t drawable = 0U;
    std::uintptr_t context = 0U;
    std::int32_t screen = 0;
};

struct WindowRenderContextDescriptor {
    std::uint32_t abiVersion = WindowRenderContextAbiVersion;
    WindowRenderContextKind kind = WindowRenderContextKind::Invalid;
    WindowRenderContextOwnership ownership =
        WindowRenderContextOwnership::Borrowed;
    PresentMode presentMode = PresentMode::Fifo;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    GraphicsTextureFormat colorFormat = GraphicsTextureFormat::Bgra8Unorm;
    GraphicsTextureFormat depthStencilFormat =
        GraphicsTextureFormat::Depth24Stencil8;
    std::uint8_t sampleCount = 1U;
    std::uint64_t stableId = 0U;
    D3D11WindowContextNative d3d11;
    WglWindowContextNative wgl;
    GlxWindowContextNative glx;
};

struct WindowRenderContextCaps {
    std::uint32_t abiVersion = WindowRenderContextAbiVersion;
    WindowRenderContextKindFlags supportedKinds = 0U;
    std::uint32_t maxWidth = 16384U;
    std::uint32_t maxHeight = 16384U;
    bool supportsResize = false;
    bool supportsPresent = false;
    bool supportsContextLossRecovery = false;
};

// Native framebuffer/texture attachment returned by a concrete D3D11, WGL or
// GLX context. This is a value binding, not a second render-target object.
struct RenderTargetBinding {
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

Base::Result<void> ValidateWindowRenderContextDescriptor(
    const WindowRenderContextDescriptor& descriptor,
    const WindowRenderContextCaps& capabilities) noexcept;
Base::Result<void> ValidateRenderTargetBinding(
    const RenderTargetBinding& descriptor) noexcept;

} // namespace Aero::Graphics
