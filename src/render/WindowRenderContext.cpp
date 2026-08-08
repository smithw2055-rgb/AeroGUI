#include "render/WindowRenderContext.hpp"

namespace Aero::Graphics {
namespace {

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status Unsupported(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::Unsupported, message);
}

bool IsPowerOfTwo(std::uint32_t value) noexcept {
    return value != 0U && (value & (value - 1U)) == 0U;
}

bool IsDepthFormat(GraphicsTextureFormat format) noexcept {
    return format == GraphicsTextureFormat::Depth24Stencil8;
}

bool IsKnownKind(WindowRenderContextKind kind) noexcept {
    return kind == WindowRenderContextKind::D3D11 ||
        kind == WindowRenderContextKind::Wgl ||
        kind == WindowRenderContextKind::Glx;
}

bool IsKnownOwnership(WindowRenderContextOwnership ownership) noexcept {
    return ownership == WindowRenderContextOwnership::Borrowed ||
        ownership == WindowRenderContextOwnership::Owned;
}

bool IsKnownPresentMode(PresentMode mode) noexcept {
    return mode == PresentMode::Immediate ||
        mode == PresentMode::Fifo ||
        mode == PresentMode::Mailbox;
}

} // namespace

Base::Result<void> ValidateWindowRenderContextDescriptor(
    const WindowRenderContextDescriptor& descriptor,
    const WindowRenderContextCaps& capabilities) noexcept {
    if (descriptor.abiVersion != WindowRenderContextAbiVersion ||
        capabilities.abiVersion != WindowRenderContextAbiVersion) {
        return Unsupported("Window render-context ABI version is unsupported");
    }
    if (!IsKnownKind(descriptor.kind) ||
        !SupportsWindowRenderContextKind(
            capabilities.supportedKinds, descriptor.kind)) {
        return Unsupported("Window render-context kind is unsupported");
    }
    if (!IsKnownOwnership(descriptor.ownership) ||
        !IsKnownPresentMode(descriptor.presentMode)) {
        return InvalidArgument("Window context ownership or present mode is invalid");
    }
    if (descriptor.width == 0U || descriptor.height == 0U ||
        descriptor.width > capabilities.maxWidth ||
        descriptor.height > capabilities.maxHeight) {
        return InvalidArgument("Window render-context dimensions are invalid");
    }
    if (IsDepthFormat(descriptor.colorFormat) ||
        !IsPowerOfTwo(descriptor.sampleCount)) {
        return InvalidArgument("Window color format or sample count is invalid");
    }
    if (descriptor.depthStencilFormat != GraphicsTextureFormat::Depth24Stencil8) {
        return InvalidArgument("Window depth-stencil format is invalid");
    }

    switch (descriptor.kind) {
    case WindowRenderContextKind::D3D11:
        if (descriptor.d3d11.device == 0U ||
            descriptor.d3d11.immediateContext == 0U ||
            (descriptor.d3d11.window == 0U &&
             descriptor.d3d11.swapChain == 0U)) {
            return InvalidArgument(
                "D3D11 context requires a device, immediate context, and window or swap chain");
        }
        return {};
    case WindowRenderContextKind::Wgl:
        if ((descriptor.ownership == WindowRenderContextOwnership::Owned &&
             descriptor.wgl.window == 0U) ||
            (descriptor.ownership == WindowRenderContextOwnership::Borrowed &&
             (descriptor.wgl.deviceContext == 0U ||
              descriptor.wgl.renderContext == 0U))) {
            return InvalidArgument(
                "Owned WGL contexts require a window; borrowed contexts require DC and RC handles");
        }
        return {};
    case WindowRenderContextKind::Glx:
        if (descriptor.ownership == WindowRenderContextOwnership::Borrowed &&
            (descriptor.glx.display == 0U ||
             descriptor.glx.drawable == 0U ||
             descriptor.glx.context == 0U)) {
            return InvalidArgument(
                "Borrowed GLX contexts require display, drawable, and context handles");
        }
        return {};
    case WindowRenderContextKind::Invalid:
        break;
    }
    return InvalidArgument("Window render-context kind is invalid");
}

Base::Result<void> ValidateRenderTargetBinding(
    const RenderTargetBinding& descriptor) noexcept {
    if (descriptor.width == 0U || descriptor.height == 0U ||
        IsDepthFormat(descriptor.colorFormat) ||
        !IsPowerOfTwo(descriptor.sampleCount)) {
        return InvalidArgument("Render-target binding geometry is invalid");
    }
    if (descriptor.colorTarget == 0U && !descriptor.defaultFramebuffer) {
        return InvalidArgument(
            "Render-target binding requires a color handle or default framebuffer");
    }
    if (descriptor.depthStencilTarget != 0U &&
        descriptor.depthStencilFormat != GraphicsTextureFormat::Depth24Stencil8) {
        return InvalidArgument("Render-target depth-stencil format is invalid");
    }
    return {};
}

} // namespace Aero::Graphics
