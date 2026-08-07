#include "RenderContext.hpp"
#include "render/private/BackendApi.hpp"

#include <utility>

namespace Aero::App::Detail {
namespace {

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status Unsupported(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::Unsupported, message);
}

} // namespace

Base::Result<void> RenderContext::Create(
    GraphicsBackend backend,
    Platform::NativeWindowHandle window,
    std::uint32_t width,
    std::uint32_t height,
    Base::IAllocator* allocator) noexcept {
    Shutdown();
    if (width == 0U || height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render context dimensions must be nonzero");
    }

    GraphicsBackend selected = backend;
    if (selected == GraphicsBackend::Automatic) {
#if defined(_WIN32)
        selected = GraphicsBackend::D3D11;
#else
        selected = GraphicsBackend::OpenGL33;
#endif
    }

#if defined(_WIN32)
    if (selected == GraphicsBackend::D3D11) {
#if AERO_APP_HAS_D3D11
        Render::Detail::D3D11WindowSurfaceOptions options;
        options.window = window;
        options.width = width;
        options.height = height;
        options.allowWarpFallback = true;
        Base::Result<Base::Ref<RenderTarget>> created =
            Render::Detail::CreateD3D11WindowSurface(options, allocator);
        if (!created) return created.GetStatus();
        target_ = std::move(created).Value();
        return {};
#else
        return Unsupported("D3D11 application backend is not enabled");
#endif
    }
#endif

    if (selected == GraphicsBackend::OpenGL33) {
#if AERO_APP_HAS_OPENGL_WINDOW
        Render::Detail::OpenGL33WindowSurfaceOptions options;
        options.window = window;
        options.width = width;
        options.height = height;
        Base::Result<Base::Ref<RenderTarget>> created =
            Render::Detail::CreateOpenGL33WindowSurface(options, allocator);
        if (!created) return created.GetStatus();
        target_ = std::move(created).Value();
        return {};
#else
        return Unsupported("OpenGL application backend is not enabled");
#endif
    }

    return Unsupported("Requested application graphics backend is unavailable");
}

Base::Result<void> RenderContext::Resize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    if (!target_) {
        return InvalidState("Application render context is not initialized");
    }
    return target_->Resize(width, height);
}

Base::Result<void> RenderContext::Render(IRenderer& renderer) noexcept {
    if (!IsReady()) {
        return InvalidState("Application render context is unavailable");
    }
    return renderer.Render(*target_);
}

void RenderContext::Shutdown() noexcept {
    if (target_) {
        Base::Ref<RenderDevice> device = target_->GetDevice();
        if (device) static_cast<void>(device->WaitIdle());
    }
    target_.Reset();
}

} // namespace Aero::App::Detail
