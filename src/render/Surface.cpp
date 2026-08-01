#include "render/Surface.hpp"

#include <cstddef>

namespace Aero::Graphics {
namespace {

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotInitialized, message);
}

Base::Status Unsupported(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::Unsupported, message);
}

Base::Status OutOfRange(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::OutOfRange, message);
}

bool IsPowerOfTwo(std::uint32_t value) noexcept {
    return value != 0U && (value & (value - 1U)) == 0U;
}

bool IsDepthFormat(GraphicsTextureFormat format) noexcept {
    return format == GraphicsTextureFormat::Depth24Stencil8;
}

bool IsKnownSurfaceKind(SurfaceKind kind) noexcept {
    return kind >= SurfaceKind::Headless &&
        kind <= SurfaceKind::ExternalRenderTarget;
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
    if (descriptor.depthStencilFormat !=
        GraphicsTextureFormat::Depth24Stencil8) {
        return InvalidArgument("Surface depth-stencil format is invalid");
    }

    switch (descriptor.kind) {
    case SurfaceKind::Headless:
        return {};
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
    case SurfaceKind::EglWindow:
        if (descriptor.egl.display == 0U ||
            descriptor.egl.surface == 0U ||
            descriptor.egl.context == 0U) {
            return InvalidArgument(
                "EGL surface requires a display, surface, and context");
        }
        return {};
    case SurfaceKind::WebGL2Canvas:
        if (descriptor.webgl2.contextHandle == 0U) {
            return InvalidArgument(
                "WebGL 2 surface requires a valid context handle");
        }
        return {};
    case SurfaceKind::ExternalRenderTarget:
        if (!capabilities.supportsExternalRenderTargets ||
            (descriptor.external.colorTarget == 0U &&
             !descriptor.external.defaultFramebuffer)) {
            return InvalidArgument(
                "External surface requires an importable color target or default framebuffer");
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
        descriptor.depthStencilFormat !=
            GraphicsTextureFormat::Depth24Stencil8) {
        return InvalidArgument(
            "External depth-stencil target format is invalid");
    }
    return {};
}

SurfaceSession::~SurfaceSession() noexcept {
    Shutdown();
}

Base::Result<void> SurfaceSession::AdvanceGeneration() noexcept {
    if (generation_ == UINT64_MAX) {
        return OutOfRange("Surface generation space is exhausted");
    }
    ++generation_;
    return {};
}

Base::Result<void> SurfaceSession::Initialize(
    const NativeSurfaceDescriptor& descriptor) noexcept {
    if (state_ != SurfaceState::Uninitialized) {
        return InvalidState("Surface session is already initialized");
    }

    capabilities_ = backend_->QuerySurfaceCapabilities();
    Base::Result<void> valid = ValidateNativeSurfaceDescriptor(
        descriptor, capabilities_);
    if (!valid) {
        return valid;
    }
    if (backend_->IsSurfaceLost()) {
        return InvalidState("Surface backend is already lost");
    }

    Base::Result<void> created = backend_->CreateSurface(descriptor);
    if (!created) {
        return created;
    }

    Base::Result<void> advanced = AdvanceGeneration();
    if (!advanced) {
        backend_->DestroySurface();
        return advanced;
    }

    descriptor_ = descriptor;
    state_ = SurfaceState::Ready;
    nextFrameSerial_ = 1U;
    activeFrameSerial_ = 0U;
    return {};
}

Base::Result<void> SurfaceSession::VerifyReady() noexcept {
    if (state_ != SurfaceState::Ready) {
        return InvalidState("Surface session is not ready");
    }
    if (!backend_->IsSurfaceLost()) {
        return {};
    }

    if (activeFrameSerial_ != 0U) {
        backend_->DiscardSurfaceFrame(activeFrameSerial_);
        activeFrameSerial_ = 0U;
    }
    state_ = SurfaceState::Lost;
    if (generation_ != UINT64_MAX) {
        ++generation_;
    }
    return InvalidState("Surface backend reported context loss");
}

Base::Result<void> SurfaceSession::Resize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return ready;
    }
    if (!capabilities_.supportsResize) {
        return Unsupported("Surface backend does not support resize");
    }
    if (activeFrameSerial_ != 0U) {
        return InvalidState("Cannot resize while a surface frame is active");
    }
    if (generation_ == UINT64_MAX) {
        return OutOfRange("Surface generation space is exhausted");
    }

    NativeSurfaceDescriptor candidate = descriptor_;
    candidate.width = width;
    candidate.height = height;
    Base::Result<void> valid = ValidateNativeSurfaceDescriptor(
        candidate, capabilities_);
    if (!valid) {
        return valid;
    }

    Base::Result<void> resized = backend_->ResizeSurface(width, height);
    if (!resized) {
        return resized;
    }

    descriptor_ = candidate;
    ++generation_;
    return {};
}

Base::Result<SurfaceFrame> SurfaceSession::AcquireFrame() noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return ready.GetStatus();
    }
    if (activeFrameSerial_ != 0U) {
        return InvalidState("A surface frame is already active");
    }
    if (nextFrameSerial_ == UINT64_MAX) {
        return OutOfRange("Surface frame serial space is exhausted");
    }

    const std::uint64_t serial = nextFrameSerial_;
    Base::Result<ExternalRenderTargetDescriptor> acquired =
        backend_->AcquireSurfaceTarget(serial);
    if (!acquired) {
        return acquired.GetStatus();
    }

    Base::Result<void> valid = ValidateExternalRenderTargetDescriptor(
        acquired.Value());
    if (!valid) {
        backend_->DiscardSurfaceFrame(serial);
        return valid.GetStatus();
    }
    if (acquired.Value().width != descriptor_.width ||
        acquired.Value().height != descriptor_.height) {
        backend_->DiscardSurfaceFrame(serial);
        return InvalidState(
            "Acquired surface target dimensions do not match the surface");
    }

    SurfaceFrame frame;
    frame.surfaceGeneration = generation_;
    frame.frameSerial = serial;
    frame.target = acquired.Value();
    activeFrameSerial_ = serial;
    ++nextFrameSerial_;
    return frame;
}

bool SurfaceSession::IsCurrentFrame(
    const SurfaceFrame& frame) const noexcept {
    return state_ == SurfaceState::Ready &&
        frame.surfaceGeneration == generation_ &&
        frame.frameSerial != 0U &&
        frame.frameSerial == activeFrameSerial_;
}

Base::Result<void> SurfaceSession::Present(
    SurfaceFrame& frame,
    FenceValue signalFence) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return ready;
    }
    if (!IsCurrentFrame(frame)) {
        return InvalidState("Surface frame is stale or not active");
    }
    if (!capabilities_.supportsPresent) {
        return Unsupported("Surface backend does not support presentation");
    }
    if (signalFence == 0U) {
        return InvalidArgument("Present requires a non-zero fence");
    }

    Base::Result<void> presented = backend_->PresentSurface(
        frame.frameSerial, signalFence);
    if (!presented) {
        return presented;
    }

    activeFrameSerial_ = 0U;
    frame.surfaceGeneration = 0U;
    frame.frameSerial = 0U;
    return {};
}

Base::Result<void> SurfaceSession::DiscardFrame(
    SurfaceFrame& frame) noexcept {
    if (!IsCurrentFrame(frame)) {
        return InvalidState("Surface frame is stale or not active");
    }

    backend_->DiscardSurfaceFrame(frame.frameSerial);
    activeFrameSerial_ = 0U;
    frame.surfaceGeneration = 0U;
    frame.frameSerial = 0U;
    return {};
}

Base::Result<void> SurfaceSession::NotifyContextLost() noexcept {
    if (state_ == SurfaceState::Uninitialized ||
        state_ == SurfaceState::Destroyed) {
        return NotInitialized("Surface session has no active surface");
    }
    if (state_ == SurfaceState::Lost) {
        return {};
    }
    if (generation_ == UINT64_MAX) {
        return OutOfRange("Surface generation space is exhausted");
    }

    if (activeFrameSerial_ != 0U) {
        backend_->DiscardSurfaceFrame(activeFrameSerial_);
        activeFrameSerial_ = 0U;
    }
    backend_->NotifySurfaceLost();
    state_ = SurfaceState::Lost;
    ++generation_;
    return {};
}

Base::Result<void> SurfaceSession::Restore(
    const NativeSurfaceDescriptor& descriptor) noexcept {
    if (state_ != SurfaceState::Lost) {
        return InvalidState("Surface session is not in the lost state");
    }
    if (!capabilities_.supportsContextLossRecovery) {
        return Unsupported("Surface backend does not support restoration");
    }
    if (generation_ == UINT64_MAX) {
        return OutOfRange("Surface generation space is exhausted");
    }

    Base::Result<void> valid = ValidateNativeSurfaceDescriptor(
        descriptor, capabilities_);
    if (!valid) {
        return valid;
    }

    Base::Result<void> restored = backend_->RestoreSurface(descriptor);
    if (!restored) {
        return restored;
    }

    descriptor_ = descriptor;
    state_ = SurfaceState::Ready;
    ++generation_;
    return {};
}

void SurfaceSession::Shutdown() noexcept {
    if (state_ == SurfaceState::Uninitialized ||
        state_ == SurfaceState::Destroyed) {
        return;
    }
    if (activeFrameSerial_ != 0U) {
        backend_->DiscardSurfaceFrame(activeFrameSerial_);
        activeFrameSerial_ = 0U;
    }
    backend_->DestroySurface();
    state_ = SurfaceState::Destroyed;
    if (generation_ != UINT64_MAX) {
        ++generation_;
    }
}

Base::Result<FenceValue> SurfaceSession::SubmitAndPresent(
    GraphicsDevice& device,
    SurfaceFrame& frame,
    const CommandList& commands) noexcept {
    if (!IsCurrentFrame(frame)) {
        return InvalidState("Surface frame is stale or not active");
    }
    if (!capabilities_.supportsPresent) {
        static_cast<void>(DiscardFrame(frame));
        return Unsupported("Surface does not support presentation");
    }
    if (device.Backend().IsDeviceLost()) {
        static_cast<void>(DiscardFrame(frame));
        return InvalidState("Graphics backend is lost");
    }

    const std::uint64_t surfaceGeneration = frame.surfaceGeneration;
    const std::uint64_t frameSerial = frame.frameSerial;
    const ExternalRenderTargetDescriptor target = frame.target;
    Base::Result<FenceValue> submitted = device.Submit(commands);
    if (!submitted) {
        static_cast<void>(DiscardFrame(frame));
        return submitted.GetStatus();
    }

    lastCapture_.backend = device.Backend().Kind();
    lastCapture_.signalFence = submitted.Value();
    lastCapture_.surfaceGeneration = surfaceGeneration;
    lastCapture_.frameSerial = frameSerial;
    lastCapture_.targetStableId = target.stableId;
    lastCapture_.width = target.width;
    lastCapture_.height = target.height;
    lastCapture_.commandCount = commands.CommandCount();
    lastCapture_.uploadByteCount = commands.UploadByteCount();
    lastCapture_.commandHash = commands.StableHash();
    lastCapture_.presented = false;

    Base::Result<void> presented = Present(frame, submitted.Value());
    if (!presented) {
        static_cast<void>(DiscardFrame(frame));
        return presented.GetStatus();
    }
    lastCapture_.presented = true;
    return submitted.Value();
}

} // namespace Aero::Graphics
