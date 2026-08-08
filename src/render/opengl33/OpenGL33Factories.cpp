#include <Aero/Render/OpenGL33.hpp>
#include "render/RenderDeviceState.hpp"

#include <utility>

namespace Aero::Render {

Base::Result<Base::Ref<Aero::RenderTarget>> CreateOpenGL33RenderTarget(
    Base::Ref<Aero::RenderDevice> device,
    const OpenGL33RenderTargetOptions& options,
    Base::IAllocator* allocator) noexcept {
    ::Aero::Render::OpenGL33EmbeddedTargetOptions native;
    native.acquireTarget = options.acquireTarget;
    native.callbackContext = options.callbackContext;
    native.targetContext = options.targetContext;
    native.clearBeforeRender = options.clearBeforeRender;
    return ::Aero::Render::CreateOpenGL33EmbeddedTarget(
        std::move(device), native, allocator);
}

} // namespace Aero::Render
