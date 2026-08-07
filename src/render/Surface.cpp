#include "render/Surface.hpp"

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

bool IsKnownSurfaceKind(SurfaceKind kind) noexcept {
    return kind == SurfaceKind::D3D11Window ||
        kind == SurfaceKind::WglWindow ||
        kind == SurfaceKind::GlxWindow;
}

bool IsKnownOwnership(SurfaceOwnership ownership) noexcept {
    return ownership == SurfaceOwnership::Borrowed ||
        ownership == SurfaceOwnership::Owned;
}

bool IsKnownPresentMode(PresentMode mode) noexcept {
    return mode == PresentMode::Immediate ||
        mode == PresentMode::Fifo ||
        mode == PresentMode::Mailbox;
}

} // namespace

Base::Result<void> ValidateNativeSurfaceDescriptor(
    const NativeSurfaceDescriptor& descriptor,
    const SurfaceCapabilities& capabilities) noexcept {
    if (descriptor.abiVersion != SurfaceAbiVersion ||
        capabilities.abiVersion != SurfaceAbiVersion) {
        return Unsupported("Surface ABI version is unsupported");
    }
    if (!IsKnownSurfaceKind(descriptor.kind) ||
        !SupportsSurfaceKind(capabilities.supportedKinds, descriptor.kind)) {
        return Unsupported("Surface kind is not supported by the backend");
    }
    if (!IsKnownOwnership(descriptor.ownership) ||
        !IsKnownPresentMode(descriptor.presentMode)) {
        return InvalidArgument("Surface ownership or present mode is invalid");
    }
    if (descriptor.width == 0U || descriptor.height == 0U ||
        descriptor.width > capabilities.maxWidth ||
        descriptor.height > capabilities.maxHeight) {
        return InvalidArgument("Surface dimensions are invalid");
    }
    if (IsDepthFormat(descriptor.colorFormat) ||
        !IsPowerOfTwo(descriptor.sampleCount)) {
        return InvalidArgument("Surface color format or sample count is invalid");
    }
    if (descriptor.depthStencilFormat != GraphicsTextureFormat::Depth24Stencil8) {
        return InvalidArgument("Surface depth-stencil format is invalid");
    }

    switch (descriptor.kind) {
    case SurfaceKind::D3D11Window:
        if (descriptor.d3d11.device == 0U ||
            descriptor.d3d11.immediateContext == 0U ||
            (descriptor.d3d11.window == 0U &&
             descriptor.d3d11.swapChain == 0U)) {
            return InvalidArgument(
                "D3D11 surface requires a device, immediate context, and window or swap chain");
        }
        return {};
    case SurfaceKind::WglWindow:
        if ((descriptor.ownership == SurfaceOwnership::Owned &&
             descriptor.wgl.window == 0U) ||
            (descriptor.ownership == SurfaceOwnership::Borrowed &&
             (descriptor.wgl.deviceContext == 0U ||
              descriptor.wgl.renderContext == 0U))) {
            return InvalidArgument(
                "Owned WGL surfaces require a window; borrowed surfaces require a device and rendering context");
        }
        return {};
    case SurfaceKind::GlxWindow:
        if (descriptor.ownership == SurfaceOwnership::Borrowed &&
            (descriptor.glx.display == 0U ||
             descriptor.glx.drawable == 0U ||
             descriptor.glx.context == 0U)) {
            return InvalidArgument(
                "Borrowed GLX surfaces require a display, drawable, and context");
        }
        return {};
    case SurfaceKind::Invalid:
        break;
    }
    return InvalidArgument("Surface kind is invalid");
}

Base::Result<void> ValidateExternalRenderTargetDescriptor(
    const ExternalRenderTargetDescriptor& descriptor) noexcept {
    if (descriptor.width == 0U || descriptor.height == 0U ||
        IsDepthFormat(descriptor.colorFormat) ||
        !IsPowerOfTwo(descriptor.sampleCount)) {
        return InvalidArgument("External render target geometry is invalid");
    }
    if (descriptor.colorTarget == 0U && !descriptor.defaultFramebuffer) {
        return InvalidArgument(
            "External render target requires a color handle or default framebuffer");
    }
    if (descriptor.depthStencilTarget != 0U &&
        descriptor.depthStencilFormat != GraphicsTextureFormat::Depth24Stencil8) {
        return InvalidArgument(
            "External depth-stencil target format is invalid");
    }
    return {};
}

} // namespace Aero::Graphics
