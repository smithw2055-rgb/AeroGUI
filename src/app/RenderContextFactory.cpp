#include "RenderContextFactory.hpp"

namespace Aero::App {
namespace {

Base::Status Unsupported(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::Unsupported, message);
}

} // namespace

Base::Result<Render::RenderContext*> CreateRenderContext(
    GraphicsBackend backend,
    Platform::NativeWindowHandle window,
    std::uint32_t width,
    std::uint32_t height,
    Base::IAllocator* allocator) noexcept {
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

    if (selected == GraphicsBackend::D3D11) {
#if defined(_WIN32) && AERO_APP_HAS_D3D11
        return Render::CreateD3D11RenderContext(
            window, width, height, allocator);
#else
        return Unsupported("D3D11 application graphics backend is unavailable");
#endif
    }

    if (selected == GraphicsBackend::OpenGL33) {
#if AERO_APP_HAS_OPENGL_WINDOW
        return Render::CreateOpenGL33RenderContext(
            window, width, height, allocator);
#else
        return Unsupported("OpenGL application graphics backend is unavailable");
#endif
    }

    return Unsupported("Requested application graphics backend is unavailable");
}

} // namespace Aero::App