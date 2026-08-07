#include <Aero/Render/OpenGL33.hpp>
#include "render/private/BackendApi.hpp"

#include <utility>

namespace Aero::Render {

Base::Result<Base::Ref<Aero::RenderDevice>> CreateOpenGL33Device(
    const OpenGL33DeviceOptions& options,
    Base::IAllocator* allocator) noexcept {
    return ::Aero::Render::Detail::CreateOpenGL33Device(options, allocator);
}

Base::Result<Base::Ref<Aero::RenderTarget>>
CreateOpenGL33RenderTarget(
    Base::Ref<Aero::RenderDevice> device,
    const OpenGL33RenderTargetOptions& options,
    Base::IAllocator* allocator) noexcept {
    return ::Aero::Render::Detail::CreateOpenGL33EmbeddedSurface(
        std::move(device), options, allocator);
}

Base::Result<Base::Ref<Aero::RenderTarget>> CreateOpenGL33WindowSurface(
    const OpenGL33WindowSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    return ::Aero::Render::Detail::CreateOpenGL33WindowSurface(options, allocator);
}

} // namespace Aero::Render
