#include <Aero/Render/OpenGL33.hpp>
#include "render/private/RenderDevice.hpp"

#include <utility>

namespace Aero::Render {

Base::Result<Base::Ref<Aero::RenderDevice>> CreateOpenGL33Device(
    const ::Aero::Render::OpenGL33DeviceOptions& options,
    Base::IAllocator* allocator) noexcept {
    return ::Aero::Render::Detail::CreateOpenGL33Device(options, allocator);
}

Base::Result<Base::Ref<Aero::RenderTarget>> CreateOpenGL33RenderTarget(
    Base::Ref<Aero::RenderDevice> device,
    const OpenGL33RenderTargetOptions& options,
    Base::IAllocator* allocator) noexcept {
    ::Aero::Render::Detail::OpenGL33EmbeddedTargetOptions native;
    native.acquireTarget = options.acquireTarget;
    native.callbackContext = options.callbackContext;
    native.targetContext = options.targetContext;
    return ::Aero::Render::Detail::CreateOpenGL33EmbeddedTarget(
        std::move(device), native, allocator);
}

} // namespace Aero::Render
