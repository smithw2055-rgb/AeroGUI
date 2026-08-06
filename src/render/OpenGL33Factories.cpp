#include <Aero/Render/OpenGL33.hpp>
#include "integration/BackendApi.hpp"

#include <utility>

namespace Aero::Render {

Base::Result<Base::Ref<Aero::RenderDevice>> CreateOpenGL33Device(
    const OpenGL33DeviceOptions& options,
    Base::IAllocator* allocator) noexcept {
    return Integration::CreateOpenGL33Device(options, allocator);
}

Base::Result<Base::Ref<Aero::RenderSurface>>
CreateOpenGL33EmbeddedSurface(
    Base::Ref<Aero::RenderDevice> device,
    const OpenGL33EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    return Integration::CreateOpenGL33EmbeddedSurface(
        std::move(device), options, allocator);
}

Base::Result<Base::Ref<Aero::RenderSurface>>
CreateOpenGL33EmbeddedSurface(
    const OpenGL33EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    return Integration::CreateOpenGL33EmbeddedSurface(options, allocator);
}

Base::Result<Base::Ref<Aero::RenderSurface>> CreateOpenGL33WindowSurface(
    const OpenGL33WindowSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    return Integration::CreateOpenGL33WindowSurface(options, allocator);
}

} // namespace Aero::Render
